#ifndef HUD_H_HEADER
#define HUD_H_HEADER
#include "draw.h"
#include "textures.h"

#define HUD_SWING_TIME 0.4f
#define HUD_HIT_TIME 0.25f
#define HUD_PLUS_TIME 0.8f

typedef struct { int swinging; float swing_t; int hit_fired; float plus_t; } Hud;

void hud_init(Hud *h);
void hud_start_swing(Hud *h);
int  hud_step(Hud *h, float dt);          /* 1 on the step the hit lands */
void hud_flash_plus(Hud *h);
int  hud_frame(const Hud *h);             /* 0 rest, 1 raised, 2 down */
void hud_draw(const Hud *h, const Textures *t, const Color *pal, Framebuffer *fb, int bob_px);

#endif
