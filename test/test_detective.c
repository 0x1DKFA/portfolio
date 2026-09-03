#include "test.h"
#include "vignettes/detective.h"

#define DT (1.0f / 60.0f)

static World w;
static int boundaries;

static void run(const Vignette *v, int steps) {
    for (int i = 0; i < steps; i++) {
        world_update_actors(&w, DT); v->update(&w, DT);
        if (v->at_beat_boundary(&w)) boundaries++;
    }
}

static Bug *hidden_bug(void) {
    for (int i = 0; i < MAX_BUGS; i++) if (w.bugs[i].state != BUG_DEAD && w.bugs[i].hidden) return &w.bugs[i];
    return 0;
}

void test_detective(void) {
    EventQueue ev;
    events_init(&ev);
    world_init(&w, 21u, 96, 240, 200, &ev);
    const Vignette *v = &vignette_detective;
    CHECK_EQ(v->id, VIG_DETECTIVE);
    boundaries = 0;

    v->enter(&w);
    CHECK_EQ(detective_phase(), DET_DIM);
    CHECK(hidden_bug() != 0);
    CHECK_EQ(hidden_bug()->revealed, 0);
    CHECK_EQ(detective_footprints(), 0);

    run(v, 18);  CHECK_EQ(detective_phase(), DET_DIM);          /* t = 0.3 */
    CHECK(detective_dim() > 0 && detective_dim() < 140);
    run(v, 42);  CHECK_EQ(detective_phase(), DET_FOOTPRINTS);   /* t = 1.0 */
    CHECK_EQ(detective_dim(), 140);
    run(v, 60);  CHECK_EQ(detective_footprints(), 2);          /* t = 2.0: prints at 1.1 and 1.7 */
    CHECK_EQ(w.hero.detective, 0);
    run(v, 120); CHECK_EQ(detective_phase(), DET_GEAR);         /* t = 4.0 */
    CHECK_EQ(detective_footprints(), 5);
    CHECK_EQ(w.hero.detective, 1);
    run(v, 60);  CHECK_EQ(detective_phase(), DET_FOLLOW);       /* t = 5.0 */
    run(v, 210); CHECK_EQ(detective_phase(), DET_REVEAL);       /* t = 8.5 */
    CHECK_EQ(hidden_bug()->revealed, 1);
    CHECK_EQ(w.squashed_total, 0);
    run(v, 45);  CHECK_EQ(detective_phase(), DET_BONK);         /* t = 9.25 */
    run(v, 30);  CHECK_EQ(detective_phase(), DET_UNDIM);        /* t = 9.75 */
    CHECK_EQ(w.squashed_total, 1);
    CHECK(detective_dim() < 140);
    run(v, 30);  CHECK_EQ(detective_phase(), DET_DONE);         /* t = 10.25 */
    CHECK(v->is_done(&w));
    CHECK_EQ(detective_dim(), 0);
    CHECK_EQ(w.hero.detective, 0);
    CHECK(boundaries >= 7 && boundaries <= 8);                 /* 5 prints + follow end + bonk end (+ done) */

    v->exit(&w);
    CHECK(hidden_bug() == 0 || hidden_bug()->state == BUG_SQUASHED || hidden_bug()->state == BUG_DEAD);
    CHECK_EQ(world_bug_count(&w, SIDE_LEFT), 0);
}
