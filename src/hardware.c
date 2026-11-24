#include "hardware.h"


// Address list
#define LEDS_ADDR       ((volatile uint32_t *)0x04000000)
#define SWITCHES_ADDR   ((volatile uint32_t *)0x04000010)
#define SEG7_BASE_ADDR   0x04000050
#define SEG7_STRIDE      0x10
#define BUTTON_ADDR     ((volatile uint32_t *)0x040000d0)

#define VGA_DMA         ((volatile uint32_t *)0x04000100)
#define VGA_BUF_0       ((uint8_t *)0x08000000)
#define VGA_BUF_1       ((uint8_t *)(0x08000000 + SCREEN_W * SCREEN_H))

// 7-segment bit patterns
static const uint8_t SEG7_LUT[16] = {
    0xC0, // 0
    0xF9, // 1
    0xA4, // 2
    0xB0, // 3
    0x99, // 4
    0x92, // 5
    0x82, // 6
    0xF8, // 7
    0x80, // 8
    0x90, // 9
};
#define SEG7_BLANK 0xFF


// Private Module State
static uint8_t *back_buffer = VGA_BUF_1;
static int btn_now = 0;
static int btn_prev = 0;
static uint16_t switches_now = 0;

// Last outputs to not push redundant writes
static uint32_t last_leds = 0xFFFFFFFF;
static uint32_t last_number = 0xFFFFFFFF;


// Public Functions
void hardware_init(void) {
    volatile uint32_t *buffer0 = (volatile uint32_t *)VGA_BUF_0;
    volatile uint32_t *buffer1 = (volatile uint32_t *)VGA_BUF_1;
    for (int i = 0; i < (SCREEN_W * SCREEN_H) / 4; i++) {
        buffer0[i] = 0;
        buffer1[i] = 0;
    }

    // Display BUF_0 first, draw into BUF_1
    back_buffer = VGA_BUF_1;
    VGA_DMA[1] = (uint32_t)VGA_BUF_0;
    VGA_DMA[0] = 0;
    while (VGA_DMA[3] & 1) {}

    hardware_set_leds(0);
    hardware_display_number(0);
    hardware_poll_input();
    btn_prev = btn_now;
}

uint8_t *hardware_get_backbuffer(void) {
    return back_buffer;
}

void hardware_swap_buffers(void) {
    // Queue back_buffer to display on the next VSync
    VGA_DMA[1] = (uint32_t)back_buffer;
    VGA_DMA[0] = 0;

    // Wait for VSync swap to finish (Status bit 0 clears to 0)
    while (VGA_DMA[3] & 1) {}

    // Flip to the other buffer for the next frame
    back_buffer = (back_buffer == VGA_BUF_0) ? VGA_BUF_1 : VGA_BUF_0;
}

void hardware_poll_input(void) {
    btn_prev = btn_now;
    btn_now = (*BUTTON_ADDR) & 1;
    switches_now = (*SWITCHES_ADDR) & 0x3FF;
}

int hardware_btn_pressed(void) {
    return btn_now && !btn_prev;
}

int hardware_btn_down(void) {
    return btn_now;
}

uint16_t hardware_get_switches(void) {
    return switches_now;
}

int hardware_get_switch(int index) {
    return (switches_now >> index) & 1;
}

void hardware_set_leds(uint16_t mask) {
    mask &= 0x3FF;
    if (mask == last_leds) return;
    last_leds = mask;
    *LEDS_ADDR = mask;
}

void hardware_set_seg7_digit(int display, int value) {
    if (display < 0 || display > 5) return;
    volatile uint32_t *reg = (volatile uint32_t *)(SEG7_BASE_ADDR + display * SEG7_STRIDE);
    *reg = (value >= 0 && value < 16) ? SEG7_LUT[value] : SEG7_BLANK;
}

void hardware_display_number(unsigned int value) {
    if (value == last_number) return;
    last_number = value;

    for (int i = 0; i < 6; i++) {
        if (i == 0 || value > 0) {
            hardware_set_seg7_digit(i, value % 10);
            value /= 10;
        } else {
            hardware_set_seg7_digit(i, -1); // Blank leading zeros
        }
    }
}