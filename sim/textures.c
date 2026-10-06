#include "textures.h"
#include "palette.h"
#include "font.h"
#include "map.h"

static const Color CLEAR = { 0, 0, 0, 0 };

static void begin(Framebuffer *fb, Color *px, int w, int h, Color fill) {
    fb_init(fb, (uint8_t *)px, w, h);
    draw_clear(fb, fill);
}

/* ---- walls ---------------------------------------------------------------- */

static void gen_brick(Color *px, Rng *rng, const Color *pal) {
    Framebuffer fb; begin(&fb, px, TEX_SIZE, TEX_SIZE, pal[COL_BRICK]);
    for (int r = 0; r < 4; r++) {
        int y = r * 16, off = (r & 1) ? 16 : 0;
        for (int c = -1; c < 3; c++) {
            int x = c * 32 + off;
            if (rng_float(rng) < 0.3f) draw_rect(&fb, x, y, 32, 16, pal[COL_BRICK_DARK]);
            draw_rect(&fb, x, y, 2, 16, pal[COL_MORTAR]);
        }
        draw_rect(&fb, 0, y, TEX_SIZE, 2, pal[COL_MORTAR]);
    }
    draw_dim(&fb, 0, 52, TEX_SIZE, 12, 90);
}

static void gen_concrete(Color *px, Rng *rng, const Color *pal) {
    Framebuffer fb; begin(&fb, px, TEX_SIZE, TEX_SIZE, pal[COL_CONCRETE]);
    draw_rect(&fb, 0, 21, TEX_SIZE, 3, pal[COL_CONCRETE_DARK]);
    draw_rect(&fb, 0, 42, TEX_SIZE, 3, pal[COL_CONCRETE_DARK]);
    for (int i = 0; i < 6; i++) {
        int x = rng_range(rng, 0, TEX_SIZE - 1), y = rng_range(rng, 10, 50);
        draw_rect(&fb, x, y, 1, TEX_SIZE - y, pal[COL_GRIME]);
    }
    draw_dim(&fb, 0, 54, TEX_SIZE, 10, 70);
}

static void gen_window(Color *px, const Color *pal) {
    Framebuffer fb; begin(&fb, px, TEX_SIZE, TEX_SIZE, pal[COL_CONCRETE_DARK]);
    Color pane = pal[COL_WINDOW_DARK]; pane.a = WINDOW_PANE_ALPHA;
    for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) {
        int x = 4 + i * 15, y = 4 + j * 15;
        draw_rect(&fb, x - 1, y - 1, 14, 14, pal[COL_MORTAR]);
        draw_rect(&fb, x, y, 12, 12, pane);
    }
}

static void gen_neon(Color *px, const Color *pal, const char *text) {
    Framebuffer fb; begin(&fb, px, TEX_SIZE, TEX_SIZE, pal[COL_CONCRETE_DARK]);
    int n = 0; while (text[n] && n < 8) n++;
    int width = n ? (n * 4 - 1) * 3 : 0;
    int scale = 3;
    if (width > 60) { scale = 2; width = (n * 4 - 1) * 2; }
    int x0 = (TEX_SIZE - width) / 2, y0 = (TEX_SIZE - 5 * scale) / 2;
    draw_rect(&fb, 2, y0 - 6, TEX_SIZE - 4, 5 * scale + 12, pal[COL_GRIME]);
    draw_text_scaled(&fb, x0 - 1, y0, text, scale, pal[COL_NEON_GLOW]);
    draw_text_scaled(&fb, x0 + 1, y0, text, scale, pal[COL_NEON_GLOW]);
    draw_text_scaled(&fb, x0, y0 - 1, text, scale, pal[COL_NEON_GLOW]);
    draw_text_scaled(&fb, x0, y0 + 1, text, scale, pal[COL_NEON_GLOW]);
    draw_text_scaled(&fb, x0, y0, text, scale, pal[COL_NEON]);
}

