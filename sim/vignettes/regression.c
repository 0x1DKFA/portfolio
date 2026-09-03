#include "vignettes/regression.h"
#include "sprites.h"
#include "fmath.h"

#define REG_MAX_SPAWN 8
#define REG_EPS 1e-4f   /* float32 dt accumulation can land a hair under an exact-frame duration */

static const float DURATION[] = { 1.5f, 1.5f, 1.0f, 1.0f, 2.0f, 1.0f };

static struct {
    int phase, boundary, side;
    float t;
    Bug *spawned[REG_MAX_SPAWN];
    int n_spawned;
    int swung;                 /* bonk already started in this phase */
    float shield_x, shield_y;
    int shield, bounced;
    Bug *bouncer;
} S;

int regression_phase(void) { return S.phase; }
int regression_shield(void) { return S.shield; }
int regression_bounced(void) { return S.bounced; }

static float clamp_x(const World *w, float x) {
    float x0 = world_panel_x0(w, S.side);
    return fm_clamp(x, x0 + BUG_W / 2, x0 + (float)w->panel_w - BUG_W / 2);
}

static Bug *spawn_still(World *w, float x, float y) {
    if (S.n_spawned >= REG_MAX_SPAWN) return 0;
    Bug *b = world_spawn_bug_at(w, S.side, clamp_x(w, x), y);
    if (!b) return 0;
    b->state = BUG_WAIT;
    S.spawned[S.n_spawned++] = b;
    return b;
}

static Bug *nearest_waiting(const World *w) {
    Bug *best = 0; float best_d = 0;
    for (int i = 0; i < S.n_spawned; i++) {
        Bug *b = S.spawned[i];
        if (!b || b->state != BUG_WAIT) continue;
        float dx = b->x - w->hero.x, dy = b->y - w->hero.y, d = dx * dx + dy * dy;
        if (!best || d < best_d) { best = b; best_d = d; }
    }
    return best;
}

/* Walk toward the nearest waiting bug and swing when in reach. */
static void hunt(World *w) {
    Hero *h = &w->hero;
    if (hero_busy(h) || S.swung) return;
    Bug *b = nearest_waiting(w);
    if (!b) return;
    float dx = b->x - h->x, dy = b->y - h->y;
    if (fm_abs(dx) <= HERO_REACH - 1.0f && fm_abs(dy) <= 3.0f) {
        h->facing = dx < 0 ? -1 : 1;
        hero_start_bonk(h);
        S.swung = 1;
    } else if (hero_arrived(h)) {
        hero_walk_to(h, b->x - (dx < 0 ? -6.0f : 6.0f), b->y);
    }
}

static void begin_phase(World *w, int phase) {
    S.phase = phase; S.t = 0.0f; S.swung = 0;
    Hero *h = &w->hero;
    switch (phase) {
    case REG_THINK:  hero_set_action(h, HERO_THINK); break;
    case REG_SHIELD: hero_set_action(h, HERO_IDLE); break;
    case REG_BOUNCE: {
        float from = clamp_x(w, S.shield_x + (float)h->facing * 50.0f);
        S.bouncer = spawn_still(w, from, S.shield_y);
        if (S.bouncer) bug_set_target(S.bouncer, S.shield_x + (float)h->facing * 5.0f, S.shield_y);
        break;
    }
    case REG_CLEAR:
        for (int i = 0; i < S.n_spawned; i++) {
            Bug *b = S.spawned[i];
            if (b && b->state == BUG_WAIT) { b->state = BUG_POOF; b->timer = BUG_POOF_TIME; world_spawn_fx(w, FX_POOF, b->x, b->y); }
        }
        break;
    case REG_DONE: S.boundary = 1; break;
    default: break;
    }
}

static void enter(World *w) {
    S.side = world_hero_side(w);
    if (S.side == SIDE_GAP) S.side = SIDE_LEFT;
    Hero *h = &w->hero;
    S.n_spawned = 0; S.shield = 0; S.bounced = 0; S.bouncer = 0; S.boundary = 0; S.swung = 0;
    hero_set_action(h, HERO_IDLE);
    spawn_still(w, h->x + (float)h->facing * 6.0f, h->y);
    S.phase = REG_BONK1; S.t = 0.0f;
}

static void update(World *w, float dt) {
    S.boundary = 0;
    if (S.phase == REG_DONE) return;
    S.t += dt;
    Hero *h = &w->hero;

    switch (S.phase) {
    case REG_BONK1:
        if (S.t >= 0.2f) hunt(w);
        if (h->bonk_landed) {
            float px = h->x + (float)h->facing * 6.0f;
            world_squash_near(w, px, h->y, HERO_REACH);
            spawn_still(w, px - 8.0f, h->y); spawn_still(w, px + 8.0f, h->y);
            S.boundary = 1;
        }
        break;
    case REG_BONK2:
        if (S.t >= 0.2f) hunt(w);
        if (h->bonk_landed) {
            float px = h->x + (float)h->facing * 6.0f;
            world_squash_near(w, px, h->y, HERO_REACH);
            spawn_still(w, px - 20.0f, h->y - 6.0f); spawn_still(w, px - 10.0f, h->y + 4.0f);
            spawn_still(w, px + 10.0f, h->y + 4.0f); spawn_still(w, px + 20.0f, h->y - 6.0f);
            S.boundary = 1;
        }
        break;
    case REG_SHIELD:
        if (!S.shield && S.t >= 0.5f) {
            S.shield_x = h->x + (float)h->facing * 12.0f; S.shield_y = h->y;
            S.shield = 1; S.boundary = 1;
        }
        break;
    case REG_BOUNCE:
        if (S.bouncer && !S.bounced && S.bouncer->state == BUG_WAIT) {
            S.bouncer->state = BUG_POOF; S.bouncer->timer = BUG_POOF_TIME;
            world_spawn_fx(w, FX_POOF, S.bouncer->x, S.bouncer->y);
            S.bounced = 1;
        }
        break;
    default: break;
    }
    if (S.t >= DURATION[S.phase] - REG_EPS) begin_phase(w, S.phase + 1);
}

static void draw_back(World *w, Framebuffer *fb, const Color *pal) {
    (void)w;
    if (S.shield && S.phase < REG_DONE) sprite_prop(fb, pal, PROP_SHIELD, (int)S.shield_x, (int)S.shield_y, 0);
}

static int at_beat_boundary(const World *w) { (void)w; return S.boundary; }
static int is_done(const World *w) { (void)w; return S.phase == REG_DONE; }
static void exit_(World *w) {
    if (w->hero.action == HERO_THINK) hero_set_action(&w->hero, HERO_IDLE);
    world_clear_scripted_bugs(w);
    S.shield = 0; S.n_spawned = 0; S.bouncer = 0;
}

const Vignette vignette_regression = {
    VIG_REGRESSION, enter, update, draw_back, 0, at_beat_boundary, is_done, exit_
};
