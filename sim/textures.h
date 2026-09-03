#ifndef TEXTURES_H_HEADER
#define TEXTURES_H_HEADER
#include <stdint.h>
#include "draw.h"
#include "rng.h"

#define TEX_SIZE 64
#define SPR_W 32
#define SPR_H 64
#define SKY_W 1024
#define SKY_H 192
#define HUD_W 96
#define HUD_H 64
#define HUD_FRAMES 3
#define WINDOW_PANE_ALPHA 200

enum { TEX_BRICK = 0, TEX_CONCRETE, TEX_WINDOW, TEX_NEON, TEX_SIGN, TEX_POSTER, TEX_DUMPSTER, TEX_ASPHALT, TEX_PUDDLE, TEX_COUNT };
enum { SPR_DOG_A = 0, SPR_DOG_B, SPR_DOG_SNIFF, SPR_TRASHCAN, SPR_PAPER_A, SPR_PAPER_B, SPR_NEWS_A, SPR_NEWS_B,
       SPR_BUG_HIDDEN, SPR_BUG_PEEK, SPR_BUG_EXPOSED, SPR_DUST_A, SPR_DUST_B, SPR_LAMP, SPR_COUNT };

typedef struct {
    Color wall[TEX_COUNT][TEX_SIZE * TEX_SIZE];
    Color sprite[SPR_COUNT][SPR_W * SPR_H];
    int   sprite_h[SPR_COUNT];
    Color sky[SKY_H * SKY_W];
    Color hud[HUD_FRAMES][HUD_W * HUD_H];
} Textures;

void     textures_generate(Textures *t, Rng *rng, const Color *pal, const char *neon_text);
int      texture_for_cell(int cell);                                                       /* CELL_* -> TEX_* */
Color    texture_wall(const Textures *t, const Color *pal, int tex, uint32_t variant, int u, int v);
Color    texture_sprite(const Textures *t, int spr, int u, int v);
Color    texture_sky(const Textures *t, int x, int y);
uint32_t texture_hash(int x, int y);

#endif