static void gen_sign(Color *px, const Color *pal) {
    Framebuffer fb; begin(&fb, px, TEX_SIZE, TEX_SIZE, pal[COL_SIGN]);
    draw_rect(&fb, 0, 0, TEX_SIZE, 2, pal[COL_TEXT]); draw_rect(&fb, 0, TEX_SIZE - 2, TEX_SIZE, 2, pal[COL_TEXT]);
    draw_rect(&fb, 0, 0, 2, TEX_SIZE, pal[COL_TEXT]); draw_rect(&fb, TEX_SIZE - 2, 0, 2, TEX_SIZE, pal[COL_TEXT]);
    draw_text_scaled(&fb, (TEX_SIZE - 15 * 3) / 2, 24, "OPEN", 3, pal[COL_TEXT]);
}

static void gen_poster(Color *px, Rng *rng, const Color *pal) {
    gen_brick(px, rng, pal);
    Framebuffer fb; fb_init(&fb, (uint8_t *)px, TEX_SIZE, TEX_SIZE);
    draw_rect(&fb, 12, 6, 40, 52, pal[COL_POSTER]);
    draw_rect(&fb, 16, 10, 32, 6, pal[COL_POSTER_INK]);
    for (int y = 22; y <= 46; y += 8) draw_rect(&fb, 16, y, 28, 2, pal[COL_POSTER_INK]);
    draw_rect(&fb, 44, 50, 8, 8, pal[COL_BRICK]);
}

static void gen_dumpster(Color *px, const Color *pal) {
    Framebuffer fb; begin(&fb, px, TEX_SIZE, TEX_SIZE, pal[COL_DUMPSTER]);
    draw_rect(&fb, 0, 0, TEX_SIZE, 10, pal[COL_DUMPSTER_DARK]);
    for (int x = 16; x < TEX_SIZE; x += 16) draw_rect(&fb, x, 10, 2, 46, pal[COL_DUMPSTER_DARK]);
    draw_rect(&fb, 8, 56, 8, 8, pal[COL_GRIME]); draw_rect(&fb, 48, 56, 8, 8, pal[COL_GRIME]);
}

static void gen_asphalt(Color *px, Rng *rng, const Color *pal) {
    Framebuffer fb; begin(&fb, px, TEX_SIZE, TEX_SIZE, pal[COL_ASPHALT]);
    for (int i = 0; i < 120; i++) draw_pixel(&fb, rng_range(rng, 0, 63), rng_range(rng, 0, 63), pal[COL_ASPHALT_DARK]);
    int y = rng_range(rng, 20, 44);
    for (int x = 0; x < TEX_SIZE; x++) { draw_pixel(&fb, x, y, pal[COL_ASPHALT_DARK]); y += rng_range(rng, -1, 1); if (y < 0) y = 0; if (y > 63) y = 63; }
}

static void gen_puddle(Color *px, const Color *pal) {
    Framebuffer fb; begin(&fb, px, TEX_SIZE, TEX_SIZE, pal[COL_PUDDLE]);
    draw_rect(&fb, 18, 28, 28, 8, pal[COL_PUDDLE_LIGHT]);
    draw_rect(&fb, 22, 24, 20, 4, pal[COL_PUDDLE_LIGHT]);
    draw_rect(&fb, 22, 36, 20, 4, pal[COL_PUDDLE_LIGHT]);
    draw_rect(&fb, 8, 12, 10, 4, pal[COL_PUDDLE_LIGHT]);
}

/* ---- sprites (bottom-anchored, drawn in a 32-wide, h-tall frame) ------------- */

static void gen_dog(Color *px, const Color *pal, int frame) {          /* 0 A, 1 B, 2 sniff */
    Framebuffer fb; begin(&fb, px, SPR_W, SPR_H, CLEAR);
    int hy = frame == 2 ? 18 : 12;
    draw_rect(&fb, 8, 16, 16, 8, pal[COL_DOG]);                          /* body */
    draw_rect(&fb, 22, hy, 8, 7, pal[COL_DOG]);                          /* head */
    draw_rect(&fb, 28, hy - 2, 3, 4, pal[COL_DOG_DARK]);                 /* ear */
    draw_pixel(&fb, 27, hy + 2, pal[COL_BUG_DARK]);                      /* eye */
    draw_rect(&fb, 4, frame == 1 ? 12 : 14, 4, 3, pal[COL_DOG_DARK]);    /* tail wags */
    int lx0 = frame == 1 ? 10 : 9, lx1 = frame == 1 ? 12 : 13;
    draw_rect(&fb, lx0, 24, 3, 8, pal[COL_DOG_DARK]); draw_rect(&fb, lx1 + 4, 24, 3, 8, pal[COL_DOG_DARK]);
    draw_rect(&fb, lx0 + 9, 24, 3, 8, pal[COL_DOG_DARK]); draw_rect(&fb, lx1 + 13, 24, 3, 8, pal[COL_DOG_DARK]);
}

