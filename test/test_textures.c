#include "test.h"
#include "textures.h"
#include "palette.h"
#include "map.h"

static int same(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

static int count_color(const Color *px, int n, Color c) {
    int k = 0; for (int i = 0; i < n; i++) if (same(px[i], c)) k++; return k;
}

static int distinct(const Color *px, int n) {
    int k = 0;
    for (int i = 0; i < n; i++) {
        int seen = 0;
        for (int j = 0; j < i && j < 400; j++) if (same(px[i], px[j])) { seen = 1; break; }
        if (!seen) k++;
        if (k > 3) return k;
    }
    return k;
}

static uint32_t checksum(const Color *px, int n) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < n; i++) { h = (h ^ px[i].r) * 16777619u; h = (h ^ px[i].g) * 16777619u; h = (h ^ px[i].b) * 16777619u; h = (h ^ px[i].a) * 16777619u; }
    return h;
}

void test_textures(void) {
    static Textures t, t2;
    const Color *pal = PALETTE_NIGHT;
    Rng rng; rng_seed(&rng, 11u);
    textures_generate(&t, &rng, pal, "ROHIT");

    for (int k = 0; k < TEX_COUNT; k++) CHECK(distinct(t.wall[k], TEX_SIZE * TEX_SIZE) >= 2);
    CHECK(count_color(t.wall[TEX_BRICK], TEX_SIZE * TEX_SIZE, pal[COL_MORTAR]) > 200);
    CHECK(count_color(t.wall[TEX_NEON], TEX_SIZE * TEX_SIZE, pal[COL_NEON]) > 40);
    CHECK(count_color(t.wall[TEX_NEON], TEX_SIZE * TEX_SIZE, pal[COL_NEON_GLOW]) > 40);
    CHECK(count_color(t.wall[TEX_SIGN], TEX_SIZE * TEX_SIZE, pal[COL_TEXT]) > 40);
    CHECK(count_color(t.wall[TEX_POSTER], TEX_SIZE * TEX_SIZE, pal[COL_POSTER]) > 800);
    CHECK(count_color(t.wall[TEX_DUMPSTER], TEX_SIZE * TEX_SIZE, pal[COL_DUMPSTER]) > 1500);
    CHECK(count_color(t.wall[TEX_ASPHALT], TEX_SIZE * TEX_SIZE, pal[COL_ASPHALT_DARK]) > 60);
    CHECK(count_color(t.wall[TEX_PUDDLE], TEX_SIZE * TEX_SIZE, pal[COL_PUDDLE_LIGHT]) > 100);

    /* window panes carry the marker alpha and resolve per variant */
    int panes = 0;
    for (int i = 0; i < TEX_SIZE * TEX_SIZE; i++) if (t.wall[TEX_WINDOW][i].a == WINDOW_PANE_ALPHA) panes++;
    CHECK(panes > 1000);
    int diff = 0, opaque = 1;
    for (int v = 0; v < TEX_SIZE; v++) for (int u = 0; u < TEX_SIZE; u++) {
        Color a = texture_wall(&t, pal, TEX_WINDOW, 0xFFFFFFFFu, u, v);
        Color b = texture_wall(&t, pal, TEX_WINDOW, 0u, u, v);
        if (a.a != 255 || b.a != 255) opaque = 0;
        if (!same(a, b)) diff++;
    }
    CHECK(opaque);
    CHECK(diff > 500);
    CHECK(same(texture_wall(&t, pal, TEX_BRICK, 0u, 3, 3), t.wall[TEX_BRICK][3 * TEX_SIZE + 3]));
    CHECK_EQ(texture_for_cell(CELL_BRICK), TEX_BRICK);
    CHECK_EQ(texture_for_cell(CELL_NEON), TEX_NEON);
    CHECK_EQ(texture_for_cell(CELL_DUMPSTER), TEX_DUMPSTER);
    CHECK(texture_hash(3, 4) != texture_hash(4, 3));
    CHECK(texture_hash(3, 4) == texture_hash(3, 4));

    /* sprites: transparent corners, real content, heights */
    for (int k = 0; k < SPR_COUNT; k++) {
        CHECK(t.sprite_h[k] == 32 || t.sprite_h[k] == 64);
        CHECK_EQ(texture_sprite(&t, k, 0, 0).a, 0);
        int solid = 0;
        for (int v = 0; v < t.sprite_h[k]; v++) for (int u = 0; u < SPR_W; u++) if (texture_sprite(&t, k, u, v).a == 255) solid++;
        CHECK(solid > 20);
    }
    CHECK_EQ(t.sprite_h[SPR_LAMP], 64);
    CHECK_EQ(t.sprite_h[SPR_DOG_A], 32);
    CHECK(count_color(t.sprite[SPR_DOG_A], SPR_W * SPR_H, pal[COL_DOG]) > 100);
    CHECK(count_color(t.sprite[SPR_BUG_EXPOSED], SPR_W * SPR_H, pal[COL_BUG]) > 60);
    CHECK(count_color(t.sprite[SPR_BUG_HIDDEN], SPR_W * SPR_H, pal[COL_BUG]) < count_color(t.sprite[SPR_BUG_PEEK], SPR_W * SPR_H, pal[COL_BUG]));
    CHECK(checksum(t.sprite[SPR_DOG_A], SPR_W * SPR_H) != checksum(t.sprite[SPR_DOG_B], SPR_W * SPR_H));
    CHECK(count_color(t.sprite[SPR_LAMP], SPR_W * SPR_H, pal[COL_LAMP_LIGHT]) > 20);

    /* sky: gradient at the top, towers with windows at the bottom, wrapping x */
    CHECK(count_color(t.sky, SKY_W, pal[COL_SKY_TOP]) > 900);                /* top row is gradient start, bar a few stars */
    CHECK(count_color(&t.sky[(SKY_H - 1) * SKY_W], SKY_W, pal[COL_TOWER]) > 600);
    CHECK(count_color(t.sky, SKY_W * SKY_H, pal[COL_TOWER_WINDOW]) > 300);
    CHECK(count_color(t.sky, SKY_W * SKY_H, pal[COL_STAR]) >= 40);
    CHECK(same(texture_sky(&t, SKY_W + 5, 100), texture_sky(&t, 5, 100)));
    CHECK(same(texture_sky(&t, -3, 100), texture_sky(&t, SKY_W - 3, 100)));
    CHECK(same(texture_sky(&t, 7, -10), texture_sky(&t, 7, 0)));
    CHECK(same(texture_sky(&t, 7, SKY_H + 10), texture_sky(&t, 7, SKY_H - 1)));

    /* hud frames exist and differ; the hammer head is yellow */
    for (int f = 0; f < HUD_FRAMES; f++) CHECK(count_color(t.hud[f], HUD_W * HUD_H, pal[COL_HAMMER]) > 200);
    CHECK(checksum(t.hud[0], HUD_W * HUD_H) != checksum(t.hud[1], HUD_W * HUD_H));
    CHECK(checksum(t.hud[1], HUD_W * HUD_H) != checksum(t.hud[2], HUD_W * HUD_H));
    CHECK_EQ(t.hud[0][0].a, 0);

    /* deterministic for a seed */
    rng_seed(&rng, 11u);
    textures_generate(&t2, &rng, pal, "ROHIT");
    CHECK(checksum(t.wall[TEX_BRICK], TEX_SIZE * TEX_SIZE) == checksum(t2.wall[TEX_BRICK], TEX_SIZE * TEX_SIZE));
    CHECK(checksum(t.sky, SKY_W * SKY_H) == checksum(t2.sky, SKY_W * SKY_H));
    rng_seed(&rng, 12u);
    textures_generate(&t2, &rng, pal, "ROHIT");
    CHECK(checksum(t.sky, SKY_W * SKY_H) != checksum(t2.sky, SKY_W * SKY_H));
}
