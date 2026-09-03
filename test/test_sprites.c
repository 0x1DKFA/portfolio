#include "test.h"
#include "sprites.h"
#include "palette.h"

static uint8_t px[96 * 96 * 4];
static int same(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

/* count pixels inside a rect that are not the background colour */
static int painted(const Framebuffer *fb, int x, int y, int w, int h) {
    int n = 0;
    for (int yy = y; yy < y + h; yy++) for (int xx = x; xx < x + w; xx++)
        if (!same(fb_get(fb, xx, yy), PALETTE_BUGS[COL_BG])) n++;
    return n;
}

void test_sprites(void) {
    Framebuffer fb;
    fb_init(&fb, px, 96, 96);
    const Color *pal = PALETTE_BUGS;

    Bug b;
    bug_spawn(&b, SIDE_LEFT, 40.0f, 60.0f, 1);
    draw_clear(&fb, pal[COL_BG]);
    sprite_bug(&fb, pal, &b);
    CHECK(painted(&fb, 34, 51, 12, 10) > 20);          /* body lands inside its box */
    CHECK_EQ(painted(&fb, 0, 0, 96, 50), 0);            /* nothing above the antennae */
    CHECK_EQ(painted(&fb, 0, 62, 96, 34), 0);           /* nothing below the feet */
    CHECK(same(fb_get(&fb, 40, 57), pal[COL_BUG_B]));   /* variant 1 body colour */

    b.hidden = 1; b.revealed = 0;
    draw_clear(&fb, pal[COL_BG]);
    sprite_bug(&fb, pal, &b);
    CHECK_EQ(painted(&fb, 0, 0, 96, 96), 0);
    b.revealed = 1;
    sprite_bug(&fb, pal, &b);
    CHECK(painted(&fb, 34, 51, 12, 10) > 20);

    b.hidden = 0; b.state = BUG_SQUASHED;
    draw_clear(&fb, pal[COL_BG]);
    sprite_bug(&fb, pal, &b);
    CHECK(painted(&fb, 34, 58, 12, 3) > 10);            /* flat splat */
    CHECK_EQ(painted(&fb, 0, 0, 96, 55), 0);

    Hero h;
    hero_init(&h, 48.0f, 80.0f);
    draw_clear(&fb, pal[COL_BG]);
    sprite_hero(&fb, pal, &h);
    CHECK(same(fb_get(&fb, 48, 62), pal[COL_HERO_SKIN]));   /* head */
    CHECK(same(fb_get(&fb, 48, 70), pal[COL_HERO_BODY]));   /* torso */
    CHECK(painted(&fb, 40, 58, 20, 23) > 60);
    CHECK_EQ(painted(&fb, 0, 82, 96, 14), 0);               /* nothing below the feet */

    h.action = HERO_HIDDEN;
    draw_clear(&fb, pal[COL_BG]);
    sprite_hero(&fb, pal, &h);
    CHECK_EQ(painted(&fb, 0, 0, 96, 96), 0);

    h.action = HERO_WINDUP;
    draw_clear(&fb, pal[COL_BG]);
    sprite_hero(&fb, pal, &h);
    CHECK(painted(&fb, 40, 48, 20, 10) > 0);                /* hammer raised above the head */

    h.action = HERO_IDLE; h.detective = 1; h.glass_x = 70.0f; h.glass_y = 80.0f;
    draw_clear(&fb, pal[COL_BG]);
    sprite_hero(&fb, pal, &h);
    CHECK(same(fb_get(&fb, 48, 58), pal[COL_HAT]));
    CHECK(painted(&fb, 64, 74, 13, 13) > 10);               /* glass ring */

    Fx f = { 1, FX_PLUS1, 40.0f, 60.0f, 0.0f };
    draw_clear(&fb, pal[COL_BG]);
    sprite_fx(&fb, pal, &f);
    CHECK(painted(&fb, 30, 40, 24, 20) > 5);
    f.kind = FX_DUST; f.t = 0.2f;
    draw_clear(&fb, pal[COL_BG]);
    sprite_fx(&fb, pal, &f);
    CHECK(painted(&fb, 30, 45, 24, 18) >= 4);

    draw_clear(&fb, pal[COL_BG]);
    sprite_prop(&fb, pal, PROP_BOX, 48, 60, BOX_GLITCH);
    CHECK(same(fb_get(&fb, 42, 55), pal[COL_RED]));
    CHECK(painted(&fb, 38, 42, 20, 6) > 10);                /* COUNT label above */
    draw_clear(&fb, pal[COL_BG]);
    sprite_prop(&fb, pal, PROP_BOX, 48, 60, BOX_GREEN);
    CHECK(same(fb_get(&fb, 42, 55), pal[COL_GREEN]));
    draw_clear(&fb, pal[COL_BG]);
    sprite_prop(&fb, pal, PROP_BOX, 48, 60, BOX_LOCKED);
    CHECK(painted(&fb, 45, 52, 7, 6) > 0);                  /* padlock on the box front */

    draw_clear(&fb, pal[COL_BG]);
    sprite_prop(&fb, pal, PROP_FOOTPRINT, 20, 70, 0);
    CHECK(same(fb_get(&fb, 20, 70), pal[COL_PROP_DARK]));
    sprite_prop(&fb, pal, PROP_FOOTPRINT, 30, 70, 1);
    CHECK(same(fb_get(&fb, 30, 70), pal[COL_GLOW]));

    draw_clear(&fb, pal[COL_BG]);
    sprite_prop(&fb, pal, PROP_SHIELD, 48, 60, 0);
    CHECK(painted(&fb, 44, 50, 9, 11) > 20);
    draw_clear(&fb, pal[COL_BG]);
    sprite_prop(&fb, pal, PROP_HIDING_SPOT, 48, 60, 0);
    CHECK(painted(&fb, 42, 51, 12, 10) > 20);
}
