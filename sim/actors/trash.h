#ifndef ACTORS_TRASH_H_HEADER
#define ACTORS_TRASH_H_HEADER
#include "map.h"
#include "sprites.h"
#include "rng.h"

#define TRASH_GUST_MIN 4.0f
#define TRASH_GUST_MAX 10.0f
#define TRASH_GUST_LEN 1.2f
#define TRASH_GUST_SPEED 2.0f
#define TRASH_FRICTION 3.0f

enum { LOOSE_PAPER = 0, LOOSE_NEWS = 1 };

typedef struct { float until_next; float left; int dir_sign; } Wind;
typedef struct { float x, y, vx, vy; int kind; float anim; Sprite *spr; } Loose;

void wind_init(Wind *w, Rng *rng);
void wind_step(Wind *w, Rng *rng, float dt);
int  wind_gusting(const Wind *w);
void loose_init(Loose *l, int kind, int tile_x, int tile_y, Sprite *spr);
void loose_step(Loose *l, const Map *m, const Wind *w, float dt);

#endif
