#include <windows.h>

#include "watcher_platform.h"

typedef void GameUpdateAndRenderFunc(GameOffscreenBuffer* buffer);
typedef void GameGetSoundSamplesFunc();

#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH
struct Win32State {
    uint64_t total_size;
    void* game_memory_block;

    char executable_filename[WIN32_STATE_FILE_NAME_COUNT];
    char* one_past_last_executable_filename_slash;
};

struct Win32GameCode {
    HMODULE game_code_dll;
    FILETIME dll_last_write_time;

    // Note: Can be null, must check before calling
    GameUpdateAndRenderFunc* update_and_render;
    GameGetSoundSamplesFunc* get_sound_samples;

    bool32 is_valid;
};

struct Win32OffscreenBuffer {
    BITMAPINFO info;
    void* memory;
    int width;
    int height;
    int pitch;
    int bytes_per_pixel;
};

struct Win32WindowDimension {
    int width;
    int height;
};

// TODO: Make these not global?
global_variable bool32 g_is_running;
global_variable Win32OffscreenBuffer g_back_buffer;
WINDOWPLACEMENT g_previous_window_position = { sizeof(WINDOWPLACEMENT) };

extern "C" {
    _declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;  // Enable nVidia GPU selection system
}

internal void Win32GetExecutableFileName(Win32State* state) {
    DWORD size_of_filename = GetModuleFileNameA(0, state->executable_filename, sizeof(state->executable_filename));
    state->one_past_last_executable_filename_slash = state->executable_filename;

    // TODO: Error handling of size_of_filename == 0?

    for(char *scan_index = state->executable_filename; *scan_index; ++scan_index) {
        if(*scan_index == '\\') {
            state->one_past_last_executable_filename_slash = scan_index + 1;
        }
    }
}

internal int GetStringLength_(char* string) {
    Assert(string != NULL);
    int result = 0;

    while(*string++) {
        ++result;
    }

    return result;
}

internal void ConcatStrings_(
        size_t source_a_count, char* source_a,
        size_t source_b_count, char* source_b,
        size_t dest_count, char* dest) {
    Assert(dest_count >= source_a_count + source_b_count + 1);

    for(int index = 0; index < source_a_count; ++index) {
        *dest++ = *source_a++;
    }

    for(int index = 0; index < source_b_count; ++index) {
        *dest++ = *source_b++;
    }

    *dest++ = 0;
}

internal void Win32BuildExecutablePathFileName(Win32State* state, char* filename, int dest_count, char* dest) {
    ConcatStrings_(
        state->one_past_last_executable_filename_slash - state->executable_filename, state->executable_filename,
        GetStringLength_(filename), filename,
        dest_count, dest);
}

inline FILETIME Win32GetLastWriteTime(char* filename) {
    FILETIME result = {};
    WIN32_FILE_ATTRIBUTE_DATA data;

    if(GetFileAttributesEx(filename, GetFileExInfoStandard, &data)) {
        result = data.ftLastWriteTime;
    }

    return result;
}

internal Win32GameCode Win32LoadGameCode(char* source_dll_filename, char* temp_dll_filename, char* lock_filename) {
    Win32GameCode result = {};
    WIN32_FILE_ATTRIBUTE_DATA ignored_;

    if(!GetFileAttributesEx(lock_filename, GetFileExInfoStandard, &ignored_)) {
        result.dll_last_write_time = Win32GetLastWriteTime(source_dll_filename);

        CopyFile(source_dll_filename, temp_dll_filename, FALSE);

        result.game_code_dll = LoadLibraryA(temp_dll_filename);

        if(result.game_code_dll) {
            result.update_and_render = reinterpret_cast<GameUpdateAndRenderFunc*>(
                GetProcAddress(result.game_code_dll, "GameUpdateAndRender"));

            result.get_sound_samples = reinterpret_cast<GameGetSoundSamplesFunc*>(
                GetProcAddress(result.game_code_dll, "GameGetSoundSamples"));

            result.is_valid = result.update_and_render && result.get_sound_samples;
        }
    }

    if(!result.is_valid) {
        result.update_and_render = 0;
        result.get_sound_samples = 0;
    }

    return result;
}

internal void Win32UnloadGameCode(Win32GameCode* game_code) {
    Assert(game_code != NULL);

    if(game_code->game_code_dll) {
        FreeLibrary(game_code->game_code_dll);
        game_code->game_code_dll = 0;
    }

    game_code->is_valid = false;
    game_code->update_and_render = 0;
    game_code->get_sound_samples = 0;
}

