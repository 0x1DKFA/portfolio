#include "test.h"
#include "sim.h"
#include "vignettes/table.h"
#include "palette.h"
#include "world.h"

static int same(Color a, const Color *b) { return a.r == b->r && a.g == b->g && a.b == b->b; }

/* count non-background pixels in a column range of the framebuffer */
static int painted_cols(const uint8_t *px, int w, int h, int x0, int x1) {
    int n = 0;
    for (int y = 0; y < h; y++) for (int x = x0; x < x1; x++) {
        const uint8_t *p = px + (y * w + x) * 4;
        Color c = { p[0], p[1], p[2], p[3] };
        if (!same(c, &PALETTE_BUGS[COL_BG]) && !same(c, &PALETTE_BUGS[COL_GRID]) &&
            !same(c, &PALETTE_BUGS[COL_FLOOR]) && !same(c, &PALETTE_BUGS[COL_HORIZON])) n++;
    }
    return n;
}

static int has_color(const uint8_t *px, int w, int h, int x0, int x1, const Color *c) {
    for (int y = 0; y < h; y++) for (int x = x0; x < x1; x++) {
        const uint8_t *p = px + (y * w + x) * 4;
        if (p[0] == c->r && p[1] == c->g && p[2] == c->b) return 1;
    }
    return 0;
}

/* advance in 17ms frames, collecting the first event of a given type and id; returns 1 if seen */
static int run_until_event(int frames, int type, int id) {
    for (int i = 0; i < frames; i++) {
        sim_update(17);
        uint32_t e;
        while ((e = sim_poll_event()) != 0)
            if (event_type(e) == type && (id < 0 || event_a(e) == id)) return 1;
    }
    return 0;
}

void test_sim(void) {
    for (int i = 0; i < VIG_COUNT; i++) CHECK_EQ(VIGNETTES[i]->id, i);

    CHECK_EQ(sim_init(1u, 96, 1000, 200), -1);
    CHECK_EQ(sim_init(1u, 200, 240, 200), -1);
    CHECK_EQ(sim_init(1u, 96, 240, -5), -1);
    CHECK_EQ(sim_init(1234u, 96, 240, 200), 0);
    CHECK_EQ(sim_framebuffer_len(), 2 * 96 * 240 * 4);
    CHECK(sim_framebuffer() != 0);

    /* first frame: both panels show bugs (4 per side spawned at init) */
    sim_update(17);
    sim_render();
    const uint8_t *px = sim_framebuffer();
    CHECK(painted_cols(px, 192, 240, 0, 96) > 40);
    CHECK(painted_cols(px, 192, 240, 96, 192) > 40);
    CHECK(has_color(px, 192, 240, 0, 96, &PALETTE_BUGS[COL_HERO_SKIN]));      /* hero starts left */
    CHECK(!has_color(px, 192, 240, 96, 192, &PALETTE_BUGS[COL_HERO_SKIN]));

    /* a request plays the race vignette: START then, about ten seconds later, END */
    while (sim_poll_event()) {}
    sim_request(1);
    CHECK(run_until_event(120, EV_VIGNETTE_START, 1));
    CHECK(run_until_event(800, EV_VIGNETTE_END, 1));

    /* non-linked ids never start anything */
    while (sim_poll_event()) {}
    sim_request(0); sim_request(4); sim_request(7);
    CHECK(!run_until_event(120, EV_VIGNETTE_START, -1));

    /* squashes arrive as events while patrolling */
    CHECK(run_until_event(1200, EV_BUG_SQUASHED, -1));

    /* update is clamped: a huge elapsed time does not fast-forward for minutes */
    sim_request(2);
    CHECK(run_until_event(120, EV_VIGNETTE_START, 2));
    sim_update(60000);
    uint32_t e; int ended = 0;
    while ((e = sim_poll_event()) != 0) if (event_type(e) == EV_VIGNETTE_END) ended = 1;
    CHECK(!ended);

    /* static frame: hero mid-bonk on the left, bugs on both sides */
    sim_render_static();
    e = sim_poll_event();
    CHECK_EQ(event_type(e), EV_VIGNETTE_END); CHECK_EQ(event_a(e), 2);
    CHECK_EQ(sim_poll_event(), 0u);
    px = sim_framebuffer();
    CHECK(has_color(px, 192, 240, 0, 96, &PALETTE_BUGS[COL_HERO_SKIN]));
    CHECK(has_color(px, 192, 240, 0, 96, &PALETTE_BUGS[COL_HAMMER]));
    CHECK(painted_cols(px, 192, 240, 96, 192) > 40);

    /* avatar: 24x24 with the hero and one bug */
    CHECK_EQ(sim_avatar_len(), 24 * 24 * 4);
    sim_render_avatar();
    const uint8_t *av = sim_avatar_buffer();
    CHECK(has_color(av, 24, 24, 0, 24, &PALETTE_BUGS[COL_HERO_SKIN]));
    CHECK(has_color(av, 24, 24, 0, 24, &PALETTE_BUGS[COL_BUG_A]));

    /* detective dims only its own panel */
    CHECK_EQ(sim_init(5u, 96, 240, 200), 0);
    sim_request(2);
    CHECK(run_until_event(120, EV_VIGNETTE_START, 2));
    for (int i = 0; i < 350; i++) sim_update(17);
    sim_render();
    px = sim_framebuffer();
    const uint8_t *row2 = px + 2 * 192 * 4;
    CHECK(has_color(row2, 192, 1, 96, 192, &PALETTE_BUGS[COL_BG]));
    int left_dimmed = 1;
    for (int x = 0; x < 96; x++) {
        const uint8_t *p = row2 + x * 4;
        if (!(p[0] < PALETTE_BUGS[COL_BG].r && p[1] < PALETTE_BUGS[COL_BG].g && p[2] < PALETTE_BUGS[COL_BG].b)) { left_dimmed = 0; break; }
    }
    CHECK(left_dimmed);

    /* a real crossover happens through natural scene transitions, not a scripted request */
    CHECK_EQ(sim_init(21u, 96, 240, 200), 0);
    int seen_left = 0, seen_right = 0, both_at_once = 0;
    int total_frames = 130000 / 17;
    for (int i = 0; i < total_frames; i++) {
        sim_update(17);
        if ((i + 1) % 60 == 0) {
            sim_render();
            px = sim_framebuffer();
            int l = has_color(px, 192, 240, 0, 96, &PALETTE_BUGS[COL_HERO_SKIN]);
            int r = has_color(px, 192, 240, 96, 192, &PALETTE_BUGS[COL_HERO_SKIN]);
            if (l) seen_left = 1;
            if (r) seen_right = 1;
            if (l && r) both_at_once = 1;
        }
    }
    CHECK(!both_at_once);
    CHECK(seen_right);
    (void)seen_left;

    /* re-init resets the scene without leaking state */
    CHECK_EQ(sim_init(99u, 96, 200, 100), 0);
    CHECK_EQ(sim_framebuffer_len(), 2 * 96 * 200 * 4);
    CHECK_EQ(sim_poll_event(), 0u);
}
