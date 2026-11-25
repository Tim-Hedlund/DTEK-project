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

void renderer_draw_frame(uint8_t *frame_buffer, const World *world, const Camera *camera);