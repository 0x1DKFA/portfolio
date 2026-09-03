#include "test.h"
#include "hud.h"
#include "palette.h"

#define DT (1.0f / 60.0f)
static uint8_t px[320 * 200 * 4];
static int same(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b; }
static int count(const Framebuffer *fb, Color c, int y0, int y1) {
    int n = 0; for (int y = y0; y < y1; y++) for (int x = 0; x < fb->w; x++) if (same(fb_get(fb, x, y), c)) n++; return n;
}

void test_hud(void) {
    static Textures t;
    const Color *pal = PALETTE_NIGHT;
    Rng rng; rng_seed(&rng, 2u);
    textures_generate(&t, &rng, pal, "ROHIT");
    Hud h; hud_init(&h);
    CHECK_EQ(hud_frame(&h), 0);
    CHECK(!h.swinging);

    hud_start_swing(&h);
    CHECK(h.swinging);
    int hits = 0, hit_step = -1, saw_raised = 0, saw_down = 0;
    for (int i = 0; i < 40; i++) {
        if (hud_step(&h, DT)) { hits++; hit_step = i; }
        if (hud_frame(&h) == 1) saw_raised = 1;
        if (hud_frame(&h) == 2) saw_down = 1;
    }
    CHECK_EQ(hits, 1);
    CHECK(hit_step >= 13 && hit_step <= 16);                 /* 0.25 s */
    CHECK(saw_raised); CHECK(saw_down);
    CHECK(!h.swinging);                                        /* 0.4 s swing is over after 40 steps */
    CHECK_EQ(hud_frame(&h), 0);
    for (int i = 0; i < 10; i++) CHECK(!hud_step(&h, DT));   /* no second hit */

    hud_flash_plus(&h);
    CHECK(h.plus_t > 0.79f);
    for (int i = 0; i < 48; i++) hud_step(&h, DT);
    CHECK(h.plus_t <= 0.001f);

    /* drawing: hammer yellow at the bottom centre, nothing at the top; +1 while flashing; bob shifts it */
    Framebuffer fb; fb_init(&fb, px, 320, 200);
    draw_clear(&fb, pal[COL_BG]);
    hud_draw(&h, &t, pal, &fb, 0);
    CHECK(count(&fb, pal[COL_HAMMER], 130, 200) > 150);
    CHECK_EQ(count(&fb, pal[COL_HAMMER], 0, 100), 0);
    CHECK_EQ(count(&fb, pal[COL_FOOTPRINT], 0, 200), 0);
    int hand_row = -1;
    for (int y = 199; y >= 0 && hand_row < 0; y--) if (same(fb_get(&fb, 160, y), pal[COL_HAND])) hand_row = y;
    CHECK(hand_row > 150);
    draw_clear(&fb, pal[COL_BG]);
    hud_flash_plus(&h);
    hud_draw(&h, &t, pal, &fb, 0);
    CHECK(count(&fb, pal[COL_FOOTPRINT], 0, 200) > 20);
    draw_clear(&fb, pal[COL_BG]);
    hud_draw(&h, &t, pal, &fb, 3);
    int top_a = 200; for (int y = 0; y < 200; y++) if (same(fb_get(&fb, 160, y), pal[COL_HAMMER]) || same(fb_get(&fb, 150, y), pal[COL_HAMMER])) { top_a = y; break; }
    draw_clear(&fb, pal[COL_BG]);
    hud_draw(&h, &t, pal, &fb, -3);
    int top_b = 200; for (int y = 0; y < 200; y++) if (same(fb_get(&fb, 160, y), pal[COL_HAMMER]) || same(fb_get(&fb, 150, y), pal[COL_HAMMER])) { top_b = y; break; }
    CHECK_EQ(top_a - top_b, 6);
}
