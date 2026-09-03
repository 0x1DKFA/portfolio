#include "test.h"
#include "decals.h"

void test_decals(void) {
    static Decals d;
    decals_clear(&d);
    CHECK_EQ(d.n, 0);
    CHECK_NEAR(decals_sample(&d, 1.5f, 1.5f), 0.0f, 1e-6);

    /* straight path along y = 1 from x = 1 to x = 5: four tiles, a pair every half tile */
    Path p; p.n = 5;
    for (int i = 0; i < 5; i++) { p.t[i].x = 1 + i; p.t[i].y = 1; }
    int laid = decals_lay_trail(&d, &p);
    CHECK(laid >= 16 && laid <= 18);
    CHECK_EQ(d.n, laid);
    CHECK_EQ(d.wp_first[0], 0);
    for (int i = 1; i < p.n; i++) CHECK(d.wp_first[i] >= d.wp_first[i - 1]);
    CHECK(d.wp_first[4] > d.wp_first[1]);

    /* prints sit 0.15 either side of the centre line, oriented along +x */
    int left = 0, right = 0;
    for (int i = 0; i < d.n; i++) {
        CHECK_NEAR(d.p[i].ca, 1.0f, 1e-3); CHECK_NEAR(d.p[i].sa, 0.0f, 1e-3);
        if (d.p[i].y < 1.5f) left++; else right++;
        CHECK(d.p[i].x >= 1.4f && d.p[i].x <= 5.6f);
    }
    CHECK(left >= 8 && right >= 8);

    /* sampling: on a print > 0, on the centre line 0, far away 0 */
    CHECK(decals_sample(&d, d.p[0].x, d.p[0].y) > 0.0f);
    CHECK_NEAR(decals_sample(&d, d.p[0].x, 1.5f), 0.0f, 1e-6);
    CHECK_NEAR(decals_sample(&d, 20.0f, 20.0f), 0.0f, 1e-6);
    CHECK(decals_sample(&d, d.p[0].x + 0.2f, d.p[0].y) > 0.0f);          /* within half length */
    CHECK_NEAR(decals_sample(&d, d.p[0].x + 0.25f, d.p[0].y), 0.0f, 1e-6); /* in the 0.06 gap before the next pair */

    /* glow: bright ahead of the head, dim behind */
    decals_set_head_waypoint(&d, 0);
    CHECK_NEAR(decals_sample(&d, d.p[0].x, d.p[0].y), 1.0f, 1e-6);
    decals_set_head_waypoint(&d, 3);
    CHECK_NEAR(decals_sample(&d, d.p[0].x, d.p[0].y), DECAL_GLOW_DIM, 1e-6);
    int h = d.wp_first[3];
    CHECK_NEAR(decals_sample(&d, d.p[h].x, d.p[h].y), 1.0f, 1e-6);

    CHECK(decals_tile_has_print(&d, 2, 1));
    CHECK(!decals_tile_has_print(&d, 2, 4));
    CHECK(!decals_tile_has_print(&d, -1, 1));

    /* culling removes prints more than 2 tiles behind a camera at x = 5.5 */
    int culled = decals_cull_behind(&d, 5.5f, 1.5f, 2.0f);
    CHECK(culled > 0);
    CHECK_NEAR(decals_sample(&d, d.p[0].x, d.p[0].y), 0.0f, 1e-6);
    CHECK(decals_sample(&d, d.p[h].x, d.p[h].y) > 0.0f);
    CHECK_EQ(decals_cull_behind(&d, 5.5f, 1.5f, 2.0f), 0);
    CHECK(!decals_tile_has_print(&d, 2, 1));                  /* every print on that tile was culled */

    /* an L-shaped path orients the second leg along +y */
    decals_clear(&d);
    p.n = 5; p.t[0] = (Tile){2, 2}; p.t[1] = (Tile){3, 2}; p.t[2] = (Tile){4, 2}; p.t[3] = (Tile){4, 3}; p.t[4] = (Tile){4, 4};
    decals_lay_trail(&d, &p);
    CHECK_NEAR(d.p[d.n - 1].ca, 0.0f, 1e-3); CHECK_NEAR(d.p[d.n - 1].sa, 1.0f, 1e-3);
    CHECK(d.p[d.n - 1].x > 4.3f && d.p[d.n - 1].x < 4.7f);
    CHECK_NEAR(decals_sample(&d, 4.5f, d.p[d.n - 1].y), 0.0f, 1e-6);      /* centre line of the +y leg */

    /* a path too long for the print budget is truncated safely */
    decals_clear(&d);
    p.n = MAP_MAX_PATH;
    for (int i = 0; i < p.n; i++) { p.t[i].x = i % 40; p.t[i].y = i / 40; }
    laid = decals_lay_trail(&d, &p);
    CHECK(laid <= DECAL_MAX);
    CHECK_EQ(d.n, laid);
}
