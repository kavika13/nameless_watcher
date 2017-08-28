#include <windows.h>
#include <stdint.h>

#define internal static
#define local_persist static
#define global_variable static

struct win32_offscreen_buffer {
    BITMAPINFO info;
    void* memory;
    int width;
    int height;
    int pitch;
    int bytes_per_pixel;
};

struct win32_window_dimension {
    int width;
    int height;
};

// TODO: Make these not global?
global_variable bool g_is_running;
global_variable win32_offscreen_buffer g_back_buffer;

win32_window_dimension Win32GetWindowDimension(HWND window) {
    win32_window_dimension result;

    RECT client_rect;
    GetClientRect(window, &client_rect);
    result.width = client_rect.right - client_rect.left;
    result.height = client_rect.bottom - client_rect.top;

    return result;
}

internal void RenderWeirdGradient(win32_offscreen_buffer buffer, int x_offset, int y_offset) {
    uint8_t* row = static_cast<uint8_t*>(buffer.memory);

    for(int y = 0; y < buffer.height; ++y) {
        uint32_t* pixel = reinterpret_cast<uint32_t*>(row);

        for(int x = 0; x < buffer.width; ++x) {
            *pixel++ = ((x + x_offset) << 8) | (y + y_offset);
        }

        row += buffer.pitch;
    }
}

internal void Win32ResizeDibSection(win32_offscreen_buffer* buffer, int width, int height) {
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
        HDC device_context, win32_offscreen_buffer buffer, int window_width, int window_height, int x, int y, int width, int height) {
    // TODO: Correct aspect ratio
    StretchDIBits(
        device_context,
        0, 0, window_width, window_height,
        0, 0, buffer.width, buffer.height,
        buffer.memory, &buffer.info, DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK Win32MainWindowCallback(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
    LRESULT result = 0;

    switch(message) {
        case WM_SIZE: {
            break;
        }

        case WM_ACTIVATEAPP: {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
            break;
        }

        case WM_CLOSE: {
            g_is_running = false;
            break;
        }

        case WM_DESTROY: {
            g_is_running = false;
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT paint;
            HDC device_context = BeginPaint(window, &paint);
            int x = paint.rcPaint.left;
            int y = paint.rcPaint.top;
            int width = paint.rcPaint.right - paint.rcPaint.left;
            int height = paint.rcPaint.bottom - paint.rcPaint.top;

            win32_window_dimension dimension = Win32GetWindowDimension(window);

            Win32DisplayBufferInWindow(device_context, g_back_buffer, dimension.width, dimension.height, x, y, width, height);
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

    Win32ResizeDibSection(&g_back_buffer, 1280, 720);

    window_class.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = Win32MainWindowCallback;
    window_class.hInstance = instance;
    // window_class.hIcon = ;
    window_class.lpszClassName = "NamelessWatcherWindowClass";

    if(RegisterClass(&window_class)) {
        HWND window = CreateWindowEx(
            0, window_class.lpszClassName, "Nameless Watcher", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, instance, 0);

        if(window) {
            g_is_running = true;
            int x_offset = 0;
            int y_offset = 0;

            while(g_is_running) {
                MSG message;

                while(PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
                    if(message.message == WM_QUIT) {
                        g_is_running = false;
                    }

                    TranslateMessage(&message);
                    DispatchMessage(&message);
                }

                RenderWeirdGradient(g_back_buffer, x_offset, y_offset);

                HDC device_context = GetDC(window);
                win32_window_dimension dimension = Win32GetWindowDimension(window);
                Win32DisplayBufferInWindow(
                    device_context, g_back_buffer, dimension.width, dimension.height,
                    0, 0, dimension.width, dimension.height);
                ReleaseDC(window, device_context);

                ++x_offset;
                ++y_offset;
            }
        } else {
            // TODO: Log
        }
    } else {
        // TODO: Log
    }

    return 0;
}
