#include "test.h"
#include "draw.h"
#include "font.h"

static uint8_t px[32 * 8 * 4];
static int lit(const Framebuffer *fb, int x, int y) { return fb_get(fb, x, y).r == 255; }

void test_font(void) {
    CHECK(font_glyph('0') != NULL);
    CHECK(font_glyph('c') == font_glyph('C'));
    CHECK(font_glyph('~') == NULL);
    CHECK_EQ(strlen(font_glyph('1')), 15);

    CHECK_EQ(draw_text_width(""), 0);
    CHECK_EQ(draw_text_width("A"), 3);
    CHECK_EQ(draw_text_width("AB"), 7);
    CHECK_EQ(draw_text_width("COUNT"), 19);

    Framebuffer fb;
    fb_init(&fb, px, 32, 8);
    draw_clear(&fb, COLOR(0, 0, 0));
    draw_text(&fb, 0, 0, "1", COLOR(255, 255, 255));
    /* glyph '1' is " # ,## , # , # ,###" */
    CHECK(lit(&fb, 1, 0)); CHECK(!lit(&fb, 0, 0)); CHECK(!lit(&fb, 2, 0));
    CHECK(lit(&fb, 0, 1)); CHECK(lit(&fb, 1, 1)); CHECK(!lit(&fb, 2, 1));
    CHECK(lit(&fb, 0, 4)); CHECK(lit(&fb, 1, 4)); CHECK(lit(&fb, 2, 4));
    CHECK(!lit(&fb, 3, 0));   /* gap column */

    draw_clear(&fb, COLOR(0, 0, 0));
    draw_text(&fb, 0, 0, "+1", COLOR(255, 255, 255));
    CHECK(lit(&fb, 1, 1)); CHECK(lit(&fb, 0, 2)); CHECK(lit(&fb, 2, 2)); CHECK(!lit(&fb, 0, 0));
    CHECK(lit(&fb, 5, 0));    /* '1' starts at x=4 */

    /* unknown glyph: hollow box has corners lit and centre dark */
    draw_clear(&fb, COLOR(0, 0, 0));
    draw_text(&fb, 0, 0, "~", COLOR(255, 255, 255));
    CHECK(lit(&fb, 0, 0)); CHECK(lit(&fb, 2, 4)); CHECK(!lit(&fb, 1, 2));

    /* clipping through the view */
    draw_clear(&fb, COLOR(0, 0, 0));
    draw_text(&fb, 30, 0, "8", COLOR(255, 255, 255));
    CHECK(lit(&fb, 31, 0));
}
