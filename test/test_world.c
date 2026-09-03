#include "test.h"
#include "world.h"

#define DT (1.0f / 60.0f)

void test_world(void) {
    static World w;
    EventQueue ev;
    events_init(&ev);
    world_init(&w, 1u, 96, 240, 200, &ev);

    CHECK_EQ(w.world_w, 392);
    CHECK_EQ(w.floor_top, 132);
    CHECK_EQ(w.floor_bottom, 216);
    CHECK_EQ(world_side_of_x(&w, 10.0f), SIDE_LEFT);
    CHECK_EQ(world_side_of_x(&w, 150.0f), SIDE_GAP);
    CHECK_EQ(world_side_of_x(&w, 300.0f), SIDE_RIGHT);
    CHECK_NEAR(world_panel_x0(&w, SIDE_LEFT), 0.0f, 1e-6);
    CHECK_NEAR(world_panel_x0(&w, SIDE_RIGHT), 296.0f, 1e-6);
    CHECK_NEAR(world_panel_center_x(&w, SIDE_LEFT), 48.0f, 1e-6);
    CHECK_EQ(world_hero_side(&w), SIDE_LEFT);
    CHECK(w.hero.y >= w.floor_top && w.hero.y <= w.floor_bottom);
    CHECK_EQ(world_bug_count(&w, SIDE_LEFT), 0);

    /* spawn to the cap, then NULL */
    for (int i = 0; i < MAX_BUGS_PER_SIDE; i++) CHECK(world_spawn_bug(&w, SIDE_LEFT) != NULL);
    CHECK(world_spawn_bug(&w, SIDE_LEFT) == NULL);
    CHECK_EQ(world_bug_count(&w, SIDE_LEFT), MAX_BUGS_PER_SIDE);
    CHECK_EQ(world_bug_count(&w, SIDE_RIGHT), 0);
    CHECK(world_spawn_bug(&w, SIDE_RIGHT) != NULL);

    /* every spawned bug is on its own side's floor band and at a panel edge */
    for (int i = 0; i < MAX_BUGS; i++) {
        Bug *b = &w.bugs[i];
        if (!bug_alive(b)) continue;
        CHECK_EQ(world_side_of_x(&w, b->x), b->side);
        CHECK(b->y >= w.floor_top && b->y <= w.floor_bottom);
        float rel = b->x - world_panel_x0(&w, b->side);
        CHECK(rel <= BUG_W || rel >= 96 - BUG_W);
    }

    /* nearest and squash */
    world_init(&w, 2u, 96, 240, 200, &ev);
    Bug *far = world_spawn_bug_at(&w, SIDE_LEFT, 80.0f, 200.0f);
    Bug *near = world_spawn_bug_at(&w, SIDE_LEFT, 20.0f, 200.0f);
    CHECK(world_nearest_bug(&w, SIDE_LEFT, 10.0f, 200.0f) == near);
    CHECK(world_nearest_bug(&w, SIDE_RIGHT, 10.0f, 200.0f) == NULL);
    CHECK_EQ(world_squash_near(&w, 22.0f, 200.0f, HERO_REACH), 1);
    CHECK_EQ(near->state, BUG_SQUASHED);
    CHECK_EQ(far->state, BUG_WANDER);
    CHECK_EQ(events_count(&ev), 1);
    CHECK_EQ(event_type(events_pop(&ev)), EV_BUG_SQUASHED);
    CHECK_EQ(w.squashed_total, 1);
    int dust = 0, plus = 0;
    for (int i = 0; i < MAX_FX; i++) if (w.fx[i].active) { if (w.fx[i].kind == FX_DUST) dust++; if (w.fx[i].kind == FX_PLUS1) plus++; }
    CHECK_EQ(dust, 1); CHECK_EQ(plus, 1);
    CHECK(world_nearest_bug(&w, SIDE_LEFT, 10.0f, 200.0f) == far);   /* squashed bugs are not targets */

    /* actors update: fx expire, squashed bug dies, wanderers stay in bounds */
    for (int i = 0; i < 60; i++) world_update_actors(&w, DT);
    CHECK_EQ(near->state, BUG_DEAD);
    for (int i = 0; i < MAX_FX; i++) CHECK(!w.fx[i].active);
    CHECK(far->x >= BUG_W / 2 && far->x <= 96 - BUG_W / 2);

    /* scripted bugs are cleared, wanderers kept */
    Bug *s = world_spawn_bug_at(&w, SIDE_LEFT, 30.0f, 200.0f);
    bug_set_target(s, 50.0f, 200.0f);
    world_clear_scripted_bugs(&w);
    CHECK_EQ(s->state, BUG_DEAD);
    CHECK_EQ(far->state, BUG_WANDER);
}
