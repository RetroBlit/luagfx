#include "graphics.h"
#include "font.h"
#include "rendtext.h"

#include <string.h>
#include <unistd.h>

#ifdef USE_NANOX_BACKEND
#include "nanox.h"
#else
#include "vgax.h"
#endif

static int gfx_opened = 0;
static int gfx_backend = GFX_BACKEND_NONE;
static int gfx_w = 0;
static int gfx_h = 0;
static unsigned int gfx_cap_bits = 0;
static const char *gfx_err = "no error";

struct gfx_sprite_slot {
    unsigned char used;
    unsigned char w;
    unsigned char h;
    unsigned char frames;
    unsigned char transparent;
    const unsigned char *pixels;

#ifndef USE_NANOX_BACKEND
    unsigned char mx_compiled;
    struct vgax_compiled_sprite mx_sprite;
#endif
};

struct gfx_tileset_slot {
    unsigned char used;
    unsigned char tile_w;
    unsigned char tile_h;
    unsigned char count;
    unsigned char transparent;
    const unsigned char *pixels;
};

struct gfx_tilemap_slot {
    unsigned char used;
    unsigned char map_w;
    unsigned char map_h;
    unsigned char tileset_id;
    const unsigned char *map;
};

static struct gfx_sprite_slot sprites[GFX_MAX_SPRITES];
static struct gfx_tileset_slot tilesets[GFX_MAX_TILESETS];
static struct gfx_tilemap_slot tilemaps[GFX_MAX_TILEMAPS];

#ifndef USE_NANOX_BACKEND

#define GFX_MX_MAX_SPRITE_PHASES 64
#define GFX_MX_MAX_SPRITE_RUNS   2048

static struct vgax_sprite_phase mx_sprite_phases[GFX_MX_MAX_SPRITE_PHASES];
static struct vgax_sprite_run mx_sprite_runs[GFX_MX_MAX_SPRITE_RUNS];

static unsigned short mx_sprite_phase_used = 0;
static unsigned short mx_sprite_run_used = 0;

static unsigned short mx_visible_page = VGAX_PAGE0;
static unsigned short mx_draw_page = VGAX_PAGE1;

static unsigned short modex_page_from_id(int page)
{
    if (page == GFX_PAGE_DRAW)
        return mx_draw_page;
    if (page == GFX_PAGE_VISIBLE)
        return mx_visible_page;
    if (page == GFX_PAGE_BACKGROUND)
        return VGAX_PAGE2;
    if (page == GFX_PAGE1)
        return VGAX_PAGE1;
    if (page == GFX_PAGE2)
        return VGAX_PAGE2;
    return VGAX_PAGE0;
}

static void modex_copy_pixels(unsigned short src, unsigned short dst,
                              int x, int y, int w, int h)
{
    vgax_copy_rect(src, dst, x, y, w, h);
}

#endif /* !USE_NANOX_BACKEND */

static void backend_hline(int x, int y, int w, unsigned char c)
{
    if (!gfx_opened)
        return;

    if (w <= 0 || y < 0 || y >= gfx_h)
        return;

    if (x < 0) {
        w += x;
        x = 0;
    }

    if (x >= gfx_w || w <= 0)
        return;

    if (x + w > gfx_w)
        w = gfx_w - x;

#ifdef USE_NANOX_BACKEND
    nanox_hline(x, y, w, c);
#else
    vgax_hline(mx_draw_page, x, y, w, c);
#endif
}

/*
 * Return nonzero when one pixel in a monochrome glyph is set.
 *
 * Future font formats can add separate helpers for 8-bit coverage,
 * antialiasing, or compressed glyph data.
 */
static int
text_glyph_bit(const GfxGlyph *glyph,
               unsigned int row,
               unsigned int column)
{
    const unsigned char *row_bits;
    unsigned char value;
    unsigned char mask;

    row_bits = glyph->bits + row * glyph->stride;
    value = row_bits[column >> 3];

    if ((glyph->flags & GFX_FONT_FLAG_LSB_LEFT) != 0)
        mask = (unsigned char)(1U << (column & 7U));
    else
        mask = (unsigned char)(0x80U >> (column & 7U));

    return (value & mask) != 0;
}

