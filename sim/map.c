#include "map.h"
#include "fmath.h"

/* 48 x 48. Legend: # brick, = concrete, W windows, N neon name sign, S generic sign, P poster,
 * D dumpster (wall), . asphalt, ~ puddle, L lamp, T trash can, c paper, n newspaper, d dog, @ start.
 * Alleys: full-width rows 1-2, 10-11, 19-20, 28-29, 37-38, 46; columns 1-2, 10-11, 19-20, 28-29, 37-38, 46.
 * Dead ends: column 46 rows 3-9, column 28-29 rows 12-15, column 10-11 rows 21-24, column 37-38 rows 30-33. */
const char *const CITY_MAP[MAP_H] = {
    "################################################",
    "#..L.....~........T.........L.........c......~.#",
    "#.@............................................#",
    "#..#W#W#W#..=======..#######..=W=W=W=..#########",
    "#..#######L.=W===W=~.P#####W..=======..#W#######",
    "#..W#####W..=======..#######.TW=====W..#########",
    "#d.#######..W=====W..W######..=======n.#####W###",
    "#..#W###W#..=======..#######..=W===W=..#########",
    "#..#######..=W===W=c.######P..=======..W#####W##",
    "#..#W#W#W#..=======..#######..=W=W=W=..#########",
    "#.......L..........~..........T..........L.....#",
    "#..c...........................n...............#",
    "#..=======..#W#W#W#..=======#########..=W=W=W=.#",
    "#L.=W===W=..#######..=W===W=##P######..=======.#",
    "#..=======..W#####W..=======#########..W=====W.#",
    "#..=W===W=..N######..=======#########T.=======.#",
    "#..=======..#######..=W===W=..#######..=W===W=.#",
    "#..=W===W=~.#####W#..=======..#######..=======.#",
    "#..=======..#W#W#W#..=======..###S###..=W=W=W=.#",
    "#....T...........L...........~..........c......#",
    "#...................d..........................#",
    "#..#########=W=W=W=..#######..=======..#W#W#W#.#",
    "#..#W###W###=======..P#####W..=W===W=..#######.#",
    "#..#########W=====W~.#######..=======L.W#####W.#",
    "#..W#####W##=======..#######..=W===W=..#######.#",
    "#..#######..=W===W=..D######..=======..#####W#.#",
    "#.T#W###W#..=======..#######n.=W===W=..#######.#",
    "#..#######..=W=W=W=..#######..=======..#W#W#W#.#",
    "#.L.........~.........T.........L..........n...#",
    "#.........................................c....#",
    "#..=======..#######..=W=W=W=..#########=======.#",
    "#..=W===W=..#W###W#..=======..P#####W##=W===W=.#",
    "#~.=======..#######..W=====W..#########=======.#",
    "#..=W===W=L.W#####W..=======..#########W=====W.#",
    "#..=======..#######T.=W===W=..D######..=======.#",
    "#..=W===W=..#W###W#..=======..#######..=W===W=.#",
    "#..=======..#######..=W=W=W=..#######..=======.#",
    "#....~..........L..........T..........L.......d#",
    "#..............................................#",
    "#..#W#W#W#..=======..#######..=W=W=W=..#######.#",
    "#..#######..=W===W=..P#####W..=======..#W#####.#",
    "#c.W#####W..=======L.#######..W=====W..#######.#",
    "#..#######..W=====W..W######..=======..#####W#.#",
    "#..#W###W#..=======..#######~.=W===W=..#######.#",
    "#..#######T.=W===W=..######P..=======..W#####W.#",
    "#..#W#W#W#..=======..#######..=W=W=W=..#######.#",
    "#.L..........T..........L..........T..........L#",
    "################################################",
};

