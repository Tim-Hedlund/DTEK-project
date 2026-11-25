#pragma once
#include <stdint.h>
#include <hardware.h>

#define TEX_SIZE     32
#define TEX_SHIFT    5
#define TEX_MASK     (TEX_SIZE - 1)

#define OBS_TEX_H    16
#define OBS_TEX_MASK (OBS_TEX_H - 1)

enum {
    THEME_BRICK = 0,
    THEME_MOSSY = 1,
    THEME_COUNT
};

typedef struct {
    const uint8_t (*wall)[TEX_SIZE];      
    const uint8_t (*low_bar)[OBS_TEX_H];  
    const uint8_t (*high_bar)[OBS_TEX_H]; 
    uint8_t ceil_color;
    uint8_t floor_color;
} Theme;

const Theme *assets_get_theme(int theme_id);

const uint8_t (*assets_get_title_screen(void))[SCREEN_W];