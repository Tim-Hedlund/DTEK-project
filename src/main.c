#include "hardware.h"
#include "world.h"
#include "fixmath.h"
#include "renderer.h"

void handle_interrupt(unsigned _irq) {}

int main(void) {
    hardware_init();

    World world;
    world_init(&world, hardware_get_cycles());

    Camera cam = {
        .x     = INT_TO_FIX(PLAYER_START_X) + FIX_HALF,
        .y     = INT_TO_FIX(PLAYER_START_Y) + FIX_HALF,
        .z     = 0,
        .angle = 0
    };

    const fix_t move_speed = FIX_ONE / 16; // 0.0625 tiles per frame
    const int   turn_speed = 8;            // ~3 deg per frame

    while (1) {
        renderer_draw_frame(hardware_get_backbuffer(), &world, &cam);
    }
}