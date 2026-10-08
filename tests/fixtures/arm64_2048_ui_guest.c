/*
 * Project-owned ARM64 iPhoneOS guest GUI/input interoperability fixture.
 * The rules of 2048 are general gameplay mechanics; this is NOT the
 * original Objective-C danqing/2048 app or any of its source code.
 * No Apple SDK, OS libraries, imports, UIKit or SpriteKit are linked.
 */
typedef unsigned int u32;

struct Anyios2048State {
    u32 magic;
    u32 version;
    u32 tiles[16];
    u32 score;
    u32 best;
    u32 moves;
    u32 won;
    u32 game_over;
    u32 rng;
};

__attribute__((used, visibility("default")))
volatile struct Anyios2048State anyios_2048_state;

static u32 random_u32(void) {
    u32 x = anyios_2048_state.rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    anyios_2048_state.rng = x;
    return x;
}

static u32 free_cells(void) {
    u32 free = 0;
    for (u32 i = 0; i < 16; ++i) {
        if (anyios_2048_state.tiles[i] == 0) ++free;
    }
    return free;
}

static void spawn(void) {
    u32 count = free_cells();
    if (!count) return;
    u32 slot = random_u32() % count;
    for (u32 i = 0; i < 16; ++i) {
        if (anyios_2048_state.tiles[i] == 0) {
            if (slot == 0) {
                anyios_2048_state.tiles[i] =
                    ((random_u32() % 10) == 0) ? 4 : 2;
                return;
            }
            --slot;
        }
    }
}

__attribute__((used, visibility("default")))
void anyios_2048_reset(u32 seed) {
    if (!seed) seed = 0x20482048U;
    for (u32 i = 0; i < 16; ++i) anyios_2048_state.tiles[i] = 0;
    anyios_2048_state.magic = 0x32303438U; /* ASCII 8042 in LE */
    anyios_2048_state.version = 1;
    anyios_2048_state.score = 0;
    anyios_2048_state.moves = 0;
    anyios_2048_state.won = 0;
    anyios_2048_state.game_over = 0;
    anyios_2048_state.rng = seed;
    spawn();
    spawn();
}

static u32 cell_index(u32 direction, u32 line, u32 step) {
    switch (direction) {
    case 0: return step * 4 + line;             /* up */
    case 1: return line * 4 + step;             /* left */
    case 2: return (3 - step) * 4 + line;       /* down */
    default: return line * 4 + (3 - step);      /* right */
    }
}

static u32 available_moves(void) {
    if (free_cells()) return 1;
    for (u32 i = 0; i < 16; ++i) {
        if ((i % 4) != 3 &&
            anyios_2048_state.tiles[i] == anyios_2048_state.tiles[i + 1])
            return 1;
        if (i < 12 &&
            anyios_2048_state.tiles[i] == anyios_2048_state.tiles[i + 4])
            return 1;
    }
    return 0;
}

__attribute__((used, visibility("default")))
u32 anyios_2048_move(u32 direction) {
    if (direction > 3 || anyios_2048_state.magic != 0x32303438U ||
        anyios_2048_state.game_over) return 0;

    u32 changed = 0;
    for (u32 line = 0; line < 4; ++line) {
        u32 values[4] = {0, 0, 0, 0};
        u32 next[4] = {0, 0, 0, 0};
        u32 used = 0;
        for (u32 step = 0; step < 4; ++step) {
            u32 tile = anyios_2048_state.tiles[cell_index(direction, line, step)];
            if (tile) values[used++] = tile;
        }
        u32 out = 0;
        for (u32 i = 0; i < used; ++i) {
            if (i + 1 < used && values[i] == values[i + 1]) {
                u32 merged = values[i] + values[i];
                next[out++] = merged;
                anyios_2048_state.score += merged;
                if (merged >= 2048) anyios_2048_state.won = 1;
                ++i;
            } else {
                next[out++] = values[i];
            }
        }
        for (u32 step = 0; step < 4; ++step) {
            u32 index = cell_index(direction, line, step);
            if (anyios_2048_state.tiles[index] != next[step]) changed = 1;
            anyios_2048_state.tiles[index] = next[step];
        }
    }
    if (!changed) return 0;
    ++anyios_2048_state.moves;
    if (anyios_2048_state.score > anyios_2048_state.best)
        anyios_2048_state.best = anyios_2048_state.score;
    spawn();
    if (!available_moves()) anyios_2048_state.game_over = 1;
    return 1;
}

int main(void) {
    anyios_2048_reset(0x15c0ffeeU);
    return 0;
}
