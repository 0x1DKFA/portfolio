#ifndef RNG_H
#define RNG_H
#include <stdint.h>

typedef struct { uint32_t state; } Rng;

void     rng_seed(Rng *r, uint32_t seed);
uint32_t rng_next(Rng *r);
int      rng_range(Rng *r, int lo, int hi);   /* inclusive both ends */
float    rng_float(Rng *r);                   /* [0, 1) */

#endif
