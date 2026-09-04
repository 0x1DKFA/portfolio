#include "test.h"
#include "sim.h"
#include "palette.h"
#include "world.h"

static int same(Color a, const Color *b) { return a.r == b->r && a.g == b->g && a.b == b->b; }
static int yellow(const uint8_t *p) { return p[0] > 180 && p[1] > 130 && p[2] < 110; }

static int painted(const uint8_t *px, int w, int h) {
    int n = 0;
    for (int i = 0; i < w * h; i++) if (px[i * 4] || px[i * 4 + 1] || px[i * 4 + 2]) n++;
    return n;
}

static int has_color(const uint8_t *px, int w, int h, const Color *c) {
    for (int i = 0; i < w * h; i++) if (px[i * 4] == c->r && px[i * 4 + 1] == c->g && px[i * 4 + 2] == c->b) return 1;
    return 0;
}

void test_sim(void) {
    CHECK_EQ(sim_framebuffer_len(), 0);
    CHECK_EQ(sim_init(1u, 100, 200), -1);
    CHECK_EQ(sim_init(1u, 700, 200), -1);
    CHECK_EQ(sim_init(1u, 356, 100), -1);
    CHECK_EQ(sim_init(1u, 356, 400), -1);
    CHECK_EQ(sim_init(1234u, 356, 200), 0);
    CHECK_EQ(sim_framebuffer_len(), 356 * 200 * 4);
    CHECK(sim_framebuffer() != NULL);
    CHECK_EQ(sim_poll_event(), 0u);

    /* a first frame paints everything */
    sim_update(17);
    sim_render();
    const uint8_t *px = sim_framebuffer();
    CHECK(painted(px, 356, 200) > 356 * 200 * 9 / 10);
    CHECK(has_color(px, 356, 200, &PALETTE_NIGHT[COL_HAMMER]));

    /* the hunt produces a squash within two minutes of simulated time */
    int squashed = 0, frames = 0;
    while (!squashed && frames < 120 * 1000 / 17) {
        sim_update(17); frames++;
        for (uint32_t e = sim_poll_event(); e != 0; e = sim_poll_event())
            if (event_type(e) == EV_BUG_SQUASHED && event_stage(e) == STAGE_BUGS) squashed++;
    }
    CHECK_EQ(squashed, 1);
    CHECK(frames * 17 >= 10 * 1000);

    /* the update clamps: a minute of elapsed time is at most 250 ms of simulation */
    CHECK_EQ(sim_init(9u, 356, 200), 0);
    sim_update(60000);
    int events_after_jump = 0;
    for (uint32_t e = sim_poll_event(); e != 0; e = sim_poll_event()) events_after_jump++;
    CHECK_EQ(events_after_jump, 0);

    /* static frame: footprints and the hammer, deterministic */
    sim_render_static();
    px = sim_framebuffer();
    int yellow_px = 0;
    for (int i = 0; i < 356 * 200; i++) if (yellow(px + i * 4) && !same((Color){ px[i*4], px[i*4+1], px[i*4+2], 255 }, &PALETTE_NIGHT[COL_HAMMER])) yellow_px++;
    CHECK(yellow_px > 20);
    CHECK(has_color(px, 356, 200, &PALETTE_NIGHT[COL_HAMMER]));
    CHECK_EQ(sim_poll_event(), 0u);
    static uint8_t copy[640 * 320 * 4];
    for (int i = 0; i < 356 * 200 * 4; i++) copy[i] = px[i];
    sim_render_static();
    int diff = 0;
    for (int i = 0; i < 356 * 200 * 4; i++) if (copy[i] != sim_framebuffer()[i]) diff++;
    CHECK_EQ(diff, 0);

    /* extremes */
    CHECK_EQ(sim_init(3u, 200, 160), 0);
    CHECK_EQ(sim_framebuffer_len(), 200 * 160 * 4);
    sim_render();
    CHECK(painted(sim_framebuffer(), 200, 160) > 200 * 160 * 9 / 10);
    CHECK_EQ(sim_init(3u, 640, 320), 0);
    CHECK_EQ(sim_framebuffer_len(), 640 * 320 * 4);
    sim_update(17); sim_render();
    CHECK(painted(sim_framebuffer(), 640, 320) > 640 * 320 * 9 / 10);
}
