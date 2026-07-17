#ifndef LUAGFX_FONT_H
#define LUAGFX_FONT_H

/*
 * font.h - backend-independent bitmap-font storage and glyph lookup.
 *
 * Version 1 intentionally supports compact 1-bit-per-pixel bitmap fonts.
 * The structures already leave room for:
 *
 *   - proportional glyph widths;
 *   - offset tables for variable-size glyph storage;
 *   - fonts wider than 8 pixels;
 *   - additional compiled-in or runtime-loaded fonts;
 *   - UTF-8 decoding in the text-layout layer;
 *   - 8-bit coverage/antialiasing in a future renderer.
 *
 * This module must not include Lua, Mode X, Nano-X, X11, or any other
 * graphics-backend header.
 */

#define GFX_MAX_FONTS 8

#define GFX_FONT_BUILTIN_8X8 0

#define GFX_FONT_BPP_MONO 1

/*
 * Bit ordering inside each bitmap byte.
 *
 * The public-domain built-in font uses bit 0 as the leftmost pixel.
 * Future font importers can set MSB_LEFT instead.
 */
#define GFX_FONT_FLAG_LSB_LEFT 0x01
#define GFX_FONT_FLAG_MSB_LEFT 0x00

typedef struct gfx_glyph {
    const unsigned char *bits;

    unsigned char width;
    unsigned char height;
    unsigned char stride;
    unsigned char advance;
    unsigned char bits_per_pixel;
    unsigned char flags;
} GfxGlyph;

typedef struct gfx_font {
    const char *name;

    /*
     * Version 1 uses contiguous character ranges. unsigned long leaves
     * room for future Unicode code points without changing the public API.
     *
     * A future sparse-range table can be added here if fonts need several
     * non-contiguous Unicode blocks.
     */
    unsigned long first_char;
    unsigned short char_count;
    unsigned long default_char;

    unsigned char max_width;
    unsigned char height;
    unsigned char ascent;
    unsigned char line_gap;
    unsigned char bits_per_pixel;
    unsigned char flags;

    /*
     * Number of bytes used by one bitmap row.
     *
     * For a fixed 8x8 font this is 1. For a proportional font with tightly
     * packed glyphs this may be zero, in which case font.c derives the
     * stride from the glyph width.
     */
    unsigned char row_stride;

    /*
     * Glyph bitmap storage.
     *
     * If offsets is NULL, every glyph occupies:
     *
     *     row_stride * height
     *
     * bytes. If offsets is non-NULL, offsets[index] is the byte offset of
     * the glyph. A future large-font format may replace unsigned short
     * offsets with a wider or segmented representation.
     */
    const unsigned char *bits;
    const unsigned short *offsets;

    /*
     * Optional proportional glyph widths. When NULL, max_width is used.
     * A future advance table can be added separately when glyph width and
     * cursor advance need to differ.
     */
    const unsigned char *widths;
} GfxFont;


/* Public-domain built-in font descriptor. */
extern const GfxFont gfx_font_builtin_8x8;


/*
 * Initialize the fixed-size font registry and register built-in fonts.
 * Safe to call more than once.
 */
void font_init(void);


/*
 * Register a font under a numeric LuaGFX font ID.
 *
 * The font object and all data referenced by it must remain alive while
 * registered. Version 1 is intended primarily for compiled-in fonts.
 *
 * Returns 0 on success and -1 on an invalid ID or NULL font.
 */
int font_register(int id, const GfxFont *font);


/* Remove a font from the registry. Built-in font 0 may be restored later. */
void font_unregister(int id);


/* Return a registered font or NULL when the ID is invalid/unregistered. */
const GfxFont *font_get(int id);


/*
 * Decode one character into a lightweight glyph view.
 *
 * Version 1 accepts a code point but uses contiguous bitmap-font ranges.
 * Missing characters are replaced with font->default_char.
 *
 * Returns 0 on success and -1 when neither the requested character nor
 * the replacement character is available.
 */
int font_get_glyph(const GfxFont *font,
                   unsigned long character,
                   GfxGlyph *glyph);


/* Return the advance of a character, using the replacement glyph if needed. */
int font_character_advance(const GfxFont *font, unsigned long character);


/* Return the vertical distance between two text baselines/lines. */
int font_line_height(const GfxFont *font);

#endif /* LUAGFX_FONT_H */
