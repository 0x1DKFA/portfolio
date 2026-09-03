#ifndef FONT_H_HEADER
#define FONT_H_HEADER
#include "draw.h"

#define FONT_W 3
#define FONT_H 5
#define FONT_ADVANCE 4

const char *font_glyph(char c);                  /* 15 chars or NULL */
void draw_text(Framebuffer *fb, int x, int y, const char *s, Color c);
int  draw_text_width(const char *s);

#endif
