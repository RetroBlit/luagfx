#include "graphics.h"

#include <string.h>
#include <unistd.h>

#ifdef USE_NANOX_BACKEND
#include "nano-X.h"
#ifndef MWRGB
#define MWRGB(r,g,b) ((((unsigned long)(r)) << 16) | \
                      (((unsigned long)(g)) << 8)  | \
                      ((unsigned long)(b)))
#endif
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

#ifdef USE_NANOX_BACKEND

static GR_WINDOW_ID nx_win = 0;
static GR_GC_ID nx_gc = 0;
static int nx_quit_requested = 0;

static unsigned long soft_palette[256];

static unsigned long make_rgb63(int r, int g, int b)
{
    unsigned long rr;
    unsigned long gg;
    unsigned long bb;

    if (r < 0) r = 0;
    if (g < 0) g = 0;
    if (b < 0) b = 0;
    if (r > 63) r = 63;
    if (g > 63) g = 63;
    if (b > 63) b = 63;

    rr = (unsigned long)((r * 255) / 63);
    gg = (unsigned long)((g * 255) / 63);
    bb = (unsigned long)((b * 255) / 63);

    return (rr << 16) | (gg << 8) | bb;
}

static void soft_palette_defaults(void)
{
    int i;

    for (i = 0; i < 256; i++)
        soft_palette[i] = make_rgb63(i & 63, i & 63, i & 63);

    soft_palette[0]  = make_rgb63(0,  0,  0);
    soft_palette[1]  = make_rgb63(0,  0, 42);
    soft_palette[2]  = make_rgb63(0, 42,  0);
    soft_palette[3]  = make_rgb63(0, 42, 42);
    soft_palette[4]  = make_rgb63(42, 0,  0);
    soft_palette[5]  = make_rgb63(42, 0, 42);
    soft_palette[6]  = make_rgb63(42,21,  0);
    soft_palette[7]  = make_rgb63(42,42, 42);
    soft_palette[8]  = make_rgb63(21,21, 21);
    soft_palette[9]  = make_rgb63(21,21, 63);
    soft_palette[10] = make_rgb63(21,63, 21);
    soft_palette[11] = make_rgb63(21,63, 63);
    soft_palette[12] = make_rgb63(63,21, 21);
    soft_palette[13] = make_rgb63(63,21, 63);
    soft_palette[14] = make_rgb63(63,63, 21);
    soft_palette[15] = make_rgb63(63,63, 63);
}

static GR_COLOR nx_color(unsigned char c)
{
    return (GR_COLOR)soft_palette[c];
}

static void nx_close_backend(void)
{
    if (nx_gc)
        GrDestroyGC(nx_gc);
    if (nx_win)
        GrDestroyWindow(nx_win);
    GrClose();
    nx_win = 0;
    nx_gc = 0;
    nx_quit_requested = 0;
}

static int nx_open_backend(int w, int h)
{
    if (w <= 0)
        w = 320;
    if (h <= 0)
        h = 200;

    if (GrOpen() < 0) {
        gfx_err = "cannot open Nano-X";
        return -1;
    }

    nx_win = GrNewWindow(GR_ROOT_WINDOW_ID,
                         0, 0,
                         w, h,
                         0,
                         nx_color(0),
                         nx_color(0));
    if (!nx_win) {
        GrClose();
        gfx_err = "cannot create Nano-X window";
        return -1;
    }

    nx_gc = GrNewGC();
    if (!nx_gc) {
        GrDestroyWindow(nx_win);
        GrClose();
        nx_win = 0;
        gfx_err = "cannot create Nano-X GC";
        return -1;
    }

    GrSelectEvents(nx_win,
                   GR_EVENT_MASK_EXPOSURE |
                   GR_EVENT_MASK_KEY_DOWN |
                   GR_EVENT_MASK_CLOSE_REQ);

    GrMapWindow(nx_win);
    GrFlush();

    gfx_w = w;
    gfx_h = h;
    nx_quit_requested = 0;
    return 0;
}

static void nx_handle_event(GR_EVENT *ev)
{
    switch (ev->type) {
    case GR_EVENT_TYPE_EXPOSURE:
        if (nx_win)
            GrClearWindow(nx_win, 0);
        break;
    case GR_EVENT_TYPE_CLOSE_REQ:
        nx_quit_requested = 1;
        break;
    case GR_EVENT_TYPE_KEY_DOWN:
        if (ev->keystroke.ch == 27 || ev->keystroke.ch == 'q')
            nx_quit_requested = 1;
        break;
    default:
        break;
    }
}

static int nx_process_events(unsigned int timeout_ms)
{
    GR_EVENT ev;

    if (!gfx_opened || gfx_backend != GFX_BACKEND_NANOX)
        return 0;

    memset(&ev, 0, sizeof(ev));
    GrGetNextEventTimeout(&ev, timeout_ms);

    if (ev.type != 0)
        nx_handle_event(&ev);

    if (nx_quit_requested) {
        gfx_err = "Interrupted";
        return -1;
    }

    return 0;
}

#else /* VGA Mode X backend */

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

#endif /* USE_NANOX_BACKEND */

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
    GrSetGCForeground(nx_gc, nx_color(c));
    GrLine(nx_win, nx_gc, x, y, x + w - 1, y);
#else
    vgax_hline(mx_draw_page, x, y, w, c);
#endif
}

