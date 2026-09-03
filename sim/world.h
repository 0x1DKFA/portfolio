#ifndef WORLD_H_HEADER
#define WORLD_H_HEADER
#include <stdint.h>
#include "rng.h"
#include "events.h"
#include "map.h"
#include "textures.h"
#include "camera.h"
#include "decals.h"
#include "sprites.h"
#include "hud.h"
#include "hunt.h"
#include "actors/dog.h"
#include "actors/trash.h"
#include "actors/bug.h"

#define MAX_DOGS 4
#define MAX_LOOSE 16
#define WORLD_MAX_W 640
#define WORLD_MAX_H 320
#define WORLD_MIN_W 200
#define WORLD_MIN_H 160

typedef struct World {
    int w, h;
    Map map;
    Textures tex;
    const Color *pal;
    Camera cam;
    Decals decals;
    Sprites sprites;
    Hud hud;
    Hunt hunt;
    Dog dogs[MAX_DOGS]; int n_dogs;
    Loose loose[MAX_LOOSE]; int n_loose;
    Wind wind;
    HiddenBug bug;
    Sprite *dust; float dust_t;
    Rng rng;
    EventQueue *events;
    float time;
    int squashed_total;
    float depth[WORLD_MAX_W];
} World;

int  world_init(World *w, uint32_t seed, int width, int height, EventQueue *events, const char *neon_text);
void world_step(World *w, float dt);
int  world_horizon(const World *w);
void world_render(World *w, Framebuffer *fb);

#endif
