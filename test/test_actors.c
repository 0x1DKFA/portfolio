#include "test.h"
#include "actors/dog.h"
#include "actors/trash.h"
#include "actors/bug.h"
#include "decals.h"

#define DT (1.0f / 60.0f)

static const char *const ROOM[7] = {
    "#######", "#.....#", "#.....#", "#.....#", "#.....#", "#.....#", "#######",
};

void test_actors(void) {
    static Map city, room; static Decals d; static Sprites sp;
    CHECK_EQ(map_parse(&city, CITY_MAP, MAP_W, MAP_H), 0);
    CHECK_EQ(map_parse(&room, ROOM, 7, 7), 0);
    decals_clear(&d);
    sprites_clear(&sp);
    Rng rng; rng_seed(&rng, 1u);

    /* dog on the city: stays on floor for two minutes through dead ends, moves, animates */
    Dog dog;
    Sprite *ds = sprites_add(&sp, SPR_DOG_A, 1.5f, 6.5f, 0.6f);
    dog_init(&dog, 1, 6, ds);
    CHECK_NEAR(dog.x, 1.5f, 1e-5); CHECK_NEAR(dog.y, 6.5f, 1e-5);
    int moved = 0, saw_a = 0, saw_b = 0;
    float px0 = dog.x, py0 = dog.y;
    for (int i = 0; i < 120 * 60; i++) {
        dog_step(&dog, &city, &d, &rng, DT);
        CHECK(map_is_floor(&city, (int)dog.x, (int)dog.y));
        if (dog.x != px0 || dog.y != py0) moved = 1;
        if (ds->kind == SPR_DOG_A) saw_a = 1;
        if (ds->kind == SPR_DOG_B) saw_b = 1;
        if (dog.state == DOG_SNIFF) CHECK_EQ(ds->kind, SPR_DOG_SNIFF);
        CHECK_NEAR(ds->x, dog.x, 1e-5); CHECK_NEAR(ds->y, dog.y, 1e-5);
    }
    CHECK(moved); CHECK(saw_a); CHECK(saw_b);

    /* dog in the room with a footprint trail across the middle row: it sniffs */
    Path trail; trail.n = 5;
    for (int i = 0; i < 5; i++) { trail.t[i].x = 1 + i; trail.t[i].y = 3; }
    decals_lay_trail(&d, &trail);
    Dog dog2; dog_init(&dog2, 3, 3, ds);
    int saw_sniff = 0;
    for (int i = 0; i < 120 * 60 && !saw_sniff; i++) {
        dog_step(&dog2, &room, &d, &rng, DT);
        CHECK(map_is_floor(&room, (int)dog2.x, (int)dog2.y));
        if (dog2.state == DOG_SNIFF) { saw_sniff = 1; CHECK_EQ(ds->kind, SPR_DOG_SNIFF); }
    }
    CHECK(saw_sniff);
    decals_clear(&d);

    /* wind: gusts are scheduled 4-10 s apart and last 1.2 s */
    Wind wind; wind_init(&wind, &rng);
    CHECK(!wind_gusting(&wind));
    CHECK(wind.until_next >= TRASH_GUST_MIN && wind.until_next <= TRASH_GUST_MAX);
    int gust_steps = 0, gusts = 0, was = 0;
    for (int i = 0; i < 60 * 60; i++) {
        wind_step(&wind, &rng, DT);
        int g = wind_gusting(&wind);
        if (g) gust_steps++;
        if (g && !was) gusts++;
        was = g;
    }
    CHECK(gusts >= 5 && gusts <= 15);
    CHECK(gust_steps >= (gusts - 1) * 70 && gust_steps <= gusts * 74);   /* the last gust may be cut off at 60 s */

    /* loose trash: still without wind, moves during a gust, decays after, stops at walls */
    Loose paper;
    Sprite *ps = sprites_add(&sp, SPR_PAPER_A, 3.5f, 3.5f, 0.3f);
    loose_init(&paper, LOOSE_PAPER, 3, 3, ps);
    Wind calm = { 100.0f, 0.0f, 1 };
    for (int i = 0; i < 60; i++) loose_step(&paper, &room, &calm, DT);
    CHECK_NEAR(paper.x, 3.5f, 1e-5); CHECK_NEAR(paper.y, 3.5f, 1e-5);
    CHECK_EQ(ps->kind, SPR_PAPER_A);
    Wind gust = { 100.0f, TRASH_GUST_LEN, 1 };
    int saw_b_frame = 0;
    for (int i = 0; i < 30; i++) { loose_step(&paper, &room, &gust, DT); if (ps->kind == SPR_PAPER_B) saw_b_frame = 1; }
    CHECK(paper.x > 3.6f);
    CHECK(paper.vx > 0.5f);
    CHECK(saw_b_frame);
    for (int i = 0; i < 90; i++) loose_step(&paper, &room, &calm, DT);
    CHECK(paper.vx < 0.05f);
    CHECK(map_is_floor(&room, (int)paper.x, (int)paper.y));
    Wind west = { 100.0f, TRASH_GUST_LEN, -1 };
    for (int i = 0; i < 600; i++) { west.left = TRASH_GUST_LEN; loose_step(&paper, &room, &west, DT); }
    CHECK(paper.x >= 1.0f);                                   /* the x = 0 column is wall */
    CHECK(map_is_floor(&room, (int)paper.x, (int)paper.y));
    CHECK_NEAR(paper.vx, 0.0f, 1e-3);                          /* pinned against the wall */
    CHECK_NEAR(ps->x, paper.x, 1e-5);

    /* the hidden bug swaps sprites and disappears when dead */
    HiddenBug bug;
    int before = 0; for (int i = 0; i < MAX_SPRITES; i++) if (sp.s[i].active) before++;
    bug_place(&bug, &sp, 4.3f, 3.5f);
    CHECK_EQ(bug.state, BUGSTATE_HIDDEN);
    CHECK(bug.spr != NULL); CHECK_EQ(bug.spr->kind, SPR_BUG_HIDDEN);
    bug_set_state(&bug, &sp, BUGSTATE_PEEK);    CHECK_EQ(bug.spr->kind, SPR_BUG_PEEK);
    bug_set_state(&bug, &sp, BUGSTATE_EXPOSED); CHECK_EQ(bug.spr->kind, SPR_BUG_EXPOSED);
    bug_set_state(&bug, &sp, BUGSTATE_DEAD);
    CHECK_EQ(bug.state, BUGSTATE_DEAD);
    CHECK(bug.spr == NULL);
    int after = 0; for (int i = 0; i < MAX_SPRITES; i++) if (sp.s[i].active) after++;
    CHECK_EQ(after, before);
}
