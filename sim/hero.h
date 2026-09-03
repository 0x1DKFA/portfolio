#ifndef HERO_H
#define HERO_H

#define HERO_W 14
#define HERO_H 20
#define HERO_WALK_SPEED 24.0f
#define HERO_WINDUP_TIME 0.25f
#define HERO_BONK_TIME 0.15f
#define HERO_REACH 8.0f

enum { HERO_IDLE = 0, HERO_WALK, HERO_WINDUP, HERO_BONK, HERO_HIDDEN, HERO_SIT, HERO_THINK };

typedef struct {
    float x, y;                 /* feet position, world coords */
    int facing;                 /* +1 right, -1 left */
    int action;
    float timer;
    float target_x, target_y;
    int has_target;
    int detective;              /* draws hat and glass */
    float glass_x, glass_y;     /* magnifying glass centre, world coords */
    int bonk_landed;            /* 1 only on the step the swing lands */
} Hero;

void hero_init(Hero *h, float x, float y);
void hero_walk_to(Hero *h, float x, float y);
int  hero_arrived(const Hero *h);
void hero_start_bonk(Hero *h);
int  hero_busy(const Hero *h);              /* WINDUP or BONK */
void hero_hide(Hero *h);
void hero_show(Hero *h, float x, float y);
void hero_set_action(Hero *h, int action);  /* IDLE, SIT, THINK */
void hero_update(Hero *h, float dt);

#endif
