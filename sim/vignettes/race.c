#include "vignettes/race.h"
#include "sprites.h"
#include "font.h"
#include "fmath.h"
#include "palette.h"

static const float DURATION[] = { 1.0f, 1.5f, 2.0f, 2.0f, 3.0f, 0.5f };

static struct {
    int phase, boundary, side;
    float t, glitch_t;
    float box_x, box_y;
    int box_state, value, jitter;
    Bug *q[3];
    int q_started[3], q_counted[3];
} S;

int race_phase(void) { return S.phase; }
int race_box_state(void) { return S.box_state; }
int race_value(void) { return S.value; }

static float x0(const World *w) { return world_panel_x0(w, S.side); }

static void begin_phase(World *w, int phase) {
    S.phase = phase; S.t = 0.0f; S.boundary = 1;
    switch (phase) {
    case RACE_RUSH:
        S.q[0] = world_spawn_bug_at(w, S.side, x0(w) + 6.0f, S.box_y);
        S.q[1] = world_spawn_bug_at(w, S.side, x0(w) + (float)w->panel_w - 6.0f, S.box_y);
        if (S.q[0]) bug_set_target(S.q[0], S.box_x - 8.0f, S.box_y);
        if (S.q[1]) bug_set_target(S.q[1], S.box_x + 8.0f, S.box_y);
        break;
    case RACE_COLLIDE:
        S.box_state = BOX_GLITCH; S.glitch_t = 0.0f;
        break;
    case RACE_LOCK:
        hero_walk_to(&w->hero, S.box_x - 14.0f, S.box_y + 4.0f);
        /* the two racers back off into a queue line; a third joins */
        if (S.q[0]) bug_set_target(S.q[0], S.box_x - 20.0f, S.box_y);
        if (S.q[1]) bug_set_target(S.q[1], S.box_x - 30.0f, S.box_y);
        S.q[2] = world_spawn_bug_at(w, S.side, x0(w) + 6.0f, S.box_y);
        if (S.q[2]) bug_set_target(S.q[2], S.box_x - 40.0f, S.box_y);
        break;
    case RACE_QUEUE:
        S.box_state = BOX_LOCKED; S.value = 0; S.jitter = 0;
        for (int i = 0; i < 3; i++) { S.q_started[i] = 0; S.q_counted[i] = 0; }
        break;
    case RACE_RESOLVED:
        S.box_state = BOX_GREEN;
        break;
    default: break;
    }
}

static void enter(World *w) {
    S.side = world_hero_side(w);
    if (S.side == SIDE_GAP) S.side = SIDE_LEFT;
    S.box_x = world_panel_center_x(w, S.side);
    S.box_y = (float)((w->floor_top + w->floor_bottom) / 2);
    S.box_state = BOX_NORMAL; S.value = 0; S.jitter = 0;
    for (int i = 0; i < 3; i++) S.q[i] = 0;
    hero_walk_to(&w->hero, x0(w) + 16.0f, S.box_y + 12.0f);
    S.phase = RACE_SETUP; S.t = 0.0f; S.boundary = 0;
}

static void update(World *w, float dt) {
    S.boundary = 0;
    if (S.phase == RACE_DONE) return;
    S.t += dt;

    if (S.phase == RACE_COLLIDE) {
        S.glitch_t += dt;
        if (S.glitch_t >= 0.1f) { S.glitch_t = 0.0f; S.value = rng_range(&w->rng, 0, 9); S.jitter = rng_range(&w->rng, -1, 1); }
    }
    if (S.phase == RACE_QUEUE) {
        for (int i = 0; i < 3; i++) {
            Bug *b = S.q[i];
            if (!b) continue;
            /* a 0.3s settle beat, then one bug every 0.7s; each needs at most 31px / 30px/s, so all three finish inside the 3s phase */
            if (!S.q_started[i] && S.t >= 0.3f + (float)i * 0.7f) { S.q_started[i] = 1; bug_set_target(b, S.box_x - 9.0f, S.box_y); }
            if (S.q_started[i] && !S.q_counted[i] && b->state == BUG_WAIT && fm_abs(b->x - (S.box_x - 9.0f)) < 0.5f) {
                S.q_counted[i] = 1; S.value++;
                b->state = BUG_POOF; b->timer = BUG_POOF_TIME;
                world_spawn_fx(w, FX_POOF, b->x, b->y);
            }
        }
    }
    if (S.t >= DURATION[S.phase]) begin_phase(w, S.phase + 1);
}

static void draw_back(World *w, Framebuffer *fb, const Color *pal) {
    (void)w;
    if (S.phase >= RACE_DONE) return;
    int bx = (int)S.box_x + (S.box_state == BOX_GLITCH ? S.jitter : 0), by = (int)S.box_y;
    sprite_prop(fb, pal, PROP_BOX, bx, by, S.box_state);
    char digit[2] = { (char)('0' + S.value % 10), 0 };
    draw_text(fb, bx - 1, by - 7, digit, pal[S.box_state == BOX_GLITCH ? COL_RED : COL_TEXT]);
}

static int at_beat_boundary(const World *w) { (void)w; return S.boundary; }
static int is_done(const World *w) { (void)w; return S.phase == RACE_DONE; }
static void exit_(World *w) {
    world_clear_scripted_bugs(w);
    for (int i = 0; i < 3; i++) S.q[i] = 0;
}

const Vignette vignette_race = {
    VIG_RACE, enter, update, draw_back, 0, at_beat_boundary, is_done, exit_
};
