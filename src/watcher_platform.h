#ifndef WATCHER_PLATFORM_H
#define WATCHER_PLATFORM_H

#include <stdint.h>

#define internal static
#define local_persist static
#define global_variable static

typedef int32_t bool32;
typedef float float32;

#if NAMELESS_WATCHER_SLOW
// TODO: Assert macro that takes a message as well
#define Assert(expression) if(!(expression)) {*(int *)0 = 0;}
#else
#define Assert(expression)
#endif

struct GameOffscreenBuffer {
    // Pixels are always 32-bits wide, in BB GG RR XX order
    void *memory;
    int width;
    int height;
    int pitch;
    int bytes_per_pixel;
};

#endif  // !WATCHER_PLATFORM_H
