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

void GameUpdateAndRender(GameOffscreenBuffer* buffer) {
    ++g_x_offset;
    ++g_y_offset;
    RenderWeirdGradient(buffer, g_x_offset, g_y_offset);
}

void GameGetSoundSamples() {
}
