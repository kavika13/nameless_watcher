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

struct GameButtonState {
    int half_transition_count;
    bool32 ended_down;
};

#define NUM_SUPPORTED_CONTROLLER_BUTTONS 12

struct GameControllerInput {
    bool32 is_connected;
    bool32 is_analog;
    float32 stick_average_x;
    float32 stick_average_y;

    union {
        GameButtonState buttons[NUM_SUPPORTED_CONTROLLER_BUTTONS];

        struct {
            GameButtonState move_up;
            GameButtonState move_down;
            GameButtonState move_left;
            GameButtonState move_right;

            GameButtonState action_up;
            GameButtonState action_down;
            GameButtonState action_left;
            GameButtonState action_right;

            GameButtonState left_shoulder;
            GameButtonState right_shoulder;

            GameButtonState back;
            GameButtonState start;

            // All buttons must be added above this line

            GameButtonState terminator;
        };
    };
};

#define NUM_SUPPORTED_MOUSE_BUTTONS 5
#define NUM_SUPPORTED_CONTROLLERS 5

struct GameInput {
    GameButtonState mouse_buttons[NUM_SUPPORTED_MOUSE_BUTTONS];
    int32_t mouse_x, mouse_y, mouse_z;

    float32 delta_time_for_frame;

    GameControllerInput controllers[NUM_SUPPORTED_CONTROLLERS];
};

typedef void GameUpdateAndRenderFunc(GameInput* input, GameOffscreenBuffer* buffer);
typedef void GameGetSoundSamplesFunc();

inline GameControllerInput* GetController(GameInput* input, int unsigned controller_index) {
    Assert(controller_index < NUM_SUPPORTED_CONTROLLERS);

    GameControllerInput* result = &input->controllers[controller_index];

    return result;
}

#endif  // !WATCHER_PLATFORM_H
