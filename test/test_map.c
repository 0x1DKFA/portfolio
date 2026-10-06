#include "test.h"
#include "map.h"

static const char *const SMALL[8] = {
    "########",
    "#......#",
    "#.####.#",
    "#.#..#.#",
    "#.#..#.#",
    "#.####.#",
    "#......#",
    "########",
};

static const char *const BAD_CHAR[3] = { "###", "#x#", "###" };
static const char *const BAD_LEN[3]  = { "###", "####", "###" };
static const char *const SHORT_ROW[3] = { "###", "##", "###" };

void test_map(void) {
    static Map m;
    CHECK_EQ(map_parse(&m, CITY_MAP, MAP_W, MAP_H), 0);
    CHECK_EQ(m.w, 48); CHECK_EQ(m.h, 48);
    CHECK_EQ(m.start_x, 2); CHECK_EQ(m.start_y, 2);
    CHECK(map_is_floor(&m, 2, 2));
    CHECK(!map_is_floor(&m, 0, 0));
    CHECK(!map_is_floor(&m, -1, 5));
    CHECK(!map_is_floor(&m, 5, 48));
    CHECK_EQ(map_wall_kind(&m, 0, 0), CELL_BRICK);
    CHECK_EQ(map_wall_kind(&m, -3, 0), CELL_BRICK);
    CHECK_EQ(map_wall_kind(&m, 12, 15), CELL_NEON);
    CHECK_EQ(map_wall_kind(&m, 33, 18), CELL_SIGN);
    CHECK_EQ(map_wall_kind(&m, 21, 25), CELL_DUMPSTER);
    CHECK_EQ(map_wall_kind(&m, 30, 34), CELL_DUMPSTER);
    CHECK_EQ(map_wall_kind(&m, 3, 12), CELL_CONCRETE);
    CHECK_EQ(map_wall_kind(&m, 4, 3), CELL_WINDOW);
    CHECK_EQ(map_wall_kind(&m, 21, 4), CELL_POSTER);
    CHECK_EQ(m.floor_kind[1][9], FLOOR_PUDDLE);
    CHECK_EQ(m.floor_kind[2][2], FLOOR_ASPHALT);

    int dogs = 0, lamps = 0, trash = 0, paper = 0, news = 0;
    for (int i = 0; i < m.n_spawns; i++) {
        switch (m.spawns[i].kind) {
        case PROP_DOG: dogs++; break; case PROP_LAMP: lamps++; break; case PROP_TRASH: trash++; break;
        case PROP_PAPER: paper++; break; case PROP_NEWSPAPER: news++; break; default: break;
        }
    }
    CHECK_EQ(dogs, 3);
    CHECK(lamps >= 12);
    CHECK(trash >= 8);
    CHECK(paper >= 3);
    CHECK(news >= 3);
    CHECK_EQ(m.prop[6][1], PROP_DOG);
    CHECK_EQ(m.prop[1][3], PROP_LAMP);
    CHECK(!map_is_walkable(&m, 3, 1));
    CHECK(!map_is_walkable(&m, 18, 1));
    CHECK(map_is_walkable(&m, 2, 2));

    /* light: a lamp tile is fully lit, far floor is ambient */
    CHECK_NEAR(map_light(&m, 3, 1), 1.0f, 1e-4);
    CHECK_NEAR(map_light(&m, 2, 2), 0.25f + 0.75f * (1.0f - 1.41421f / 3.0f), 1e-3);   /* one lamp at distance sqrt(2) */
    CHECK_NEAR(map_light(&m, 24, 38), 0.25f, 1e-4);                                        /* no lamp within 3 */
    CHECK(map_is_connected(&m));

    /* hiding spots: floor next to a trash can or dumpster */
    CHECK(map_is_hiding_spot(&m, 20, 25));     /* west of the dumpster at (21,25) */
    CHECK(map_is_hiding_spot(&m, 17, 1));      /* next to the trash can at (18,1) */
    CHECK(!map_is_hiding_spot(&m, 18, 1));     /* the trash tile itself is not a spot */
    CHECK(!map_is_hiding_spot(&m, 2, 2));
    CHECK(!map_is_hiding_spot(&m, 21, 25));    /* a wall */

    Rng rng; rng_seed(&rng, 3u);
    Tile from = { 2, 2 }, spot;
    for (int i = 0; i < 20; i++) {
        CHECK(map_pick_hiding_spot(&m, &rng, from, 12, 30, &spot));
        CHECK(map_is_hiding_spot(&m, spot.x, spot.y));
        Path p;
        CHECK(map_bfs(&m, from, spot, &p));
        CHECK(p.n - 1 >= 12);
        CHECK(p.n - 1 <= 30);                    /* the band holds when candidates exist */
        CHECK_EQ(p.t[0].x, 2); CHECK_EQ(p.t[0].y, 2);
        CHECK_EQ(p.t[p.n - 1].x, spot.x); CHECK_EQ(p.t[p.n - 1].y, spot.y);
        for (int k = 1; k < p.n; k++) {          /* consecutive tiles are 4-neighbours on floor */
            int dx = p.t[k].x - p.t[k - 1].x, dy = p.t[k].y - p.t[k - 1].y;
            CHECK((dx == 0 && (dy == 1 || dy == -1)) || (dy == 0 && (dx == 1 || dx == -1)));
            CHECK(map_is_walkable(&m, p.t[k].x, p.t[k].y));
        }
    }
    CHECK(map_pick_hiding_spot(&m, &rng, from, 200, 300, &spot));  /* no candidate that far: fallback to the farthest */

    /* Equal length routes should vary with the supplied RNG. */
    Rng route_rng; rng_seed(&route_rng, 77u);
    Path route, first_route;
    CHECK(map_bfs_random(&m, &route_rng, (Tile){2, 2}, (Tile){17, 1}, &route));
    first_route = route;
    int varied = 0;
    for (int i = 0; i < 12; i++) {
        CHECK(map_bfs_random(&m, &route_rng, (Tile){2, 2}, (Tile){17, 1}, &route));
        for (int k = 0; k < route.n; k++) if (route.t[k].x != first_route.t[k].x || route.t[k].y != first_route.t[k].y) { varied = 1; break; }
    }
    CHECK(varied);

    /* small map: BFS detour length, unreachable pocket, connectivity false */
    static Map s;
    CHECK_EQ(map_parse(&s, SMALL, 8, 8), 0);
    Path p;
    CHECK(map_bfs(&s, (Tile){1, 1}, (Tile){6, 6}, &p));
    CHECK_EQ(p.n, 11);
    CHECK(!map_bfs(&s, (Tile){1, 1}, (Tile){3, 3}, &p));
    CHECK(!map_is_connected(&s));
    static int16_t dist[8 * 8];
    int reach = map_distances(&s, (Tile){1, 1}, dist);
    CHECK_EQ(dist[1 * 8 + 1], 0);
    CHECK_EQ(dist[6 * 8 + 6], 10);
    CHECK_EQ(dist[3 * 8 + 3], -1);
    CHECK_EQ(reach, 20);
    CHECK(!map_pick_hiding_spot(&s, &rng, (Tile){1, 1}, 1, 5, &spot));   /* no T or D anywhere */

    CHECK_EQ(map_parse(&s, BAD_CHAR, 3, 3), -1);
    CHECK_EQ(map_parse(&s, BAD_LEN, 3, 3), -1);
    CHECK_EQ(map_parse(&s, SHORT_ROW, 3, 3), -1);
    static const char *const OK3[3] = { "###", "#.#", "###" };
    CHECK_EQ(map_parse(&s, OK3, 3, 3), 0);
    CHECK_EQ(map_parse(&s, OK3, 2, 3), -1);                   /* the size guard still applies */
    CHECK_EQ(map_parse(&s, SMALL, MAP_W + 1, 8), -1);
}
