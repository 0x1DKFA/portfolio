#ifndef SPRITES_H
#define SPRITES_H
#include "draw.h"
#include "world.h"

enum { PROP_BOX = 0, PROP_PADLOCK, PROP_FOOTPRINT, PROP_HIDING_SPOT, PROP_SHIELD };
enum { BOX_NORMAL = 0, BOX_GLITCH = 1, BOX_LOCKED = 2, BOX_GREEN = 3 };

void sprite_bug(Framebuffer *fb, const Color *pal, const Bug *b);
void sprite_hero(Framebuffer *fb, const Color *pal, const Hero *h);
void sprite_fx(Framebuffer *fb, const Color *pal, const Fx *f);
void sprite_prop(Framebuffer *fb, const Color *pal, int kind, int x, int y, int state);
void sprite_glass(Framebuffer *fb, const Color *pal, int cx, int cy);

#endif