/*
 * Render one monochrome glyph as horizontal runs.
 *
 * Using backend_hline() is more efficient for Mode X than calling
 * vgax_plot() for every individual text pixel. It also allows this
 * implementation to work through the existing Nano-X backend.
 *
 * Future optimizations can replace this callback with:
 *
 *   - cached glyph runs;
 *   - four precompiled Mode X alignment phases;
 *   - native Nano-X bitmap/text requests;
 *   - scaled or antialiased glyph rendering;
 */
static void backend_draw_text_glyph(const GfxGlyph *glyph,
									int x,
									int y,
									unsigned char color,
									const GfxTextClip *clip)
{
    unsigned int row;
    unsigned int column;
    int py;

    if (!gfx_opened || glyph == 0 || glyph->bits == 0)
        return;

    /*
     * Version 1 supports one-bit monochrome glyphs only.
     */
    if (glyph->bits_per_pixel != GFX_FONT_BPP_MONO)
        return;

    for (row = 0; row < glyph->height; ++row) {
        py = y + (int)row;

        if (py < 0 || py >= gfx_h)
            continue;

        if (clip != 0) {
            if (py < clip->y ||
                py >= clip->y + clip->height)
                continue;
        }

        column = 0;

        while (column < glyph->width) {
            unsigned int run_start;
            int run_x;
            int run_width;

            /*
             * Skip transparent pixels.
             */
            while (column < glyph->width &&
                   !text_glyph_bit(glyph, row, column))
                ++column;

            if (column >= glyph->width)
                break;

            run_start = column;

            /*
             * Find one continuous foreground run.
             */
            while (column < glyph->width &&
                   text_glyph_bit(glyph, row, column))
                ++column;

            run_x = x + (int)run_start;
            run_width = (int)(column - run_start);

            /*
             * Apply the renderer-specific clipping rectangle.
             * backend_hline() also clips against gfx_w and gfx_h.
             */
            if (clip != 0) {
                if (run_x < clip->x) {
                    run_width -= clip->x - run_x;
                    run_x = clip->x;
                }

                if (run_x + run_width >
                    clip->x + clip->width) {
                    run_width =
                        clip->x + clip->width - run_x;
                }
            }

            if (run_width > 0)
                backend_hline(run_x, py, run_width, color);
        }
    }
}

int gfx_open(int w, int h)
{
    if (gfx_opened)
        gfx_close();

#ifdef USE_NANOX_BACKEND
    if (nanox_open(w, h) != 0) {
        gfx_err = nanox_error();
        return -1;
    }

    gfx_w = nanox_width();
    gfx_h = nanox_height();

    gfx_backend = GFX_BACKEND_NANOX;
    gfx_cap_bits = GFX_CAP_WINDOWED |
                   GFX_CAP_COPYRECT |
                   GFX_CAP_SPRITES |
                   GFX_CAP_TILES |
                   GFX_CAP_TILEMAP;
#else
    vgax_init();

    mx_visible_page = VGAX_PAGE0;
    mx_draw_page = VGAX_PAGE1;

    mx_sprite_phase_used = 0;
    mx_sprite_run_used = 0;

    if (w <= 0)
        w = VGAX_W;
    if (h <= 0)
        h = VGAX_H;
    if (w > VGAX_W)
        w = VGAX_W;
    if (h > VGAX_H)
        h = VGAX_H;
    if (w < 1)
        w = 1;
    if (h < 1)
        h = 1;

    gfx_w = w;
    gfx_h = h;

    gfx_backend = GFX_BACKEND_MODEX;
    gfx_cap_bits = GFX_CAP_PAGEFLIP |
                   GFX_CAP_VSYNC |
                   GFX_CAP_BACKPAGE |
                   GFX_CAP_COPYRECT |
                   GFX_CAP_SPRITES |
                   GFX_CAP_TILES |
                   GFX_CAP_TILEMAP;
#endif

	memset(sprites, 0, sizeof(sprites));
	memset(tilesets, 0, sizeof(tilesets));
	memset(tilemaps, 0, sizeof(tilemaps));

	gfx_opened = 1;

	/*
	 * Initialize backend-independent font and text handling.
	 *
	 * rendtext_set_backend() connects the generic string-layout module
	 * to this graphics backend.
	 */
	font_init();
	rendtext_init();
	rendtext_set_backend(backend_draw_text_glyph);

	if (rendtext_set_font(GFX_FONT_BUILTIN_8X8) != 0) {
		rendtext_set_backend(0);

#ifdef USE_NANOX_BACKEND
		nanox_close();
#else
		vgax_text_mode();
#endif

		gfx_opened = 0;
		gfx_backend = GFX_BACKEND_NONE;
		gfx_w = 0;
		gfx_h = 0;
		gfx_cap_bits = 0;
		gfx_err = "cannot select built-in font";

		return -1;
	}

	gfx_err = "no error";
	return 0;
}

