#pragma once
#include <stdint.h>

#define SCREEN_W 320
#define SCREEN_H 240

// Call once at startup before the main loop
void hardware_init(void);

// VGA Double Buffering 
uint8_t *hardware_get_backbuffer(void);
void hardware_swap_buffers(void);

// Input
void hardware_poll_input(void);
int hardware_btn_pressed(void);          // 1 only on the frame the button is clicked
int hardware_btn_down(void);             // 1 while the button is held down
uint16_t hardware_get_switches(void);    // Bitmask of all 10 switches (bits 0..9)
int hardware_get_switch(int index);      // 1 if switch `index` (0..9) is UP

// Outputs 
void hardware_set_leds(uint16_t mask);               // 10 LEDs (bits 0..9)
void hardware_set_seg7_digit(int display, int value); // display: 0 (rightmost) .. 5 (leftmost)
void hardware_display_number(unsigned int value);    // Shows 0..999999 across all 6 displays

// Cycle Counter (30 MHz = 30,000,000 cycles/sec) 
static inline uint32_t hardware_get_cycles(void) {
    uint32_t cycles;
    __asm__ volatile ("csrr %0, mcycle" : "=r"(cycles));
    return cycles;
}