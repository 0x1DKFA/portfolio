#ifndef BUG_H
#define BUG_H

#define BUG_W 10
#define BUG_H 8
#define BUG_WANDER_SPEED 6.0f
#define BUG_RUSH_SPEED 30.0f
#define BUG_SQUASH_TIME 0.4f
#define BUG_POOF_TIME 0.4f

enum { BUG_DEAD = 0, BUG_WANDER, BUG_RUSH, BUG_WAIT, BUG_QUEUE, BUG_SQUASHED, BUG_POOF };

typedef struct {
    int state, side, variant;
    float x, y;              /* feet position: bottom centre, world coords */
    float vx, vy;
    float timer;
    float target_x, target_y;
    int hidden;              /* drawn only while revealed */
    int revealed;
} Bug;

struct World;

void bug_spawn(Bug *b, int side, float x, float y, int variant);
void bug_set_target(Bug *b, float x, float y);          /* enters BUG_RUSH */
void bug_update(Bug *b, struct World *w, float dt);
int  bug_alive(const Bug *b);                            /* not DEAD, SQUASHED, or POOF */

#endif