static int legend(char c, int *cell, int *floor_kind, int *prop) {
    *cell = CELL_FLOOR; *floor_kind = FLOOR_ASPHALT; *prop = PROP_NONE;
    switch (c) {
    case '#': *cell = CELL_BRICK; return 1;
    case '=': *cell = CELL_CONCRETE; return 1;
    case 'W': *cell = CELL_WINDOW; return 1;
    case 'N': *cell = CELL_NEON; return 1;
    case 'S': *cell = CELL_SIGN; return 1;
    case 'P': *cell = CELL_POSTER; return 1;
    case 'D': *cell = CELL_DUMPSTER; return 1;
    case '.': return 1;
    case '~': *floor_kind = FLOOR_PUDDLE; return 1;
    case 'L': *prop = PROP_LAMP; return 1;
    case 'T': *prop = PROP_TRASH; return 1;
    case 'c': *prop = PROP_PAPER; return 1;
    case 'n': *prop = PROP_NEWSPAPER; return 1;
    case 'd': *prop = PROP_DOG; return 1;
    case '@': *prop = PROP_START; return 1;
    default: return 0;
    }
}

static void compute_light(Map *m) {
    for (int y = 0; y < m->h; y++) for (int x = 0; x < m->w; x++) m->light[y][x] = 0.25f;
    for (int i = 0; i < m->n_spawns; i++) {
        if (m->spawns[i].kind != PROP_LAMP) continue;
        int lx = m->spawns[i].x, ly = m->spawns[i].y;
        for (int dy = -3; dy <= 3; dy++) for (int dx = -3; dx <= 3; dx++) {
            int x = lx + dx, y = ly + dy;
            if (x < 0 || y < 0 || x >= m->w || y >= m->h) continue;
            float d = fm_sqrt((float)(dx * dx + dy * dy));
            if (d >= 3.0f) continue;
            m->light[y][x] = fm_min(1.0f, m->light[y][x] + 0.75f * (1.0f - d / 3.0f));
        }
    }
}

int map_parse(Map *m, const char *const *rows, int w, int h) {
    if (w < 3 || h < 3 || w > MAP_W || h > MAP_H) return -1;
    m->w = w; m->h = h; m->n_spawns = 0; m->start_x = 1; m->start_y = 1;
    for (int y = 0; y < h; y++) {
        const char *row = rows[y];
        for (int x = 0; x < w; x++) {
            int cell, fk, prop;
            if (row[x] == '\0' || !legend(row[x], &cell, &fk, &prop)) return -1;
            m->cell[y][x] = (uint8_t)cell;
            m->floor_kind[y][x] = (uint8_t)fk;
            m->prop[y][x] = (uint8_t)prop;
            if (prop == PROP_START) { m->start_x = x; m->start_y = y; }
            else if (prop != PROP_NONE && m->n_spawns < MAP_MAX_SPAWNS) {
                m->spawns[m->n_spawns].kind = prop; m->spawns[m->n_spawns].x = x; m->spawns[m->n_spawns].y = y;
                m->n_spawns++;
            }
        }
        if (row[w] != '\0') return -1;          /* row longer than w */
    }
    compute_light(m);
    return 0;
}

int map_is_floor(const Map *m, int x, int y) {
    if (x < 0 || y < 0 || x >= m->w || y >= m->h) return 0;
    return m->cell[y][x] == CELL_FLOOR;
}

int map_wall_kind(const Map *m, int x, int y) {
    if (x < 0 || y < 0 || x >= m->w || y >= m->h) return CELL_BRICK;
    return m->cell[y][x];
}

float map_light(const Map *m, int x, int y) {
    if (x < 0 || y < 0 || x >= m->w || y >= m->h) return 0.25f;
    return m->light[y][x];
}

static const int DX[4] = { 1, -1, 0, 0 }, DY[4] = { 0, 0, 1, -1 };

/* Uses static scratch buffers: not reentrant. The simulation is single-threaded and never nests these calls. */
int map_distances(const Map *m, Tile from, int16_t *dist) {
    static int queue[MAP_W * MAP_H];
    int n = m->w * m->h, head = 0, tail = 0, reach = 0;
    for (int i = 0; i < n; i++) dist[i] = -1;
    if (!map_is_floor(m, from.x, from.y)) return 0;
    dist[from.y * m->w + from.x] = 0;
    queue[tail++] = from.y * m->w + from.x;
    while (head < tail) {
        int cur = queue[head++], cx = cur % m->w, cy = cur / m->w;
        reach++;
        for (int d = 0; d < 4; d++) {
            int nx = cx + DX[d], ny = cy + DY[d];
            if (!map_is_floor(m, nx, ny)) continue;
            int ni = ny * m->w + nx;
            if (dist[ni] >= 0) continue;
            dist[ni] = (int16_t)(dist[cur] + 1);
            queue[tail++] = ni;
        }
    }
    return reach;
}

