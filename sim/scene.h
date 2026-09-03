#ifndef SCENE_H
#define SCENE_H
#include "vignette.h"

#define SCENE_IDLE_AUTOPLAY 45.0f
#define SCENE_CROSSOVER_INTERVAL 120.0f
#define SCENE_CROSSOVER_IMBALANCE 6

typedef struct {
    const Vignette *const *table;   /* VIG_COUNT entries indexed by id */
    World *world;
    int current, current_auto;
    int pending, pending_auto;      /* pending == -1 when none */
    float idle_timer;               /* seconds since the last external request */
    int last_auto;                  /* last auto-played id, -1 none */
    float since_crossover;
} Scene;

void scene_init(Scene *sc, World *w, const Vignette *const *table);
void scene_reset(Scene *sc);   /* leaves the current vignette (END event if linked, exit hook), clears any pending request, and enters patrol */
void scene_request(Scene *sc, int id);
void scene_step(Scene *sc, float dt);
void scene_draw_back(Scene *sc, Framebuffer *fb, const Color *pal);
void scene_draw_front(Scene *sc, Framebuffer *fb, const Color *pal);

#endif
