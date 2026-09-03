#include "test.h"
#include "vignettes/regression.h"

#define DT (1.0f / 60.0f)

static World w;
static int boundaries;

static void run(const Vignette *v, int steps) {
    for (int i = 0; i < steps; i++) {
        world_update_actors(&w, DT); v->update(&w, DT);
        if (v->at_beat_boundary(&w)) boundaries++;
    }
}

static int waiting(void) {
    int n = 0;
    for (int i = 0; i < MAX_BUGS; i++) if (w.bugs[i].state == BUG_WAIT) n++;
    return n;
}

void test_regression(void) {
    EventQueue ev;
    events_init(&ev);
    world_init(&w, 33u, 96, 240, 200, &ev);
    const Vignette *v = &vignette_regression;
    CHECK_EQ(v->id, VIG_REGRESSION);
    boundaries = 0;

    v->enter(&w);
    CHECK_EQ(regression_phase(), REG_BONK1);
    CHECK_EQ(waiting(), 1);                                  /* the first target */
    CHECK_EQ(regression_shield(), 0);

    run(v, 90);  CHECK_EQ(regression_phase(), REG_BONK2);    /* t = 1.5 */
    CHECK_EQ(w.squashed_total, 1);
    CHECK_EQ(waiting(), 2);                                  /* one bonk, two appear */
    run(v, 90);  CHECK_EQ(regression_phase(), REG_THINK);    /* t = 3.0 */
    CHECK(w.squashed_total >= 2);
    CHECK(waiting() >= 4);                                   /* bonk again, four appear */
    CHECK_EQ(w.hero.action, HERO_THINK);
    run(v, 60);  CHECK_EQ(regression_phase(), REG_SHIELD);   /* t = 4.0 */
    CHECK(w.hero.action != HERO_THINK);
    run(v, 60);  CHECK_EQ(regression_phase(), REG_BOUNCE);   /* t = 5.0 */
    CHECK_EQ(regression_shield(), 1);
    CHECK_EQ(regression_bounced(), 0);
    run(v, 120); CHECK_EQ(regression_phase(), REG_CLEAR);    /* t = 7.0 */
    CHECK_EQ(regression_bounced(), 1);
    run(v, 60);  CHECK_EQ(regression_phase(), REG_DONE);     /* t = 8.0 */
    CHECK(v->is_done(&w));
    CHECK_EQ(waiting(), 0);                                  /* everything dissipated */
    CHECK(boundaries >= 3 && boundaries <= 4);               /* two bonks + shield (+ done) */

    v->exit(&w);
    CHECK_EQ(regression_shield(), 0);
    CHECK_EQ(world_bug_count(&w, SIDE_LEFT), 0);

    /* bouncer must spawn within reach of the shield even near a panel edge */
    events_init(&ev);
    world_init(&w, 33u, 96, 240, 200, &ev);
    w.hero.x = 84.0f; w.hero.facing = 1;
    boundaries = 0;
    v->enter(&w);
    run(v, 420);                                              /* t = 7.0, end of BOUNCE */
    CHECK_EQ(regression_phase(), REG_CLEAR);
    CHECK_EQ(regression_bounced(), 1);
}
