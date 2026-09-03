#ifndef DRAW_H
#define DRAW_H
#include <stdint.h>

typedef struct { uint8_t r, g, b, a; } Color;
#define COLOR(r, g, b) ((Color){ (uint8_t)(r), (uint8_t)(g), (uint8_t)(b), 255 })

typedef struct {
    uint8_t *px;            /* RGBA, row-major, w*h*4 bytes */
    int w, h;
    int clip_x0, clip_y0;   /* inclusive */
    int clip_x1, clip_y1;   /* exclusive */
    int ox, oy;             /* translation added to draw coordinates */
} Framebuffer;

void  fb_init(Framebuffer *fb, uint8_t *px, int w, int h);
void  fb_set_view(Framebuffer *fb, int clip_x, int clip_y, int clip_w, int clip_h, int ox, int oy);
void  fb_reset_view(Framebuffer *fb);
Color fb_get(const Framebuffer *fb, int x, int y);        /* raw coords, no view */

void  draw_clear(Framebuffer *fb, Color c);                 /* whole buffer, no view */
void  draw_pixel(Framebuffer *fb, int x, int y, Color c);
void  draw_rect(Framebuffer *fb, int x, int y, int w, int h, Color c);
void  draw_dim(Framebuffer *fb, int x, int y, int w, int h, int amount); /* 0 none .. 255 black */

#endif
