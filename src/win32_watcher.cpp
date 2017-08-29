#include <windows.h>
#include <xinput.h>

#include "watcher_platform.h"

// Dynamically loaded XInput functions
typedef DWORD WINAPI XInputGetStateFunc(DWORD dwUserIndex, XINPUT_STATE* pState);
DWORD WINAPI XInputGetStateStub(DWORD dwUserIndex, XINPUT_STATE* pState) {
    return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable XInputGetStateFunc* XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_


typedef DWORD WINAPI XInputSetStateFunc(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration);
DWORD WINAPI XInputSetStateStub(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration) {
    return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable XInputSetStateFunc* XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

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

    for(char* scan_index = state->executable_filename; *scan_index; ++scan_index) {
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

internal void Win32LoadXInput() {
    HMODULE x_input_library = LoadLibraryA("xinput1_4.dll");

    if(!x_input_library) {
        // TODO: Log
        x_input_library = LoadLibraryA("xinput9_1_0.dll");
    }

    if(!x_input_library) {
        // TODO: Log
        x_input_library = LoadLibraryA("xinput1_3.dll");
    }

    if(x_input_library) {
        // TODO: Log

        XInputGetState = reinterpret_cast<XInputGetStateFunc*>(GetProcAddress(x_input_library, "XInputGetState"));

        if(!XInputGetState) {
            // TODO: Log
            XInputGetState = XInputGetStateStub;
        }

        XInputSetState = reinterpret_cast<XInputSetStateFunc*>(GetProcAddress(x_input_library, "XInputSetState"));

        if(!XInputSetState) {
            // TODO: Log
            XInputSetState = XInputSetStateStub;
        }
    } else {
        // TODO: Log
    }
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

internal void Win32ProcessKeyboardMessage(GameButtonState* new_state, bool32 is_down) {
    if(new_state->ended_down != is_down) {
        new_state->ended_down = is_down;
        ++new_state->half_transition_count;
    }
}

internal void Win32ProcessXInputDigitalButton(
        DWORD x_input_button_state, GameButtonState* old_state, DWORD button_bit, GameButtonState* new_state) {
    new_state->ended_down = (x_input_button_state & button_bit) == button_bit;
    new_state->half_transition_count = (old_state->ended_down != new_state->ended_down) ? 1 : 0;
}

internal float32 Win32ProcessXInputStickValue(SHORT value, SHORT dead_zone_threshold) {
    float32 result = 0;

    if(value < -dead_zone_threshold) {
        result = static_cast<float32>((value + dead_zone_threshold) / (32768.0f - dead_zone_threshold));
    } else if(value > dead_zone_threshold) {
        result = static_cast<float32>((value - dead_zone_threshold) / (32767.0f - dead_zone_threshold));
    }

    return result;
}

internal void Win32ProcessPendingMessages(GameControllerInput* keyboard_controller) {
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
                            Win32ProcessKeyboardMessage(&keyboard_controller->move_up, is_down);
                            break;
                        }
                        case 'A': {
                            Win32ProcessKeyboardMessage(&keyboard_controller->move_left, is_down);
                            break;
                        }
                        case 'S': {
                            Win32ProcessKeyboardMessage(&keyboard_controller->move_down, is_down);
                            break;
                        }
                        case 'D': {
                            Win32ProcessKeyboardMessage(&keyboard_controller->move_right, is_down);
                            break;
                        }
                        case 'Q': {
                            Win32ProcessKeyboardMessage(&keyboard_controller->left_shoulder, is_down);
                            break;
                        }
                        case 'E': {
                            Win32ProcessKeyboardMessage(&keyboard_controller->right_shoulder, is_down);
                            break;
                        }
                        case VK_UP: {
                            Win32ProcessKeyboardMessage(&keyboard_controller->action_up, is_down);
                            break;
                        }
                        case VK_LEFT: {
                            Win32ProcessKeyboardMessage(&keyboard_controller->action_left, is_down);
                            break;
                        }
                        case VK_DOWN: {
                            Win32ProcessKeyboardMessage(&keyboard_controller->action_down, is_down);
                            break;
                        }
                        case VK_RIGHT: {
                            Win32ProcessKeyboardMessage(&keyboard_controller->action_right, is_down);
                            break;
                        }
                        case VK_ESCAPE: {
                            Win32ProcessKeyboardMessage(&keyboard_controller->back, is_down);
                            break;
                        }
                        case VK_SPACE: {
                            Win32ProcessKeyboardMessage(&keyboard_controller->start, is_down);
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

    Win32LoadXInput();

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

    GameInput input[2] = {};
    GameInput* new_input = &input[0];
    GameInput* old_input = &input[1];

    Win32GameCode game = Win32LoadGameCode(
        source_game_code_dll_full_path, temp_game_code_dll_full_path, game_code_lock_full_path);

    while(g_is_running) {
        FILETIME new_dll_write_time = Win32GetLastWriteTime(source_game_code_dll_full_path);

        if(CompareFileTime(&new_dll_write_time, &game.dll_last_write_time) != 0) {
            Win32UnloadGameCode(&game);
            game = Win32LoadGameCode(
                source_game_code_dll_full_path, temp_game_code_dll_full_path, game_code_lock_full_path);
        }

        GameControllerInput* old_keyboard_controller = GetController(old_input, 0);
        GameControllerInput* new_keyboard_controller = GetController(new_input, 0);
        *new_keyboard_controller = {};
        new_keyboard_controller->is_connected = true;

        for(int button_index = 0; button_index < NUM_SUPPORTED_CONTROLLER_BUTTONS; ++button_index) {
            new_keyboard_controller->buttons[button_index].ended_down =
                old_keyboard_controller->buttons[button_index].ended_down;
        }

        Win32ProcessPendingMessages(new_keyboard_controller);

        POINT mouse_pos;
        GetCursorPos(&mouse_pos);
        ScreenToClient(window, &mouse_pos);

        new_input->mouse_x = mouse_pos.x;
        new_input->mouse_y = mouse_pos.y;
        new_input->mouse_z = 0;

        Win32ProcessKeyboardMessage(&new_input->mouse_buttons[0], GetKeyState(VK_LBUTTON) & (1 << 15));
        Win32ProcessKeyboardMessage(&new_input->mouse_buttons[1], GetKeyState(VK_MBUTTON) & (1 << 15));
        Win32ProcessKeyboardMessage(&new_input->mouse_buttons[2], GetKeyState(VK_RBUTTON) & (1 << 15));
        Win32ProcessKeyboardMessage(&new_input->mouse_buttons[3], GetKeyState(VK_XBUTTON1) & (1 << 15));
        Win32ProcessKeyboardMessage(&new_input->mouse_buttons[4], GetKeyState(VK_XBUTTON2) & (1 << 15));

        DWORD MaxControllerCount = XUSER_MAX_COUNT;

        for (DWORD controller_index = 0; controller_index < MaxControllerCount; ++controller_index) {
            DWORD our_controller_index = controller_index + 1;
            GameControllerInput* old_controller = GetController(old_input, our_controller_index);
            GameControllerInput* new_controller = GetController(new_input, our_controller_index);

            XINPUT_STATE controller_state;

            if(XInputGetState(controller_index, &controller_state) == ERROR_SUCCESS) {
                new_controller->is_connected = true;
                new_controller->is_analog = old_controller->is_analog;

                XINPUT_GAMEPAD* pad = &controller_state.Gamepad;

                new_controller->stick_average_x = Win32ProcessXInputStickValue(
                    pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                new_controller->stick_average_y = Win32ProcessXInputStickValue(
                    pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

                if(new_controller->stick_average_x != 0.0f || new_controller->stick_average_y != 0.0f) {
                    new_controller->is_analog = true;
                }

                if(pad->wButtons & XINPUT_GAMEPAD_DPAD_UP) {
                    new_controller->stick_average_y = 1.0f;
                    new_controller->is_analog = false;
                }

                if(pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
                    new_controller->stick_average_y = -1.0f;
                    new_controller->is_analog = false;
                }

                if(pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
                    new_controller->stick_average_x = -1.0f;
                    new_controller->is_analog = false;
                }

                if(pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
                    new_controller->stick_average_x = 1.0f;
                    new_controller->is_analog = false;
                }

                float32 stick_direction_threshold = 0.5f;
                Win32ProcessXInputDigitalButton(
                    (new_controller->stick_average_x < -stick_direction_threshold) ? 1 : 0,
                    &old_controller->move_left, 1, &new_controller->move_left);
                Win32ProcessXInputDigitalButton(
                    (new_controller->stick_average_x > stick_direction_threshold) ? 1 : 0,
                    &old_controller->move_right, 1, &new_controller->move_right);
                Win32ProcessXInputDigitalButton(
                    (new_controller->stick_average_y < -stick_direction_threshold) ? 1 : 0,
                    &old_controller->move_down, 1, &new_controller->move_down);
                Win32ProcessXInputDigitalButton(
                    (new_controller->stick_average_y > stick_direction_threshold) ? 1 : 0,
                    &old_controller->move_up, 1, &new_controller->move_up);

                Win32ProcessXInputDigitalButton(
                    pad->wButtons, &old_controller->action_down, XINPUT_GAMEPAD_A, &new_controller->action_down);
                Win32ProcessXInputDigitalButton(
                    pad->wButtons, &old_controller->action_right, XINPUT_GAMEPAD_B, &new_controller->action_right);
                Win32ProcessXInputDigitalButton(
                    pad->wButtons, &old_controller->action_left, XINPUT_GAMEPAD_X, &new_controller->action_left);
                Win32ProcessXInputDigitalButton(
                    pad->wButtons, &old_controller->action_up, XINPUT_GAMEPAD_Y, &new_controller->action_up);
                Win32ProcessXInputDigitalButton(
                    pad->wButtons,
                    &old_controller->left_shoulder, XINPUT_GAMEPAD_LEFT_SHOULDER, &new_controller->left_shoulder);
                Win32ProcessXInputDigitalButton(
                    pad->wButtons,
                    &old_controller->right_shoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER, &new_controller->right_shoulder);

                Win32ProcessXInputDigitalButton(
                    pad->wButtons, &old_controller->start, XINPUT_GAMEPAD_START, &new_controller->start);
                Win32ProcessXInputDigitalButton(
                    pad->wButtons, &old_controller->back, XINPUT_GAMEPAD_BACK, &new_controller->back);
            } else {
                new_controller->is_connected = false;
            }
        }

        GameOffscreenBuffer offscreen_buffer = {};
        offscreen_buffer.memory = g_back_buffer.memory;
        offscreen_buffer.width = g_back_buffer.width;
        offscreen_buffer.height = g_back_buffer.height;
        offscreen_buffer.pitch = g_back_buffer.pitch;
        offscreen_buffer.bytes_per_pixel = g_back_buffer.bytes_per_pixel;

        if(game.update_and_render) {
            game.update_and_render(new_input, &offscreen_buffer);
        }

        HDC device_context = GetDC(window);
        Win32WindowDimension dimension = Win32GetWindowDimension(window);
        Win32DisplayBufferInWindow(&g_back_buffer, device_context, dimension.width, dimension.height);
        ReleaseDC(window, device_context);

		GameInput* temp_input = new_input;
		new_input = old_input;
		old_input = temp_input;
    }

    return 0;
}
