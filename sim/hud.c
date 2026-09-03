#include "hud.h"
#include "font.h"
#include "palette.h"

void hud_init(Hud *h) { h->swinging = 0; h->swing_t = 0.0f; h->hit_fired = 0; h->plus_t = 0.0f; }

void hud_start_swing(Hud *h) { h->swinging = 1; h->swing_t = 0.0f; h->hit_fired = 0; }

int hud_step(Hud *h, float dt) {
    int hit = 0;
    if (h->swinging) {
        h->swing_t += dt;
        if (!h->hit_fired && h->swing_t >= HUD_HIT_TIME - 1e-4f) { h->hit_fired = 1; hit = 1; }
        if (h->swing_t >= HUD_SWING_TIME - 1e-4f) { h->swinging = 0; h->swing_t = 0.0f; }
    }
    if (h->plus_t > 0.0f) { h->plus_t -= dt; if (h->plus_t < 1e-4f) h->plus_t = 0.0f; }
    return hit;
}

void hud_flash_plus(Hud *h) { h->plus_t = HUD_PLUS_TIME; }

int hud_frame(const Hud *h) {
    if (!h->swinging) return 0;
    return h->swing_t < 0.15f ? 1 : 2;
}

void hud_draw(const Hud *h, const Textures *t, const Color *pal, Framebuffer *fb, int bob_px) {
    int frame = hud_frame(h);
    int x0 = (fb->w - HUD_W) / 2, y0 = fb->h - HUD_H + 4 + bob_px;
    for (int v = 0; v < HUD_H; v++) {
        int row = y0 + v;
        if (row < 0 || row >= fb->h) continue;
        for (int u = 0; u < HUD_W; u++) {
            int col = x0 + u;
            if (col < 0 || col >= fb->w) continue;
            Color c = t->hud[frame][v * HUD_W + u];
            if (c.a != 255) continue;
            uint8_t *px = fb->px + ((row * fb->w) + col) * 4;
            px[0] = c.r; px[1] = c.g; px[2] = c.b; px[3] = 255;
        }
    }
    if (h->plus_t > 0.0f) {
        int rise = (int)((HUD_PLUS_TIME - h->plus_t) * 25.0f);
        draw_text_scaled(fb, fb->w / 2 - 10, y0 - 24 - rise, "+1", 3, pal[COL_FOOTPRINT]);
    }
}
