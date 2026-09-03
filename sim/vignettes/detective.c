#include "vignettes/detective.h"
#include "sprites.h"
#include "fmath.h"

#define DET_MAX_DIM 140
#define DET_PRINTS 5
#define DET_GLASS_RADIUS 7.0f

static const float DURATION[] = { 0.5f, 3.0f, 1.0f, 3.0f, 1.5f, 0.5f, 0.5f };

static struct {
    int phase, boundary, side;
    float t;
    int dim;
    float print_x[DET_PRINTS], print_y[DET_PRINTS];
    int n_prints, follow_idx;
    float spot_x, spot_y;
    Bug *hidden;
} S;

int detective_phase(void) { return S.phase; }
int detective_dim(void) { return S.dim; }
int detective_footprints(void) { return S.n_prints; }

static void aim_glass(Hero *h) {
    h->glass_x = h->x + (float)h->facing * 8.0f;
    h->glass_y = h->y - 8.0f;
}

static int near_glass(const Hero *h, float x, float y) {
    float dx = h->glass_x - x, dy = h->glass_y - y;
    return dx * dx + dy * dy <= DET_GLASS_RADIUS * DET_GLASS_RADIUS;
}

static void begin_phase(World *w, int phase) {
    S.phase = phase; S.t = 0.0f;
    Hero *h = &w->hero;
    switch (phase) {
    case DET_GEAR:   h->detective = 1; aim_glass(h); break;
    case DET_FOLLOW: S.follow_idx = 0; hero_walk_to(h, S.print_x[0], S.print_y[0] + 4.0f); break;
    case DET_REVEAL: S.boundary = 1; hero_walk_to(h, S.spot_x - 10.0f, S.spot_y); break;
    case DET_BONK:   h->facing = 1; hero_start_bonk(h); break;
    case DET_UNDIM:  S.boundary = 1; break;
    case DET_DONE:   S.boundary = 1; h->detective = 0; break;
    default: break;
    }
}

static void enter(World *w) {
    S.side = world_hero_side(w);
    if (S.side == SIDE_GAP) S.side = SIDE_LEFT;
    float x0 = world_panel_x0(w, S.side);
    S.spot_x = x0 + (float)w->panel_w - 16.0f;
    S.spot_y = (float)(w->floor_bottom - 4);
    S.hidden = world_spawn_bug_at(w, S.side, S.spot_x, S.spot_y);
    if (S.hidden) { S.hidden->hidden = 1; S.hidden->revealed = 0; S.hidden->state = BUG_WAIT; }
    float start_x = x0 + 16.0f, end_x = S.spot_x - 12.0f;
    for (int i = 0; i < DET_PRINTS; i++) {
        float f = (float)i / (float)(DET_PRINTS - 1);
        S.print_x[i] = start_x + (end_x - start_x) * f;
        S.print_y[i] = (float)(w->floor_bottom - 10) + 6.0f * f + (float)((i % 2) ? 3 : -3);
    }
    S.n_prints = 0; S.follow_idx = 0; S.dim = 0; S.boundary = 0;
    w->hero.detective = 0;
    hero_walk_to(&w->hero, x0 + 12.0f, (float)(w->floor_bottom - 6));
    S.phase = DET_DIM; S.t = 0.0f;
}

static void update(World *w, float dt) {
    S.boundary = 0;
    if (S.phase == DET_DONE) return;
    S.t += dt;
    Hero *h = &w->hero;

    switch (S.phase) {
    case DET_DIM:
        S.dim = (int)(DET_MAX_DIM * fm_clamp(S.t / DURATION[DET_DIM], 0.0f, 1.0f));
        break;
    case DET_FOOTPRINTS:
        if (S.n_prints < DET_PRINTS && S.t >= 0.6f * (float)(S.n_prints + 1)) { S.n_prints++; S.boundary = 1; }
        break;
    case DET_FOLLOW:
        aim_glass(h);
        if (hero_arrived(h) && S.follow_idx < DET_PRINTS - 1) {
            S.follow_idx++;
            hero_walk_to(h, S.print_x[S.follow_idx], S.print_y[S.follow_idx] + 4.0f);
        }
        break;
    case DET_REVEAL:
        if (hero_arrived(h)) { h->facing = 1; h->glass_x = S.spot_x; h->glass_y = S.spot_y - 4.0f; }
        else aim_glass(h);
        break;
    case DET_BONK:
        if (h->bonk_landed) world_squash_near(w, S.spot_x, S.spot_y, 10.0f);
        break;
    case DET_UNDIM:
        S.dim = (int)(DET_MAX_DIM * (1.0f - fm_clamp(S.t / DURATION[DET_UNDIM], 0.0f, 1.0f)));
        break;
    default: break;
    }
    if (S.hidden && S.hidden->state != BUG_DEAD)
        S.hidden->revealed = h->detective && near_glass(h, S.hidden->x, S.hidden->y - 4.0f);
    if (S.phase == DET_FOOTPRINTS) { if (S.n_prints == DET_PRINTS && S.t >= DURATION[S.phase]) begin_phase(w, S.phase + 1); }
    else if (S.t >= DURATION[S.phase]) begin_phase(w, S.phase + 1);
    if (S.phase == DET_DONE) S.dim = 0;
}

static void draw_prints(World *w, Framebuffer *fb, const Color *pal, int only_glowing) {
    for (int i = 0; i < S.n_prints; i++) {
        int glow = w->hero.detective && near_glass(&w->hero, S.print_x[i], S.print_y[i]);
        if (only_glowing && !glow) continue;
        sprite_prop(fb, pal, PROP_FOOTPRINT, (int)S.print_x[i], (int)S.print_y[i], glow);
    }
}

static void draw_back(World *w, Framebuffer *fb, const Color *pal) {
    if (S.phase >= DET_DONE) return;
    sprite_prop(fb, pal, PROP_HIDING_SPOT, (int)S.spot_x + 6, (int)S.spot_y, 0);
    draw_prints(w, fb, pal, 0);
}

static void draw_front(World *w, Framebuffer *fb, const Color *pal) {
    if (S.dim <= 0) return;
    draw_dim(fb, (int)world_panel_x0(w, S.side), 0, w->panel_w, w->panel_h, S.dim);
    if (w->hero.detective) {
        sprite_glass(fb, pal, (int)w->hero.glass_x, (int)w->hero.glass_y);
        draw_prints(w, fb, pal, 1);
        if (S.hidden && S.hidden->revealed) sprite_bug(fb, pal, S.hidden);
    }
}

static int at_beat_boundary(const World *w) { (void)w; return S.boundary; }
static int is_done(const World *w) { (void)w; return S.phase == DET_DONE; }
static void exit_(World *w) {
    w->hero.detective = 0;
    S.dim = 0;
    world_clear_scripted_bugs(w);
    S.hidden = 0;
}

const Vignette vignette_detective = {
    VIG_DETECTIVE, enter, update, draw_back, draw_front, at_beat_boundary, is_done, exit_
};
