#ifndef FONTS_H
#define FONTS_H

#include "utils/vector.h"
#include "video/surface.h"
#include <stdbool.h>

typedef enum font_size
{
    FONT_NONE = 0, // For debugging.
    FONT_BIG,
    FONT_SMALL,
    FONT_NET1,
    FONT_NET2
} font_size;

struct ko_table;

typedef struct font {
    font_size size;
    int w; // Note that this is only for compatibility with the old text renderer.
    int h; // This is the default row height, if there is no text on it.
    vector surfaces;
    struct ko_table *ko; // Hangul glyphs from KOREAN.FNT (NULL if not loaded)
} font;

void font_create(font *f);
void font_free(font *font);
const surface *font_get_surface(const font *font, char ch);

// Glyph at the start of s (avail bytes). In UTF-8 mode (Korean language file) multi-byte sequences are
// decoded and Hangul is taken from KOREAN.FNT; *len receives the number of bytes used (always >= 1).
const surface *font_get_glyph(const font *font, const char *s, size_t avail, size_t *len);
// UTF-8 mode is enabled while a Korean language file is loaded; otherwise text is CP437 as in the original game.
void fonts_set_utf8(bool enabled);
bool fonts_get_utf8(void);

bool fonts_init(void);
void fonts_close(void);
const font *fonts_get_font(font_size font);

void fonts_set_font(font *font, font_size font_sz);

#endif // FONTS_H
