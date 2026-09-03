#include "draw.h"

void fb_init(Framebuffer *fb, uint8_t *px, int w, int h) {
    fb->px = px; fb->w = w; fb->h = h;
    fb_reset_view(fb);
}

void fb_set_view(Framebuffer *fb, int clip_x, int clip_y, int clip_w, int clip_h, int ox, int oy) {
    fb->clip_x0 = clip_x < 0 ? 0 : clip_x;
    fb->clip_y0 = clip_y < 0 ? 0 : clip_y;
    fb->clip_x1 = clip_x + clip_w > fb->w ? fb->w : clip_x + clip_w;
    fb->clip_y1 = clip_y + clip_h > fb->h ? fb->h : clip_y + clip_h;
    fb->ox = ox; fb->oy = oy;
}

void fb_reset_view(Framebuffer *fb) { fb_set_view(fb, 0, 0, fb->w, fb->h, 0, 0); }

Color fb_get(const Framebuffer *fb, int x, int y) {
    const uint8_t *p = fb->px + ((y * fb->w) + x) * 4;
    return (Color){ p[0], p[1], p[2], p[3] };
}

static void put(Framebuffer *fb, int x, int y, Color c) {
    uint8_t *p = fb->px + ((y * fb->w) + x) * 4;
    p[0] = c.r; p[1] = c.g; p[2] = c.b; p[3] = c.a;
}

void draw_clear(Framebuffer *fb, Color c) {
    for (int y = 0; y < fb->h; y++)
        for (int x = 0; x < fb->w; x++) put(fb, x, y, c);
}

/* Translate and clip a rectangle; returns 0 when nothing is visible. */
static int clip_rect(const Framebuffer *fb, int *x, int *y, int *w, int *h) {
    int x0 = *x + fb->ox, y0 = *y + fb->oy, x1 = x0 + *w, y1 = y0 + *h;
    if (x0 < fb->clip_x0) x0 = fb->clip_x0;
    if (y0 < fb->clip_y0) y0 = fb->clip_y0;
    if (x1 > fb->clip_x1) x1 = fb->clip_x1;
    if (y1 > fb->clip_y1) y1 = fb->clip_y1;
    if (x1 <= x0 || y1 <= y0) return 0;
    *x = x0; *y = y0; *w = x1 - x0; *h = y1 - y0;
    return 1;
}

void draw_pixel(Framebuffer *fb, int x, int y, Color c) { draw_rect(fb, x, y, 1, 1, c); }

void draw_rect(Framebuffer *fb, int x, int y, int w, int h, Color c) {
    if (!clip_rect(fb, &x, &y, &w, &h)) return;
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++) put(fb, xx, yy, c);
}

void draw_dim(Framebuffer *fb, int x, int y, int w, int h, int amount) {
    if (!clip_rect(fb, &x, &y, &w, &h)) return;
    int keep = 255 - (amount < 0 ? 0 : amount > 255 ? 255 : amount);
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++) {
            uint8_t *p = fb->px + ((yy * fb->w) + xx) * 4;
            p[0] = (uint8_t)((p[0] * keep) / 255);
            p[1] = (uint8_t)((p[1] * keep) / 255);
            p[2] = (uint8_t)((p[2] * keep) / 255);
        }
}

Color draw_shade(Color c, float f) {
    if (f < 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;
    c.r = (uint8_t)((float)c.r * f);
    c.g = (uint8_t)((float)c.g * f);
    c.b = (uint8_t)((float)c.b * f);
    return c;
}
