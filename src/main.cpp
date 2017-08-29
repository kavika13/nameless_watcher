#include "watcher_platform.h"

// TODO: Correct keywords
static int g_x_offset;
static int g_y_offset;

internal void RenderWeirdGradient(GameOffscreenBuffer* buffer, int x_offset, int y_offset) {
    uint8_t* row = static_cast<uint8_t*>(buffer->memory);

    for(int y = 0; y < buffer->height; ++y) {
        uint32_t* pixel = reinterpret_cast<uint32_t*>(row);

        for(int x = 0; x < buffer->width; ++x) {
            *pixel++ = ((x + x_offset) << 8) | (y + y_offset);
        }

        row += buffer->pitch;
    }
}

void GameUpdateAndRender(GameInput* input, GameOffscreenBuffer* buffer) {
    bool is_up_pressed = false;
    bool is_down_pressed = false;
    bool is_left_pressed = false;
    bool is_right_pressed = false;

    for(int i = 0; i < NUM_SUPPORTED_CONTROLLERS; ++i) {
        GameControllerInput* controller = GetController(input, i);

        if(controller->is_connected) {
            if(controller->move_up.ended_down) {
                is_up_pressed = true;
            }

            if(controller->move_down.ended_down) {
                is_down_pressed = true;
            }

            if(controller->move_left.ended_down) {
                is_left_pressed = true;
            }

            if(controller->move_right.ended_down) {
                is_right_pressed = true;
            }
        }
    }

    if(is_up_pressed) {
        --g_y_offset;
    }

    if(is_down_pressed) {
        ++g_y_offset;
    }

    if(is_left_pressed) {
        --g_x_offset;
    }

    if(is_right_pressed) {
        ++g_x_offset;
    }

    RenderWeirdGradient(buffer, g_x_offset, g_y_offset);
}

void GameGetSoundSamples() {
}
