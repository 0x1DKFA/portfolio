#include "test.h"
#include "fmath.h"
#include "palette.h"
#include "world.h"

void test_bug(void) {
    CHECK_NEAR(fm_sqrt(16.0f), 4.0f, 1e-3);
    CHECK_NEAR(fm_sqrt(2.0f), 1.41421f, 1e-3);
    CHECK_NEAR(fm_sqrt(0.0f), 0.0f, 1e-6);
    CHECK_NEAR(fm_abs(-3.5f), 3.5f, 1e-6);
    CHECK_NEAR(fm_clamp(5.0f, 0.0f, 2.0f), 2.0f, 1e-6);
    CHECK_NEAR(fm_clamp(-1.0f, 0.0f, 2.0f), 0.0f, 1e-6);
    CHECK_EQ(PALETTE_BUGS[COL_BG].a, 255);
    CHECK(PALETTE_BUGS[COL_RED].r > PALETTE_BUGS[COL_RED].g);

    /* a hand-built world: 96x240 panels, 200px gap */
    World w;
    memset(&w, 0, sizeof w);
    w.panel_w = 96; w.panel_h = 240; w.gap_w = 200; w.world_w = 96 + 200 + 96;
    w.floor_top = 132; w.floor_bottom = 216;
    rng_seed(&w.rng, 5u);

    Bug b;
    bug_spawn(&b, SIDE_RIGHT, 300.0f, 150.0f, 1);
    CHECK_EQ(b.state, BUG_WANDER);
    CHECK_EQ(b.side, SIDE_RIGHT);
    CHECK_EQ(b.variant, 1);
    CHECK(bug_alive(&b));

    /* wander stays inside the right panel's floor band for 10 simulated seconds */
    int moved = 0;
    for (int i = 0; i < 600; i++) {
        float ox = b.x;
        bug_update(&b, &w, 1.0f / 60.0f);
        if (b.x != ox) moved = 1;
        CHECK(b.x >= 296.0f + BUG_W / 2 && b.x <= 392.0f - BUG_W / 2);
        CHECK(b.y >= 132.0f && b.y <= 216.0f);
    }
    CHECK(moved);

    /* rush reaches its target and waits there */
    bug_spawn(&b, SIDE_LEFT, 10.0f, 200.0f, 0);
    bug_set_target(&b, 40.0f, 200.0f);
    CHECK_EQ(b.state, BUG_RUSH);
    for (int i = 0; i < 90; i++) bug_update(&b, &w, 1.0f / 60.0f);   /* 30px at 30px/s = 1s */
    CHECK_EQ(b.state, BUG_WAIT);
    CHECK_NEAR(b.x, 40.0f, 0.01);

    /* squashed then dead after 0.4s */
    b.state = BUG_SQUASHED; b.timer = BUG_SQUASH_TIME;
    CHECK(!bug_alive(&b));
    for (int i = 0; i < 23; i++) bug_update(&b, &w, 1.0f / 60.0f);
    CHECK_EQ(b.state, BUG_SQUASHED);
    for (int i = 0; i < 3; i++) bug_update(&b, &w, 1.0f / 60.0f);
    CHECK_EQ(b.state, BUG_DEAD);
}
