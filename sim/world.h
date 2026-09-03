#ifndef WORLD_H
#define WORLD_H
#include <stdint.h>
#include "rng.h"
#include "events.h"
#include "bug.h"
#include "hero.h"

#define PANEL_W_MAX 96
#define PANEL_H_MAX 320
#define MAX_BUGS_PER_SIDE 20
#define MAX_BUGS (2 * MAX_BUGS_PER_SIDE)
#define MAX_FX 16
#define FLOOR_TOP_FRAC 0.55f
#define FLOOR_BOTTOM_FRAC 0.90f

enum { SIDE_LEFT = 0, SIDE_RIGHT = 1, SIDE_GAP = -1 };
enum { FX_DUST = 0, FX_PLUS1 = 1, FX_POOF = 2 };

typedef struct { int active, kind; float x, y, t; } Fx;

typedef struct World {
    int panel_w, panel_h, gap_w, world_w;
    int floor_top, floor_bottom;          /* y range for actor feet */
    Rng rng;
    Bug bugs[MAX_BUGS];
    Hero hero;
    Fx fx[MAX_FX];
    EventQueue *events;
    int squashed_total;
    float time;
} World;

#define FX_DUST_TIME 0.4f
#define FX_PLUS1_TIME 0.8f
#define FX_POOF_TIME 0.4f

void  world_init(World *w, uint32_t seed, int panel_w, int panel_h, int gap_w, EventQueue *events);
int   world_side_of_x(const World *w, float x);
float world_panel_x0(const World *w, int side);
float world_panel_center_x(const World *w, int side);
int   world_hero_side(const World *w);
int   world_bug_count(const World *w, int side);
Bug  *world_spawn_bug(World *w, int side);
Bug  *world_spawn_bug_at(World *w, int side, float x, float y);
Bug  *world_nearest_bug(World *w, int side, float x, float y);
int   world_squash_near(World *w, float x, float y, float radius);
Fx   *world_spawn_fx(World *w, int kind, float x, float y);
void  world_clear_scripted_bugs(World *w);
void  world_update_actors(World *w, float dt);

#endif
