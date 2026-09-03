#include "vignettes/crossover.h"

static struct { int phase, from, to; float hidden_left; } S;

int crossover_phase(void) { return S.phase; }

static float exit_x(const World *w, int from) {
    return from == SIDE_LEFT ? (float)(w->panel_w + HERO_W / 2) : (float)(w->panel_w + w->gap_w - HERO_W / 2);
}
static float enter_x(const World *w, int to) {
    return to == SIDE_RIGHT ? (float)(w->panel_w + w->gap_w + HERO_W / 2) : (float)(w->panel_w - HERO_W / 2);
}
static float final_x(const World *w, int to) {
    return to == SIDE_RIGHT ? (float)(w->panel_w + w->gap_w + 14) : (float)(w->panel_w - 14);
}

static void enter(World *w) {
    S.from = world_hero_side(w);
    if (S.from == SIDE_GAP) S.from = SIDE_LEFT;
    S.to = 1 - S.from;
    S.phase = XO_WALK;
    S.hidden_left = 0.0f;
    hero_walk_to(&w->hero, exit_x(w, S.from), w->hero.y);
}

static void update(World *w, float dt) {
    Hero *h = &w->hero;
    switch (S.phase) {
    case XO_WALK:
        if (hero_arrived(h)) {
            hero_hide(h);
            float span = (float)(w->gap_w - HERO_W);
            S.hidden_left = span > 0 ? span / HERO_WALK_SPEED : 0.0f;
            S.phase = XO_HIDDEN;
        }
        break;
    case XO_HIDDEN:
        S.hidden_left -= dt;
        if (S.hidden_left <= 0.0f) {
            hero_show(h, enter_x(w, S.to), h->y);
            hero_walk_to(h, final_x(w, S.to), h->y);
            S.phase = XO_EMERGE;
        }
        break;
    case XO_EMERGE:
        if (hero_arrived(h)) S.phase = XO_DONE;
        break;
    default: break;
    }
}

static int at_beat_boundary(const World *w) { (void)w; return S.phase == XO_DONE; }
static int is_done(const World *w) { (void)w; return S.phase == XO_DONE; }
static void exit_(World *w) { (void)w; }

const Vignette vignette_crossover = {
    VIG_CROSSOVER, enter, update, 0, 0, at_beat_boundary, is_done, exit_
};
