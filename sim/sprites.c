#include "sprites.h"
#include "palette.h"
#include "font.h"

static int fi(float v) { return (int)(v + (v >= 0 ? 0.5f : -0.5f)); }

void sprite_bug(Framebuffer *fb, const Color *pal, const Bug *b) {
    if (b->state == BUG_DEAD || b->state == BUG_POOF) return;
    if (b->hidden && !b->revealed) return;
    int x = fi(b->x), y = fi(b->y);
    if (b->state == BUG_SQUASHED) { draw_rect(fb, x - 6, y - 2, 12, 2, pal[COL_BUG_DARK]); return; }
    Color body = pal[b->variant == 0 ? COL_BUG_A : b->variant == 1 ? COL_BUG_B : COL_BUG_C];
    draw_rect(fb, x - 5, y - 7, 10, 6, body);                 /* round-ish body */
    draw_pixel(fb, x - 5, y - 7, pal[COL_BG]); draw_pixel(fb, x + 4, y - 7, pal[COL_BG]);
    draw_pixel(fb, x - 5, y - 2, pal[COL_BG]); draw_pixel(fb, x + 4, y - 2, pal[COL_BG]);
    draw_pixel(fb, x - 3, y - 6, pal[COL_BUG_DARK]);          /* eyes */
    draw_pixel(fb, x + 2, y - 6, pal[COL_BUG_DARK]);
    draw_pixel(fb, x - 3, y - 8, body); draw_pixel(fb, x - 4, y - 9, body);   /* antennae */
    draw_pixel(fb, x + 2, y - 8, body); draw_pixel(fb, x + 3, y - 9, body);
    draw_pixel(fb, x - 4, y - 1, pal[COL_BUG_DARK]);          /* stub legs */
    draw_pixel(fb, x - 1, y - 1, pal[COL_BUG_DARK]);
    draw_pixel(fb, x + 2, y - 1, pal[COL_BUG_DARK]);
}

static void hammer(Framebuffer *fb, const Color *pal, int x, int y, int facing, int action) {
    if (action == HERO_WINDUP) {                               /* raised above the head */
        draw_rect(fb, x + facing * 3, y - 26, 1, 8, pal[COL_HANDLE]);
        draw_rect(fb, x + facing * 3 - 2, y - 30, 5, 4, pal[COL_HAMMER]);
    } else if (action == HERO_BONK) {                          /* down in front */
        int hx = facing > 0 ? x + 5 : x - 11;
        draw_rect(fb, hx, y - 8, 7, 1, pal[COL_HANDLE]);
        draw_rect(fb, facing > 0 ? x + 11 : x - 15, y - 9, 4, 5, pal[COL_HAMMER]);
    } else {                                                   /* resting at the side */
        draw_rect(fb, x + facing * 5, y - 14, 1, 8, pal[COL_HANDLE]);
        draw_rect(fb, x + facing * 5 - 2, y - 18, 5, 4, pal[COL_HAMMER]);
    }
}

void sprite_hero(Framebuffer *fb, const Color *pal, const Hero *h) {
    if (h->action == HERO_HIDDEN) return;
    int x = fi(h->x), y = fi(h->y);
    int sit = h->action == HERO_SIT ? 4 : 0;
    if (sit) draw_rect(fb, x - 3, y - 2, 8, 2, pal[COL_PROP_DARK]);          /* legs out */
    else { draw_rect(fb, x - 3, y - 6, 2, 6, pal[COL_PROP_DARK]); draw_rect(fb, x + 1, y - 6, 2, 6, pal[COL_PROP_DARK]); }
    draw_rect(fb, x - 4, y - 14 + sit, 8, 8, pal[COL_HERO_BODY]);           /* torso */
    draw_rect(fb, x - 3, y - 20 + sit, 6, 6, pal[COL_HERO_SKIN]);           /* head */
    draw_pixel(fb, x + h->facing * 2, y - 18 + sit, pal[COL_BUG_DARK]);      /* eye */
    if (h->detective) {
        draw_rect(fb, x - 4, y - 22 + sit, 8, 2, pal[COL_HAT]);
        draw_rect(fb, x - 2, y - 24 + sit, 4, 2, pal[COL_HAT]);
        sprite_glass(fb, pal, fi(h->glass_x), fi(h->glass_y));
    }
    if (h->action == HERO_THINK) draw_text(fb, x - 1, y - 28, "?", pal[COL_TEXT]);
    if (h->action != HERO_SIT) hammer(fb, pal, x, y, h->facing, h->action);
}

