#include "hardware.h"
#include "world.h"
#include "dtekv-lib.h"

void handle_interrupt(unsigned _irq) {}

static void print_uint(uint32_t v) {
    char buf[12];
    int i = 0;
    if (v == 0) {
        print("0");
        return;
    }
    while (v > 0) {
        buf[i++] = (char)('0' + (v % 10));
        v /= 10;
    }
    char out[12];
    for (int j = 0; j < i; j++) out[j] = buf[i - 1 - j];
    out[i] = '\0';
    print(out);
}

static const char *dir_name(Direction d) {
    if (d == DIR_NORTH) return "NORTH";
    if (d == DIR_EAST)  return "EAST";
    return "WEST";
}

static char tile_char(const World *w, int x, int y, int px, int py) {
    if (x == px && y == py) return '@';

    uint8_t vis  = w->visuals[y][x];
    uint8_t rule = w->rules[y][x];

    if (vis & TILE_WALL_FLAG)    return '#';
    if (vis == TILE_LOW_BARRIER) return 'J';
    if (vis == TILE_HIGH_BEAM)   return 'D';
    if (rule == RULE_DEAD)       return ' ';

    int can_l = (rule & RULE_TURN_LEFT) != 0;
    int can_r = (rule & RULE_TURN_RIGHT) != 0;
    if (can_l && can_r) return 'T';
    if (can_l)          return 'L';
    if (can_r)          return 'R';
    return '.';
}

static void print_world(const World *w, int px, int py) {
    print("========================================\n");
    print("Cleared: "); print_uint(w->chunks_cleared);
    print(" | cur=");   print_uint(w->cur_id);
    print(" | next[N="); print_uint(w->branch_id[DIR_NORTH]);
    print(", E=");        print_uint(w->branch_id[DIR_EAST]);
    print(", W=");        print_uint(w->branch_id[DIR_WEST]);
    print("] | @=(");     print_uint(px);
    print(",");           print_uint(py);
    print(")\n");

    // Print top-to-bottom (y = 13 down to 0) with chunk dividers
    for (int y = WORLD_H - 1; y >= 0; y--) {
        char line[32];
        int p = 0;
        for (int x = 0; x < WORLD_W; x++) {
            if (x == CHUNK_SIZE || x == CHUNK_SIZE * 2) {
                line[p++] = '|';
            }
            line[p++] = tile_char(w, x, y, px, py);
        }
        line[p++] = '\n';
        line[p]   = '\0';
        print(line);

        if (y == CHUNK_SIZE) {
            print("-------+-------+-------\n");
        }
    }
}

// Walks the player through the current chunk until they cross a chunk border
static void step_to_next_chunk(World *w, int *px, int *py) {
    Direction facing = DIR_NORTH;

    while (1) {
        // Check for a turn at the center of the current chunk (x=10, y=3)
        if (facing == DIR_NORTH && *py == 3) {
            uint8_t rule = world_get_rule(w, *px, *py);
            int can_l = (rule & RULE_TURN_LEFT) != 0;
            int can_r = (rule & RULE_TURN_RIGHT) != 0;

            if (can_l && can_r) {
                facing = hardware_get_switch(0) ? DIR_WEST : DIR_EAST;
            } else if (can_r) {
                facing = DIR_EAST;
            } else if (can_l) {
                facing = DIR_WEST;
            }
        }

        // Advance one tile
        if (facing == DIR_NORTH)     (*py)++;
        else if (facing == DIR_EAST) (*px)++;
        else                         (*px)--;

        // Detect crossing out of the bottom-center chunk (x: 7..13, y: 0..6)
        if (*py >= CHUNK_SIZE) {
            print("-> Player exited NORTH into next chunk\n");
            world_advance(w, DIR_NORTH);
            *px = PLAYER_START_X;
            *py = 0;
            return;
        }
        if (*px >= CHUNK_SIZE * 2) {
            print("-> Player exited EAST into next chunk\n");
            world_advance(w, DIR_EAST);
            *px = PLAYER_START_X;
            *py = 0;
            return;
        }
        if (*px < CHUNK_SIZE) {
            print("-> Player exited WEST into next chunk\n");
            world_advance(w, DIR_WEST);
            *px = PLAYER_START_X;
            *py = 0;
            return;
        }
    }
}

int main(void) {
    hardware_init();

    World world;
    // Seed with mcycle so runs vary, or replace with a fixed number to debug
    world_init(&world, hardware_get_cycles());

    int px = PLAYER_START_X;
    int py = PLAYER_START_Y;

    print("World test started. Legend: @=Player #=Wall .=Floor J=Jump D=Duck R/L/T=Turn\n");
    print_world(&world, px, py);

    // Automatically walk through 3 chunk transitions on startup
    for (int i = 0; i < 3; i++) {
        step_to_next_chunk(&world, &px, &py);
        print_world(&world, px, py);
    }

    print("Press BUTTON to advance another chunk (SW0 UP=Left, DOWN=Right at T-junctions).\n");

    while (1) {
        hardware_poll_input();
        if (hardware_btn_pressed()) {
            step_to_next_chunk(&world, &px, &py);
            print_world(&world, px, py);
        }
    }
}