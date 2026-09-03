#include "test.h"
#include "vignettes/patrol.h"

#define DT (1.0f / 60.0f)

static void run(World *w, const Vignette *v, int steps) {
    for (int i = 0; i < steps; i++) { world_update_actors(w, DT); v->update(w, DT); }
}

void test_patrol(void) {
    static World w;
    EventQueue ev;
    events_init(&ev);
    world_init(&w, 11u, 96, 240, 200, &ev);
    for (int i = 0; i < 4; i++) { world_spawn_bug(&w, SIDE_LEFT); world_spawn_bug(&w, SIDE_RIGHT); }

    const Vignette *v = &vignette_patrol;
    CHECK_EQ(v->id, VIG_PATROL);
    CHECK(!vignette_is_linked(VIG_PATROL));
    CHECK(vignette_is_linked(VIG_RACE));
    CHECK(vignette_is_linked(VIG_REGRESSION));
    CHECK(!vignette_is_linked(VIG_CROSSOVER));

    v->enter(&w);
    CHECK(!v->is_done(&w));
    CHECK(v->at_beat_boundary(&w));

    /* within 10 seconds the hero has squashed something and was mid-swing at some point */
    int saw_busy_boundary_false = 0, saw_busy = 0;
    for (int i = 0; i < 600; i++) {
        world_update_actors(&w, DT); v->update(&w, DT);
        if (hero_busy(&w.hero)) { saw_busy = 1; if (!v->at_beat_boundary(&w)) saw_busy_boundary_false = 1; }
    }
    CHECK(saw_busy);
    CHECK(saw_busy_boundary_false);
    CHECK(w.squashed_total >= 1);
    CHECK(events_count(&ev) >= 1);
    CHECK_EQ(event_type(events_pop(&ev)), EV_BUG_SQUASHED);

    /* cadence: at least 4 squashes in the first 30 seconds, never more than cap alive */
    run(&w, v, 1200);
    CHECK(w.squashed_total >= 4);
    CHECK(world_bug_count(&w, SIDE_LEFT) <= MAX_BUGS_PER_SIDE);
    CHECK(world_bug_count(&w, SIDE_RIGHT) <= MAX_BUGS_PER_SIDE);

    /* spawning: the unattended right side grows over time */
    int right_before = world_bug_count(&w, SIDE_RIGHT);
    run(&w, v, 1800);
    CHECK(world_bug_count(&w, SIDE_RIGHT) > right_before || world_bug_count(&w, SIDE_RIGHT) == MAX_BUGS_PER_SIDE);

    /* the hero stays on the left; patrol never crosses */
    CHECK_EQ(world_hero_side(&w), SIDE_LEFT);
    CHECK(!v->is_done(&w));
    v->exit(&w);
}
