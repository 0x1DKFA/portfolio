#include "vignettes/patrol.h"
#include "fmath.h"

#define PATROL_SPAWN_MIN 4.0f
#define PATROL_SPAWN_SPAN 4.0f
#define PATROL_REST_MIN 2.5f
#define PATROL_REST_SPAN 2.0f

static struct {
    float next_spawn[2];
    float rest;
    Bug *target;
} S;

static void enter(World *w) {
    for (int s = 0; s < 2; s++) S.next_spawn[s] = PATROL_SPAWN_MIN + rng_float(&w->rng) * PATROL_SPAWN_SPAN;
    S.rest = 0.5f;
    S.target = 0;
}

static void update(World *w, float dt) {
    for (int s = 0; s < 2; s++) {
        S.next_spawn[s] -= dt;
        if (S.next_spawn[s] <= 0.0f) {
            world_spawn_bug(w, s);
            S.next_spawn[s] = PATROL_SPAWN_MIN + rng_float(&w->rng) * PATROL_SPAWN_SPAN;
        }
    }
    Hero *h = &w->hero;
    if (h->action == HERO_HIDDEN || h->action == HERO_SIT) return;
    if (hero_busy(h)) {
        if (h->bonk_landed) {
            world_squash_near(w, h->x + (float)h->facing * 6.0f, h->y, HERO_REACH);
            S.rest = PATROL_REST_MIN + rng_float(&w->rng) * PATROL_REST_SPAN;
            S.target = 0;
        }
        return;
    }
    if (S.rest > 0.0f) { S.rest -= dt; return; }
    int side = world_hero_side(w);
    if (side == SIDE_GAP) return;
    if (!S.target || S.target->state != BUG_WANDER) S.target = world_nearest_bug(w, side, h->x, h->y);
    if (!S.target) return;
    float dx = S.target->x - h->x, dy = S.target->y - h->y;
    if (fm_abs(dx) <= HERO_REACH - 1.0f && fm_abs(dy) <= 3.0f) {
        h->facing = dx < 0 ? -1 : 1;
        hero_start_bonk(h);
    } else {
        hero_walk_to(h, S.target->x - (dx < 0 ? -6.0f : 6.0f), S.target->y);
    }
}

static int at_beat_boundary(const World *w) { return !hero_busy(&w->hero); }
static int is_done(const World *w) { (void)w; return 0; }
static void exit_(World *w) { (void)w; S.target = 0; }

const Vignette vignette_patrol = {
    VIG_PATROL, enter, update, 0, 0, at_beat_boundary, is_done, exit_
};
