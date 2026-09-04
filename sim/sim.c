#include "export.h"
#include "sim.h"
#include "stage.h"
#include "world.h"
#include "palette.h"

#define STATIC_SEED 7u
#define STATIC_WARMUP_STEPS 150      /* 2.5 s: LOOK is over and the walk has begun */

static uint8_t g_fb_px[WORLD_MAX_W * WORLD_MAX_H * 4];
static World g_world;
static EventQueue g_events;
static Stage g_stage;
static int g_ready;
static const char NEON_TEXT[] = "ROHIT";

static void hunt_stage_step(Stage *s, float dt) { world_step((World *)s->state, dt); }
static void hunt_stage_render(Stage *s) { world_render((World *)s->state, &s->fb); }

SIM_EXPORT("sim_init")
int sim_init(uint32_t seed, int w, int h) {
    if (w < WORLD_MIN_W || w > WORLD_MAX_W || h < WORLD_MIN_H || h > WORLD_MAX_H) return -1;
    g_ready = 0;
    events_init(&g_events);
    if (world_init(&g_world, seed, w, h, &g_events, NEON_TEXT) != 0) return -1;
    stage_init(&g_stage, g_fb_px, w, h, PALETTE_NIGHT, hunt_stage_step, hunt_stage_render, &g_world);
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

SIM_EXPORT("sim_poll_event")
uint32_t sim_poll_event(void) { return events_pop(&g_events); }

SIM_EXPORT("sim_render_static")
void sim_render_static(void) {
    if (!g_ready) return;
    int w = g_stage.fb.w, h = g_stage.fb.h;
    events_init(&g_events);
    world_init(&g_world, STATIC_SEED, w, h, &g_events, NEON_TEXT);
    for (int i = 0; i < STATIC_WARMUP_STEPS; i++) world_step(&g_world, STAGE_STEP);
    bug_set_state(&g_world.bug, &g_world.sprites, BUGSTATE_PEEK);
    events_init(&g_events);
    stage_render(&g_stage);
}
