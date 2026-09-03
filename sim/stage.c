#include "stage.h"

void stage_init(Stage *s, uint8_t *px, int w, int h, const Color *palette,
                void (*step)(Stage *, float), void (*render)(Stage *), void *state) {
    fb_init(&s->fb, px, w, h);
    s->accum = 0.0f; s->palette = palette; s->step = step; s->render = render; s->state = state;
}

int stage_update(Stage *s, int elapsed_ms) {
    if (elapsed_ms <= 0) return 0;
    float dt = (float)elapsed_ms / 1000.0f;
    if (dt > STAGE_MAX_DT) dt = STAGE_MAX_DT;
    s->accum += dt;
    int n = 0;
    while (s->accum >= STAGE_STEP) {
        s->step(s, STAGE_STEP);
        s->accum -= STAGE_STEP;
        n++;
    }
    return n;
}

void stage_render(Stage *s) { s->render(s); }
