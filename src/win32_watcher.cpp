#include <windows.h>
#include <stdint.h>

#define internal static
#define local_persist static
#define global_variable static

typedef int32_t bool32;
typedef float float32;

#if NAMELESS_WATCHER_SLOW
#define Assert(expression) if(!(expression)) {*(int *)0 = 0;}
#else
#define Assert(expression)
#endif

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

Win32WindowDimension Win32GetWindowDimension(HWND window) {
    Win32WindowDimension result;

    RECT client_rect;
    GetClientRect(window, &client_rect);
    result.width = client_rect.right - client_rect.left;
    result.height = client_rect.bottom - client_rect.top;

    return result;
}

internal void RenderWeirdGradient(Win32OffscreenBuffer buffer, int x_offset, int y_offset) {
    uint8_t* row = static_cast<uint8_t*>(buffer.memory);

    for(int y = 0; y < buffer.height; ++y) {
        uint32_t* pixel = reinterpret_cast<uint32_t*>(row);

        for(int x = 0; x < buffer.width; ++x) {
            *pixel++ = ((x + x_offset) << 8) | (y + y_offset);
        }

        row += buffer.pitch;
    }
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

internal void ToggleFullscreen(HWND window) {
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
                            ToggleFullscreen(message.hwnd);
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
    int x_offset = 0;
    int y_offset = 0;

    while(g_is_running) {
        Win32ProcessPendingMessages();

        RenderWeirdGradient(g_back_buffer, x_offset, y_offset);

        HDC device_context = GetDC(window);
        Win32WindowDimension dimension = Win32GetWindowDimension(window);
        Win32DisplayBufferInWindow(&g_back_buffer, device_context, dimension.width, dimension.height);
        ReleaseDC(window, device_context);

        ++x_offset;
        ++y_offset;
    }

    return 0;
}
