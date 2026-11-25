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

static inline void draw_vspan(uint8_t *fb, int x, int y0, int y1, uint8_t color) {
    if (y0 < 0) y0 = 0;
    if (y1 >= SCREEN_H) y1 = SCREEN_H - 1;
    if (y0 > y1) return;

    uint8_t *p = fb + y0 * SCREEN_W + x;
    for (int y = y0; y <= y1; y++) {
        *p = color;
        p += SCREEN_W;
    }
}

void renderer_draw_frame(uint8_t *fb, const World *world, const Camera *cam) {
    draw_background(fb);

    // Camera forward vector: angle 0 = North (+Y), 256 = East (+X)
    fix_t dirX = fix_sin(cam->angle);
    fix_t dirY = fix_cos(cam->angle);

    // Camera plane perpendicular to dir, pointing right (+X when facing North)
    fix_t planeX =  fix_mul(dirY, FOV_SCALE);
    fix_t planeY = -fix_mul(dirX, FOV_SCALE);

    int startMapX = FIX_TO_INT(cam->x);
    int startMapY = FIX_TO_INT(cam->y);

    fix_t cameraX = -FIX_ONE; //left
    const fix_t cameraX_step = (2 * FIX_ONE) / SCREEN_W;
    //setup about 400 cycles + background drawing

    for (int x = 0; x < SCREEN_W; x++) {
        fix_t rayDirX = dirX + fix_mul(planeX, cameraX);
        fix_t rayDirY = dirY + fix_mul(planeY, cameraX);
        cameraX += cameraX_step;

        fix_t deltaDistX = fix_inv_abs(rayDirX);
        fix_t deltaDistY = fix_inv_abs(rayDirY);

        int mapX = startMapX;
        int mapY = startMapY;
        int stepX, stepY;
        fix_t sideDistX, sideDistY;

        if (rayDirX < 0) {
            stepX = -1;
            sideDistX = fix_mul(cam->x - INT_TO_FIX(mapX), deltaDistX);
        } else {
            stepX = 1;
            sideDistX = fix_mul(INT_TO_FIX(mapX + 1) - cam->x, deltaDistX);
        }

        if (rayDirY < 0) {
            stepY = -1;
            sideDistY = fix_mul(cam->y - INT_TO_FIX(mapY), deltaDistY);
        } else {
            stepY = 1;
            sideDistY = fix_mul(INT_TO_FIX(mapY + 1) - cam->y, deltaDistY);
        }

        uint8_t obs_type[MAX_OBS];
        fix_t   obs_dist[MAX_OBS];
        int     obs_count = 0;

        int hit  = 0;
        int side = 0;

        // DDA grid traversal
        for (int step = 0; step < MAX_RAY_STEPS; step++) {
            if (sideDistX < sideDistY) {
                sideDistX += deltaDistX;
                mapX += stepX;
                side = 0;
            } else {
                sideDistY += deltaDistY;
                mapY += stepY;
                side = 1;
            }

            if ((unsigned)mapX >= WORLD_W || (unsigned)mapY >= WORLD_H) {
                hit = 1;
                break;
            }

            uint8_t tile = world->visuals[mapY][mapX];
            if (tile & TILE_WALL_FLAG) {
                hit = 1;
                break;
            } else if (tile != TILE_EMPTY && obs_count < MAX_OBS) {
                obs_type[obs_count] = tile;
                obs_dist[obs_count] = (side == 0) ? (sideDistX - deltaDistX)
                                                  : (sideDistY - deltaDistY);
                obs_count++;
            }
        }

        // 1. Draw solid wall
        if (hit) {
            fix_t perpDist = (side == 0) ? (sideDistX - deltaDistX)
                                         : (sideDistY - deltaDistY);
            if (perpDist < MIN_DIST) perpDist = MIN_DIST;

            int lineH  = (SCREEN_H << FIX_SHIFT) / perpDist;
            int zShift = (int)(((int64_t)lineH * cam->z) >> FIX_SHIFT);
            int y0     = (SCREEN_H - lineH) / 2 + zShift;
            int y1     = y0 + lineH;

            uint8_t color = (side == 0) ? COLOR_WALL_EW : COLOR_WALL_NS;
            draw_vspan(fb, x, y0, y1, color);
        }

        // 2. Overlay obstacles back-to-front
        for (int i = obs_count - 1; i >= 0; i--) {
            fix_t d = obs_dist[i];
            if (d < MIN_DIST) d = MIN_DIST;

            int lineH  = (SCREEN_H << FIX_SHIFT) / d;
            int zShift = (int)(((int64_t)lineH * cam->z) >> FIX_SHIFT);
            int y0     = (SCREEN_H - lineH) / 2 + zShift;
            int y1     = y0 + lineH;

            if (obs_type[i] == TILE_LOW_BARRIER) {
                int barTop = y1 - ((lineH * 3) >> 3);
                draw_vspan(fb, x, barTop, y1, COLOR_JUMP);
            } else if (obs_type[i] == TILE_HIGH_BEAM) {
                int beamBot = y0 + ((lineH * 2) / 5);
                draw_vspan(fb, x, y0, beamBot, COLOR_DUCK);
            }
        }
    }
}