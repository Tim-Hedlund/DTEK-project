#include "world.h"

// ============================================================
// Private Chunk Blueprints (7x7, 1-wide corridor at index 3)
// ============================================================
#define EXIT_NORTH (1 << DIR_NORTH)
#define EXIT_EAST  (1 << DIR_EAST)
#define EXIT_WEST  (1 << DIR_WEST)

enum {
    _ = 0, // Void (dead)
    W,     // Wall texture 0 (dead)
    F,     // Normal floor (straight, jump, or duck allowed)
    J,     // Low barrier (jump required)
    D,     // High beam (duck required)
    R,     // Right-turn zone (straight or turn right)
    L,     // Left-turn zone (straight or turn left)
    T,     // T-junction zone (turn left/right)
    X      // Full-function zome (straight, turn left/right) 
};

typedef struct {
    uint8_t exits;
    uint8_t tiles[CHUNK_SIZE][CHUNK_SIZE];
} Chunk;

static const Chunk CHUNKS[] = {
    // 0: Plain straight corridor (safe starting runway)
    {
        .exits = EXIT_NORTH,
        .tiles = {
            { _, _, W, F, W, _, _ },
            { _, _, W, F, W, _, _ },
            { _, _, W, F, W, _, _ },
            { _, _, W, F, W, _, _ },
            { _, _, W, F, W, _, _ },
            { _, _, W, F, W, _, _ },
            { _, _, W, F, W, _, _ },
        }
    },
    // 1: Jump obstacle
    {
        .exits = EXIT_NORTH,
        .tiles = {
            { _, _, W, F, W, _, _ },
            { _, _, W, F, W, _, _ },
            { _, _, W, F, W, _, _ },
            { _, _, W, J, W, _, _ },
            { _, _, W, F, W, _, _ },
            { _, _, W, F, W, _, _ },
            { _, _, W, F, W, _, _ },
        }
    },
    // 2: Duck obstacle
    {
        .exits = EXIT_NORTH,
        .tiles = {
            { _, _, W, F, W, _, _ },
            { _, _, W, F, W, _, _ },
            { _, _, W, F, W, _, _ },
            { _, _, W, D, W, _, _ },
            { _, _, W, F, W, _, _ },
            { _, _, W, F, W, _, _ },
            { _, _, W, F, W, _, _ },
        }
    },
    // 3: Right turn (90 deg CW)
    {
        .exits = EXIT_EAST,
        .tiles = {
            { _, _, _, _, _, _, _ },
            { _, _, _, _, _, _, _ },
            { _, _, W, W, W, W, W },
            { _, _, W, R, F, F, F },
            { _, _, W, F, W, W, W },
            { _, _, W, F, W, _, _ },
            { _, _, W, F, W, _, _ },
        }
    },
    // 4: Left turn (90 deg CCW)
    {
        .exits = EXIT_WEST,
        .tiles = {
            { _, _, _, _, _, _, _ },
            { _, _, _, _, _, _, _ },
            { W, W, W, W, W, _, _ },
            { F, F, F, L, W, _, _ },
            { W, W, W, F, W, _, _ },
            { _, _, W, F, W, _, _ },
            { _, _, W, F, W, _, _ },
        }
    },
    // 5: T-junction (choose left or right)
    {
        .exits = EXIT_EAST | EXIT_WEST,
        .tiles = {
            { _, _, _, _, _, _, _ },
            { _, _, _, _, _, _, _ },
            { W, W, W, W, W, W, W },
            { F, F, F, T, F, F, F },
            { W, W, W, F, W, W, W },
            { _, _, W, F, W, _, _ },
            { _, _, W, F, W, _, _ },
        }
    },
    // 6: X-junction (choose forward, left or right)
    {
        .exits = EXIT_EAST | EXIT_WEST | EXIT_NORTH,
        .tiles = {
            { _, _, W, F, W, _, _ },
            { _, _, W, F, W, _, _ },
            { W, W, W, F, W, W, W },
            { F, F, F, X, F, F, F },
            { W, W, W, F, W, W, W },
            { _, _, W, F, W, _, _ },
            { _, _, W, F, W, _, _ },
        }
    }
};

#define NUM_CHUNKS (sizeof(CHUNKS) / sizeof(CHUNKS[0]))


// Private Helpers
static uint32_t rng_next(World *world) {
    uint32_t x = world->rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    world->rng_state = x;
    return x;
}

static uint8_t pick_random_chunk(World *world) {
    return (uint8_t)(rng_next(world) % NUM_CHUNKS);
}

