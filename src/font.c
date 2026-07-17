/*
 * font.c - backend-independent bitmap-font registry and glyph lookup.
 */

#include "font.h"

static const GfxFont *font_registry[GFX_MAX_FONTS];
static int font_initialized;


/*
 * Return nonzero when a character belongs to the font's contiguous range.
 *
 * Future extension:
 * Replace or augment this helper with sparse Unicode range lookup.
 */
static int
font_contains(const GfxFont *font, unsigned long character)
{
    unsigned long end;

    if (font == 0)
        return 0;

    end = font->first_char + (unsigned long)font->char_count;

    return character >= font->first_char && character < end;
}


void
font_init(void)
{
    int i;

    if (font_initialized)
        return;

    for (i = 0; i < GFX_MAX_FONTS; ++i)
        font_registry[i] = 0;

    font_registry[GFX_FONT_BUILTIN_8X8] = &gfx_font_builtin_8x8;
    font_initialized = 1;
}


int
font_register(int id, const GfxFont *font)
{
    if (id < 0 || id >= GFX_MAX_FONTS || font == 0)
        return -1;

    /*
     * Version 1 renders monochrome fonts only.
     *
     * Future extension:
     * Permit 8-bit coverage fonts when rendtext and each backend know how
     * to map coverage values to palette shades or true-color alpha.
     */
    if (font->bits_per_pixel != GFX_FONT_BPP_MONO ||
        font->bits == 0 ||
        font->height == 0 ||
        font->max_width == 0 ||
        font->char_count == 0)
        return -1;

    font_registry[id] = font;
    return 0;
}


void
font_unregister(int id)
{
    if (id < 0 || id >= GFX_MAX_FONTS)
        return;

    /*
     * Keep the mandatory fallback font available in version 1.
     * A future dynamic-font manager may allow replacing font 0 while
     * preserving a separate emergency fallback glyph.
     */
    if (id == GFX_FONT_BUILTIN_8X8)
        return;

    font_registry[id] = 0;
}


const GfxFont *
font_get(int id)
{
    if (!font_initialized)
        font_init();

    if (id < 0 || id >= GFX_MAX_FONTS)
        return 0;

    return font_registry[id];
}


int
font_get_glyph(const GfxFont *font,
               unsigned long character,
               GfxGlyph *glyph)
{
    unsigned long index;
    unsigned long offset;
    unsigned int stride;
    unsigned int width;

    if (font == 0 || glyph == 0 || font->bits == 0)
        return -1;

    if (!font_contains(font, character))
        character = font->default_char;

    if (!font_contains(font, character))
        return -1;

    index = character - font->first_char;

    if (font->widths != 0)
        width = font->widths[(unsigned int)index];
    else
        width = font->max_width;

    if (width == 0)
        width = font->max_width;

    stride = font->row_stride;
    if (stride == 0)
        stride = (width + 7U) >> 3;

    if (font->offsets != 0) {
        offset = font->offsets[(unsigned int)index];
    } else {
        offset = index *
                 (unsigned long)stride *
                 (unsigned long)font->height;
    }

    glyph->bits = font->bits + offset;
    glyph->width = (unsigned char)width;
    glyph->height = font->height;
    glyph->stride = (unsigned char)stride;
    glyph->advance = (unsigned char)width;
    glyph->bits_per_pixel = font->bits_per_pixel;
    glyph->flags = font->flags;

    return 0;
}


int
font_character_advance(const GfxFont *font, unsigned long character)
{
    GfxGlyph glyph;

    if (font_get_glyph(font, character, &glyph) != 0)
        return 0;

    return glyph.advance;
}


int
font_line_height(const GfxFont *font)
{
    if (font == 0)
        return 0;

    return (int)font->height + (int)font->line_gap;
}
