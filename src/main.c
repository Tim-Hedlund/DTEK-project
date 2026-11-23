#include <stdint.h>
#include "dtekv-lib.h"

#define SCREEN_W 320
#define SCREEN_H 240
#define VGA_BUFFER ((volatile uint8_t *)0x08000000)

// Required by boot.S for external interrupts
void handle_interrupt(unsigned _irq) {}

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
}