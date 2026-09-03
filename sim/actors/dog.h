#ifndef ACTORS_DOG_H_HEADER
#define ACTORS_DOG_H_HEADER
#include "map.h"
#include "decals.h"
#include "sprites.h"
#include "rng.h"

#define DOG_SPEED 1.0f
#define DOG_SNIFF_MIN 1.0f
#define DOG_SNIFF_MAX 2.0f
#define DOG_SNIFF_CHANCE 0.25f

enum { DOG_WALK = 0, DOG_SNIFF = 1 };

typedef struct { float x, y; int dir; int state; float t; float anim; int tx, ty; Sprite *spr; } Dog;

void dog_init(Dog *d, int tile_x, int tile_y, Sprite *spr);
void dog_step(Dog *d, const Map *m, const Decals *decals, Rng *rng, float dt);

#endif
