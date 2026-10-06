#include "hunt.h"
#include "world.h"
#include "trig.h"

static float wp_x(const Hunt *h, int i) { return (float)h->path.t[i].x + 0.5f; }
static float wp_y(const Hunt *h, int i) { return (float)h->path.t[i].y + 0.5f; }

static void begin(Hunt *h, int state) { h->state = state; h->t = 0.0f; }

static void end_inspect(World *w) {
    Hunt *h = &w->hunt; Camera *c = &w->cam;
    h->inspecting = 0; h->inspect_t = 0.0f; c->pitch_px = 0;
    if (h->walking_to >= 0) c->walking = 1;
}

static float next_inspect(World *w) {
    return HUNT_INSPECT_MEAN + (rng_float(&w->rng) * 2.0f - 1.0f) * HUNT_INSPECT_SPREAD;
}

static void new_trail(World *w) {
    Hunt *h = &w->hunt; Camera *c = &w->cam;
    Tile from = { (int)c->x, (int)c->y };
    Tile previous = h->hiding;
    int has_previous = h->cycles > 0;
    int found = 0, picked = 0;
    for (int attempt = 0; attempt < 8; attempt++) {
        if (!map_pick_hiding_spot(&w->map, &w->rng, from, HUNT_MIN_STEPS, HUNT_MAX_STEPS, &h->hiding)) break;
        picked = 1;
        if (!has_previous || h->hiding.x != previous.x || h->hiding.y != previous.y) { found = 1; break; }
    }
    if (!found && !picked) h->hiding = from;
    if (!map_bfs_random(&w->map, &w->rng, from, h->hiding, &h->path) || h->path.n < 1) { h->path.n = 1; h->path.t[0] = from; }
    decals_lay_trail(&w->decals, &h->path);
    decals_set_head_waypoint(&w->decals, 0);
    h->waypoint = 1; h->walking_to = -1;

    float bx = (float)h->hiding.x + 0.5f, by = (float)h->hiding.y + 0.5f;
    static const int DX[4] = { 1, -1, 0, 0 }, DY[4] = { 0, 0, 1, -1 };
    for (int k = 0; k < 4; k++) {
        int nx = h->hiding.x + DX[k], ny = h->hiding.y + DY[k];
        if (nx < 0 || ny < 0 || nx >= w->map.w || ny >= w->map.h) continue;
        int dumpster = w->map.cell[ny][nx] == CELL_DUMPSTER;
        int trash = w->map.cell[ny][nx] == CELL_FLOOR && w->map.prop[ny][nx] == PROP_TRASH;
        if (dumpster || trash) { bx += 0.3f * (float)DX[k]; by += 0.3f * (float)DY[k]; break; }
    }
    h->bug_x = bx; h->bug_y = by;
    if (w->bug.spr) bug_set_state(&w->bug, &w->sprites, BUGSTATE_DEAD);
    bug_place(&w->bug, &w->sprites, bx, by);

    h->inspect_timer = next_inspect(w);
    h->look_base = c->angle;
    camera_stop(c);
    end_inspect(w);
    begin(h, HUNT_LOOK);
}

void hunt_init(World *w) { w->hunt.cycles = 0; w->dust = 0; w->dust_t = 0.0f; new_trail(w); }

int hunt_state(const World *w) { return w->hunt.state; }
int hunt_cycles(const World *w) { return w->hunt.cycles; }

/* Advance along the path: register arrival, then start the next leg. */
static void follow_path(World *w) {
    Hunt *h = &w->hunt; Camera *c = &w->cam;
    if (c->walking) return;
    if (h->inspecting) return;
    if (h->walking_to >= 0 && !h->inspecting) {
        h->waypoint = h->walking_to + 1;
        decals_set_head_waypoint(&w->decals, h->walking_to);
        h->walking_to = -1;
    }
    if (h->waypoint < h->path.n) { camera_walk_to(c, wp_x(h, h->waypoint), wp_y(h, h->waypoint)); h->walking_to = h->waypoint; }
}

