#include "world.h"
#include "palette.h"
#include "raycast.h"

static const int DX[4] = { 1, -1, 0, 0 }, DY[4] = { 0, 0, 1, -1 };

/* Nudge a prop from the tile centre toward an adjacent wall so the camera does not walk through it. */
static void prop_position(const Map *m, int x, int y, float *px, float *py) {
    *px = (float)x + 0.5f; *py = (float)y + 0.5f;
    for (int k = 0; k < 4; k++) {
        if (!map_is_floor(m, x + DX[k], y + DY[k])) { *px += 0.35f * (float)DX[k]; *py += 0.35f * (float)DY[k]; return; }
    }
}

int world_init(World *w, uint32_t seed, int width, int height, EventQueue *events, const char *neon_text) {
    if (width < WORLD_MIN_W || width > WORLD_MAX_W || height < WORLD_MIN_H || height > WORLD_MAX_H) return -1;
    w->w = width; w->h = height; w->events = events; w->time = 0.0f; w->squashed_total = 0;
    w->pal = PALETTE_NIGHT;
    rng_seed(&w->rng, seed);
    if (map_parse(&w->map, CITY_MAP, MAP_W, MAP_H) != 0) return -1;
    textures_generate(&w->tex, &w->rng, w->pal, neon_text);
    camera_init(&w->cam, (float)w->map.start_x + 0.5f, (float)w->map.start_y + 0.5f, 0.0f);
    sprites_clear(&w->sprites);
    decals_clear(&w->decals);
    hud_init(&w->hud);
    wind_init(&w->wind, &w->rng);
    w->n_dogs = 0; w->n_loose = 0; w->bug.spr = 0; w->bug.state = BUGSTATE_DEAD; w->dust = 0; w->dust_t = 0.0f;

    for (int i = 0; i < w->map.n_spawns; i++) {
        const Spawn *s = &w->map.spawns[i];
        float px, py;
        switch (s->kind) {
        case PROP_LAMP:  prop_position(&w->map, s->x, s->y, &px, &py); sprites_add(&w->sprites, SPR_LAMP, px, py, 2.2f); break;
        case PROP_TRASH: prop_position(&w->map, s->x, s->y, &px, &py); sprites_add(&w->sprites, SPR_TRASHCAN, px, py, 0.8f); break;
        case PROP_PAPER:
        case PROP_NEWSPAPER:
            if (w->n_loose < MAX_LOOSE) {
                int kind = s->kind == PROP_PAPER ? LOOSE_PAPER : LOOSE_NEWS;
                Sprite *sp = sprites_add(&w->sprites, kind == LOOSE_PAPER ? SPR_PAPER_A : SPR_NEWS_A, (float)s->x + 0.5f, (float)s->y + 0.5f, kind == LOOSE_PAPER ? 0.3f : 0.4f);
                loose_init(&w->loose[w->n_loose++], kind, s->x, s->y, sp);
            }
            break;
        case PROP_DOG:
            if (w->n_dogs < MAX_DOGS) {
                Sprite *sp = sprites_add(&w->sprites, SPR_DOG_A, (float)s->x + 0.5f, (float)s->y + 0.5f, 0.6f);
                dog_init(&w->dogs[w->n_dogs++], s->x, s->y, sp);
            }
            break;
        default: break;
        }
    }
    hunt_init(w);
    return 0;
}

void world_step(World *w, float dt) {
    wind_step(&w->wind, &w->rng, dt);
    for (int i = 0; i < w->n_loose; i++) loose_step(&w->loose[i], &w->map, &w->wind, dt);
    for (int i = 0; i < w->n_dogs; i++) dog_step(&w->dogs[i], &w->map, &w->decals, &w->rng, dt);
    hunt_step(w, dt);
    camera_step(&w->cam, dt);
    w->time += dt;
}

int world_horizon(const World *w) {
    int hz = w->h / 2 + w->cam.pitch_px + camera_bob_px(&w->cam);
    int lo = w->h / 4, hi = 3 * w->h / 4;
    return hz < lo ? lo : hz > hi ? hi : hz;
}

void world_render(World *w, Framebuffer *fb) {
    int horizon = world_horizon(w);
    float proj = camera_proj(fb->w);
    raycast_sky(&w->tex, &w->cam, proj, fb, horizon);
    raycast_walls(&w->map, &w->tex, w->pal, &w->cam, proj, fb, horizon, w->depth);
    raycast_floor(&w->map, &w->tex, w->pal, &w->decals, &w->cam, proj, fb, horizon, w->depth);
    sprites_draw(&w->sprites, &w->cam, &w->map, &w->tex, w->pal, fb, w->depth, horizon, proj);
    hud_draw(&w->hud, &w->tex, w->pal, fb, camera_bob_px(&w->cam));
}