int map_bfs(const Map *m, Tile from, Tile to, Path *out) {
    static int16_t dist[MAP_W * MAP_H];
    out->n = 0;
    if (!map_is_floor(m, to.x, to.y)) return 0;
    map_distances(m, from, dist);
    int ti = to.y * m->w + to.x;
    if (dist[ti] < 0 || dist[ti] + 1 > MAP_MAX_PATH) return 0;
    int len = dist[ti] + 1;
    out->n = len;
    Tile cur = to;
    for (int k = len - 1; k >= 0; k--) {           /* walk downhill in distance back to from */
        out->t[k] = cur;
        if (k == 0) break;
        int cd = dist[cur.y * m->w + cur.x];
        for (int d = 0; d < 4; d++) {
            int nx = cur.x + DX[d], ny = cur.y + DY[d];
            if (map_is_floor(m, nx, ny) && dist[ny * m->w + nx] == cd - 1) { cur.x = nx; cur.y = ny; break; }
        }
    }
    return 1;
}

int map_is_connected(const Map *m) {
    static int16_t dist[MAP_W * MAP_H];
    int floors = 0;
    Tile any = { -1, -1 };
    for (int y = 0; y < m->h; y++) for (int x = 0; x < m->w; x++)
        if (m->cell[y][x] == CELL_FLOOR) { floors++; if (any.x < 0) { any.x = x; any.y = y; } }
    if (floors == 0) return 1;
    return map_distances(m, any, dist) == floors;
}

int map_is_hiding_spot(const Map *m, int x, int y) {
    if (!map_is_floor(m, x, y) || m->prop[y][x] == PROP_TRASH) return 0;
    for (int d = 0; d < 4; d++) {
        int nx = x + DX[d], ny = y + DY[d];
        if (nx < 0 || ny < 0 || nx >= m->w || ny >= m->h) continue;
        if (m->cell[ny][nx] == CELL_DUMPSTER) return 1;
        if (m->cell[ny][nx] == CELL_FLOOR && m->prop[ny][nx] == PROP_TRASH) return 1;
    }
    return 0;
}

/* Picks a random hiding spot with min_steps <= dist <= max_steps from `from`. If none fall in that
 * band, picks at random among spots at least min_steps away. If none of those exist either, falls
 * back to the single farthest reachable hiding spot (deterministic first-in-scan on ties). */
int map_pick_hiding_spot(const Map *m, Rng *rng, Tile from, int min_steps, int max_steps, Tile *out) {
    static int16_t dist[MAP_W * MAP_H];
    static Tile cands[MAP_W * MAP_H];
    int n, best, best_d;
    map_distances(m, from, dist);

    n = 0;
    for (int y = 0; y < m->h; y++) for (int x = 0; x < m->w; x++) {
        int d = dist[y * m->w + x];
        if (d < min_steps || d > max_steps || !map_is_hiding_spot(m, x, y)) continue;
        cands[n].x = x; cands[n].y = y; n++;
    }
    if (n > 0) { *out = cands[rng_range(rng, 0, n - 1)]; return 1; }

    n = 0;
    for (int y = 0; y < m->h; y++) for (int x = 0; x < m->w; x++) {
        int d = dist[y * m->w + x];
        if (d < min_steps || !map_is_hiding_spot(m, x, y)) continue;
        cands[n].x = x; cands[n].y = y; n++;
    }
    if (n > 0) { *out = cands[rng_range(rng, 0, n - 1)]; return 1; }

    best = -1; best_d = 0;
    for (int y = 0; y < m->h; y++) for (int x = 0; x < m->w; x++) {
        int d = dist[y * m->w + x];
        if (d <= 0 || !map_is_hiding_spot(m, x, y)) continue;
        if (d > best_d) { best_d = d; best = y * m->w + x; }
    }
    if (best >= 0) { out->x = best % m->w; out->y = best / m->w; return 1; }
    return 0;
}
