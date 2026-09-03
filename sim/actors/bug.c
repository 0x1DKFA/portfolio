#include "actors/bug.h"

void bug_place(HiddenBug *b, Sprites *sp, float x, float y) {
    b->state = BUGSTATE_HIDDEN; b->x = x; b->y = y;
    b->spr = sprites_add(sp, SPR_BUG_HIDDEN, x, y, BUG_SPRITE_SIZE);
}

void bug_set_state(HiddenBug *b, Sprites *sp, int state) {
    b->state = state;
    if (!b->spr) return;
    if (state == BUGSTATE_DEAD) { sprites_remove(sp, b->spr); b->spr = 0; return; }
    b->spr->kind = state == BUGSTATE_HIDDEN ? SPR_BUG_HIDDEN : state == BUGSTATE_PEEK ? SPR_BUG_PEEK : SPR_BUG_EXPOSED;
}
