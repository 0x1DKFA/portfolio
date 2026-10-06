#include "test.h"
#include "raycast.h"
#include "decals.h"
#include "palette.h"
#include "trig.h"

static const char *const ROOM[7] = {
    "#######", "#.....#", "#.....#", "#.....#", "#.....#", "#.....#", "#######",
};

static uint8_t px[320 * 200 * 4];
static float depth[320];
static int same(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b; }
static int accent(Color c) { return c.r > 180 && c.g > 55 && c.g < 130 && c.b < 100; }

void test_raycast(void) {
    static Map m; static Textures t; static Decals d;
    const Color *pal = PALETTE_NIGHT;
    Rng rng; rng_seed(&rng, 9u);
    CHECK_EQ(map_parse(&m, ROOM, 7, 7), 0);
    textures_generate(&t, &rng, pal, "ROHIT");
    int w = 320, h = 200, horizon = 100;
    float proj = camera_proj(w);
    Camera cam; camera_init(&cam, 1.5f, 3.5f, 0.0f);          /* facing +x down the middle */

    CHECK_NEAR(raycast_shade(0.0f, 1.0f, 0), 1.0f, 1e-5);
    CHECK_NEAR(raycast_shade(0.0f, 0.25f, 0), 0.45f + 0.55f * 0.25f, 1e-5);
    CHECK_NEAR(raycast_shade(2.0f, 1.0f, 0), 1.0f / 1.7f, 1e-5);
    CHECK_NEAR(raycast_shade(2.0f, 1.0f, 1), 0.8f / 1.7f, 1e-5);
    CHECK_NEAR(raycast_row_dist(proj, horizon + 50, horizon), 0.5f * proj / 50.0f, 1e-4);
    CHECK_NEAR(raycast_row_dist(proj, horizon + 1, horizon), 0.5f * proj, 1e-4);

    /* centre column hits the far wall at x = 6: distance 4.5 */
    RayHit hit;
    CHECK(raycast_column(&m, &cam, proj, w, w / 2, &hit));
    CHECK(hit.hit);
    CHECK_NEAR(hit.dist, 4.5f, 0.01);
    CHECK_EQ(hit.side, 0);
    CHECK_EQ(hit.tile_x, 6); CHECK_EQ(hit.tile_y, 3);
    CHECK_EQ(hit.tex, TEX_BRICK);
    CHECK(hit.u >= 0.0f && hit.u < 1.0f);
    CHECK_NEAR(hit.light, 0.25f, 1e-5);
    CHECK(raycast_column(&m, &cam, proj, w, 0, &hit));       /* far left ray hits something farther than the centre */
    CHECK(hit.hit);
    CHECK(hit.dist > 2.0f);
    for (int c = 0; c < w / 2; c++) {                          /* the room is symmetric about y = 3.5 */
        RayHit a, b;
        raycast_column(&m, &cam, proj, w, c, &a);
        raycast_column(&m, &cam, proj, w, w - 1 - c, &b);
        CHECK_NEAR(a.dist, b.dist, 0.05);
    }
    Camera side; camera_init(&side, 3.5f, 3.5f, TRIG_HALF_PI);  /* facing +y: hits the y = 6 wall, side 1 */
    CHECK(raycast_column(&m, &side, proj, w, w / 2, &hit));
    CHECK_EQ(hit.side, 1);
    CHECK_NEAR(hit.dist, 2.5f, 0.01);
    CHECK_EQ(hit.tile_y, 6);

    /* sky fills the rows above the horizon and changes with the view direction */
    Framebuffer fb; fb_init(&fb, px, w, h);
    draw_clear(&fb, COLOR(0, 0, 0));
    raycast_sky(&t, &cam, proj, &fb, horizon);
    CHECK(fb_get(&fb, 160, 0).b > 0);
    CHECK(same(fb_get(&fb, 160, horizon + 5), COLOR(0, 0, 0)));   /* nothing below the horizon */
    int diff = 0;
    Camera turned; camera_init(&turned, 1.5f, 3.5f, TRIG_PI);
    static uint8_t px2[320 * 200 * 4];
    Framebuffer fb2; fb_init(&fb2, px2, w, h);
    draw_clear(&fb2, COLOR(0, 0, 0));
    raycast_sky(&t, &turned, proj, &fb2, horizon);
    for (int y = horizon - 40; y < horizon; y++) for (int x = 0; x < w; x += 4) if (!same(fb_get(&fb, x, y), fb_get(&fb2, x, y))) diff++;
    CHECK(diff > 100);

    /* walls: depth buffer, wall pixels, sky left above a far wall */
    Color sky_before = fb_get(&fb, 160, 5);
    raycast_walls(&m, &t, pal, &cam, proj, &fb, horizon, depth);
    CHECK_NEAR(depth[w / 2], 4.5f, 0.01);
    Color wc = fb_get(&fb, 160, horizon);
    CHECK(wc.r > wc.g);                                        /* brick, reddish */
    CHECK(same(fb_get(&fb, 160, 5), sky_before));              /* sky untouched above the far wall (top ~ row 22) */
    int expected_top = horizon - (int)(RAY_WALL_TOP / 4.5f * proj);
    CHECK(expected_top > 10 && expected_top < 40);
    CHECK(fb_get(&fb, 160, expected_top + 2).r > fb_get(&fb, 160, expected_top + 2).b);   /* wall just below its top */

    /* floor: asphalt below the horizon, footprints glow along the trail, never above the horizon */
    decals_clear(&d);
    Path p; p.n = 5;
    for (int i = 0; i < 5; i++) { p.t[i].x = 1 + i; p.t[i].y = 3; }
    decals_lay_trail(&d, &p);
    decals_set_head_waypoint(&d, 0);
    raycast_floor(&m, &t, pal, &d, &cam, proj, &fb, horizon, depth);
    Color fl = fb_get(&fb, 160, h - 1);
    CHECK(fl.r > fl.b && fl.r > 10);                           /* warm asphalt, shaded */
    int accent_below = 0, accent_above = 0;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        if (accent(fb_get(&fb, x, y))) { if (y > horizon) accent_below++; else accent_above++; }
    }
    CHECK(accent_below > 40);
    CHECK_EQ(accent_above, 0);
    /* the floor never paints over the wall: the row just above the wall's bottom is wall-coloured */
    int wall_bottom = horizon + (int)(RAY_WALL_BOTTOM / 4.5f * proj);
    CHECK(fb_get(&fb, 160, wall_bottom - 1).r > fb_get(&fb, 160, wall_bottom - 1).b);
}
