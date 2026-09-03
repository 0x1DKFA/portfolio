#include "test.h"
#include "vignettes/race.h"
#include "sprites.h"
#include "palette.h"

#define DT (1.0f / 60.0f)

static World w;
static int boundaries;

static void run(const Vignette *v, int steps) {
    for (int i = 0; i < steps; i++) {
        world_update_actors(&w, DT); v->update(&w, DT);
        if (v->at_beat_boundary(&w)) boundaries++;
    }
}

void test_race(void) {
    EventQueue ev;
    events_init(&ev);
    world_init(&w, 8u, 96, 240, 200, &ev);
    const Vignette *v = &vignette_race;
    CHECK_EQ(v->id, VIG_RACE);
    boundaries = 0;

    v->enter(&w);
    CHECK_EQ(race_phase(), RACE_SETUP);
    CHECK_EQ(race_box_state(), BOX_NORMAL);
    CHECK(!v->is_done(&w));

    run(v, 30);  CHECK_EQ(race_phase(), RACE_SETUP);        /* t = 0.5 */
    run(v, 42);  CHECK_EQ(race_phase(), RACE_RUSH);         /* t = 1.2 */
    CHECK_EQ(world_bug_count(&w, SIDE_LEFT), 2);
    run(v, 108); CHECK_EQ(race_phase(), RACE_COLLIDE);      /* t = 3.0 */
    CHECK_EQ(race_box_state(), BOX_GLITCH);
    run(v, 120); CHECK_EQ(race_phase(), RACE_LOCK);         /* t = 5.0 */
    run(v, 120); CHECK_EQ(race_phase(), RACE_QUEUE);        /* t = 7.0 */
    CHECK_EQ(race_box_state(), BOX_LOCKED);
    CHECK_EQ(race_value(), 0);
    run(v, 162); CHECK_EQ(race_phase(), RACE_RESOLVED);     /* t = 9.7 */
    CHECK_EQ(race_box_state(), BOX_GREEN);
    CHECK_EQ(race_value(), 3);
    CHECK(!v->is_done(&w));
    run(v, 30);  CHECK_EQ(race_phase(), RACE_DONE);         /* t = 10.2 */
    CHECK(v->is_done(&w));
    CHECK_EQ(boundaries, 6);                                 /* one boundary per phase change */

    /* the box is drawn into the panel */
    static uint8_t px[192 * 240 * 4];
    Framebuffer fb;
    fb_init(&fb, px, 192, 240);
    draw_clear(&fb, PALETTE_BUGS[COL_BG]);
    fb_set_view(&fb, 0, 0, 96, 240, 0, 0);
    v->exit(&w);
    v->enter(&w);
    run(v, 200);                                             /* mid-COLLIDE: red box */
    v->draw_back(&w, &fb, PALETTE_BUGS);
    int red = 0;
    for (int y = 0; y < 240; y++) for (int x = 0; x < 96; x++) {
        Color c = fb_get(&fb, x, y);
        if (c.r == PALETTE_BUGS[COL_RED].r && c.g == PALETTE_BUGS[COL_RED].g && c.b == PALETTE_BUGS[COL_RED].b) red++;
    }
    CHECK(red > 30);

    v->exit(&w);
    CHECK_EQ(world_bug_count(&w, SIDE_LEFT), 0);              /* scripted bugs are cleared */
}
