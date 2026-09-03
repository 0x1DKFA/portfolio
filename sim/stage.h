#ifndef STAGE_H
#define STAGE_H
#include <stdint.h>
#include "draw.h"

#define STAGE_STEP (1.0f / 60.0f)
#define STAGE_MAX_DT 0.25f

typedef struct Stage Stage;
struct Stage {
    Framebuffer fb;
    float accum;
    const Color *palette;
    void (*step)(Stage *s, float dt);
    void (*render)(Stage *s);
    void *state;
};

void stage_init(Stage *s, uint8_t *px, int w, int h, const Color *palette,
                void (*step)(Stage *, float), void (*render)(Stage *), void *state);
int  stage_update(Stage *s, int elapsed_ms);
void stage_render(Stage *s);

#endif