static void decode_tile(uint8_t sym, uint8_t *vis_out, uint8_t *rule_out) {
    switch (sym) {
        case W: *vis_out = TILE_WALL(0);     *rule_out = RULE_DEAD; break;
        case F: *vis_out = TILE_EMPTY;       *rule_out = RULE_STRAIGHT | RULE_JUMP | RULE_DUCK; break;
        case J: *vis_out = TILE_LOW_BARRIER; *rule_out = RULE_JUMP; break;
        case D: *vis_out = TILE_HIGH_BEAM;   *rule_out = RULE_DUCK; break;
        case R: *vis_out = TILE_EMPTY;       *rule_out = RULE_STRAIGHT | RULE_TURN_RIGHT; break;
        case L: *vis_out = TILE_EMPTY;       *rule_out = RULE_STRAIGHT | RULE_TURN_LEFT; break;
        case T: *vis_out = TILE_EMPTY;       *rule_out = RULE_STRAIGHT | RULE_TURN_LEFT | RULE_TURN_RIGHT; break;
        case X: *vis_out = TILE_EMPTY;       *rule_out = RULE_STRAIGHT | RULE_TURN_LEFT | RULE_TURN_RIGHT; break;
        default:
            *vis_out = TILE_EMPTY;
            *rule_out = RULE_DEAD;
            break;
    }
}

static void stamp_chunk(World *world, uint8_t chunk_id, int cx, int cy, Direction facing) {
    const Chunk *c = &CHUNKS[chunk_id];
    int ox = cx * CHUNK_SIZE;
    int oy = cy * CHUNK_SIZE;

    for (int ly = 0; ly < CHUNK_SIZE; ly++) {
        // Flip row index so row 0 in CHUNKS[] maps to top (ly = 6)
        const uint8_t *row = c->tiles[CHUNK_SIZE - 1 - ly];

        for (int lx = 0; lx < CHUNK_SIZE; lx++) {
            uint8_t vis, rule;
            decode_tile(row[lx], &vis, &rule);

            int wx, wy;
            if (facing == DIR_NORTH) {
                wx = ox + lx;
                wy = oy + ly;
            } else if (facing == DIR_EAST) {
                wx = ox + ly;
                wy = oy + (CHUNK_SIZE - 1 - lx);
            } else {
                wx = ox + (CHUNK_SIZE - 1 - ly);
                wy = oy + lx;
            }

            world->visuals[wy][wx] = vis;
            world->rules[wy][wx]   = rule;
        }
    }
}

static void rebuild_grid(World *world) {
    for (int y = 0; y < WORLD_H; y++) {
        for (int x = 0; x < WORLD_STRIDE; x++) {
            world->visuals[y][x] = TILE_EMPTY;
            world->rules[y][x]   = RULE_DEAD;
        }
    }

    const Chunk *cur = &CHUNKS[world->cur_id];
    stamp_chunk(world, world->cur_id, 2, 0, DIR_NORTH);

    if (cur->exits & EXIT_NORTH) {
        uint8_t next_id = world->branch_id[DIR_NORTH];
        stamp_chunk(world, next_id, 2, 1, DIR_NORTH);

        if (CHUNKS[next_id].exits & EXIT_NORTH) {
            stamp_chunk(world, world->far_id[DIR_NORTH], 2, 2, DIR_NORTH);
        }
    }

    if (cur->exits & EXIT_EAST) {
        uint8_t next_id = world->branch_id[DIR_EAST];
        stamp_chunk(world, next_id, 3, 0, DIR_EAST);

        if (CHUNKS[next_id].exits & EXIT_NORTH) {
            stamp_chunk(world, world->far_id[DIR_EAST], 4, 0, DIR_EAST);
        }
    }

    if (cur->exits & EXIT_WEST) {
        uint8_t next_id = world->branch_id[DIR_WEST];
        stamp_chunk(world, next_id, 1, 0, DIR_WEST);

        if (CHUNKS[next_id].exits & EXIT_NORTH) {
            stamp_chunk(world, world->far_id[DIR_WEST], 0, 0, DIR_WEST);
        }
    }
}

void world_init(World *world, uint32_t seed) {
    if (seed != 0) {
        world->rng_state = seed;
    } else {
        world->rng_state = 0x12345678;
    }
    world->chunks_cleared = 0;
    world->cur_id = 0;

    for (int d = 0; d < 3; d++) {
        world->branch_id[d] = pick_random_chunk(world);
        world->far_id[d]    = pick_random_chunk(world);
    }

    rebuild_grid(world);
}

void world_advance(World *world, Direction exit_dir) {
    world->chunks_cleared++;
    world->cur_id = world->branch_id[exit_dir];

    world->branch_id[DIR_NORTH] = world->far_id[exit_dir];
    world->branch_id[DIR_EAST]  = pick_random_chunk(world);
    world->branch_id[DIR_WEST]  = pick_random_chunk(world);

    for (int d = 0; d < 3; d++) {
        world->far_id[d] = pick_random_chunk(world);
    }

    rebuild_grid(world);
}