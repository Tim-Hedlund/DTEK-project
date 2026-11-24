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