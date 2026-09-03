#include "test.h"
#include "scene.h"

#define DT (1.0f / 60.0f)

static int fake_boundary = 1, fake_done = 0;
static int enters[VIG_COUNT], exits[VIG_COUNT];

#define FAKE(n) \
    static void enter##n(World *w) { (void)w; enters[n]++; } \
    static void exit##n(World *w) { (void)w; exits[n]++; }
FAKE(0) FAKE(1) FAKE(2) FAKE(3) FAKE(4)

static void upd(World *w, float dt) { (void)w; (void)dt; }
static int boundary(const World *w) { (void)w; return fake_boundary; }
static int never_done(const World *w) { (void)w; return 0; }
static int done(const World *w) { (void)w; return fake_done; }

static const Vignette F0 = { VIG_PATROL, enter0, upd, 0, 0, boundary, never_done, exit0 };
static const Vignette F1 = { VIG_RACE, enter1, upd, 0, 0, boundary, done, exit1 };
static const Vignette F2 = { VIG_DETECTIVE, enter2, upd, 0, 0, boundary, done, exit2 };
static const Vignette F3 = { VIG_REGRESSION, enter3, upd, 0, 0, boundary, done, exit3 };
static const Vignette F4 = { VIG_CROSSOVER, enter4, upd, 0, 0, boundary, done, exit4 };
static const Vignette *const TABLE[VIG_COUNT] = { &F0, &F1, &F2, &F3, &F4 };

static World w;
static EventQueue ev;
static Scene sc;

static void fresh(uint32_t seed) {
    events_init(&ev);
    world_init(&w, seed, 96, 240, 200, &ev);
    memset(enters, 0, sizeof enters); memset(exits, 0, sizeof exits);
    fake_boundary = 1; fake_done = 0;
    scene_init(&sc, &w, TABLE);
}

static void steps(int n) { for (int i = 0; i < n; i++) { world_update_actors(&w, DT); scene_step(&sc, DT); } }

/* run until a START event or the step budget runs out; returns packed event or 0 */
static uint32_t until_start(int budget) {
    for (int i = 0; i < budget; i++) {
        steps(1);
        uint32_t e;
        while ((e = events_pop(&ev)) != 0) if (event_type(e) == EV_VIGNETTE_START) return e;
    }
    return 0;
}

