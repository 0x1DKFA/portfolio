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

#endif
