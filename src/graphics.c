#include "graphics.h"

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
    gfx_err = "no error";
    return 0;
}

void gfx_close(void)
{
    if (!gfx_opened)
        return;

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
