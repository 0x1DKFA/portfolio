#ifndef HUNT_H_HEADER
#define HUNT_H_HEADER
#include "map.h"

enum { HUNT_LOOK = 0, HUNT_FOLLOW, HUNT_APPROACH, HUNT_SMASH, HUNT_CELEBRATE, HUNT_NEW_TRAIL, HUNT_COUNT };

#define HUNT_LOOK_TIME 1.5f
#define HUNT_LOOK_SWEEP 0.35f
#define HUNT_INSPECT_MEAN 8.0f
#define HUNT_INSPECT_SPREAD 3.0f
#define HUNT_INSPECT_TIME 0.6f
#define HUNT_INSPECT_DIP 12
#define HUNT_PEEK_DIST 2.5f
#define HUNT_EXPOSE_DIST 1.5f
#define HUNT_SMASH_DIST 1.0f
#define HUNT_FACE_TOL 0.2f
#define HUNT_CELEBRATE_TIME 1.0f
#define HUNT_HOP_PX 8
#define HUNT_MIN_STEPS 36
#define HUNT_MAX_STEPS 76
#define HUNT_CULL_BEHIND 6.0f
#define HUNT_DUST_TIME 0.5f

typedef struct {
    int   state;
    float t;
    Path  path;
    int   waypoint;       /* next waypoint to walk to */
    int   walking_to;     /* waypoint currently being walked to, -1 none */
    Tile  hiding;
    float bug_x, bug_y;
    float inspect_timer, inspect_t;
    int   inspecting;
    float look_base;
    int   cycles;
} Hunt;

struct World;
void hunt_init(struct World *w);
void hunt_step(struct World *w, float dt);
int  hunt_state(const struct World *w);
int  hunt_cycles(const struct World *w);

#endif
