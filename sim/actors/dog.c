#include "actors/dog.h"
#include "fmath.h"

static const int DX[4] = { 1, -1, 0, 0 }, DY[4] = { 0, 0, 1, -1 };
static const int REV[4] = { 1, 0, 3, 2 };

void dog_init(Dog *d, int tile_x, int tile_y, Sprite *spr) {
    d->x = (float)tile_x + 0.5f; d->y = (float)tile_y + 0.5f;
    d->tx = tile_x; d->ty = tile_y; d->dir = 0; d->state = DOG_WALK; d->t = 0.0f; d->anim = 0.0f; d->spr = spr;
    if (spr) { spr->x = d->x; spr->y = d->y; spr->kind = SPR_DOG_A; }
}

static int near_interest(const Dog *d, const Map *m, const Decals *decals) {
    if (decals_tile_has_print(decals, d->tx, d->ty)) return 1;
    for (int k = 0; k < 4; k++) {
        int nx = d->tx + DX[k], ny = d->ty + DY[k];
        if (nx >= 0 && ny >= 0 && nx < m->w && ny < m->h && m->cell[ny][nx] == CELL_FLOOR && m->prop[ny][nx] == PROP_TRASH) return 1;
    }
    return 0;
}

static void choose_dir(Dog *d, const Map *m, Rng *rng) {
    int weights[4] = { 0, 0, 0, 0 }, total = 0;
    for (int k = 0; k < 4; k++) {
        if (!map_is_floor(m, d->tx + DX[k], d->ty + DY[k])) continue;
        if (k == REV[d->dir]) continue;
        weights[k] = (k == d->dir) ? 3 : 1;
        total += weights[k];
    }
    if (total == 0) {                                   /* dead end: turn around */
        int k = REV[d->dir];
        if (map_is_floor(m, d->tx + DX[k], d->ty + DY[k])) d->dir = k;
        return;
    }
    int pick = rng_range(rng, 1, total);
    for (int k = 0; k < 4; k++) { pick -= weights[k]; if (weights[k] && pick <= 0) { d->dir = k; return; } }
}

void dog_step(Dog *d, const Map *m, const Decals *decals, Rng *rng, float dt) {
    if (d->state == DOG_SNIFF) {
        d->t -= dt;
        if (d->t <= 0.0f) d->state = DOG_WALK;
    } else {
        float gx = (float)d->tx + 0.5f, gy = (float)d->ty + 0.5f;
        float dx = gx - d->x, dy = gy - d->y, dist = fm_sqrt(dx * dx + dy * dy), step = DOG_SPEED * dt;
        if (dist <= step) {
            d->x = gx; d->y = gy;
            if (near_interest(d, m, decals) && rng_float(rng) < DOG_SNIFF_CHANCE) {
                d->state = DOG_SNIFF; d->t = DOG_SNIFF_MIN + rng_float(rng) * (DOG_SNIFF_MAX - DOG_SNIFF_MIN);
            } else {
                choose_dir(d, m, rng);
                if (map_is_floor(m, d->tx + DX[d->dir], d->ty + DY[d->dir])) { d->tx += DX[d->dir]; d->ty += DY[d->dir]; }
            }
        } else {
            d->x += dx / dist * step; d->y += dy / dist * step;
            d->anim += dt;
        }
    }
    if (d->spr) {
        d->spr->x = d->x; d->spr->y = d->y;
        d->spr->kind = d->state == DOG_SNIFF ? SPR_DOG_SNIFF : (((int)(d->anim * 4.0f)) & 1 ? SPR_DOG_B : SPR_DOG_A);
    }
}