static void gen_trashcan(Color *px, const Color *pal) {
    Framebuffer fb; begin(&fb, px, SPR_W, SPR_H, CLEAR);
    draw_rect(&fb, 10, 10, 12, 22, pal[COL_TRASH]);
    draw_rect(&fb, 8, 8, 16, 3, pal[COL_TRASH_DARK]);
    draw_rect(&fb, 10, 16, 12, 1, pal[COL_TRASH_DARK]); draw_rect(&fb, 10, 24, 12, 1, pal[COL_TRASH_DARK]);
}

static void gen_paper(Color *px, const Color *pal, int frame) {
    Framebuffer fb; begin(&fb, px, SPR_W, SPR_H, CLEAR);
    if (frame == 0) { draw_rect(&fb, 12, 26, 8, 6, pal[COL_PAPER]); draw_rect(&fb, 14, 24, 4, 2, pal[COL_PAPER]); }
    else            { draw_rect(&fb, 13, 24, 6, 8, pal[COL_PAPER]); draw_rect(&fb, 11, 27, 2, 3, pal[COL_PAPER]); }
}

static void gen_news(Color *px, const Color *pal, int frame) {
    Framebuffer fb; begin(&fb, px, SPR_W, SPR_H, CLEAR);
    if (frame == 0) { draw_rect(&fb, 8, 26, 16, 6, pal[COL_PAPER]); draw_rect(&fb, 10, 28, 12, 1, pal[COL_POSTER_INK]); }
    else            { draw_rect(&fb, 10, 23, 12, 9, pal[COL_PAPER]); draw_rect(&fb, 12, 25, 8, 1, pal[COL_POSTER_INK]); draw_rect(&fb, 12, 28, 8, 1, pal[COL_POSTER_INK]); }
}

static void gen_bug(Color *px, const Color *pal, int state) {          /* 0 hidden, 1 peek, 2 exposed */
    Framebuffer fb; begin(&fb, px, SPR_W, SPR_H, CLEAR);
    if (state == 0) { draw_rect(&fb, 12, 29, 8, 3, pal[COL_BUG]); return; }
    int top = state == 1 ? 25 : 20, h = state == 1 ? 7 : 12;
    draw_rect(&fb, 10, top, 12, h, pal[COL_BUG]);
    draw_pixel(&fb, 10, top, CLEAR); draw_pixel(&fb, 21, top, CLEAR);
    draw_pixel(&fb, 12, top + 2, pal[COL_BUG_DARK]); draw_pixel(&fb, 19, top + 2, pal[COL_BUG_DARK]);
    draw_pixel(&fb, 12, top - 2, pal[COL_BUG]); draw_pixel(&fb, 11, top - 3, pal[COL_BUG]);
    draw_pixel(&fb, 19, top - 2, pal[COL_BUG]); draw_pixel(&fb, 20, top - 3, pal[COL_BUG]);
    if (state == 2) { draw_pixel(&fb, 11, 31, pal[COL_BUG_DARK]); draw_pixel(&fb, 15, 31, pal[COL_BUG_DARK]); draw_pixel(&fb, 19, 31, pal[COL_BUG_DARK]); }
}

