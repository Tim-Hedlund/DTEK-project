#pragma once
#include <stdint.h>
#include "fixmath.h"
#include "world.h"

typedef struct {
    fix_t x;
    fix_t y;
    fix_t z;
    int angle;
} Camera;

typedef struct {
    uint32_t bg_cycles;    // draw_background()
    uint32_t dda_cycles;   // Ray setup + DDA grid traversal (all 320 cols)
    uint32_t draw_cycles;  // Wall + obstacle height math & draw_vspan()
    uint32_t total_cycles; // Full renderer_draw_frame()
} RenderStats;

void renderer_draw_frame(uint8_t *frame_buffer, const World *world, const Camera *camera);