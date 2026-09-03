#ifndef VIGNETTE_H
#define VIGNETTE_H
#include "world.h"
#include "draw.h"

enum { VIG_PATROL = 0, VIG_RACE = 1, VIG_DETECTIVE = 2, VIG_REGRESSION = 3, VIG_CROSSOVER = 4, VIG_COUNT = 5 };

typedef struct {
    int id;
    void (*enter)(World *w);
    void (*update)(World *w, float dt);                                  /* runs after world_update_actors */
    void (*draw_back)(World *w, Framebuffer *fb, const Color *pal);      /* before actors; may be NULL */
    void (*draw_front)(World *w, Framebuffer *fb, const Color *pal);     /* after actors; may be NULL */
    int  (*at_beat_boundary)(const World *w);
    int  (*is_done)(const World *w);
    void (*exit)(World *w);
} Vignette;

static inline int vignette_is_linked(int id) { return id >= VIG_RACE && id <= VIG_REGRESSION; }

#endif
