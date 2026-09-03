#ifndef SPRITES_H_HEADER
#define SPRITES_H_HEADER
#include "draw.h"
#include "camera.h"
#include "map.h"
#include "textures.h"

#define MAX_SPRITES 64
#define SPRITE_MIN_DEPTH 0.15f

typedef struct { int active; float x, y; int kind; float size; } Sprite;
typedef struct { Sprite s[MAX_SPRITES]; int n; } Sprites;
typedef struct { int visible; float depth; int screen_x; int height, width; int top, bottom; } SpriteProj;

void    sprites_clear(Sprites *sp);
Sprite *sprites_add(Sprites *sp, int kind, float x, float y, float size);
void    sprites_remove(Sprites *sp, Sprite *s);
int     sprite_project(const Camera *cam, float proj, int w, int horizon, float sx, float sy, float size, float aspect, SpriteProj *out);
void    sprites_draw(const Sprites *sp, const Camera *cam, const Map *m, const Textures *t, const Color *pal,
                     Framebuffer *fb, const float *depth, int horizon, float proj);

#endif
