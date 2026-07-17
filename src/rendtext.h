#ifndef LUAGFX_RENDTEXT_H
#define LUAGFX_RENDTEXT_H

/*
 * rendtext.h - backend-independent text layout.
 *
 * This module:
 *
 *   - selects the current font;
 *   - walks through a byte string;
 *   - handles CR, LF, and tab;
 *   - measures text;
 *   - delegates complete glyph drawing to the active graphics backend.
 *
 * It does not know about Mode X, Nano-X, Lua, X11, or VGA pages.
 */

#include "font.h"

#define RENDTEXT_TAB_COLUMNS 4

typedef struct gfx_text_clip {
    int x;
    int y;
    int width;
    int height;
} GfxTextClip;


/*
 * Draw one complete glyph.
 *
 * The current Mode X/Nano-X implementation is supplied by graphics.c.
 * Future backends can replace it with an optimized whole-glyph renderer
 * without changing font.c, rendtext.c, or Lua code.
 */
typedef void (*GfxTextDrawGlyph)(const GfxGlyph *glyph,
                                 int x,
                                 int y,
                                 unsigned char color,
                                 const GfxTextClip *clip);


/* Initialize text state. Safe to call more than once. */
void rendtext_init(void);


/* Install or remove the active backend glyph renderer. */
void rendtext_set_backend(GfxTextDrawGlyph draw_glyph);


/* Select a registered font ID. Returns 0 on success, -1 on error. */
int rendtext_set_font(int font_id);


/* Return the current font ID. */
int rendtext_get_font(void);


/*
 * Draw a string using the current font.
 *
 * Version 1 treats input as bytes. Bytes outside the font range use the
 * replacement glyph. Future UTF-8 decoding can be added here while keeping
 * font_get_glyph's unsigned-long code-point interface unchanged.
 *
 * Returns the maximum line width drawn.
 */
int rendtext_draw(const char *text,
                  int x,
                  int y,
                  unsigned char color,
                  const GfxTextClip *clip);


/* Measure the widest line of a string using the current font. */
int rendtext_width(const char *text);


/* Return the current font's line height. */
int rendtext_height(void);

#endif /* LUAGFX_RENDTEXT_H */
