#include "test.h"
#include "sprites.h"
#include "textures.h"
#include "palette.h"
#include "map.h"
#include "camera.h"

static const char *const ROOM[7] = {
    "#######", "#.....#", "#.....#", "#.....#", "#.....#", "#.....#", "#######",
};

static uint8_t px[320 * 200 * 4];
static float depth[320];
static int same(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

void test_sprites(void) {
    static Map m; static Textures t; static Sprites sp;
    const Color *pal = PALETTE_NIGHT;
    Rng rng; rng_seed(&rng, 5u);
    CHECK_EQ(map_parse(&m, ROOM, 7, 7), 0);
    textures_generate(&t, &rng, pal, "ROHIT");
    Camera cam; camera_init(&cam, 2.5f, 3.5f, 0.0f);
    int w = 320, h = 200, horizon = 100;
    float proj = camera_proj(w);

    SpriteProj pr;
    CHECK(sprite_project(&cam, proj, w, horizon, 4.5f, 3.5f, 1.0f, 1.0f, &pr));     /* dead ahead, depth 2 */
    CHECK_NEAR(pr.depth, 2.0f, 1e-4);
    CHECK_EQ(pr.screen_x, 160);
    CHECK_EQ(pr.height, (int)(proj * 1.0f / 2.0f));
    CHECK_EQ(pr.bottom, horizon + (int)(proj * 0.5f / 2.0f));
    CHECK_EQ(pr.top, pr.bottom - pr.height);
    CHECK_EQ(pr.width, pr.height);
    CHECK(sprite_project(&cam, proj, w, horizon, 4.5f, 3.5f, 2.0f, 0.5f, &pr));     /* lamp aspect */
    CHECK_EQ(pr.width, pr.height / 2);
    CHECK(!sprite_project(&cam, proj, w, horizon, 0.5f, 3.5f, 1.0f, 1.0f, &pr));    /* behind */
    CHECK(!sprite_project(&cam, proj, w, horizon, 2.5f, 5.5f, 1.0f, 1.0f, &pr));    /* beside: depth 0 */
    CHECK(sprite_project(&cam, proj, w, horizon, 4.5f, 4.5f, 1.0f, 1.0f, &pr));     /* right of centre (+y is right) */
    CHECK(pr.screen_x > 200);
    CHECK(sprite_project(&cam, proj, w, horizon, 4.5f, 2.5f, 1.0f, 1.0f, &pr));
    CHECK(pr.screen_x < 120);

    /* list management */
    sprites_clear(&sp);
    CHECK_EQ(sp.n, 0);
    Sprite *a = sprites_add(&sp, SPR_TRASHCAN, 4.5f, 3.5f, 1.0f);
    CHECK(a != NULL); CHECK_EQ(sp.n, 1); CHECK(a->active);
    for (int i = 1; i < MAX_SPRITES; i++) CHECK(sprites_add(&sp, SPR_PAPER_A, 1.5f, 1.5f, 0.3f) != NULL);
    CHECK(sprites_add(&sp, SPR_PAPER_A, 1.5f, 1.5f, 0.3f) == NULL);
    sprites_remove(&sp, a);
    CHECK(!a->active);
    CHECK(sprites_add(&sp, SPR_TRASHCAN, 4.5f, 3.5f, 1.0f) != NULL);              /* slot reused */

    /* drawing: occluded by a near depth buffer, visible against a far one */
    Framebuffer fb; fb_init(&fb, px, w, h);
    sprites_clear(&sp);
    sprites_add(&sp, SPR_TRASHCAN, 4.5f, 3.5f, 1.0f);
    for (int i = 0; i < w; i++) depth[i] = 1.0f;
    draw_clear(&fb, pal[COL_BG]);
    sprites_draw(&sp, &cam, &m, &t, pal, &fb, depth, horizon, proj);
    int painted = 0;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) if (!same(fb_get(&fb, x, y), pal[COL_BG])) painted++;
    CHECK_EQ(painted, 0);
    for (int i = 0; i < w; i++) depth[i] = 10.0f;
    sprites_draw(&sp, &cam, &m, &t, pal, &fb, depth, horizon, proj);
    painted = 0;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) if (!same(fb_get(&fb, x, y), pal[COL_BG])) painted++;
    CHECK(painted > 200);
    CHECK_EQ(painted < 100, 0);
    for (int y = 0; y < 30; y++) for (int x = 0; x < w; x++) CHECK(same(fb_get(&fb, x, y), pal[COL_BG]));   /* nothing above the sprite */
    Color c = fb_get(&fb, 160, 138);                                    /* inside the can body */
    CHECK(c.r < pal[COL_TRASH].r && c.r > 20);                           /* shaded, not black */

    /* far-to-near order: the nearer bug wins the centre pixel */
    sprites_clear(&sp);
    sprites_add(&sp, SPR_BUG_EXPOSED, 3.5f, 3.5f, 1.0f);                /* near, depth 1 */
    sprites_add(&sp, SPR_TRASHCAN, 5.5f, 3.5f, 1.0f);                   /* far, depth 3 */
    draw_clear(&fb, pal[COL_BG]);
    sprites_draw(&sp, &cam, &m, &t, pal, &fb, depth, horizon, proj);
    Color mid = fb_get(&fb, 160, horizon + 60);
    CHECK(mid.g > mid.r);                                                /* bug green, not trash grey */
}
