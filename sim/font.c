#include "font.h"

typedef struct { char ch; const char *rows; } Glyph;

static const Glyph GLYPHS[] = {
    { '0', "###" "# #" "# #" "# #" "###" },
    { '1', " # " "## " " # " " # " "###" },
    { '2', "###" "  #" "###" "#  " "###" },
    { '3', "###" "  #" "###" "  #" "###" },
    { '4', "# #" "# #" "###" "  #" "  #" },
    { '5', "###" "#  " "###" "  #" "###" },
    { '6', "###" "#  " "###" "# #" "###" },
    { '7', "###" "  #" "  #" "  #" "  #" },
    { '8', "###" "# #" "###" "# #" "###" },
    { '9', "###" "# #" "###" "  #" "###" },
    { '+', "   " " # " "###" " # " "   " },
    { '-', "   " "   " "###" "   " "   " },
    { '.', "   " "   " "   " "   " " # " },
    { '?', "###" "  #" " ##" "   " " # " },
    { ':', "   " " # " "   " " # " "   " },
    { ' ', "   " "   " "   " "   " "   " },
    { 'C', "###" "#  " "#  " "#  " "###" },
    { 'D', "## " "# #" "# #" "# #" "## " },
    { 'E', "###" "#  " "###" "#  " "###" },
    { 'K', "# #" "# #" "## " "# #" "# #" },
    { 'L', "#  " "#  " "#  " "#  " "###" },
    { 'M', "# #" "###" "###" "# #" "# #" },
    { 'N', "###" "# #" "# #" "# #" "# #" },
    { 'O', "###" "# #" "# #" "# #" "###" },
    { 'T', "###" " # " " # " " # " " # " },
    { 'U', "# #" "# #" "# #" "# #" "###" },
    { 'V', "# #" "# #" "# #" "# #" " # " },
    { 'W', "# #" "# #" "# #" "###" "# #" },
    { 'X', "# #" "# #" " # " "# #" "# #" },
    { 'A', "###" "# #" "###" "# #" "# #" },
    { 'B', "## " "# #" "## " "# #" "## " },
    { 'F', "###" "#  " "###" "#  " "#  " },
    { 'G', "###" "#  " "# #" "# #" "###" },
    { 'H', "# #" "# #" "###" "# #" "# #" },
    { 'I', "###" " # " " # " " # " "###" },
    { 'J', "  #" "  #" "  #" "# #" "###" },
    { 'P', "###" "# #" "###" "#  " "#  " },
    { 'Q', "###" "# #" "# #" "###" "  #" },
    { 'R', "###" "# #" "## " "# #" "# #" },
    { 'S', "###" "#  " "###" "  #" "###" },
    { 'Y', "# #" "# #" "###" " # " " # " },
    { 'Z', "###" "  #" " # " "#  " "###" },
};

static const char BOX[] = "###" "# #" "# #" "# #" "###";

const char *font_glyph(char c) {
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    for (unsigned i = 0; i < sizeof(GLYPHS) / sizeof(GLYPHS[0]); i++)
        if (GLYPHS[i].ch == c) return GLYPHS[i].rows;
    return 0;
}

int draw_text_width(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n ? n * FONT_ADVANCE - 1 : 0;
}

void draw_text(Framebuffer *fb, int x, int y, const char *s, Color c) {
    for (; *s; s++, x += FONT_ADVANCE) {
        const char *g = font_glyph(*s);
        if (!g) g = BOX;
        for (int row = 0; row < FONT_H; row++)
            for (int col = 0; col < FONT_W; col++)
                if (g[row * FONT_W + col] == '#') draw_pixel(fb, x + col, y + row, c);
    }
}

void draw_text_scaled(Framebuffer *fb, int x, int y, const char *s, int scale, Color c) {
    if (scale < 1) scale = 1;
    for (; *s; s++, x += FONT_ADVANCE * scale) {
        const char *g = font_glyph(*s);
        if (!g) g = BOX;
        for (int row = 0; row < FONT_H; row++)
            for (int col = 0; col < FONT_W; col++)
                if (g[row * FONT_W + col] == '#') draw_rect(fb, x + col * scale, y + row * scale, scale, scale, c);
    }
}