#ifdef USE_NANOX_BACKEND
static void draw_bitmap_generic(const unsigned char *pixels,
                                int bw, int bh,
                                int transparent,
                                int x, int y,
                                int flip_x)
{
    int row;

    if (pixels == 0 || bw <= 0 || bh <= 0)
        return;

    for (row = 0; row < bh; row++) {
        int sy;
        int col;

        sy = y + row;
        if (sy < 0 || sy >= gfx_h)
            continue;

        col = 0;
        while (col < bw) {
            int src_col;
            int run_start;
            int run_len;
            unsigned char c;
            unsigned char run_color;

            src_col = flip_x ? (bw - 1 - col) : col;
            c = pixels[row * bw + src_col];
            if ((int)c == transparent) {
                col++;
                continue;
            }

            run_start = col;
            run_len = 1;
            run_color = c;
            col++;

            while (col < bw) {
                src_col = flip_x ? (bw - 1 - col) : col;
                c = pixels[row * bw + src_col];
                if ((int)c == transparent || c != run_color)
                    break;
                run_len++;
                col++;
            }

            backend_hline(x + run_start, sy, run_len, run_color);
        }
    }
}
#endif

int gfx_open(int w, int h)
{
    if (gfx_opened)
        gfx_close();

#ifdef USE_NANOX_BACKEND
    soft_palette_defaults();
    if (nx_open_backend(w, h) != 0)
        return -1;
    gfx_backend = GFX_BACKEND_NANOX;
    gfx_cap_bits = GFX_CAP_WINDOWED |
                   GFX_CAP_SPRITES |
                   GFX_CAP_TILES |
                   GFX_CAP_TILEMAP;
#else
    vgax_init();
    mx_visible_page = VGAX_PAGE0;
    mx_draw_page = VGAX_PAGE1;

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

    gfx_opened = 1;
    gfx_err = "no error";
    return 0;
}

void gfx_close(void)
{
    if (!gfx_opened)
        return;

#ifdef USE_NANOX_BACKEND
    nx_close_backend();
#else
    vgax_text_mode();
#endif

    gfx_opened = 0;
    gfx_backend = GFX_BACKEND_NONE;
    gfx_w = 0;
    gfx_h = 0;
    gfx_cap_bits = 0;
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
    GrFlush();
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
    if (gfx_opened && gfx_backend == GFX_BACKEND_NANOX)
        return nx_process_events(ms);
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
    GrSetGCForeground(nx_gc, nx_color(c));
    GrFillRect(nx_win, nx_gc, 0, 0, gfx_w, gfx_h);
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
    GrSetGCForeground(nx_gc, nx_color(c));
    GrPoint(nx_win, nx_gc, x, y);
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
    GrSetGCForeground(nx_gc, nx_color(c));
    GrLine(nx_win, nx_gc, x0, y0, x1, y1);
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
    backend_hline(x, y, w, c);
    backend_hline(x, y + h - 1, w, c);
#ifdef USE_NANOX_BACKEND
    GrSetGCForeground(nx_gc, nx_color(c));
    GrLine(nx_win, nx_gc, x, y, x, y + h - 1);
    GrLine(nx_win, nx_gc, x + w - 1, y, x + w - 1, y + h - 1);
#else
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
    GrSetGCForeground(nx_gc, nx_color(c));
    GrFillRect(nx_win, nx_gc, x, y, w, h);
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
    draw_bitmap_generic(p, s->w, s->h, s->transparent, x, y, flip_x);
#else
    vgax_draw_bitmap(mx_draw_page, p, s->w, s->h,
                     s->transparent, x, y, flip_x);
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
    draw_bitmap_generic(p, t->tile_w, t->tile_h, t->transparent, x, y, 0);
#else
    {
        struct vgax_tileset vx;
        vx.tile_w = t->tile_w;
        vx.tile_h = t->tile_h;
        vx.count = t->count;
        vx.transparent = t->transparent;
        vx.pixels = t->pixels;
        vgax_draw_tile(mx_draw_page, &vx, (unsigned char)tile_id, x, y);
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
        vgax_draw_tilemap(mx_draw_page, &vx, m->map,
                          m->map_w, m->map_h, scroll_x, scroll_y);
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
#ifndef USE_NANOX_BACKEND
    if (!gfx_opened)
        return;

    modex_copy_pixels(mx_draw_page, VGAX_PAGE2, 0, 0, VGAX_W, VGAX_H);
    modex_copy_pixels(VGAX_PAGE2, VGAX_PAGE0, 0, 0, VGAX_W, VGAX_H);
    modex_copy_pixels(VGAX_PAGE2, VGAX_PAGE1, 0, 0, VGAX_W, VGAX_H);
#endif
}

void gfx_restore(int x, int y, int w, int h)
{
#ifndef USE_NANOX_BACKEND
    if (!gfx_opened)
        return;

    modex_copy_pixels(VGAX_PAGE2, mx_draw_page, x, y, w, h);
#else
    (void)x;
    (void)y;
    (void)w;
    (void)h;
#endif
}

void gfx_copy_rect(int src_page, int dst_page, int x, int y, int w, int h)
{
#ifndef USE_NANOX_BACKEND
    unsigned short src;
    unsigned short dst;

    if (!gfx_opened)
        return;

    src = modex_page_from_id(src_page);
    dst = modex_page_from_id(dst_page);
    modex_copy_pixels(src, dst, x, y, w, h);
#else
    (void)src_page;
    (void)dst_page;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
#endif
}