void gfx_close(void)
{
    if (!gfx_opened)
        return;

    /*
     * Disconnect the active backend callback before closing graphics.
     * Font descriptors remain registered and can be reused after another
     * gfx.open().
     */
    rendtext_set_backend(0);

#ifdef USE_NANOX_BACKEND
    nanox_close();
#else
    vgax_text_mode();
#endif

    gfx_opened = 0;
    gfx_backend = GFX_BACKEND_NONE;
    gfx_w = 0;
    gfx_h = 0;
    gfx_cap_bits = 0;
    gfx_err = "no error";
}

int gfx_is_open(void)
{
    return gfx_opened;
}

const char *gfx_error(void)
{
    return gfx_err;
}

const char *gfx_backend_name(void)
{
    if (gfx_backend == GFX_BACKEND_NANOX)
        return "nanox";
    if (gfx_backend == GFX_BACKEND_MODEX)
        return "vgax";
    return "none";
}

int gfx_backend_id(void)
{
    return gfx_backend;
}

int gfx_width(void)
{
    return gfx_w;
}

int gfx_height(void)
{
    return gfx_h;
}

unsigned int gfx_caps(void)
{
    return gfx_cap_bits;
}

void gfx_present(void)
{
    if (!gfx_opened)
        return;

#ifdef USE_NANOX_BACKEND
    nanox_present();
#else
    vgax_wait_vsync();
    vgax_flip(mx_draw_page);

    mx_visible_page = mx_draw_page;

    if (mx_draw_page == VGAX_PAGE0)
        mx_draw_page = VGAX_PAGE1;
    else
        mx_draw_page = VGAX_PAGE0;
#endif
}

int gfx_sleep_ms(unsigned int ms)
{
#ifdef USE_NANOX_BACKEND
    if (gfx_opened && gfx_backend == GFX_BACKEND_NANOX) {
        if (nanox_sleep_ms(ms) != 0) {
            gfx_err = nanox_error();
            return -1;
        }
        return 0;
    }
#endif

    usleep(1000U * ms);
    return 0;
}

void gfx_clear(int color)
{
    unsigned char c;

    if (!gfx_opened)
        return;

    c = (unsigned char)color;

#ifdef USE_NANOX_BACKEND
    nanox_clear(c);
#else
    vgax_fill_rect(mx_draw_page, 0, 0, gfx_w, gfx_h, c);
#endif
}

void gfx_pixel(int x, int y, int color)
{
    unsigned char c;

    if (!gfx_opened)
        return;

    if (x < 0 || y < 0 || x >= gfx_w || y >= gfx_h)
        return;

    c = (unsigned char)color;

#ifdef USE_NANOX_BACKEND
    nanox_pixel(x, y, c);
#else
    vgax_plot(mx_draw_page, x, y, c);
#endif
}

void gfx_line(int x0, int y0, int x1, int y1, int color)
{
    unsigned char c;

    if (!gfx_opened)
        return;

    c = (unsigned char)color;

#ifdef USE_NANOX_BACKEND
    nanox_line(x0, y0, x1, y1, c);
#else
    vgax_line(mx_draw_page, x0, y0, x1, y1, c);
#endif
}

void gfx_rect(int x, int y, int w, int h, int color)
{
    unsigned char c;

    if (!gfx_opened || w <= 0 || h <= 0)
        return;

    c = (unsigned char)color;

#ifdef USE_NANOX_BACKEND
    nanox_rect(x, y, w, h, c);
#else
    backend_hline(x, y, w, c);
    backend_hline(x, y + h - 1, w, c);
    vgax_vline(mx_draw_page, x, y, h, c);
    vgax_vline(mx_draw_page, x + w - 1, y, h, c);
#endif
}

