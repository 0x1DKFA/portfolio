#include "test.h"
#include "vignettes/crossover.h"

#define DT (1.0f / 60.0f)

static void run(World *w, const Vignette *v, int steps) {
    for (int i = 0; i < steps; i++) { world_update_actors(w, DT); v->update(w, DT); }
}

void test_crossover(void) {
    static World w;
    EventQueue ev;
    events_init(&ev);
    world_init(&w, 3u, 96, 240, 200, &ev);
    const Vignette *v = &vignette_crossover;
    CHECK_EQ(v->id, VIG_CROSSOVER);
    CHECK_EQ(world_hero_side(&w), SIDE_LEFT);

    v->enter(&w);
    CHECK_EQ(crossover_phase(), XO_WALK);
    CHECK(!v->is_done(&w));
    CHECK(!v->at_beat_boundary(&w));

    /* hero starts at x=48, exit point is 96 + 7 = 103: 55px at 24px/s = 2.3s */
    run(&w, v, 2 * 60);
    CHECK_EQ(crossover_phase(), XO_WALK);
    run(&w, v, 1 * 60);
    CHECK_EQ(crossover_phase(), XO_HIDDEN);
    CHECK_EQ(w.hero.action, HERO_HIDDEN);
    CHECK_EQ(world_hero_side(&w), SIDE_GAP);

    /* hidden for (200 - 14) / 24 = 7.75s: count the hidden steps */
    int hidden_steps = 0;
    while (crossover_phase() == XO_HIDDEN && hidden_steps < 20 * 60) { run(&w, v, 1); hidden_steps++; }
    CHECK_EQ(crossover_phase(), XO_EMERGE);
    /* we entered HIDDEN somewhere in the last second, so allow that slack plus rounding */
    CHECK(hidden_steps >= 465 - 62 && hidden_steps <= 465 + 2);
    CHECK_EQ(world_hero_side(&w), SIDE_RIGHT);
    CHECK(w.hero.action != HERO_HIDDEN);

    /* emerge: from 289 to 310 is 21px, under a second */
    run(&w, v, 2 * 60);
    CHECK_EQ(crossover_phase(), XO_DONE);
    CHECK(v->is_done(&w));
    CHECK(v->at_beat_boundary(&w));
    CHECK_NEAR(w.hero.x, 96.0f + 200.0f + 14.0f, 0.5);
    v->exit(&w);

    /* from the right it goes back left */
    v->enter(&w);
    run(&w, v, 15 * 60);
    CHECK(v->is_done(&w));
    CHECK_EQ(world_hero_side(&w), SIDE_LEFT);
    CHECK_NEAR(w.hero.x, 96.0f - 14.0f, 0.5);
    v->exit(&w);
}