Win32WindowDimension Win32GetWindowDimension(HWND window) {
    Win32WindowDimension result;

    RECT client_rect;
    GetClientRect(window, &client_rect);
    result.width = client_rect.right - client_rect.left;
    result.height = client_rect.bottom - client_rect.top;

    return result;
}

internal void Win32ResizeDibSection(Win32OffscreenBuffer* buffer, int width, int height) {
    if(buffer->memory) {
        VirtualFree(buffer->memory, 0, MEM_RELEASE);
    }

    buffer->width = width;
    buffer->height = height;
    buffer->bytes_per_pixel = 4;

    buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
    buffer->info.bmiHeader.biWidth = width;
    buffer->info.bmiHeader.biHeight = -height;
    buffer->info.bmiHeader.biPlanes = 1;
    buffer->info.bmiHeader.biBitCount = 32;
    buffer->info.bmiHeader.biCompression = BI_RGB;

    const int bitmap_memory_size = buffer->width * buffer->height * buffer->bytes_per_pixel;
    buffer->memory = VirtualAlloc(0, bitmap_memory_size, MEM_COMMIT, PAGE_READWRITE);

    buffer->pitch = width * buffer->bytes_per_pixel;
}

internal void Win32DisplayBufferInWindow(
        Win32OffscreenBuffer* buffer, HDC device_context, int window_width, int window_height) {
    int destination_width = buffer->width;
    int destination_height = buffer->height;

    while((window_width >= destination_width * 2) && (window_height >= destination_height * 2)) {
        destination_width *= 2;
        destination_height *= 2;
    }

    int destination_x = static_cast<int>((window_width - destination_width) * 0.5f);
    int destination_y = static_cast<int>((window_height - destination_height) * 0.5f);

    if(destination_x < 0) {
        destination_x = 0;
    }

    if(destination_y < 0) {
        destination_y = 0;
    }

    if(destination_height < window_height) {
        PatBlt(device_context, 0, 0, window_width, destination_y, BLACKNESS);
        PatBlt(device_context, 0, destination_y + destination_height, window_width, window_height, BLACKNESS);
    }

    if(destination_width < window_width) {
        PatBlt(device_context, 0, 0, destination_x, window_height, BLACKNESS);
        PatBlt(device_context, destination_x + destination_width, 0, window_width, window_height, BLACKNESS);
    }

    StretchDIBits(
        device_context,
        destination_x, destination_y, destination_width, destination_height,
        0, 0, buffer->width, buffer->height,
        buffer->memory, &buffer->info, DIB_RGB_COLORS, SRCCOPY);
}