void gfx_fill(int x, int y, int w, int h, int color)
{
    unsigned char c;

    if (!gfx_opened || w <= 0 || h <= 0)
        return;

    if (x < 0) {
        w += x;
        x = 0;
    }

    if (y < 0) {
        h += y;
        y = 0;
    }

    if (x >= gfx_w || y >= gfx_h || w <= 0 || h <= 0)
        return;

    if (x + w > gfx_w)
        w = gfx_w - x;

    if (y + h > gfx_h)
        h = gfx_h - y;

    c = (unsigned char)color;

#ifdef USE_NANOX_BACKEND
    nanox_fill(x, y, w, h, c);
#else
    vgax_fill_rect(mx_draw_page, x, y, w, h, c);
#endif
}

int gfx_define_sprite(int id, int w, int h, int frames,
                      int transparent, const unsigned char *pixels)
{
    if (id < 0 || id >= GFX_MAX_SPRITES)
        return -1;
    if (w <= 0 || h <= 0 || frames <= 0 || pixels == 0)
        return -1;
    if (w > 255 || h > 255 || frames > 255)
        return -1;

    sprites[id].used = 1;
    sprites[id].w = (unsigned char)w;
    sprites[id].h = (unsigned char)h;
    sprites[id].frames = (unsigned char)frames;
    sprites[id].transparent = (unsigned char)transparent;
    sprites[id].pixels = pixels;

#ifndef USE_NANOX_BACKEND
    sprites[id].mx_compiled = 0;

    {
        unsigned short phase_need;
        unsigned short phase_left;
        unsigned short run_left;

        phase_need = (unsigned short)(frames * 4);
        phase_left = (unsigned short)(GFX_MX_MAX_SPRITE_PHASES -
                                      mx_sprite_phase_used);
        run_left = (unsigned short)(GFX_MX_MAX_SPRITE_RUNS -
                                    mx_sprite_run_used);

        if (phase_need <= phase_left && run_left > 0) {
            if (vgax_compile_sprite(&sprites[id].mx_sprite,
                                    pixels,
                                    (unsigned char)w,
                                    (unsigned char)h,
                                    (unsigned char)frames,
                                    (unsigned char)transparent,
                                    &mx_sprite_phases[mx_sprite_phase_used],
                                    &mx_sprite_runs[mx_sprite_run_used],
                                    run_left) == 0) {
                sprites[id].mx_compiled = 1;

                mx_sprite_phase_used =
                    (unsigned short)(mx_sprite_phase_used + phase_need);

                mx_sprite_run_used =
                    (unsigned short)(mx_sprite_run_used +
                                     sprites[id].mx_sprite.run_count);
            }
        }
    }
#endif

    return 0;
}

void gfx_draw_sprite(int id, int x, int y, int frame, int flip_x)
{
    struct gfx_sprite_slot *s;
    unsigned int frame_size;
    const unsigned char *p;

    if (id < 0 || id >= GFX_MAX_SPRITES)
        return;

    s = &sprites[id];

    if (!s->used || s->pixels == 0)
        return;

    if (frame < 0 || frame >= (int)s->frames)
        frame = 0;

    frame_size = (unsigned int)s->w * (unsigned int)s->h;
    p = s->pixels + (unsigned int)frame * frame_size;

#ifdef USE_NANOX_BACKEND
    nanox_save_under(x, y, s->w, s->h);
    nanox_draw_bitmap(p, s->w, s->h, s->transparent, x, y, flip_x);
#else
    if (s->mx_compiled &&
        !flip_x &&
        x >= 0 &&
        y >= 0 &&
        x + (int)s->w <= gfx_w &&
        y + (int)s->h <= gfx_h) {
        vgax_draw_compiled_sprite(mx_draw_page,
                                  &s->mx_sprite,
                                  x,
                                  y,
                                  frame);
    } else {
        vgax_draw_bitmap(mx_draw_page,
                         p,
                         s->w,
                         s->h,
                         s->transparent,
                         x,
                         y,
                         flip_x);
    }
#endif
}

