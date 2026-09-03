#include "actors/trash.h"
#include "fmath.h"

static float schedule(Rng *rng) { return TRASH_GUST_MIN + rng_float(rng) * (TRASH_GUST_MAX - TRASH_GUST_MIN); }

void wind_init(Wind *w, Rng *rng) { w->until_next = schedule(rng); w->left = 0.0f; w->dir_sign = 1; }

void wind_step(Wind *w, Rng *rng, float dt) {
    if (w->left > 0.0f) { w->left -= dt; if (w->left <= 0.0f) { w->left = 0.0f; w->until_next = schedule(rng); } return; }
    w->until_next -= dt;
    if (w->until_next <= 0.0f) { w->left = TRASH_GUST_LEN; w->dir_sign = rng_range(rng, 0, 1) ? 1 : -1; }
}

int wind_gusting(const Wind *w) { return w->left > 0.0f; }

void loose_init(Loose *l, int kind, int tile_x, int tile_y, Sprite *spr) {
    l->x = (float)tile_x + 0.5f; l->y = (float)tile_y + 0.5f; l->vx = 0.0f; l->vy = 0.0f;
    l->kind = kind; l->anim = 0.0f; l->spr = spr;
    if (spr) { spr->x = l->x; spr->y = l->y; }
}

static void move_axis(float *pos, float *vel, float other, int horizontal, const Map *m, float dt) {
    float next = *pos + *vel * dt;
    float margin = 0.2f;
    float probe = next + (*vel > 0 ? margin : -margin);
    int ok = horizontal ? map_is_floor(m, (int)probe, (int)other) : map_is_floor(m, (int)other, (int)probe);
    if (ok) *pos = next; else *vel = 0.0f;
}

void loose_step(Loose *l, const Map *m, const Wind *w, float dt) {
    if (wind_gusting(w)) {
        int tx = (int)l->x, ty = (int)l->y;
        int open_x = map_is_floor(m, tx + w->dir_sign, ty);
        int open_y = map_is_floor(m, tx, ty + w->dir_sign);
        float target = (float)w->dir_sign * TRASH_GUST_SPEED, k = fm_min(1.0f, 6.0f * dt);
        if (open_x) l->vx += (target - l->vx) * k;
        else if (open_y) l->vy += (target - l->vy) * k;
        else if (open_x || map_is_floor(m, tx - w->dir_sign, ty)) l->vx += (target - l->vx) * k;   /* blocked ahead: push into the wall, move_axis pins it */
    } else {
        float f = 1.0f - TRASH_FRICTION * dt; if (f < 0.0f) f = 0.0f;
        l->vx *= f; l->vy *= f;
        if (fm_abs(l->vx) < 0.01f) l->vx = 0.0f;
        if (fm_abs(l->vy) < 0.01f) l->vy = 0.0f;
    }
    move_axis(&l->x, &l->vx, l->y, 1, m, dt);
    move_axis(&l->y, &l->vy, l->x, 0, m, dt);
    int moving = fm_abs(l->vx) > 0.05f || fm_abs(l->vy) > 0.05f;
    if (moving) l->anim += dt;
    if (l->spr) {
        l->spr->x = l->x; l->spr->y = l->y;
        int b = moving && (((int)(l->anim * 6.0f)) & 1);
        l->spr->kind = l->kind == LOOSE_PAPER ? (b ? SPR_PAPER_B : SPR_PAPER_A) : (b ? SPR_NEWS_B : SPR_NEWS_A);
    }
}
