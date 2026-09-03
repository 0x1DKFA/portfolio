#include "test.h"
#include "rng.h"

void test_rng(void) {
    Rng a, b;
    rng_seed(&a, 42u);
    rng_seed(&b, 42u);
    for (int i = 0; i < 100; i++) CHECK_EQ(rng_next(&a), rng_next(&b));

    rng_seed(&b, 43u);
    CHECK(rng_next(&a) != rng_next(&b));

    rng_seed(&a, 7u);
    for (int i = 0; i < 1000; i++) {
        int v = rng_range(&a, 3, 9);
        CHECK(v >= 3 && v <= 9);
        float f = rng_float(&a);
        CHECK(f >= 0.0f && f < 1.0f);
    }

    int seen[7] = {0};
    rng_seed(&a, 99u);
    for (int i = 0; i < 700; i++) seen[rng_range(&a, 0, 6)]++;
    for (int i = 0; i < 7; i++) CHECK(seen[i] > 50);

    rng_seed(&a, 0u);
    CHECK(rng_next(&a) != 0u);
}
