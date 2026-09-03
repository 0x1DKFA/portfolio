#include "hero.h"
#include "fmath.h"

void hero_init(Hero *h, float x, float y) {
    h->x = x; h->y = y; h->facing = 1; h->action = HERO_IDLE; h->timer = 0;
    h->target_x = x; h->target_y = y; h->has_target = 0;
    h->detective = 0; h->glass_x = x; h->glass_y = y; h->bonk_landed = 0;
}

void hero_walk_to(Hero *h, float x, float y) {
    h->target_x = x; h->target_y = y; h->has_target = 1; h->action = HERO_WALK;
    if (x < h->x) h->facing = -1; else if (x > h->x) h->facing = 1;
}

int hero_arrived(const Hero *h) { return !h->has_target && h->action != HERO_WALK; }

void hero_start_bonk(Hero *h) {
    h->has_target = 0; h->action = HERO_WINDUP; h->timer = 0;
}

int hero_busy(const Hero *h) { return h->action == HERO_WINDUP || h->action == HERO_BONK; }

void hero_hide(Hero *h) { h->action = HERO_HIDDEN; h->has_target = 0; }

void hero_show(Hero *h, float x, float y) { h->x = x; h->y = y; h->action = HERO_IDLE; h->has_target = 0; }

void hero_set_action(Hero *h, int action) { h->action = action; h->has_target = 0; h->timer = 0; }

void hero_update(Hero *h, float dt) {
    h->bonk_landed = 0;
    switch (h->action) {
    case HERO_WALK: {
        float dx = h->target_x - h->x, dy = h->target_y - h->y;
        float dist = fm_sqrt(dx * dx + dy * dy);
        float step = HERO_WALK_SPEED * dt;
        if (dist <= step || dist < 0.001f) {
            h->x = h->target_x; h->y = h->target_y; h->has_target = 0; h->action = HERO_IDLE;
        } else {
            h->x += dx / dist * step; h->y += dy / dist * step;
            if (dx < 0) h->facing = -1; else if (dx > 0) h->facing = 1;
        }
        break;
    }
    case HERO_WINDUP:
        h->timer += dt;
        if (h->timer >= HERO_WINDUP_TIME - 1e-4f) { h->action = HERO_BONK; h->timer = 0; h->bonk_landed = 1; }
        break;
    case HERO_BONK:
        h->timer += dt;
        if (h->timer >= HERO_BONK_TIME - 1e-4f) { h->action = HERO_IDLE; h->timer = 0; }
        break;
    default: break;
    }
}
