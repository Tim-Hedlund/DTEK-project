#include "renderer.h"
#include "hardware.h"
#include "assets.h"

#define FOV_SCALE     43254 // ~0.66 in Q16.16 (~66 deg FOV)
#define MAX_RAY_STEPS 20
#define MAX_OBS       2
#define MIN_DIST      4096  // 0.0625, avoid divide by 0

static void draw_background(uint8_t *fb, uint8_t ceil_color, uint8_t floor_color) {
    uint32_t *fb32  = (uint32_t *)fb;
    uint32_t ceil4  = (uint32_t)ceil_color  * 0x01010101u;
    uint32_t floor4 = (uint32_t)floor_color * 0x01010101u;

    int half_words = (SCREEN_W * (SCREEN_H / 2)) / 4;
    for (int i = 0; i < half_words; i++) {
        fb32[i] = ceil4;
    }
    for (int i = half_words; i < half_words * 2; i++) {
        fb32[i] = floor4;
    }
}

static inline int compute_tex_x(
    const Camera *cam,
    fix_t dist,
    fix_t rayDirX, fix_t rayDirY,
    int side
) {
    fix_t wallX;
    if (side == 0) {
        wallX = cam->y + fix_mul(dist, rayDirY);
    } else {
        wallX = cam->x + fix_mul(dist, rayDirX);
    }

    // Fractional part [0..65535] mapped to [0..31]
    int texX = (wallX & (FIX_ONE - 1)) >> (FIX_SHIFT - TEX_SHIFT);

    if ((side == 0 && rayDirX > 0) || (side == 1 && rayDirY < 0)) {
        texX = TEX_SIZE - 1 - texX;
    }
    return texX;
}

static inline void draw_textured_vspan(
    uint8_t *fb,
    int x,
    int y0,
    int y1,
    const uint8_t *tex_col,
    int tex_h,
    int tex_mask
) {
    int spanH = y1 - y0;
    if (spanH <= 0) return;

    int drawStart = y0;
    if (drawStart < 0) drawStart = 0;

    int drawEnd = y1;
    if (drawEnd >= SCREEN_H) drawEnd = SCREEN_H - 1;
    if (drawStart > drawEnd) return;

    fix_t texStep = (tex_h << FIX_SHIFT) / spanH;
    fix_t texPos  = (drawStart - y0) * texStep;

    uint8_t *p = fb + drawStart * SCREEN_W + x;
    for (int y = drawStart; y <= drawEnd; y++) {
        *p = tex_col[(texPos >> FIX_SHIFT) & tex_mask];
        p += SCREEN_W;
        texPos += texStep;
    }
}

void renderer_draw_frame(uint8_t *fb, const World *world, const Camera *cam, int theme_id, RenderStats *stats) {
    uint32_t frame_start = hardware_get_cycles();

    const Theme *theme = assets_get_theme(theme_id);
    draw_background(fb, theme->ceil_color, theme->floor_color);
    uint32_t bg_end = hardware_get_cycles();

    fix_t dirX = fix_sin(cam->angle);
    fix_t dirY = fix_cos(cam->angle);

    fix_t planeX =  fix_mul(dirY, FOV_SCALE);
    fix_t planeY = -fix_mul(dirX, FOV_SCALE);

    int startMapX = FIX_TO_INT(cam->x);
    int startMapY = FIX_TO_INT(cam->y);

    fix_t cameraX = -FIX_ONE;
    const fix_t cameraX_step = (2 * FIX_ONE) / SCREEN_W;

    uint32_t dda_total  = 0;
    uint32_t draw_total = 0;

    for (int x = 0; x < SCREEN_W; x++) {
        uint32_t t_dda_start = hardware_get_cycles();

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
        uint8_t obs_side[MAX_OBS];
        int     obs_count = 0;

        int hit  = 0;
        int side = 0;

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
                if (side == 0) {
                    obs_dist[obs_count] = sideDistX - deltaDistX;
                } else {
                    obs_dist[obs_count] = sideDistY - deltaDistY;
                }
                obs_side[obs_count] = (uint8_t)side;
                obs_count++;
            }
        }

        uint32_t t_draw_start = hardware_get_cycles();
        dda_total += (t_draw_start - t_dda_start);

        // 1. Draw textured solid wall
        if (hit) {
            fix_t perpDist;
            if (side == 0) {
                perpDist = sideDistX - deltaDistX;
            } else {
                perpDist = sideDistY - deltaDistY;
            }
            if (perpDist < MIN_DIST) perpDist = MIN_DIST;

            int lineH  = (SCREEN_H << FIX_SHIFT) / perpDist;
            int zShift = (int)(((int64_t)lineH * cam->z) >> FIX_SHIFT);
            int y0     = (SCREEN_H - lineH) / 2 + zShift;
            int y1     = y0 + lineH;

            int texX = compute_tex_x(cam, perpDist, rayDirX, rayDirY, side);
            const uint8_t *col = theme->wall[texX & TEX_MASK];
            draw_textured_vspan(fb, x, y0, y1, col, TEX_SIZE, TEX_MASK);
        }

        // 2. Overlay textured obstacles back-to-front
        for (int i = obs_count - 1; i >= 0; i--) {
            fix_t d = obs_dist[i];
            if (d < MIN_DIST) d = MIN_DIST;

            int lineH  = (SCREEN_H << FIX_SHIFT) / d;
            int zShift = (int)(((int64_t)lineH * cam->z) >> FIX_SHIFT);
            int y0     = (SCREEN_H - lineH) / 2 + zShift;
            int y1     = y0 + lineH;

            int texX = compute_tex_x(cam, d, rayDirX, rayDirY, obs_side[i]);

            if (obs_type[i] == TILE_LOW_BARRIER) {
                int barTop = y1 - ((lineH * 3) >> 3);
                const uint8_t *col = theme->low_bar[texX & TEX_MASK];
                draw_textured_vspan(fb, x, barTop, y1, col, OBS_TEX_H, OBS_TEX_MASK);
            } else if (obs_type[i] == TILE_HIGH_BEAM) {
                int beamBot = y0 + ((lineH * 2) / 5);
                const uint8_t *col = theme->high_bar[texX & TEX_MASK];
                draw_textured_vspan(fb, x, y0, beamBot, col, OBS_TEX_H, OBS_TEX_MASK);
            }
        }

        draw_total += (hardware_get_cycles() - t_draw_start);
    }

    if (stats) {
        stats->bg_cycles    = bg_end - frame_start;
        stats->dda_cycles   = dda_total;
        stats->draw_cycles  = draw_total;
        stats->total_cycles = hardware_get_cycles() - frame_start;
    }
}