void sprite_glass(Framebuffer *fb, const Color *pal, int cx, int cy) {
    Color c = pal[COL_TEXT];
    draw_rect(fb, cx - 3, cy - 5, 7, 1, c); draw_rect(fb, cx - 3, cy + 5, 7, 1, c);
    draw_rect(fb, cx - 5, cy - 3, 1, 7, c); draw_rect(fb, cx + 5, cy - 3, 1, 7, c);
    draw_pixel(fb, cx - 4, cy - 4, c); draw_pixel(fb, cx + 4, cy - 4, c);
    draw_pixel(fb, cx - 4, cy + 4, c); draw_pixel(fb, cx + 4, cy + 4, c);
    draw_pixel(fb, cx + 5, cy + 5, pal[COL_HANDLE]); draw_pixel(fb, cx + 6, cy + 6, pal[COL_HANDLE]);
    draw_pixel(fb, cx + 7, cy + 7, pal[COL_HANDLE]);
}

void sprite_fx(Framebuffer *fb, const Color *pal, const Fx *f) {
    if (!f->active) return;
    int x = fi(f->x), y = fi(f->y);
    if (f->kind == FX_PLUS1) {
        draw_text(fb, x - 3, y - 6 - fi(f->t * 10.0f), "+1", pal[COL_GREEN]);
    } else if (f->kind == FX_DUST) {
        int r = 2 + fi(f->t * 10.0f);
        draw_pixel(fb, x - r, y - 1 - r / 2, pal[COL_DUST]); draw_pixel(fb, x + r, y - 1 - r / 2, pal[COL_DUST]);
        draw_pixel(fb, x - r / 2, y - r, pal[COL_DUST]);     draw_pixel(fb, x + r / 2, y - r, pal[COL_DUST]);
    } else {                                                   /* FX_POOF: expanding hollow square */
        int s = 2 + fi(f->t * 8.0f), x0 = x - s / 2, y0 = y - 4 - s / 2;
        draw_rect(fb, x0, y0, s, 1, pal[COL_DUST]); draw_rect(fb, x0, y0 + s - 1, s, 1, pal[COL_DUST]);
        draw_rect(fb, x0, y0, 1, s, pal[COL_DUST]); draw_rect(fb, x0 + s - 1, y0, 1, s, pal[COL_DUST]);
    }
}

static void padlock(Framebuffer *fb, const Color *pal, int x, int y) {   /* y = bottom of the lock */
    draw_rect(fb, x - 2, y - 4, 5, 4, pal[COL_HAMMER]);
    draw_rect(fb, x - 1, y - 6, 1, 2, pal[COL_HANDLE]); draw_rect(fb, x + 1, y - 6, 1, 2, pal[COL_HANDLE]);
    draw_pixel(fb, x, y - 6, pal[COL_HANDLE]);
}

void sprite_prop(Framebuffer *fb, const Color *pal, int kind, int x, int y, int state) {
    switch (kind) {
    case PROP_BOX: {
        Color c = pal[state == BOX_GLITCH ? COL_RED : state == BOX_GREEN ? COL_GREEN : COL_PROP];
        draw_rect(fb, x - 7, y - 10, 14, 10, c);
        draw_rect(fb, x - 5, y - 8, 10, 6, pal[COL_BG]);
        draw_text(fb, x - 9, y - 17, "COUNT", pal[COL_TEXT]);
        if (state == BOX_LOCKED) padlock(fb, pal, x, y - 2);
        break;
    }
    case PROP_PADLOCK: padlock(fb, pal, x, y); break;
    case PROP_FOOTPRINT:
        draw_rect(fb, x, y, 2, 1, pal[state ? COL_GLOW : COL_PROP_DARK]);
        break;
    case PROP_HIDING_SPOT:
        draw_rect(fb, x - 5, y - 6, 10, 6, pal[COL_PROP_DARK]);
        draw_rect(fb, x - 3, y - 8, 6, 2, pal[COL_PROP_DARK]);
        draw_pixel(fb, x - 3, y - 7, pal[COL_PROP]);
        break;
    case PROP_SHIELD:
        draw_rect(fb, x - 3, y - 9, 7, 7, pal[COL_PROP]);
        draw_rect(fb, x - 2, y - 2, 5, 2, pal[COL_PROP]);
        draw_pixel(fb, x - 2, y - 5, pal[COL_GREEN]); draw_pixel(fb, x - 1, y - 4, pal[COL_GREEN]);
        draw_pixel(fb, x, y - 5, pal[COL_GREEN]);     draw_pixel(fb, x + 1, y - 6, pal[COL_GREEN]);
        draw_pixel(fb, x + 2, y - 7, pal[COL_GREEN]);
        break;
    default: break;
    }
}
