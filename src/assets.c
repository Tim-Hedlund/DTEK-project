#include "assets.h"
#include "textures/brick.h"
#include "textures/mossy_cobble.h"

#define RGB332(r, g, b) ((uint8_t)((((r) & 7) << 5) | (((g) & 7) << 2) | ((b) & 3)))

static const Theme themes[THEME_COUNT] = {
    [THEME_BRICK] = {
        .wall        = brick_tex,
        .low_bar     = mossy_low_bar_tex,  // Can reuse any texture as a placeholder for free
        .high_bar    = mossy_high_bar_tex,
        .ceil_color  = RGB332(1, 1, 0),
        .floor_color = RGB332(2, 1, 0),
    },
    [THEME_MOSSY] = {
        .wall        = mossy_cobble_tex,
        .low_bar     = mossy_low_bar_tex,
        .high_bar    = mossy_high_bar_tex,
        .ceil_color  = RGB332(0, 1, 0),
        .floor_color = RGB332(1, 2, 0),
    },
};

const Theme *assets_get_theme(int theme_id) {
    if ((unsigned)theme_id >= THEME_COUNT) {
        theme_id = 0;
    }
    return &themes[theme_id];
}