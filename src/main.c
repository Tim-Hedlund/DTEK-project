#include "hardware.h"
#include "dtekv-lib.h"

void handle_interrupt(unsigned _irq) {}

static void fill_screen(uint8_t *buf, uint8_t color) {
    uint32_t word = color | ((uint32_t)color << 8) | ((uint32_t)color << 16) | ((uint32_t)color << 24);
    uint32_t *buf32 = (uint32_t *)buf;
    for (int i = 0; i < (SCREEN_W * SCREEN_H) / 4; i++) {
        buf32[i] = word;
    }
}

int main(void) {
    hardware_init();
    print("Hardware test running!\n");

    const uint8_t colors[4] = { 0xE0, 0x1C, 0x03, 0xFC };
    unsigned int clicks = 0;

    while (1) {
        hardware_poll_input();

        hardware_set_leds(hardware_get_switches());

        if (hardware_btn_pressed()) {
            clicks++;
            hardware_display_number(clicks);
        }

        uint8_t *screen = hardware_get_backbuffer();
        fill_screen(screen, colors[clicks & 3]);
        hardware_swap_buffers();
    }
}

/*

#include <stdint.h>
#include "dtekv-lib.h" //den här är rätt även om den flaggas som inkorrekt

#define SCREEN_W 320
#define SCREEN_H 240
#define VGA_BUFFER ((volatile uint8_t *)0x08000000)

int main(void) {
    print("Drawing VGA test pattern...\n");

    // 1. Fill the screen with 3 horizontal color bands
    for (int y = 0; y < SCREEN_H; y++) {
        uint8_t color;
        if (y < 80) {
            color = 0xE0; // Top third: Red (if RRRGGGBB)
        } else if (y < 160) {
            color = 0x1C; // Middle third: Green
        } else {
            color = 0x03; // Bottom third: Blue
        }

        for (int x = 0; x < SCREEN_W; x++) {
            VGA_BUFFER[y * SCREEN_W + x] = color;
        }
    }

    // 2. Draw a 40x40 white square in the exact center
    for (int y = 100; y < 140; y++) {
        for (int x = 140; x < 180; x++) {
            VGA_BUFFER[y * SCREEN_W + x] = 0xFF; // White
        }
    }

    print("VGA test done!\n");

    while (1) {}
}*/