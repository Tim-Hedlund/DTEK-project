#pragma once
#include <stdint.h>

#define CHUNK_SIZE   7
#define WORLD_W      21   // 3 chunks wide (x: 0..20; current chunk is 7..13)
#define WORLD_H      14   // 2 chunks tall (y: 0..13; current chunk is 0..6)
#define WORLD_STRIDE 32   // Power-of-2 row width so visuals[y][x] uses (y << 5)

// Starting tile inside the bottom-center chunk (player camera sits at x + 0.5, y + 0.5)
#define PLAYER_START_X 10
#define PLAYER_START_Y 1

// --- Visual Map Encoding ---
#define TILE_EMPTY       0x00
#define TILE_LOW_BARRIER 0x01 // Jump obstacle
#define TILE_HIGH_BEAM   0x02 // Duck obstacle
#define TILE_WALL_FLAG   0x80 // Bit 7 set = solid wall
#define TILE_WALL(tex)   (TILE_WALL_FLAG | ((tex) & 0x7F))

// --- Rule Bitmask Encoding ---
#define RULE_DEAD        0          // Fatal to enter
#define RULE_JUMP        (1 << 0)   // Jumping allowed / required
#define RULE_DUCK        (1 << 1)   // Ducking allowed / required
#define RULE_TURN_LEFT   (1 << 2)   // Turning left allowed
#define RULE_TURN_RIGHT  (1 << 3)   // Turning right allowed
#define RULE_STRAIGHT    (1 << 4)   // Walking straight allowed

typedef enum {
    DIR_NORTH = 0, // Straight ahead (+Y)
    DIR_EAST  = 1, // Right turn     (+X)
    DIR_WEST  = 2  // Left turn      (-X)
} Direction;

typedef struct {
    // Flat stamped grids read by renderer.c and player.c (672 bytes each)
    uint8_t visuals[WORLD_H][WORLD_STRIDE];
    uint8_t rules[WORLD_H][WORLD_STRIDE];

    // Pre-rolled chunk queue (prevents visual popping when advancing)
    uint8_t  cur_id;
    uint8_t  branch_id[3];
    uint8_t  far_id[3][3];
    uint32_t rng_state;
    uint32_t chunks_cleared;
} World;

void world_init(World *world, uint32_t seed);
void world_advance(World *world, Direction exit_dir);

static inline uint8_t world_get_visual(const World *world, int x, int y) {
    if ((unsigned)x >= WORLD_W || (unsigned)y >= WORLD_H) return TILE_WALL(0);
    return world->visuals[y][x];
}

static inline uint8_t world_get_rule(const World *world, int x, int y) {
    if ((unsigned)x >= WORLD_W || (unsigned)y >= WORLD_H) return RULE_DEAD;
    return world->rules[y][x];
}