static void gen_dust(Color *px, const Color *pal, int frame) {
    Framebuffer fb; begin(&fb, px, SPR_W, SPR_H, CLEAR);
    int r = frame == 0 ? 6 : 10, cx = 16, cy = 22;
    draw_rect(&fb, cx - r, cy - 2, 2 * r, 4, pal[COL_DUST]);
    draw_rect(&fb, cx - 2, cy - r, 4, 2 * r, pal[COL_DUST]);
    draw_rect(&fb, cx - r + 2, cy - r + 2, 3, 3, pal[COL_DUST]); draw_rect(&fb, cx + r - 5, cy - r + 2, 3, 3, pal[COL_DUST]);
    draw_rect(&fb, cx - r + 2, cy + r - 5, 3, 3, pal[COL_DUST]); draw_rect(&fb, cx + r - 5, cy + r - 5, 3, 3, pal[COL_DUST]);
    draw_rect(&fb, cx - 2, cy - 2, 4, 4, CLEAR);
}

static void gen_lamp(Color *px, const Color *pal) {
    Framebuffer fb; begin(&fb, px, SPR_W, SPR_H, CLEAR);
    draw_rect(&fb, 14, 12, 4, 52, pal[COL_LAMP]);
    draw_rect(&fb, 8, 6, 16, 6, pal[COL_LAMP]);
    draw_rect(&fb, 10, 12, 12, 3, pal[COL_LAMP_LIGHT]);
    draw_rect(&fb, 10, 60, 12, 4, pal[COL_LAMP]);
}

/* ---- HUD ------------------------------------------------------------------ */

static void gen_hud(Color *px, const Color *pal, int frame) {
    Framebuffer fb; begin(&fb, px, HUD_W, HUD_H, CLEAR);
    int hx = 36, hy = 44, handle_x = 46, handle_y = 14, head_x = 34, head_y = 4;
    if (frame == 1) { hy = 40; handle_y = 4; head_x = 26; head_y = -4; }
    if (frame == 2) { hx = 44; hy = 46; handle_x = 52; handle_y = 32; head_x = 40; head_y = 20; }
    draw_rect(&fb, handle_x - 1, handle_y - 1, 8, hy - handle_y + 6, pal[COL_BG]);
    draw_rect(&fb, handle_x, handle_y, 6, hy - handle_y + 4, pal[COL_HANDLE]);
    draw_rect(&fb, head_x - 1, head_y - 1, 32, 16, pal[COL_BG]);
    draw_rect(&fb, head_x, head_y, 30, 14, pal[COL_HAMMER]);
    draw_rect(&fb, head_x, head_y + 6, 30, 2, pal[COL_HANDLE]);
    draw_rect(&fb, hx, hy, 24, HUD_H - hy, pal[COL_HAND]);
    draw_rect(&fb, hx + 4, hy + 8, 16, 1, pal[COL_HANDLE]);
}

/* ---- sky ------------------------------------------------------------------ */

static void gen_sky(Color *sky, Rng *rng, const Color *pal) {
    Framebuffer fb; fb_init(&fb, (uint8_t *)sky, SKY_W, SKY_H);
    for (int y = 0; y < SKY_H; y++) {
        int t = y * 255 / (SKY_H - 1);
        Color c = COLOR((pal[COL_SKY_TOP].r * (255 - t) + pal[COL_SKY_BOTTOM].r * t) / 255,
                        (pal[COL_SKY_TOP].g * (255 - t) + pal[COL_SKY_BOTTOM].g * t) / 255,
                        (pal[COL_SKY_TOP].b * (255 - t) + pal[COL_SKY_BOTTOM].b * t) / 255);
        draw_rect(&fb, 0, y, SKY_W, 1, c);
    }
    for (int i = 0; i < 60; i++) draw_pixel(&fb, rng_range(rng, 0, SKY_W - 1), rng_range(rng, 0, 40), pal[COL_STAR]);   /* above the tallest tower */
    int x = 0;
    while (x < SKY_W) {
        int w = rng_range(rng, 24, 64), hgt = rng_range(rng, 40, 150);
        if (x + w > SKY_W) w = SKY_W - x;
        draw_rect(&fb, x, SKY_H - hgt, w, hgt, pal[COL_TOWER]);
        for (int wy = SKY_H - hgt + 4; wy < SKY_H - 3; wy += 5)
            for (int wx = x + 3; wx < x + w - 4; wx += 5)
                if (rng_float(rng) < 0.28f) draw_rect(&fb, wx, wy, 2, 2, pal[COL_TOWER_WINDOW]);
        x += w + rng_range(rng, 0, 6);
    }
}

