#include "scene.h"

static void emit(Scene *sc, int type, int a, int b) {
    if (sc->world->events) events_push(sc->world->events, event_pack(STAGE_BUGS, type, a, b));
}

static void enter(Scene *sc, int id, int auto_flag) {
    sc->current = id; sc->current_auto = auto_flag;
    sc->table[id]->enter(sc->world);
}

static void leave(Scene *sc) {
    if (vignette_is_linked(sc->current)) emit(sc, EV_VIGNETTE_END, sc->current, 0);
    sc->table[sc->current]->exit(sc->world);
}

void scene_init(Scene *sc, World *w, const Vignette *const *table) {
    sc->table = table; sc->world = w;
    sc->pending = -1; sc->pending_auto = 0;
    sc->idle_timer = 0.0f; sc->last_auto = -1; sc->since_crossover = 0.0f;
    enter(sc, VIG_PATROL, 0);
}

void scene_request(Scene *sc, int id) {
    if (!vignette_is_linked(id)) return;
    sc->idle_timer = 0.0f;
    sc->pending = id; sc->pending_auto = 0;
}

static int pick_auto(Scene *sc) {
    int pick = VIG_RACE + rng_range(&sc->world->rng, 0, 2);
    if (pick == sc->last_auto) pick = VIG_RACE + ((pick - VIG_RACE + 1) % 3);
    return pick;
}

static int imbalance(const Scene *sc) {
    int side = world_hero_side(sc->world);
    if (side == SIDE_GAP) return 0;
    int near = world_bug_count(sc->world, side), far = world_bug_count(sc->world, 1 - side);
    return far - near >= SCENE_CROSSOVER_IMBALANCE;
}

void scene_step(Scene *sc, float dt) {
    World *w = sc->world;
    const Vignette *cur = sc->table[sc->current];
    cur->update(w, dt);
    sc->idle_timer += dt;
    sc->since_crossover += dt;

    if (cur->is_done(w)) {
        leave(sc);
        enter(sc, VIG_PATROL, 0);
        return;
    }
    if (sc->pending >= 0) {
        if (sc->pending == sc->current) { sc->pending = -1; }
        else if (sc->current != VIG_CROSSOVER && cur->at_beat_boundary(w)) {
            leave(sc);
            enter(sc, sc->pending, sc->pending_auto);
            emit(sc, EV_VIGNETTE_START, sc->current, sc->current_auto);
            sc->pending = -1;
            return;
        }
    }
    if (sc->current == VIG_PATROL && sc->pending < 0) {
        if (sc->idle_timer >= SCENE_IDLE_AUTOPLAY) {
            int pick = pick_auto(sc);
            sc->pending = pick; sc->pending_auto = 1; sc->last_auto = pick;
            sc->idle_timer = 0.0f;
        } else if (sc->since_crossover >= SCENE_CROSSOVER_INTERVAL || imbalance(sc)) {
            leave(sc);
            enter(sc, VIG_CROSSOVER, 0);
            sc->since_crossover = 0.0f;
        }
    }
}

void scene_draw_back(Scene *sc, Framebuffer *fb, const Color *pal) {
    const Vignette *cur = sc->table[sc->current];
    if (cur->draw_back) cur->draw_back(sc->world, fb, pal);
}

void scene_draw_front(Scene *sc, Framebuffer *fb, const Color *pal) {
    const Vignette *cur = sc->table[sc->current];
    if (cur->draw_front) cur->draw_front(sc->world, fb, pal);
}