internal void Win32ToggleFullscreen(HWND window) {
    // See: http://blogs.msdn.com/b/oldnewthing/archive/2010/04/12/9994016.aspx
    DWORD style = GetWindowLong(window, GWL_STYLE);

    if(style & WS_OVERLAPPEDWINDOW) {
        MONITORINFO monitor_info = { sizeof(MONITORINFO) };

        if(GetWindowPlacement(window, &g_previous_window_position) &&
                GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY), &monitor_info)) {
            SetWindowLong(window, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(
                window, HWND_TOP,
                monitor_info.rcMonitor.left, monitor_info.rcMonitor.top,
                monitor_info.rcMonitor.right - monitor_info.rcMonitor.left,
                monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top,
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    } else {
        SetWindowLong(window, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(window, &g_previous_window_position);
        SetWindowPos(
            window, 0, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

internal void Win32ProcessPendingMessages() {
    MSG message;

    while(PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
        switch(message.message) {
            case WM_QUIT: {
                g_is_running = false;
                break;
            }

            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP: {
                uint32_t vk_code = static_cast<uint32_t>(message.wParam);

                bool32 was_down = (message.lParam & (1 << 30)) != 0;
                bool32 is_down = (message.lParam & (1 << 31)) == 0;

                if(was_down != is_down) {
                    switch(vk_code) {
                        case 'W': {
                            break;
                        }
                        case 'A': {
                            break;
                        }
                        case 'S': {
                            break;
                        }
                        case 'D': {
                            break;
                        }
                        case 'Q': {
                            break;
                        }
                        case 'E': {
                            break;
                        }
                        case VK_UP: {
                            break;
                        }
                        case VK_LEFT: {
                            break;
                        }
                        case VK_DOWN: {
                            break;
                        }
                        case VK_RIGHT: {
                            break;
                        }
                        case VK_ESCAPE: {
                            break;
                        }
                        case VK_SPACE: {
                            break;
                        }
                    }

                    if(is_down) {
                        bool32 alt_key_is_down = message.lParam & (1 << 29);

                        if(vk_code == VK_F4 && alt_key_is_down) {
                            g_is_running = false;
                        }

                        if(vk_code == VK_RETURN && alt_key_is_down && message.hwnd) {
                            Win32ToggleFullscreen(message.hwnd);
                        }
                    }
                }

                break;
            }

            default: {
                TranslateMessage(&message);
                DispatchMessageA(&message);
                break;
            }
        }
    }
}

internal LRESULT CALLBACK Win32MainWindowCallback(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
    LRESULT result = 0;

    switch(message) {
        case WM_CLOSE: {
            g_is_running = false;
            break;
        }

        case WM_SETCURSOR: {
            SetCursor(0);
            break;
        }

        case WM_ACTIVATEAPP: {
            break;
        }

        case WM_DESTROY: {
            g_is_running = false;
            break;
        }

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP: {
            Assert(!"Keyboard input came in through a non-dispatch message!");
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT paint;
            HDC device_context = BeginPaint(window, &paint);
            Win32WindowDimension dimension = Win32GetWindowDimension(window);
            Win32DisplayBufferInWindow(&g_back_buffer, device_context, dimension.width, dimension.height);
            EndPaint(window, &paint);
            break;
        }

        default: {
            result = DefWindowProc(window, message, w_param, l_param);
            break;
        }
    }

    return result;
}

int CALLBACK WinMain(HINSTANCE instance, HINSTANCE _unused, LPSTR command_line, int show_code) {
    Win32State win32_state = {};

    Win32GetExecutableFileName(&win32_state);

    char source_game_code_dll_full_path[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildExecutablePathFileName(
        &win32_state, "watcher.dll", sizeof(source_game_code_dll_full_path), source_game_code_dll_full_path);

    char temp_game_code_dll_full_path[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildExecutablePathFileName(
        &win32_state, "watcher_temp.dll", sizeof(temp_game_code_dll_full_path), temp_game_code_dll_full_path);

    char game_code_lock_full_path[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildExecutablePathFileName(
        &win32_state, "lock.tmp", sizeof(game_code_lock_full_path), game_code_lock_full_path);

    WNDCLASS window_class = {};

    Win32ResizeDibSection(&g_back_buffer, 960, 540);

    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = Win32MainWindowCallback;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursor(0, IDC_ARROW);
    // window_class.hIcon = ;
    window_class.lpszClassName = "NamelessWatcherWindowClass";

    if(!RegisterClass(&window_class)) {
        // TODO: Log
        return -1;
    }

    HWND window = CreateWindowEx(
        0, window_class.lpszClassName, "Nameless Watcher", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, instance, 0);

    if(!window) {
        // TODO: Log
        return -1;
    }

    g_is_running = true;

    Win32GameCode game = Win32LoadGameCode(
        source_game_code_dll_full_path, temp_game_code_dll_full_path, game_code_lock_full_path);

    while(g_is_running) {
        FILETIME new_dll_write_time = Win32GetLastWriteTime(source_game_code_dll_full_path);

        if(CompareFileTime(&new_dll_write_time, &game.dll_last_write_time) != 0) {
            Win32UnloadGameCode(&game);
            game = Win32LoadGameCode(
                source_game_code_dll_full_path, temp_game_code_dll_full_path, game_code_lock_full_path);
        }

        Win32ProcessPendingMessages();

        GameOffscreenBuffer buffer = {};
        buffer.memory = g_back_buffer.memory;
        buffer.width = g_back_buffer.width; 
        buffer.height = g_back_buffer.height;
        buffer.pitch = g_back_buffer.pitch;
        buffer.bytes_per_pixel = g_back_buffer.bytes_per_pixel;

        if(game.update_and_render) {
            game.update_and_render(&buffer);
        }

        HDC device_context = GetDC(window);
        Win32WindowDimension dimension = Win32GetWindowDimension(window);
        Win32DisplayBufferInWindow(&g_back_buffer, device_context, dimension.width, dimension.height);
        ReleaseDC(window, device_context);
    }

    return 0;
}
