#include "hardware.h"
#include "world.h"
#include "fixmath.h"
#include "renderer.h"
#include "dtekv-lib.h"
#include "assets.h"

void handle_interrupt(unsigned _irq) {}

static void print_uint(uint32_t v) {
    char buf[12], out[12];
    int i = 0;
    if (v == 0) {
        print("0");
        return;
    }
    while (v > 0) {
        buf[i++] = (char)('0' + (v % 10));
        v /= 10;
    }
    for (int j = 0; j < i; j++) out[j] = buf[i - 1 - j];
    out[i] = '\0';
    print(out);
}

static void print_stat_line(char *label, uint32_t cyc, uint32_t total) {
    uint32_t pct = (total > 0) ? ((cyc * 100u) / total) : 0;
    print(label);
    print_uint(cyc);
    print(" cyc (");
    print_uint(pct);
    print("%)\n");
}

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

    const fix_t move_speed = FIX_ONE / 16;
    const int   turn_speed = 8;
    uint32_t    frame_count = 0;

    //display main menu/intro screen
    renderer_draw_image(hardware_get_backbuffer(), assets_get_title_screen());
    hardware_swap_buffers();

    while (1) {
        hardware_poll_input();

        if (hardware_btn_down()) {
            break;
        }
    }

    while (1) {

        //INPUT:
        hardware_poll_input();

        //Turning (bad as it doesnt lock the player to the center and only uses the switch)
        if (hardware_get_switch(1)) cam.angle = (cam.angle + turn_speed) & ANGLE_MASK;
        if (hardware_get_switch(2)) cam.angle = (cam.angle - turn_speed) & ANGLE_MASK;

        //move forward, should be permanent and move_speed should increase over time, should probably be moved to player
        if (hardware_get_switch(0)) {
            cam.x += fix_mul(fix_sin(cam.angle), move_speed);
            cam.y += fix_mul(fix_cos(cam.angle), move_speed);

            int gx = FIX_TO_INT(cam.x);
            int gy = FIX_TO_INT(cam.y);
            if (gy >= CHUNK_SIZE) {
                world_advance(&world, DIR_NORTH);
                cam.y -= INT_TO_FIX(CHUNK_SIZE);
            } else if (gx >= CHUNK_SIZE * 3) {
                world_advance(&world, DIR_EAST);
                cam.x = INT_TO_FIX(PLAYER_START_X) + FIX_HALF;
                cam.y = FIX_HALF;
                cam.angle = 0;
            } else if (gx < CHUNK_SIZE * 2) {
                world_advance(&world, DIR_WEST);
                cam.x = INT_TO_FIX(PLAYER_START_X) + FIX_HALF;
                cam.y = FIX_HALF;
                cam.angle = 0;
            }
        }

        //basic jump
        if (hardware_btn_down())         cam.z =  FIX_HALF / 2;
        
        //basic duck
        else if (hardware_get_switch(3)) cam.z = -FIX_HALF / 2;

        //standing
        else                             cam.z =  0;

        //theme selector, dont quite know how this should work when finished
        int theme_id = THEME_BRICK;
        if (hardware_get_switch(4)) {
            theme_id = THEME_MOSSY;
        }

        //RENDERING:
        RenderStats stats;
        renderer_draw_frame(hardware_get_backbuffer(), &world, &cam, theme_id, &stats);

        //fps display on 7segment
        if (stats.total_cycles > 0) {
            hardware_display_number(30000000u / stats.total_cycles);
        }

        //prints frame data every 60 frames
        if ((frame_count++ % 60) == 0) {
            print("--- Frame Profile ---\n");
            print_stat_line("BG fill : ", stats.bg_cycles,    stats.total_cycles);
            print_stat_line("DDA     : ", stats.dda_cycles,   stats.total_cycles);
            print_stat_line("Draw    : ", stats.draw_cycles,  stats.total_cycles);
            print_stat_line("Total   : ", stats.total_cycles, stats.total_cycles);
        }


        hardware_swap_buffers();
    }
}