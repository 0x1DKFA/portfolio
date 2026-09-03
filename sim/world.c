#include "world.h"
#include "fmath.h"

void world_init(World *w, uint32_t seed, int panel_w, int panel_h, int gap_w, EventQueue *events) {
    w->panel_w = panel_w; w->panel_h = panel_h; w->gap_w = gap_w;
    w->world_w = 2 * panel_w + gap_w;
    w->floor_top = (int)((float)panel_h * FLOOR_TOP_FRAC);
    w->floor_bottom = (int)((float)panel_h * FLOOR_BOTTOM_FRAC);
    rng_seed(&w->rng, seed);
    for (int i = 0; i < MAX_BUGS; i++) w->bugs[i].state = BUG_DEAD;
    for (int i = 0; i < MAX_FX; i++) w->fx[i].active = 0;
    hero_init(&w->hero, world_panel_center_x(w, SIDE_LEFT), (float)(w->floor_bottom - 6));
    w->events = events; w->squashed_total = 0; w->time = 0.0f;
}

int world_side_of_x(const World *w, float x) {
    if (x < (float)w->panel_w) return SIDE_LEFT;
    if (x >= (float)(w->panel_w + w->gap_w)) return SIDE_RIGHT;
    return SIDE_GAP;
}

float world_panel_x0(const World *w, int side) { return side == SIDE_RIGHT ? (float)(w->panel_w + w->gap_w) : 0.0f; }

float world_panel_center_x(const World *w, int side) { return world_panel_x0(w, side) + (float)w->panel_w * 0.5f; }

int world_hero_side(const World *w) {
    if (w->hero.action == HERO_HIDDEN) return SIDE_GAP;
    return world_side_of_x(w, w->hero.x);
}

int world_bug_count(const World *w, int side) {
    int n = 0;
    for (int i = 0; i < MAX_BUGS; i++) if (bug_alive(&w->bugs[i]) && w->bugs[i].side == side) n++;
    return n;
}

static Bug *free_slot(World *w) {
    for (int i = 0; i < MAX_BUGS; i++) if (w->bugs[i].state == BUG_DEAD) return &w->bugs[i];
    return 0;
}

Bug *world_spawn_bug_at(World *w, int side, float x, float y) {
    if (world_bug_count(w, side) >= MAX_BUGS_PER_SIDE) return 0;
    Bug *b = free_slot(w);
    if (!b) return 0;
    bug_spawn(b, side, x, y, rng_range(&w->rng, 0, 2));
    return b;
}

Bug *world_spawn_bug(World *w, int side) {
    float x0 = world_panel_x0(w, side);
    float x = rng_range(&w->rng, 0, 1) ? x0 + BUG_W / 2 : x0 + (float)w->panel_w - BUG_W / 2;
    float y = (float)rng_range(&w->rng, w->floor_top, w->floor_bottom);
    return world_spawn_bug_at(w, side, x, y);
}

Bug *world_nearest_bug(World *w, int side, float x, float y) {
    Bug *best = 0; float best_d = 0;
    for (int i = 0; i < MAX_BUGS; i++) {
        Bug *b = &w->bugs[i];
        if (b->state != BUG_WANDER || b->side != side) continue;
        float dx = b->x - x, dy = b->y - y, d = dx * dx + dy * dy;
        if (!best || d < best_d) { best = b; best_d = d; }
    }
    return best;
}

Fx *world_spawn_fx(World *w, int kind, float x, float y) {
    for (int i = 0; i < MAX_FX; i++) {
        Fx *f = &w->fx[i];
        if (f->active) continue;
        f->active = 1; f->kind = kind; f->x = x; f->y = y; f->t = 0;
        return f;
    }
    return 0;
}

int world_squash_near(World *w, float x, float y, float radius) {
    int n = 0;
    for (int i = 0; i < MAX_BUGS; i++) {
        Bug *b = &w->bugs[i];
        if (!bug_alive(b)) continue;
        float dx = b->x - x, dy = b->y - y;
        if (dx * dx + dy * dy > radius * radius) continue;
        b->state = BUG_SQUASHED; b->timer = BUG_SQUASH_TIME;
        world_spawn_fx(w, FX_DUST, b->x, b->y);
        world_spawn_fx(w, FX_PLUS1, b->x, b->y - BUG_H);
        if (w->events) events_push(w->events, event_pack(STAGE_BUGS, EV_BUG_SQUASHED, 0, 0));
        w->squashed_total++;
        n++;
    }
    return n;
}

void world_clear_scripted_bugs(World *w) {
    for (int i = 0; i < MAX_BUGS; i++) {
        int s = w->bugs[i].state;
        if (s == BUG_RUSH || s == BUG_WAIT || s == BUG_QUEUE) w->bugs[i].state = BUG_DEAD;
    }
}

static float fx_lifetime(int kind) {
    return kind == FX_PLUS1 ? FX_PLUS1_TIME : kind == FX_POOF ? FX_POOF_TIME : FX_DUST_TIME;
}

void world_update_actors(World *w, float dt) {
    w->time += dt;
    for (int i = 0; i < MAX_BUGS; i++) if (w->bugs[i].state != BUG_DEAD) bug_update(&w->bugs[i], w, dt);
    hero_update(&w->hero, dt);
    for (int i = 0; i < MAX_FX; i++) {
        Fx *f = &w->fx[i];
        if (!f->active) continue;
        f->t += dt;
        if (f->t >= fx_lifetime(f->kind)) f->active = 0;
    }
}
