#include "test.h"
#include "hero.h"

#define DT (1.0f / 60.0f)

void test_hero(void) {
    Hero h;
    hero_init(&h, 10.0f, 100.0f);
    CHECK_EQ(h.action, HERO_IDLE);
    CHECK(hero_arrived(&h));
    CHECK_EQ(h.facing, 1);

    /* walk 24px right at 24px/s: not arrived after 59 steps, arrived by 61 */
    hero_walk_to(&h, 34.0f, 100.0f);
    CHECK_EQ(h.action, HERO_WALK);
    for (int i = 0; i < 59; i++) hero_update(&h, DT);
    CHECK(!hero_arrived(&h));
    for (int i = 0; i < 2; i++) hero_update(&h, DT);
    CHECK(hero_arrived(&h));
    CHECK_NEAR(h.x, 34.0f, 0.01);
    CHECK_EQ(h.action, HERO_IDLE);

    /* walking left flips facing */
    hero_walk_to(&h, 0.0f, 100.0f);
    hero_update(&h, DT);
    CHECK_EQ(h.facing, -1);

    /* bonk: windup then a single landed step, then idle */
    hero_init(&h, 10.0f, 100.0f);
    hero_start_bonk(&h);
    CHECK_EQ(h.action, HERO_WINDUP);
    CHECK(hero_busy(&h));
    int landed = 0, landed_step = -1;
    for (int i = 0; i < 40; i++) {
        hero_update(&h, DT);
        if (h.bonk_landed) { landed++; landed_step = i; }
    }
    CHECK_EQ(landed, 1);
    CHECK(landed_step >= 13 && landed_step <= 16);
    CHECK_EQ(h.action, HERO_IDLE);
    CHECK(!hero_busy(&h));

    /* hide and show */
    hero_hide(&h);
    CHECK_EQ(h.action, HERO_HIDDEN);
    hero_show(&h, 300.0f, 100.0f);
    CHECK_EQ(h.action, HERO_IDLE);
    CHECK_NEAR(h.x, 300.0f, 1e-6);

    hero_set_action(&h, HERO_THINK);
    CHECK_EQ(h.action, HERO_THINK);
}
