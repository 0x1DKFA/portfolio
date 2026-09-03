#include "sprites.h"
#include "trig.h"
#include "fmath.h"

void sprites_clear(Sprites *sp) { sp->n = 0; for (int i = 0; i < MAX_SPRITES; i++) sp->s[i].active = 0; }

Sprite *sprites_add(Sprites *sp, int kind, float x, float y, float size) {
    for (int i = 0; i < MAX_SPRITES; i++) {
        Sprite *s = &sp->s[i];
        if (s->active) continue;
        s->active = 1; s->kind = kind; s->x = x; s->y = y; s->size = size;
        if (i >= sp->n) sp->n = i + 1;
        return s;
    }
    return 0;
}

void sprites_remove(Sprites *sp, Sprite *s) { (void)sp; if (s) s->active = 0; }

int sprite_project(const Camera *cam, float proj, int w, int horizon, float sx, float sy, float size, float aspect, SpriteProj *out) {
    float ca = trig_cos(cam->angle), sa = trig_sin(cam->angle);
    float dx = sx - cam->x, dy = sy - cam->y;
    float depth = dx * ca + dy * sa;
    out->visible = 0;
    if (depth < SPRITE_MIN_DEPTH) return 0;
    float lateral = -dx * sa + dy * ca;
    out->depth = depth;
    out->screen_x = (int)((float)w * 0.5f + proj * lateral / depth);
    out->height = (int)(proj * size / depth);
    out->width = (int)((float)out->height * aspect);
    out->bottom = horizon + (int)(proj * 0.5f / depth);
    out->top = out->bottom - out->height;
    out->visible = out->height > 0 && out->width > 0;
    return out->visible;
}

static float sprite_shade(const Map *m, float depth, float sx, float sy) {
    float fog = 1.0f / (1.0f + 0.35f * depth);
    float light = map_light(m, (int)sx, (int)sy);
    return fog * (0.45f + 0.55f * light);
}

void sprites_draw(const Sprites *sp, const Camera *cam, const Map *m, const Textures *t, const Color *pal,
                  Framebuffer *fb, const float *depth, int horizon, float proj) {
    (void)pal;
    int order[MAX_SPRITES], n = 0;
    SpriteProj pr[MAX_SPRITES];
    for (int i = 0; i < sp->n; i++) {
        const Sprite *s = &sp->s[i];
        if (!s->active) continue;
        int sh = t->sprite_h[s->kind];
        if (!sprite_project(cam, proj, fb->w, horizon, s->x, s->y, s->size, (float)SPR_W / (float)sh, &pr[i])) continue;
        order[n++] = i;
    }
    for (int i = 1; i < n; i++) {                            /* insertion sort, far to near */
        int k = order[i], j = i - 1;
        while (j >= 0 && pr[order[j]].depth < pr[k].depth) { order[j + 1] = order[j]; j--; }
        order[j + 1] = k;
    }
    for (int oi = 0; oi < n; oi++) {
        const Sprite *s = &sp->s[order[oi]];
        const SpriteProj *p = &pr[order[oi]];
        int sh = t->sprite_h[s->kind];
        float shade = sprite_shade(m, p->depth, s->x, s->y);
        int x0 = p->screen_x - p->width / 2;
        for (int col = x0; col < x0 + p->width; col++) {
            if (col < 0 || col >= fb->w) continue;
            if (p->depth >= depth[col]) continue;
            int u = (col - x0) * SPR_W / p->width;
            int r0 = p->top < 0 ? 0 : p->top, r1 = p->bottom > fb->h ? fb->h : p->bottom;
            for (int row = r0; row < r1; row++) {
                int v = (row - p->top) * sh / p->height;
                Color c = texture_sprite(t, s->kind, u, v);
                if (c.a != 255) continue;
                c = draw_shade(c, shade);
                uint8_t *px = fb->px + ((row * fb->w) + col) * 4;
                px[0] = c.r; px[1] = c.g; px[2] = c.b; px[3] = 255;
            }
        }
    }
}
