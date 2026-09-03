#include "decals.h"
#include "fmath.h"

void decals_clear(Decals *d) {
    d->n = 0; d->head = 0;
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) for (int k = 0; k < DECAL_PER_TILE; k++) d->cell[y][x][k] = -1;
    for (int i = 0; i < MAP_MAX_PATH; i++) d->wp_first[i] = 0;
}

static void register_print(Decals *d, int idx) {
    const Footprint *p = &d->p[idx];
    int x0 = (int)(p->x - DECAL_HALF_LEN), x1 = (int)(p->x + DECAL_HALF_LEN);
    int y0 = (int)(p->y - DECAL_HALF_LEN), y1 = (int)(p->y + DECAL_HALF_LEN);
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
        if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) continue;
        for (int k = 0; k < DECAL_PER_TILE; k++) if (d->cell[y][x][k] < 0) { d->cell[y][x][k] = (int16_t)idx; break; }
    }
}

static int add_print(Decals *d, float x, float y, float ca, float sa) {
    if (d->n >= DECAL_MAX) return 0;
    Footprint *p = &d->p[d->n];
    p->x = x; p->y = y; p->ca = ca; p->sa = sa; p->index = d->n; p->active = 1;
    register_print(d, d->n);
    d->n++;
    return 1;
}

int decals_lay_trail(Decals *d, const Path *path) {
    decals_clear(d);
    if (path->n < 2) return 0;
    float carry = 0.0f;                                     /* distance since the last pair */
    for (int i = 0; i + 1 < path->n && i < MAP_MAX_PATH; i++) {
        d->wp_first[i] = d->n;
        float ax = (float)path->t[i].x + 0.5f, ay = (float)path->t[i].y + 0.5f;
        float bx = (float)path->t[i + 1].x + 0.5f, by = (float)path->t[i + 1].y + 0.5f;
        float dx = bx - ax, dy = by - ay, len = fm_sqrt(dx * dx + dy * dy);
        if (len < 0.0001f) continue;
        float ca = dx / len, sa = dy / len;                   /* heading */
        float rx = -sa, ry = ca;                              /* right-hand perpendicular */
        float s = DECAL_SPACING - carry;                      /* first pair position along this segment */
        while (s <= len) {
            float px = ax + ca * s, py = ay + sa * s;
            if (!add_print(d, px - rx * DECAL_OFFSET, py - ry * DECAL_OFFSET, ca, sa)) return d->n;
            if (!add_print(d, px + rx * DECAL_OFFSET, py + ry * DECAL_OFFSET, ca, sa)) return d->n;
            s += DECAL_SPACING;
        }
        carry = len - (s - DECAL_SPACING);
    }
    for (int i = path->n - 1; i < MAP_MAX_PATH; i++) d->wp_first[i] = d->n;
    return d->n;
}

void decals_set_head_waypoint(Decals *d, int waypoint) {
    if (waypoint < 0) waypoint = 0;
    if (waypoint >= MAP_MAX_PATH) waypoint = MAP_MAX_PATH - 1;
    d->head = d->wp_first[waypoint];
}

static float glow_for(const Decals *d, int index) {
    if (index < d->head) return DECAL_GLOW_DIM;
    int pairs_ahead = (index - d->head) / 2;
    if (pairs_ahead < DECAL_GLOW_BRIGHT_PAIRS) return 1.0f;
    float g = 1.0f - 0.5f * (float)(pairs_ahead - DECAL_GLOW_BRIGHT_PAIRS) / (float)DECAL_GLOW_BRIGHT_PAIRS;
    return g < 0.5f ? 0.5f : g;
}

float decals_sample(const Decals *d, float fx, float fy) {
    int tx = (int)fx, ty = (int)fy;
    if (fx < 0 || fy < 0 || tx >= MAP_W || ty >= MAP_H) return 0.0f;
    for (int k = 0; k < DECAL_PER_TILE; k++) {
        int idx = d->cell[ty][tx][k];
        if (idx < 0) break;
        const Footprint *p = &d->p[idx];
        if (!p->active) continue;
        float dx = fx - p->x, dy = fy - p->y;
        float along = dx * p->ca + dy * p->sa, across = -dx * p->sa + dy * p->ca;
        if (fm_abs(along) <= DECAL_HALF_LEN && fm_abs(across) <= DECAL_HALF_WID) return glow_for(d, idx);
    }
    return 0.0f;
}

int decals_tile_has_print(const Decals *d, int tx, int ty) {
    if (tx < 0 || ty < 0 || tx >= MAP_W || ty >= MAP_H) return 0;
    for (int k = 0; k < DECAL_PER_TILE; k++) {
        int idx = d->cell[ty][tx][k];
        if (idx < 0) break;
        if (d->p[idx].active) return 1;
    }
    return 0;
}

int decals_cull_behind(Decals *d, float cx, float cy, float max_behind) {
    int culled = 0;
    for (int i = 0; i < d->n && i < d->head; i++) {
        Footprint *p = &d->p[i];
        if (!p->active) continue;
        float dx = p->x - cx, dy = p->y - cy;
        if (dx * dx + dy * dy > max_behind * max_behind) { p->active = 0; culled++; }
    }
    return culled;
}