void gfx_move_sprite(int id,
                     int old_x, int old_y,
                     int new_x, int new_y,
                     int frame,
                     int flip_x)
{
    struct gfx_sprite_slot *s;

    if (!gfx_opened)
        return;

    if (id < 0 || id >= GFX_MAX_SPRITES)
        return;

    s = &sprites[id];

    if (!s->used || s->pixels == 0)
        return;

    if (frame < 0 || frame >= (int)s->frames)
        frame = 0;

#ifdef USE_NANOX_BACKEND
    {
        unsigned int frame_size;
        const unsigned char *p;

        frame_size = (unsigned int)s->w * (unsigned int)s->h;
        p = s->pixels + (unsigned int)frame * frame_size;

        /*
        ** Nano-X path:
        ** 1. Restore old sprite area from saved-under buffer/window.
        ** 2. Save the background under the new sprite position.
        ** 3. Draw the sprite at the new position.
        */
        nanox_restore(old_x, old_y, s->w, s->h);
        nanox_save_under(new_x, new_y, s->w, s->h);
        nanox_draw_bitmap(p, s->w, s->h, s->transparent,
                          new_x, new_y, flip_x);
    }
#else
    /*
    ** Mode X path:
    ** Restore from background page, then draw to current draw page.
    */
    modex_copy_pixels(VGAX_PAGE2, mx_draw_page,
                      old_x, old_y, s->w, s->h);

    gfx_draw_sprite(id, new_x, new_y, frame, flip_x);
#endif
}

int gfx_define_tileset(int id, int tile_w, int tile_h, int count,
                       int transparent, const unsigned char *pixels)
{
    if (id < 0 || id >= GFX_MAX_TILESETS)
        return -1;
    if (tile_w <= 0 || tile_h <= 0 || count <= 0 || pixels == 0)
        return -1;
    if (tile_w > 255 || tile_h > 255 || count > 255)
        return -1;

    tilesets[id].used = 1;
    tilesets[id].tile_w = (unsigned char)tile_w;
    tilesets[id].tile_h = (unsigned char)tile_h;
    tilesets[id].count = (unsigned char)count;
    tilesets[id].transparent = (unsigned char)transparent;
    tilesets[id].pixels = pixels;

    return 0;
}

void gfx_draw_tile(int tileset_id, int tile_id, int x, int y)
{
    struct gfx_tileset_slot *t;

#ifdef USE_NANOX_BACKEND
    unsigned int tile_size;
    const unsigned char *p;
#endif

    if (tileset_id < 0 || tileset_id >= GFX_MAX_TILESETS)
        return;

    t = &tilesets[tileset_id];

    if (!t->used || t->pixels == 0)
        return;

    if (tile_id < 0 || tile_id >= (int)t->count)
        return;

#ifdef USE_NANOX_BACKEND
    tile_size = (unsigned int)t->tile_w * (unsigned int)t->tile_h;
    p = t->pixels + (unsigned int)tile_id * tile_size;

    nanox_draw_bitmap(p,
                      t->tile_w,
                      t->tile_h,
                      t->transparent,
                      x,
                      y,
                      0);
#else
    {
        struct vgax_tileset vx;

        vx.tile_w = t->tile_w;
        vx.tile_h = t->tile_h;
        vx.count = t->count;
        vx.transparent = t->transparent;
        vx.pixels = t->pixels;

        vgax_draw_tile(mx_draw_page,
                       &vx,
                       (unsigned char)tile_id,
                       x,
                       y);
    }
#endif
}

int gfx_define_tilemap(int id, int map_w, int map_h, int tileset_id,
                       const unsigned char *map)
{
    if (id < 0 || id >= GFX_MAX_TILEMAPS)
        return -1;
    if (tileset_id < 0 || tileset_id >= GFX_MAX_TILESETS)
        return -1;
    if (map_w <= 0 || map_h <= 0 || map == 0)
        return -1;
    if (map_w > 255 || map_h > 255)
        return -1;

    tilemaps[id].used = 1;
    tilemaps[id].map_w = (unsigned char)map_w;
    tilemaps[id].map_h = (unsigned char)map_h;
    tilemaps[id].tileset_id = (unsigned char)tileset_id;
    tilemaps[id].map = map;

    return 0;
}

