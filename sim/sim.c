#include "export.h"
#include "sim.h"
#include "stage.h"
#include "world.h"
#include "scene.h"
#include "sprites.h"
#include "palette.h"
#include "vignettes/table.h"

static uint8_t g_fb_px[2 * PANEL_W_MAX * PANEL_H_MAX * 4];
static uint8_t g_avatar_px[AVATAR_W * AVATAR_H * 4];
static World g_world;
static EventQueue g_events;
static Scene g_scene;
static Stage g_stage;
static int g_ready;

static void bugs_step(Stage *s, float dt) {
    World *w = (World *)s->state;
    world_update_actors(w, dt);
    scene_step(&g_scene, dt);
}

static void draw_panel(Stage *s, int side) {
    World *w = (World *)s->state;
    Framebuffer *fb = &s->fb;
    const Color *pal = s->palette;
    int x0 = (int)world_panel_x0(w, side);
    fb_set_view(fb, side * w->panel_w, 0, w->panel_w, w->panel_h, side * w->panel_w - x0, 0);

    draw_rect(fb, x0, 0, w->panel_w, w->panel_h, pal[COL_BG]);
    for (int y = 4; y < w->panel_h; y += 8)
        for (int x = 4; x < w->panel_w; x += 8) draw_pixel(fb, x0 + x, y, pal[COL_GRID]);
    draw_rect(fb, x0, w->floor_top, w->panel_w, w->floor_bottom - w->floor_top + 1, pal[COL_FLOOR]);
    draw_rect(fb, x0, w->floor_top, w->panel_w, 1, pal[COL_HORIZON]);
    draw_rect(fb, x0, w->floor_bottom + 1, w->panel_w, 1, pal[COL_HORIZON]);

    scene_draw_back(&g_scene, fb, pal);
    for (int i = 0; i < MAX_BUGS; i++)
        if (w->bugs[i].state != BUG_DEAD && w->bugs[i].side == side) sprite_bug(fb, pal, &w->bugs[i]);
    if (world_hero_side(w) == side) sprite_hero(fb, pal, &w->hero);
    for (int i = 0; i < MAX_FX; i++) if (w->fx[i].active) sprite_fx(fb, pal, &w->fx[i]);
    scene_draw_front(&g_scene, fb, pal);
    fb_reset_view(fb);
}

static void bugs_render(Stage *s) {
    draw_panel(s, SIDE_LEFT);
    draw_panel(s, SIDE_RIGHT);
}

SIM_EXPORT("sim_init")
int sim_init(uint32_t seed, int panel_w, int panel_h, int gap_w) {
    if (panel_w < 32 || panel_w > PANEL_W_MAX || panel_h < 64 || panel_h > PANEL_H_MAX || gap_w < 0 || gap_w > 4096)
        return -1;
    g_ready = 0;
    events_init(&g_events);
    world_init(&g_world, seed, panel_w, panel_h, gap_w, &g_events);
    for (int i = 0; i < 4; i++) { world_spawn_bug(&g_world, SIDE_LEFT); world_spawn_bug(&g_world, SIDE_RIGHT); }
    stage_init(&g_stage, g_fb_px, 2 * panel_w, panel_h, PALETTE_BUGS, bugs_step, bugs_render, &g_world);
    scene_init(&g_scene, &g_world, VIGNETTES);
    g_ready = 1;
    return 0;
}

SIM_EXPORT("sim_update")
void sim_update(int elapsed_ms) { if (g_ready) stage_update(&g_stage, elapsed_ms); }

SIM_EXPORT("sim_render")
void sim_render(void) { if (g_ready) stage_render(&g_stage); }

SIM_EXPORT("sim_framebuffer")
uint8_t *sim_framebuffer(void) { return g_fb_px; }

SIM_EXPORT("sim_framebuffer_len")
int sim_framebuffer_len(void) { return g_ready ? g_stage.fb.w * g_stage.fb.h * 4 : 0; }

SIM_EXPORT("sim_request")
void sim_request(int vignette_id) { if (g_ready) scene_request(&g_scene, vignette_id); }

SIM_EXPORT("sim_poll_event")
uint32_t sim_poll_event(void) { return events_pop(&g_events); }

SIM_EXPORT("sim_render_static")
void sim_render_static(void) {
    if (!g_ready) return;
    World *w = &g_world;
    scene_init(&g_scene, w, VIGNETTES);   /* back to a clean, undimmed patrol state before the preview draw */
    for (int i = 0; i < MAX_BUGS; i++) w->bugs[i].state = BUG_DEAD;
    for (int i = 0; i < MAX_FX; i++) w->fx[i].active = 0;
    int mid = (w->floor_top + w->floor_bottom) / 2;
    for (int side = 0; side < 2; side++) {
        float x0 = world_panel_x0(w, side);
        world_spawn_bug_at(w, side, x0 + 20.0f, (float)(w->floor_top + 10));
        world_spawn_bug_at(w, side, x0 + 76.0f, (float)mid);
        world_spawn_bug_at(w, side, x0 + 30.0f, (float)(w->floor_bottom - 2));
    }
    hero_init(&w->hero, world_panel_x0(w, SIDE_LEFT) + 44.0f, (float)(w->floor_bottom - 6));
    w->hero.action = HERO_BONK; w->hero.facing = 1;
    Bug *victim = world_spawn_bug_at(w, SIDE_LEFT, w->hero.x + 12.0f, w->hero.y);
    if (victim) { victim->state = BUG_SQUASHED; victim->timer = BUG_SQUASH_TIME; }
    Fx *dust = world_spawn_fx(w, FX_DUST, w->hero.x + 12.0f, w->hero.y);
    if (dust) dust->t = 0.15f;
    stage_render(&g_stage);
}

SIM_EXPORT("sim_render_avatar")
void sim_render_avatar(void) {
    Framebuffer fb;
    fb_init(&fb, g_avatar_px, AVATAR_W, AVATAR_H);
    draw_clear(&fb, PALETTE_BUGS[COL_BG]);
    Bug b; bug_spawn(&b, SIDE_LEFT, 19.0f, 22.0f, 0);
    Hero h; hero_init(&h, 8.0f, 22.0f); h.action = HERO_BONK; h.facing = 1;
    sprite_bug(&fb, PALETTE_BUGS, &b);
    sprite_hero(&fb, PALETTE_BUGS, &h);
}

SIM_EXPORT("sim_avatar_buffer")
uint8_t *sim_avatar_buffer(void) { return g_avatar_px; }

SIM_EXPORT("sim_avatar_len")
int sim_avatar_len(void) { return AVATAR_W * AVATAR_H * 4; }