/* ---- public --------------------------------------------------------------- */

void textures_generate(Textures *t, Rng *rng, const Color *pal, const char *neon_text) {
    gen_brick(t->wall[TEX_BRICK], rng, pal);
    gen_concrete(t->wall[TEX_CONCRETE], rng, pal);
    gen_window(t->wall[TEX_WINDOW], pal);
    gen_neon(t->wall[TEX_NEON], pal, neon_text ? neon_text : "ROHIT");
    gen_sign(t->wall[TEX_SIGN], pal);
    gen_poster(t->wall[TEX_POSTER], rng, pal);
    gen_dumpster(t->wall[TEX_DUMPSTER], pal);
    gen_asphalt(t->wall[TEX_ASPHALT], rng, pal);
    gen_puddle(t->wall[TEX_PUDDLE], pal);

    for (int k = 0; k < SPR_COUNT; k++) t->sprite_h[k] = 32;
    gen_dog(t->sprite[SPR_DOG_A], pal, 0); gen_dog(t->sprite[SPR_DOG_B], pal, 1); gen_dog(t->sprite[SPR_DOG_SNIFF], pal, 2);
    gen_trashcan(t->sprite[SPR_TRASHCAN], pal);
    gen_paper(t->sprite[SPR_PAPER_A], pal, 0); gen_paper(t->sprite[SPR_PAPER_B], pal, 1);
    gen_news(t->sprite[SPR_NEWS_A], pal, 0); gen_news(t->sprite[SPR_NEWS_B], pal, 1);
    gen_bug(t->sprite[SPR_BUG_HIDDEN], pal, 0); gen_bug(t->sprite[SPR_BUG_PEEK], pal, 1); gen_bug(t->sprite[SPR_BUG_EXPOSED], pal, 2);
    gen_dust(t->sprite[SPR_DUST_A], pal, 0); gen_dust(t->sprite[SPR_DUST_B], pal, 1);
    gen_lamp(t->sprite[SPR_LAMP], pal); t->sprite_h[SPR_LAMP] = 64;

    for (int f = 0; f < HUD_FRAMES; f++) gen_hud(t->hud[f], pal, f);
    gen_sky(t->sky, rng, pal);
}

int texture_for_cell(int cell) {
    switch (cell) {
    case CELL_CONCRETE: return TEX_CONCRETE;
    case CELL_WINDOW:   return TEX_WINDOW;
    case CELL_NEON:     return TEX_NEON;
    case CELL_SIGN:     return TEX_SIGN;
    case CELL_POSTER:   return TEX_POSTER;
    case CELL_DUMPSTER: return TEX_DUMPSTER;
    default:            return TEX_BRICK;
    }
}

Color texture_wall(const Textures *t, const Color *pal, int tex, uint32_t variant, int u, int v) {
    Color c = t->wall[tex][v * TEX_SIZE + u];
    if (tex == TEX_WINDOW && c.a == WINDOW_PANE_ALPHA) {
        int i = (u - 4) / 15, j = (v - 4) / 15;
        int bit = (j & 3) * 4 + (i & 3);
        int lit = (variant >> bit) & 1u, warm = (variant >> (bit + 16)) & 1u;
        return lit ? pal[warm ? COL_WINDOW_LIT : COL_WINDOW_LIT2] : pal[COL_WINDOW_DARK];
    }
    c.a = 255;
    return c;
}

Color texture_sprite(const Textures *t, int spr, int u, int v) {
    if (u < 0 || v < 0 || u >= SPR_W || v >= t->sprite_h[spr]) return CLEAR;
    return t->sprite[spr][v * SPR_W + u];
}

Color texture_sky(const Textures *t, int x, int y) {
    x %= SKY_W; if (x < 0) x += SKY_W;
    if (y < 0) y = 0; if (y >= SKY_H) y = SKY_H - 1;
    return t->sky[y * SKY_W + x];
}

uint32_t texture_hash(int x, int y) {
    uint32_t h = (uint32_t)x * 0x9E3779B1u ^ ((uint32_t)y + 0x7F4A7C15u) * 0x85EBCA77u;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return h;
}
