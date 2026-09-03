#ifndef ACTORS_BUG_H_HEADER
#define ACTORS_BUG_H_HEADER
#include "sprites.h"

enum { BUGSTATE_HIDDEN = 0, BUGSTATE_PEEK = 1, BUGSTATE_EXPOSED = 2, BUGSTATE_DEAD = 3 };
#define BUG_SPRITE_SIZE 0.45f

typedef struct { int state; float x, y; Sprite *spr; } HiddenBug;

void bug_place(HiddenBug *b, Sprites *sp, float x, float y);
void bug_set_state(HiddenBug *b, Sprites *sp, int state);

#endif
