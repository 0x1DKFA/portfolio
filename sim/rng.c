#include "rng.h"

void rng_seed(Rng *r, uint32_t seed) {
    r->state = seed ? seed : 0x9E3779B9u;
}

uint32_t rng_next(Rng *r) {
    uint32_t z = (r->state += 0x6D2B79F5u);
    z = (z ^ (z >> 15)) * (z | 1u);
    z ^= z + (z ^ (z >> 7)) * (z | 61u);
    return z ^ (z >> 14);
}

int rng_range(Rng *r, int lo, int hi) {
    uint32_t span = (uint32_t)(hi - lo + 1);
    return lo + (int)(rng_next(r) % span);
}

float rng_float(Rng *r) {
    return (float)(rng_next(r) >> 8) * (1.0f / 16777216.0f);
}
