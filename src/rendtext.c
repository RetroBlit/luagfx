/*
 * rendtext.c - backend-independent text layout and measurement.
 */

#include "rendtext.h"

static GfxTextDrawGlyph rendtext_backend;
static int rendtext_font_id = GFX_FONT_BUILTIN_8X8;
static int rendtext_initialized;


void
rendtext_init(void)
{
    if (rendtext_initialized)
        return;

    font_init();
    rendtext_backend = 0;
    rendtext_font_id = GFX_FONT_BUILTIN_8X8;
    rendtext_initialized = 1;
}


void
rendtext_set_backend(GfxTextDrawGlyph draw_glyph)
{
    if (!rendtext_initialized)
        rendtext_init();

    rendtext_backend = draw_glyph;
}


int
rendtext_set_font(int font_id)
{
    if (!rendtext_initialized)
        rendtext_init();

    if (font_get(font_id) == 0)
        return -1;

    rendtext_font_id = font_id;
    return 0;
}


int
rendtext_get_font(void)
{
    if (!rendtext_initialized)
        rendtext_init();

    return rendtext_font_id;
}


/*
 * Advance x to the next tab stop, measured from the beginning of the line.
 */
static int
rendtext_tab_advance(const GfxFont *font, int line_x)
{
    int space;
    int tab_width;
    int remainder;

    space = font_character_advance(font, (unsigned long)' ');
    if (space <= 0)
        space = font->max_width;

    tab_width = space * RENDTEXT_TAB_COLUMNS;
    if (tab_width <= 0)
        return 0;

    remainder = line_x % tab_width;
    if (remainder < 0)
        remainder += tab_width;

    return tab_width - remainder;
}


int
rendtext_draw(const char *text,
              int x,
              int y,
              unsigned char color,
              const GfxTextClip *clip)
{
    const GfxFont *font;
    GfxGlyph glyph;
    int start_x;
    int line_width;
    int max_width;
    int line_height;
    unsigned char character;

    if (!rendtext_initialized)
        rendtext_init();

    if (text == 0)
        return 0;

    font = font_get(rendtext_font_id);
    if (font == 0)
        return 0;

    start_x = x;
    line_width = 0;
    max_width = 0;
    line_height = font_line_height(font);

    while (*text != '\0') {
        character = (unsigned char)*text++;

        if (character == '\n') {
            if (line_width > max_width)
                max_width = line_width;

            x = start_x;
            y += line_height;
            line_width = 0;
            continue;
        }

        if (character == '\r') {
            if (line_width > max_width)
                max_width = line_width;

            x = start_x;
            line_width = 0;
            continue;
        }

        if (character == '\t') {
            int advance;

            advance = rendtext_tab_advance(font, line_width);
            x += advance;
            line_width += advance;
            continue;
        }

        if (font_get_glyph(font, (unsigned long)character, &glyph) != 0)
            continue;

        if (rendtext_backend != 0)
            rendtext_backend(&glyph, x, y, color, clip);

        x += glyph.advance;
        line_width += glyph.advance;
    }

    if (line_width > max_width)
        max_width = line_width;

    return max_width;
}


int
rendtext_width(const char *text)
{
    const GfxFont *font;
    GfxGlyph glyph;
    int line_width;
    int max_width;
    unsigned char character;

    if (!rendtext_initialized)
        rendtext_init();

    if (text == 0)
        return 0;

    font = font_get(rendtext_font_id);
    if (font == 0)
        return 0;

    line_width = 0;
    max_width = 0;

    while (*text != '\0') {
        character = (unsigned char)*text++;

        if (character == '\n') {
            if (line_width > max_width)
                max_width = line_width;

            line_width = 0;
            continue;
        }

        if (character == '\r') {
            if (line_width > max_width)
                max_width = line_width;

            line_width = 0;
            continue;
        }

        if (character == '\t') {
            line_width += rendtext_tab_advance(font, line_width);
            continue;
        }

        if (font_get_glyph(font, (unsigned long)character, &glyph) == 0)
            line_width += glyph.advance;
    }

    if (line_width > max_width)
        max_width = line_width;

    return max_width;
}


int
rendtext_height(void)
{
    const GfxFont *font;

    if (!rendtext_initialized)
        rendtext_init();

    font = font_get(rendtext_font_id);
    return font_line_height(font);
}