static void smash_hit(World *w) {
    Hunt *h = &w->hunt;
    if (w->dust) sprites_remove(&w->sprites, w->dust);
    w->dust = sprites_add(&w->sprites, SPR_DUST_A, h->bug_x, h->bug_y, 0.6f);
    w->dust_t = 0.0f;
    bug_set_state(&w->bug, &w->sprites, BUGSTATE_DEAD);
    hud_flash_plus(&w->hud);
    w->squashed_total++;
    if (w->events) events_push(w->events, event_pack(STAGE_BUGS, EV_BUG_SQUASHED, 0, 0));
}

void hunt_step(World *w, float dt) {
    Hunt *h = &w->hunt; Camera *c = &w->cam;
    h->t += dt;
    int hit = hud_step(&w->hud, dt);

    switch (h->state) {
    case HUNT_LOOK:
        if (h->t < HUNT_LOOK_TIME / 3.0f) camera_turn_to(c, h->look_base - HUNT_LOOK_SWEEP);
        else if (h->t < HUNT_LOOK_TIME * 2.0f / 3.0f) camera_turn_to(c, h->look_base + HUNT_LOOK_SWEEP);
        else if (h->path.n > 1) camera_face(c, wp_x(h, 1), wp_y(h, 1));
        if (h->t >= HUNT_LOOK_TIME) begin(h, HUNT_FOLLOW);
        break;

    case HUNT_FOLLOW:
        if (h->inspecting) {
            h->inspect_t += dt;
            c->pitch_px = (int)((float)HUNT_INSPECT_DIP * trig_sin(TRIG_PI * h->inspect_t / HUNT_INSPECT_TIME));
            if (h->inspect_t >= HUNT_INSPECT_TIME) end_inspect(w);
            break;
        }
        follow_path(w);
        h->inspect_timer -= dt;
        if (h->inspect_timer <= 0.0f && c->walking) {
            h->inspecting = 1; h->inspect_t = 0.0f; c->walking = 0;
            h->inspect_timer = next_inspect(w);
            break;
        }
        if (camera_dist(c, h->bug_x, h->bug_y) < HUNT_PEEK_DIST) {
            bug_set_state(&w->bug, &w->sprites, BUGSTATE_PEEK);
            begin(h, HUNT_APPROACH);
        }
        break;

    case HUNT_APPROACH: {
        if (h->inspecting) end_inspect(w);
        follow_path(w);
        if (h->waypoint >= h->path.n && !c->walking && camera_dist(c, h->bug_x, h->bug_y) >= HUNT_SMASH_DIST) {
            camera_walk_to(c, (float)h->hiding.x + 0.5f, (float)h->hiding.y + 0.5f);
            h->walking_to = h->path.n - 1;
        }
        camera_face(c, h->bug_x, h->bug_y);
        float dist = camera_dist(c, h->bug_x, h->bug_y);
        if (dist < HUNT_EXPOSE_DIST && w->bug.state == BUGSTATE_PEEK) bug_set_state(&w->bug, &w->sprites, BUGSTATE_EXPOSED);
        if (dist < HUNT_SMASH_DIST && camera_facing(c, HUNT_FACE_TOL)) {
            camera_stop(c); h->walking_to = -1;
            hud_start_swing(&w->hud);
            begin(h, HUNT_SMASH);
        }
        break;
    }

    case HUNT_SMASH:
        if (hit) smash_hit(w);
        if (h->t >= HUD_SWING_TIME) begin(h, HUNT_CELEBRATE);
        break;

    case HUNT_CELEBRATE:
        c->pitch_px = h->t < 0.5f ? -(int)((float)HUNT_HOP_PX * trig_sin(TRIG_PI * h->t / 0.5f)) : 0;
        if (h->t >= HUNT_CELEBRATE_TIME) {
            c->pitch_px = 0; h->cycles++;
            begin(h, HUNT_NEW_TRAIL);
            new_trail(w);        /* the cycle count and the fresh trail land on the same step */
        }
        break;

    case HUNT_NEW_TRAIL:
    default:
        new_trail(w);
        break;
    }

    decals_cull_behind(&w->decals, c->x, c->y, HUNT_CULL_BEHIND);
    if (w->dust) {
        w->dust_t += dt;
        w->dust->kind = w->dust_t < 0.25f ? SPR_DUST_A : SPR_DUST_B;
        if (w->dust_t >= HUNT_DUST_TIME) { sprites_remove(&w->sprites, w->dust); w->dust = 0; w->dust_t = 0.0f; }
    }
}
