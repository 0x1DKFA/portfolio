#include "test.h"
#include "world.h"
#include "palette.h"

#define DT (1.0f / 60.0f)
static uint8_t px[640 * 320 * 4];
static int same(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b; }
static int accent(Color c) { return c.r > 180 && c.g > 55 && c.g < 130 && c.b < 100; }

static int run_until_state(World *w, int state, int max_steps) {
    for (int i = 0; i < max_steps; i++) { world_step(w, DT); if (hunt_state(w) == state) return i + 1; }
    return -1;
}

void test_hunt(void) {
    static World w, w2;
    EventQueue ev; events_init(&ev);
    CHECK_EQ(world_init(&w, 5u, 320, 200, &ev, "ROHIT"), 0);
    CHECK_EQ(world_init(&w2, 5u, 100, 200, &ev, "ROHIT"), -1);
    CHECK_EQ(world_init(&w2, 5u, 320, 400, &ev, "ROHIT"), -1);

    /* initial state: a trail is laid to a hiding spot, the bug is hidden there, the camera is at the start */
    CHECK_EQ(hunt_state(&w), HUNT_LOOK);
    CHECK_EQ(hunt_cycles(&w), 0);
    CHECK(w.hunt.path.n - 1 >= HUNT_MIN_STEPS && w.hunt.path.n - 1 <= HUNT_MAX_STEPS);
    CHECK(w.decals.n > 0);
    CHECK(map_is_hiding_spot(&w.map, w.hunt.hiding.x, w.hunt.hiding.y));
    CHECK_EQ(w.bug.state, BUGSTATE_HIDDEN);
    CHECK(w.bug.spr != NULL);
    CHECK_NEAR(w.cam.x, w.map.start_x + 0.5f, 1e-5);
    CHECK_NEAR(w.cam.y, w.map.start_y + 0.5f, 1e-5);
    CHECK(w.n_dogs == 3);
    CHECK(w.n_loose >= 6);
    int lamps = 0; for (int i = 0; i < MAX_SPRITES; i++) if (w.sprites.s[i].active && w.sprites.s[i].kind == SPR_LAMP) lamps++;
    CHECK(lamps >= 12);

    /* LOOK pans, then FOLLOW walks the trail on floor tiles */
    int steps = run_until_state(&w, HUNT_FOLLOW, 3 * 60);
    int expected_look_steps = (int)(HUNT_LOOK_TIME * 60.0f);
    CHECK(steps >= expected_look_steps - 2 && steps <= expected_look_steps + 2);
    int walked = 0, inspected = 0;
    float x0 = w.cam.x, y0 = w.cam.y;
    for (int i = 0; i < 4 * 60 && hunt_state(&w) == HUNT_FOLLOW; i++) {
        world_step(&w, DT);
        CHECK(map_is_floor(&w.map, (int)w.cam.x, (int)w.cam.y));
        if (w.cam.x != x0 || w.cam.y != y0) walked = 1;
        if (w.cam.pitch_px != 0) inspected = 1;
    }
    CHECK(walked);

    /* the first cycle completes with exactly one squash event, within 25 to 70 seconds */
    int total = steps;
    while (hunt_cycles(&w) < 1 && total < 150 * 60) {
        world_step(&w, DT); total++;
        CHECK(map_is_floor(&w.map, (int)w.cam.x, (int)w.cam.y));
        if (w.cam.pitch_px != 0) inspected = 1;
    }
    CHECK_EQ(hunt_cycles(&w), 1);
    CHECK(total >= 10 * 60 && total <= 30 * 60);
    CHECK_EQ(events_count(&ev), 1);
    CHECK_EQ(event_type(events_pop(&ev)), EV_BUG_SQUASHED);
    CHECK_EQ(w.squashed_total, 1);
    CHECK_EQ(hunt_state(&w), HUNT_LOOK);                     /* a fresh trail */
    CHECK(w.hunt.path.n - 1 >= HUNT_MIN_STEPS && w.hunt.path.n - 1 <= HUNT_MAX_STEPS);
    CHECK_EQ(w.bug.state, BUGSTATE_HIDDEN);
    CHECK(w.bug.spr != NULL);
    CHECK(w.dust != NULL || w.dust_t == 0.0f);

    /* two more cycles: still one event each, an inspect dip happened at some point */
    int t2 = 0;
    while (hunt_cycles(&w) < 3 && t2 < 300 * 60) { world_step(&w, DT); t2++; if (w.cam.pitch_px != 0) inspected = 1; }
    CHECK_EQ(hunt_cycles(&w), 3);
    CHECK_EQ(events_count(&ev), 2);
    CHECK(inspected);
    while (events_pop(&ev)) {}

    /* deterministic: same seed, same first squash step */
    EventQueue ev2; events_init(&ev2);
    static World a, b;
    world_init(&a, 21u, 320, 200, &ev, "ROHIT");
    world_init(&b, 21u, 320, 200, &ev2, "ROHIT");
    int sa = -1, sb = -1;
    for (int i = 0; i < 150 * 60 && (sa < 0 || sb < 0); i++) {
        if (sa < 0) { world_step(&a, DT); if (events_count(&ev)) sa = i; }
        if (sb < 0) { world_step(&b, DT); if (events_count(&ev2)) sb = i; }
    }
    CHECK(sa > 0);
    CHECK_EQ(sa, sb);
    while (events_pop(&ev)) {} while (events_pop(&ev2)) {}

    /* rendering: every quadrant painted, sky at the top, hammer at the bottom centre, horizon in range */
    Framebuffer fb; fb_init(&fb, px, 320, 200);
    draw_clear(&fb, COLOR(0, 0, 0));
    world_init(&w, 5u, 320, 200, &ev, "ROHIT");
    for (int i = 0; i < 120; i++) world_step(&w, DT);
    int hz = world_horizon(&w);
    CHECK(hz >= 50 && hz <= 150);
    world_render(&w, &fb);
    int q[4] = { 0, 0, 0, 0 };
    for (int y = 0; y < 200; y++) for (int x = 0; x < 320; x++)
        if (!same(fb_get(&fb, x, y), COLOR(0, 0, 0))) q[(y >= 100) * 2 + (x >= 160)]++;
    for (int k = 0; k < 4; k++) CHECK(q[k] > 10000);
    int hammer = 0;
    for (int y = 130; y < 200; y++) for (int x = 120; x < 200; x++) if (same(fb_get(&fb, x, y), PALETTE_NIGHT[COL_HAMMER])) hammer++;
    CHECK(hammer > 100);
    int accent_floor = 0;
    for (int y = hz + 1; y < 200; y++) for (int x = 0; x < 320; x++) if (accent(fb_get(&fb, x, y)) && !same(fb_get(&fb, x, y), PALETTE_NIGHT[COL_HAMMER])) accent_floor++;
    CHECK(accent_floor > 20);                                      /* orange footprints ahead */

    /* an inspect pause must never be read as arrival, even if APPROACH starts during it */
    world_init(&w, 5u, 320, 200, &ev, "ROHIT");
    run_until_state(&w, HUNT_FOLLOW, 3 * 60);
    for (int i = 0; i < 30 && w.hunt.walking_to < 0; i++) world_step(&w, DT);   /* a leg is in progress */
    CHECK(w.hunt.walking_to >= 0);
    int wp_before = w.hunt.waypoint, leg = w.hunt.walking_to;
    float mid_x = w.cam.x, mid_y = w.cam.y;
    w.hunt.inspecting = 1; w.hunt.inspect_t = 0.0f; w.cam.walking = 0;          /* the pause */
    w.hunt.state = HUNT_APPROACH; w.hunt.t = 0.0f;                                /* the race: APPROACH begins mid-pause */
    world_step(&w, DT);
    CHECK_EQ(w.hunt.waypoint, wp_before);                                          /* no skipped waypoint */
    CHECK_EQ(w.hunt.walking_to, leg);
    CHECK_EQ(w.hunt.inspecting, 0);                                                /* the pause was ended cleanly */
    CHECK_EQ(w.cam.pitch_px, 0);
    CHECK(w.cam.walking);                                                          /* and the leg resumed */
    CHECK(camera_dist(&w.cam, mid_x, mid_y) < 0.1f);                              /* from where it stood */

    /* A stationary dog blocks the camera instead of letting it pass through. */
    world_init(&w, 5u, 320, 200, &ev, "ROHIT");
    w.hunt.state = HUNT_FOLLOW; w.hunt.inspect_timer = 100.0f;
    w.hunt.bug_x = 20.5f; w.hunt.bug_y = 20.5f;
    for (int i = 0; i < w.n_dogs; i++) { w.dogs[i].state = DOG_SNIFF; w.dogs[i].t = 100.0f; }
    w.dogs[0].x = 3.5f; w.dogs[0].y = 2.5f;
    camera_walk_to(&w.cam, 4.5f, 2.5f);
    for (int i = 0; i < 120; i++) world_step(&w, DT);
    CHECK(w.cam.x < 3.1f);
    CHECK(w.cam.walking);

    /* the largest framebuffer works too */
    Framebuffer big; fb_init(&big, px, 640, 320);
    CHECK_EQ(world_init(&w, 5u, 640, 320, &ev, "ROHIT"), 0);
    world_step(&w, DT);
    world_render(&w, &big);
    CHECK(!same(fb_get(&big, 320, 160), COLOR(0, 0, 0)));
}
