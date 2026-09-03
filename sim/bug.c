#include "bug.h"
#include "world.h"
#include "fmath.h"

static float panel_x0(const World *w, int side) { return side == SIDE_RIGHT ? (float)(w->panel_w + w->gap_w) : 0.0f; }

void bug_spawn(Bug *b, int side, float x, float y, int variant) {
    b->state = BUG_WANDER; b->side = side; b->variant = variant;
    b->x = x; b->y = y; b->vx = 0; b->vy = 0; b->timer = 0;
    b->target_x = x; b->target_y = y; b->hidden = 0; b->revealed = 0;
}

void bug_set_target(Bug *b, float x, float y) {
    b->target_x = x; b->target_y = y; b->state = BUG_RUSH;
}

int bug_alive(const Bug *b) {
    return b->state != BUG_DEAD && b->state != BUG_SQUASHED && b->state != BUG_POOF;
}

static void wander(Bug *b, World *w, float dt) {
    b->timer -= dt;
    if (b->timer <= 0.0f) {
        b->vx = (float)rng_range(&w->rng, -6, 6);
        b->vy = (float)rng_range(&w->rng, -3, 3);
        b->timer = 1.0f + rng_float(&w->rng);
    }
    b->x += b->vx * dt;
    b->y += b->vy * dt;
    float x0 = panel_x0(w, b->side) + BUG_W / 2, x1 = panel_x0(w, b->side) + w->panel_w - BUG_W / 2;
    if (b->x < x0) { b->x = x0; b->vx = -b->vx; }
    if (b->x > x1) { b->x = x1; b->vx = -b->vx; }
    if (b->y < (float)w->floor_top)    { b->y = (float)w->floor_top;    b->vy = -b->vy; }
    if (b->y > (float)w->floor_bottom) { b->y = (float)w->floor_bottom; b->vy = -b->vy; }
}

static void rush(Bug *b, float dt) {
    float dx = b->target_x - b->x, dy = b->target_y - b->y;
    float dist = fm_sqrt(dx * dx + dy * dy);
    float step = BUG_RUSH_SPEED * dt;
    if (dist <= step || dist < 0.001f) { b->x = b->target_x; b->y = b->target_y; b->state = BUG_WAIT; return; }
    b->x += dx / dist * step;
    b->y += dy / dist * step;
}

void bug_update(Bug *b, World *w, float dt) {
    switch (b->state) {
    case BUG_WANDER: wander(b, w, dt); break;
    case BUG_RUSH:   rush(b, dt); break;
    case BUG_SQUASHED:
    case BUG_POOF:
        b->timer -= dt;
        if (b->timer <= 0.0f) b->state = BUG_DEAD;
        break;
    default: break;                       /* DEAD, WAIT, QUEUE hold still */
    }
}
