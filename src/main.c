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
        hardware_poll_input();

        if (hardware_get_switch(1)) cam.angle = (cam.angle + turn_speed) & ANGLE_MASK;
        if (hardware_get_switch(2)) cam.angle = (cam.angle - turn_speed) & ANGLE_MASK;

        if (hardware_get_switch(0)) {
            cam.x += fix_mul(fix_sin(cam.angle), move_speed);
            cam.y += fix_mul(fix_cos(cam.angle), move_speed);

            int gx = FIX_TO_INT(cam.x);
            int gy = FIX_TO_INT(cam.y);
            if (gy >= CHUNK_SIZE) {
                world_advance(&world, DIR_NORTH);
                cam.y -= INT_TO_FIX(CHUNK_SIZE);
            } else if (gx >= CHUNK_SIZE * 2) {
                world_advance(&world, DIR_EAST);
                cam.x = INT_TO_FIX(PLAYER_START_X) + FIX_HALF;
                cam.y = FIX_HALF;
                cam.angle = 0;
            } else if (gx < CHUNK_SIZE) {
                world_advance(&world, DIR_WEST);
                cam.x = INT_TO_FIX(PLAYER_START_X) + FIX_HALF;
                cam.y = FIX_HALF;
                cam.angle = 0;
            }
        }

        if (hardware_btn_down())         cam.z =  FIX_HALF / 2;
        else if (hardware_get_switch(3)) cam.z = -FIX_HALF / 2;
        else                             cam.z =  0;

        uint32_t t0 = hardware_get_cycles();
        renderer_draw_frame(hardware_get_backbuffer(), &world, &cam);
        uint32_t render_cycles = hardware_get_cycles() - t0;

        if (render_cycles > 0) {
            hardware_display_number(30000000u / render_cycles);
        }

        hardware_swap_buffers();
    }
}