#include "renderer.h"
#include "hardware.h"

#define RGB332(r, g, b) ((uint8_t)((((r) & 7) << 5) | (((g) & 7) << 2) | ((b) & 3)))

#define FOV_SCALE     43254 //0.66 degrees in fix_t
#define MAX_RAY_STEPS 20
#define MAX_OBS       2
#define MIN_DIST      4096 //0.0625, avoid divide by 0

#define COLOR_CEIL    RGB332(0, 0, 1)
#define COLOR_FLOOR   RGB332(1, 1, 1)
#define COLOR_WALL_NS RGB332(5, 6, 3)
#define COLOR_WALL_EW RGB332(3, 4, 2)
#define COLOR_JUMP    RGB332(7, 2, 0)
#define COLOR_DUCK    RGB332(7, 6, 0)

static void draw_background(uint8_t *fb) {
    uint32_t *fb32  = (uint32_t *)fb;
    uint32_t ceil4  = (uint32_t)COLOR_CEIL  * 0x01010101u;
    uint32_t floor4 = (uint32_t)COLOR_FLOOR * 0x01010101u;

    int half_words = (SCREEN_W * (SCREEN_H / 2)) / 4;
    for (int i = 0; i < half_words; i++) {
        fb32[i] = ceil4;
    }
    for (int i = half_words; i < half_words * 2; i++) {
        fb32[i] = floor4;
    }
}

void renderer_draw_frame(uint8_t *fb, const World *world, const Camera *cam) {
    draw_background(fb);
}