void test_scene(void) {
    fresh(1u);
    CHECK_EQ(sc.current, VIG_PATROL);
    CHECK_EQ(sc.pending, -1);
    CHECK_EQ(enters[0], 1);

    /* non-linked ids are ignored */
    scene_request(&sc, VIG_PATROL); scene_request(&sc, VIG_CROSSOVER); scene_request(&sc, 9); scene_request(&sc, -1);
    CHECK_EQ(sc.pending, -1);

    /* a request mid-beat waits; at a boundary it swaps and emits START */
    fake_boundary = 0;
    scene_request(&sc, VIG_RACE);
    steps(5);
    CHECK_EQ(sc.current, VIG_PATROL);
    CHECK_EQ(sc.pending, VIG_RACE);
    CHECK_EQ(events_count(&ev), 0);
    fake_boundary = 1;
    steps(1);
    CHECK_EQ(sc.current, VIG_RACE);
    CHECK_EQ(sc.pending, -1);
    CHECK_EQ(exits[0], 1); CHECK_EQ(enters[1], 1);
    uint32_t e = events_pop(&ev);
    CHECK_EQ(event_type(e), EV_VIGNETTE_START); CHECK_EQ(event_a(e), VIG_RACE); CHECK_EQ(event_b(e), 0);
    CHECK_EQ(events_count(&ev), 0);

    /* a request for the current vignette is ignored */
    scene_request(&sc, VIG_RACE);
    steps(1);
    CHECK_EQ(sc.pending, -1);
    CHECK_EQ(events_count(&ev), 0);

    /* a newer pending request replaces an older one; the interrupted vignette emits END */
    fake_boundary = 0;
    scene_request(&sc, VIG_DETECTIVE);
    scene_request(&sc, VIG_REGRESSION);
    CHECK_EQ(sc.pending, VIG_REGRESSION);
    fake_boundary = 1;
    steps(1);
    CHECK_EQ(sc.current, VIG_REGRESSION);
    CHECK_EQ(enters[2], 0); CHECK_EQ(enters[3], 1); CHECK_EQ(exits[1], 1);
    e = events_pop(&ev); CHECK_EQ(event_type(e), EV_VIGNETTE_END); CHECK_EQ(event_a(e), VIG_RACE);
    e = events_pop(&ev); CHECK_EQ(event_type(e), EV_VIGNETTE_START); CHECK_EQ(event_a(e), VIG_REGRESSION);

    /* a finished vignette returns to patrol with END, and patrol emits nothing */
    fake_done = 1;
    steps(1);
    fake_done = 0;
    CHECK_EQ(sc.current, VIG_PATROL);
    CHECK_EQ(exits[3], 1); CHECK_EQ(enters[0], 2);
    e = events_pop(&ev); CHECK_EQ(event_type(e), EV_VIGNETTE_END); CHECK_EQ(event_a(e), VIG_REGRESSION);
    CHECK_EQ(events_count(&ev), 0);

    /* idle auto-play: fires after 45s of no requests, flagged auto, never the same twice in a row */
    fresh(2u);
    int prev = -1;
    for (int round = 0; round < 6; round++) {
        steps(44 * 60);
        CHECK_EQ(sc.current, VIG_PATROL);
        uint32_t s = until_start(3 * 60);
        CHECK(s != 0u);
        CHECK_EQ(event_b(s), 1);
        CHECK(vignette_is_linked(event_a(s)));
        CHECK(event_a(s) != prev);
        prev = event_a(s);
        fake_done = 1; steps(1); fake_done = 0;
        while (events_pop(&ev)) {}
        sc.since_crossover = 0.0f;              /* keep the crossover timer out of this test */
    }

    /* an external request resets the idle timer */
    fresh(3u);
    steps(40 * 60);
    scene_request(&sc, VIG_RACE);
    steps(1);
    fake_done = 1; steps(1); fake_done = 0;
    while (events_pop(&ev)) {}
    steps(40 * 60);
    CHECK_EQ(sc.current, VIG_PATROL);
    CHECK_EQ(events_count(&ev), 0);

    /* crossover after 120s in patrol, silently; a request during it waits until it is done */
    fresh(4u);
    sc.idle_timer = -1e6f;                      /* keep idle auto-play out of this test */
    steps(121 * 60);
    CHECK_EQ(sc.current, VIG_CROSSOVER);
    CHECK_EQ(enters[4], 1);
    CHECK_EQ(events_count(&ev), 0);
    scene_request(&sc, VIG_RACE);
    steps(10);
    CHECK_EQ(sc.current, VIG_CROSSOVER);
    fake_done = 1; steps(1); fake_done = 0;
    CHECK_EQ(sc.current, VIG_PATROL);
    steps(1);
    CHECK_EQ(sc.current, VIG_RACE);

    /* crossover on a bug imbalance of 6 */
    fresh(5u);
    sc.idle_timer = -1e6f;
    for (int i = 0; i < 6; i++) world_spawn_bug(&w, SIDE_RIGHT);
    steps(1);
    CHECK_EQ(sc.current, VIG_CROSSOVER);
    fresh(6u);
    sc.idle_timer = -1e6f;
    for (int i = 0; i < 5; i++) world_spawn_bug(&w, SIDE_RIGHT);
    steps(60);
    CHECK_EQ(sc.current, VIG_PATROL);

    /* never crossover during a linked vignette */
    fresh(7u);
    scene_request(&sc, VIG_RACE);
    steps(1);
    sc.since_crossover = 500.0f;
    for (int i = 0; i < 6; i++) world_spawn_bug(&w, SIDE_RIGHT);
    steps(60);
    CHECK_EQ(sc.current, VIG_RACE);

    /* scene_reset ends a live linked vignette cleanly: exit hook runs, END fires, and patrol resumes */
    fresh(8u);
    scene_request(&sc, VIG_RACE);
    steps(1);
    CHECK_EQ(sc.current, VIG_RACE);
    while (events_pop(&ev)) {}
    scene_reset(&sc);
    CHECK_EQ(sc.current, VIG_PATROL);
    CHECK_EQ(sc.pending, -1);
    CHECK_EQ(exits[1], 1);
    e = events_pop(&ev);
    CHECK_EQ(event_type(e), EV_VIGNETTE_END); CHECK_EQ(event_a(e), VIG_RACE);
    CHECK_EQ(events_count(&ev), 0);
}