void gfx_draw_tilemap(int map_id, int scroll_x, int scroll_y)
{
    struct gfx_tilemap_slot *m;
    struct gfx_tileset_slot *t;

    if (map_id < 0 || map_id >= GFX_MAX_TILEMAPS)
        return;

    m = &tilemaps[map_id];

    if (!m->used || m->map == 0)
        return;

    if (m->tileset_id >= GFX_MAX_TILESETS)
        return;

    t = &tilesets[m->tileset_id];

    if (!t->used || t->pixels == 0)
        return;

    if (scroll_x < 0)
        scroll_x = 0;
    if (scroll_y < 0)
        scroll_y = 0;

#ifndef USE_NANOX_BACKEND
    {
        struct vgax_tileset vx;

        vx.tile_w = t->tile_w;
        vx.tile_h = t->tile_h;
        vx.count = t->count;
        vx.transparent = t->transparent;
        vx.pixels = t->pixels;

        vgax_draw_tilemap(mx_draw_page,
                          &vx,
                          m->map,
                          m->map_w,
                          m->map_h,
                          scroll_x,
                          scroll_y);
    }
#else
    {
        int tw;
        int th;
        int start_col;
        int start_row;
        int end_col;
        int end_row;
        int row;

        tw = t->tile_w;
        th = t->tile_h;

        if (tw <= 0 || th <= 0)
            return;

        start_col = scroll_x / tw;
        start_row = scroll_y / th;

        end_col = (scroll_x + gfx_w + tw - 1) / tw;
        end_row = (scroll_y + gfx_h + th - 1) / th;

        if (end_col > (int)m->map_w)
            end_col = m->map_w;
        if (end_row > (int)m->map_h)
            end_row = m->map_h;

        for (row = start_row; row < end_row; row++) {
            int col;

            for (col = start_col; col < end_col; col++) {
                unsigned char tile;
                int sx;
                int sy;

                tile = m->map[row * (int)m->map_w + col];

                sx = col * tw - scroll_x;
                sy = row * th - scroll_y;

                gfx_draw_tile(m->tileset_id, tile, sx, sy);
            }
        }
    }
#endif
}

void gfx_set_background(void)
{
    if (!gfx_opened)
        return;

#ifdef USE_NANOX_BACKEND
    nanox_set_background();
#else
    modex_copy_pixels(mx_draw_page, VGAX_PAGE2, 0, 0, VGAX_W, VGAX_H);
    modex_copy_pixels(VGAX_PAGE2, VGAX_PAGE0, 0, 0, VGAX_W, VGAX_H);
    modex_copy_pixels(VGAX_PAGE2, VGAX_PAGE1, 0, 0, VGAX_W, VGAX_H);
#endif
}

void gfx_restore(int x, int y, int w, int h)
{
    if (!gfx_opened)
        return;

#ifdef USE_NANOX_BACKEND
     nanox_restore(x, y, w, h);
#else
    modex_copy_pixels(VGAX_PAGE2, mx_draw_page, x, y, w, h);
#endif
}

void gfx_copy_rect(int src_page, int dst_page, int x, int y, int w, int h)
{
    if (!gfx_opened)
        return;

#ifdef USE_NANOX_BACKEND
    nanox_copy_rect(src_page, dst_page, x, y, w, h);
#else
    {
        unsigned short src;
        unsigned short dst;

        src = modex_page_from_id(src_page);
        dst = modex_page_from_id(dst_page);

        modex_copy_pixels(src, dst, x, y, w, h);
    }
#endif
}

int gfx_set_font(int font_id)
{
    return rendtext_set_font(font_id);
}

int gfx_get_font(void)
{
    return rendtext_get_font();
}

void gfx_print(const char *text,
			   int x,
               int y,
               int color)
{
    GfxTextClip clip;

    if (!gfx_opened || text == 0)
        return;

    /*
     * Version 1 clips text to the complete logical LuaGFX display.
     *
     * Future extension: a user-selectable graphics clipping rectangle.
     */
    clip.x = 0;
    clip.y = 0;
    clip.width = gfx_w;
    clip.height = gfx_h;

    rendtext_draw(text,
                  x,
                  y,
                  (unsigned char)color,
                  &clip);
}

int gfx_text_width(const char *text)
{
    if (text == 0)
        return 0;

    return rendtext_width(text);
}


int gfx_text_height(void)
{
    return rendtext_height();
}