#include "test.h"
#include "draw.h"

static uint8_t px[16 * 8 * 4];

static int is(Color c, int r, int g, int b) { return c.r == r && c.g == g && c.b == b && c.a == 255; }

void test_draw(void) {
    Framebuffer fb;
    fb_init(&fb, px, 16, 8);
    CHECK_EQ(fb.w, 16); CHECK_EQ(fb.h, 8);
    CHECK_EQ(fb.clip_x1, 16); CHECK_EQ(fb.clip_y1, 8);

    draw_clear(&fb, COLOR(1, 2, 3));
    CHECK(is(fb_get(&fb, 0, 0), 1, 2, 3));
    CHECK(is(fb_get(&fb, 15, 7), 1, 2, 3));

    /* rect clips at framebuffer edges without touching memory outside */
    draw_rect(&fb, 14, 6, 10, 10, COLOR(9, 9, 9));
    CHECK(is(fb_get(&fb, 14, 6), 9, 9, 9));
    CHECK(is(fb_get(&fb, 15, 7), 9, 9, 9));
    CHECK(is(fb_get(&fb, 13, 6), 1, 2, 3));
    draw_rect(&fb, -3, -3, 5, 5, COLOR(7, 7, 7));
    CHECK(is(fb_get(&fb, 0, 0), 7, 7, 7));
    CHECK(is(fb_get(&fb, 1, 1), 7, 7, 7));
    CHECK(is(fb_get(&fb, 2, 2), 1, 2, 3));

    /* view: right half of the buffer, drawing coords offset by +8 */
    draw_clear(&fb, COLOR(0, 0, 0));
    fb_set_view(&fb, 8, 0, 8, 8, 8, 0);
    draw_rect(&fb, 0, 0, 2, 2, COLOR(5, 5, 5));    /* lands at x=8..9 */
    CHECK(is(fb_get(&fb, 8, 0), 5, 5, 5));
    CHECK(is(fb_get(&fb, 9, 1), 5, 5, 5));
    CHECK(is(fb_get(&fb, 0, 0), 0, 0, 0));
    draw_rect(&fb, -4, 0, 6, 1, COLOR(6, 6, 6));   /* x=4..9 world, clipped to 8..9 */
    CHECK(is(fb_get(&fb, 7, 0), 0, 0, 0));
    CHECK(is(fb_get(&fb, 8, 0), 6, 6, 6));
    fb_reset_view(&fb);
    draw_pixel(&fb, 7, 0, COLOR(4, 4, 4));
    CHECK(is(fb_get(&fb, 7, 0), 4, 4, 4));

    /* dim reduces every channel; amount 255 gives black, 0 leaves unchanged */
    draw_clear(&fb, COLOR(200, 100, 50));
    draw_dim(&fb, 0, 0, 4, 4, 128);
    Color d = fb_get(&fb, 1, 1);
    CHECK(d.r < 200 && d.g < 100 && d.b < 50 && d.a == 255);
    CHECK(is(fb_get(&fb, 5, 5), 200, 100, 50));
    draw_dim(&fb, 0, 0, 4, 4, 255);
    CHECK(is(fb_get(&fb, 1, 1), 0, 0, 0));
    draw_dim(&fb, 5, 5, 1, 1, 0);
    CHECK(is(fb_get(&fb, 5, 5), 200, 100, 50));
}
