#include "nanox.h"
#include "graphics.h"

#include "nano-X.h"

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#ifndef MWRGB
#define MWRGB(r,g,b) ((((unsigned long)(r)) << 16) | \
                      (((unsigned long)(g)) << 8)  | \
                      ((unsigned long)(b)))
#endif

#ifndef MWROP_COPY
#ifdef MWROP_SRCCOPY
#define MWROP_COPY MWROP_SRCCOPY
#else
#define MWROP_COPY 0
#endif
#endif

static GR_WINDOW_ID nx_save_pix = 0;
static int nx_save_alloc_w = 0;
static int nx_save_alloc_h = 0;

static int nx_save_valid = 0;
static int nx_save_x = 0;
static int nx_save_y = 0;
static int nx_save_w = 0;
static int nx_save_h = 0;

static GR_WINDOW_ID nx_win = 0;
static GR_GC_ID nx_gc = 0;

/*
 * Background pixmap.
 * This is the Nano-X equivalent of Mode X PAGE2.
 */
static GR_WINDOW_ID nx_bg = 0;

static int nx_w = 0;
static int nx_h = 0;
static int nx_opened = 0;

#define NANOX_KEY_QUEUE_SIZE 16
#define NANOX_INITIAL_MAP_DELAY_MS 20

static GR_KEY nx_key_queue[NANOX_KEY_QUEUE_SIZE];
static unsigned char nx_key_queue_start;
static unsigned char nx_key_queue_count;

/*
 * Close requests are stored separately so they cannot be lost when
 * the keyboard queue is full.
 */
static unsigned char nx_close_pending;

/*
 * Nonzero when drawing or copy commands have been issued since the
 * previous GrFlush().
 */
static unsigned char nx_commands_pending;

static const char *nx_err = "no error";

static unsigned long soft_palette[256];

static int nx_fg_valid = 0;
static unsigned char nx_fg_color = 0;

#define NANOX_COMPILED_MAX_RUNS 2048
#define NANOX_COMPILED_MAX_PIXMAP_HEIGHT 32767U

struct nanox_opaque_run {
    unsigned char x;
    unsigned char y;
    unsigned char length;
};

struct nanox_compiled_sprite {
    unsigned char used;
    unsigned char w;
    unsigned char h;
    unsigned char frames;
    unsigned char transparent;

    GR_WINDOW_ID pixmap;

    struct nanox_opaque_run *runs;
    unsigned short *frame_run_start;
    unsigned short run_count;
};

static struct nanox_compiled_sprite nx_compiled[GFX_MAX_SPRITES];
static unsigned short nx_compiled_run_used = 0;

static GR_COLOR nx_color(unsigned char c)
{
    return (GR_COLOR)soft_palette[c];
}

static void nanox_set_fg(unsigned char c)
{
    if (nx_fg_valid && nx_fg_color == c)
        return;

    GrSetGCForeground(nx_gc, nx_color(c));
    nx_fg_color = c;
    nx_fg_valid = 1;
}

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

static int clip_rect(int *x, int *y, int *w, int *h)
{
    if (*w <= 0 || *h <= 0)
        return 0;

    if (*x < 0) {
        *w += *x;
        *x = 0;
    }

    if (*y < 0) {
        *h += *y;
        *y = 0;
    }

    if (*x >= nx_w || *y >= nx_h || *w <= 0 || *h <= 0)
        return 0;

    if (*x + *w > nx_w)
        *w = nx_w - *x;

    if (*y + *h > nx_h)
        *h = nx_h - *y;

    return (*w > 0 && *h > 0);
}

static unsigned long nanox_current_ms(void)
{
    struct timeval tv;

    gettimeofday(&tv, 0);

    return ((unsigned long)tv.tv_sec * 1000UL) +
           ((unsigned long)tv.tv_usec / 1000UL);
}

static void nanox_reset_input_queue(void)
{
    nx_key_queue_start = 0;
    nx_key_queue_count = 0;
    nx_close_pending = 0;
}

static void nanox_queue_key(GR_KEY key)
{
    unsigned int tail;

    /*
     * When full, discard the oldest event. For interactive movement,
     * preserving newer input is more useful than preserving stale input.
     */
    if (nx_key_queue_count >= NANOX_KEY_QUEUE_SIZE) {
        nx_key_queue_start =
            (unsigned char)(((unsigned int)nx_key_queue_start + 1U) %
                            NANOX_KEY_QUEUE_SIZE);

        nx_key_queue_count--;
    }

    tail = ((unsigned int)nx_key_queue_start +
            (unsigned int)nx_key_queue_count) %
           NANOX_KEY_QUEUE_SIZE;

    nx_key_queue[tail] = key;
    nx_key_queue_count++;
}


static int nanox_pop_key(GR_KEY *key)
{
    if (key == 0 || nx_key_queue_count == 0)
        return 0;

    *key = nx_key_queue[nx_key_queue_start];

    nx_key_queue_start =
        (unsigned char)(((unsigned int)nx_key_queue_start + 1U) %
                        NANOX_KEY_QUEUE_SIZE);

    nx_key_queue_count--;

    if (nx_key_queue_count == 0)
        nx_key_queue_start = 0;

    return 1;
}

static void nanox_mark_commands_pending(void)
{
    nx_commands_pending = 1;
}

static void nanox_flush_pending(void)
{
    if (!nx_opened || !nx_commands_pending)
        return;

    GrFlush();
    nx_commands_pending = 0;
}

static int nanox_pixel_is_transparent(int transparent, unsigned char c)
{
    return transparent != GFX_NO_TRANSPARENT && (int)c == transparent;
}

static int nanox_sprite_sizes_ok(int w,
                                 int h,
                                 int frames,
                                 unsigned int *pix_h,
                                 unsigned int *pixel_count)
{
    unsigned int uw;
    unsigned int uh;
    unsigned int uf;
    unsigned int sheet_height;
    unsigned int frame_size;
    unsigned int total_pixels;

    if (w <= 0 || h <= 0 || frames <= 0)
        return 0;

    /*
     * The public sprite structure stores these values in unsigned bytes.
     */
    if (w > 255 || h > 255 || frames > 255)
        return 0;

    uw = (unsigned int)w;
    uh = (unsigned int)h;
    uf = (unsigned int)frames;

    /*
     * GrNewPixmapEx() receives a signed int height on the ELKS build.
     * Therefore the vertical frame sheet must not exceed 32767.
     */
    if (uh > NANOX_COMPILED_MAX_PIXMAP_HEIGHT / uf)
        return 0;

    sheet_height = uh * uf;

    /*
     * Keep the complete source sprite within a 16-bit unsigned offset.
     */
    if (uw > 65535U / uh)
        return 0;

    frame_size = uw * uh;

    if (frame_size > 65535U / uf)
        return 0;

    total_pixels = frame_size * uf;

    if (pix_h != 0)
        *pix_h = sheet_height;

    if (pixel_count != 0)
        *pixel_count = total_pixels;

    return 1;
}

static void nanox_pixmap_point(GR_WINDOW_ID pix, int x, int y, unsigned char c)
{
    nanox_set_fg(c);
    GrPoint(pix, nx_gc, x, y);
}

static unsigned int nanox_count_opaque_runs(const unsigned char *pixels,
                                            int w,
                                            int h,
                                            int frames,
                                            int transparent)
{
    unsigned int run_count;
    unsigned int frame_size;
    int frame;

    run_count = 0;
    frame_size = (unsigned int)w * (unsigned int)h;

    for (frame = 0; frame < frames; frame++) {
        const unsigned char *fp;
        int row;

        fp = pixels + (unsigned int)frame * frame_size;

        for (row = 0; row < h; row++) {
            int col;

            col = 0;

            while (col < w) {
                unsigned int offset;
                unsigned char c;

                offset =
                    (unsigned int)row * (unsigned int)w +
                    (unsigned int)col;

                c = fp[offset];

                if (nanox_pixel_is_transparent(transparent, c)) {
                    col++;
                    continue;
                }

                run_count++;

                /*
                 * Stop before an unsigned-short counter could wrap.
                 * The caller will report that the run limit was exceeded.
                 */
                if (run_count > NANOX_COMPILED_MAX_RUNS)
                    return run_count;

                col++;

                while (col < w) {
                    offset =
                        (unsigned int)row * (unsigned int)w +
                        (unsigned int)col;

                    c = fp[offset];

                    if (nanox_pixel_is_transparent(transparent, c))
                        break;

                    col++;
                }
            }
        }
    }

    return run_count;
}

static int nanox_build_opaque_runs(
    struct nanox_opaque_run *runs,
    unsigned short *frame_run_start,
    const unsigned char *pixels,
    int w,
    int h,
    int frames,
    int transparent,
    unsigned short run_count)
{
    unsigned int run_index;
    unsigned int frame_size;
    int frame;

    if (frame_run_start == 0 || pixels == 0)
        return -1;

    if (run_count > 0 && runs == 0)
        return -1;

    run_index = 0;
    frame_size = (unsigned int)w * (unsigned int)h;

    for (frame = 0; frame < frames; frame++) {
        const unsigned char *fp;
        int row;

        frame_run_start[frame] = (unsigned short)run_index;

        fp = pixels + (unsigned int)frame * frame_size;

        for (row = 0; row < h; row++) {
            int col;

            col = 0;

            while (col < w) {
                unsigned int offset;
                unsigned int run_start;
                unsigned char c;

                offset =
                    (unsigned int)row * (unsigned int)w +
                    (unsigned int)col;

                c = fp[offset];

                if (nanox_pixel_is_transparent(transparent, c)) {
                    col++;
                    continue;
                }

                if (run_index >= (unsigned int)run_count)
                    return -1;

                run_start = (unsigned int)col;
                col++;

                while (col < w) {
                    offset =
                        (unsigned int)row * (unsigned int)w +
                        (unsigned int)col;

                    c = fp[offset];

                    if (nanox_pixel_is_transparent(transparent, c))
                        break;

                    col++;
                }

                runs[run_index].x =
                    (unsigned char)run_start;

                runs[run_index].y =
                    (unsigned char)row;

                runs[run_index].length =
                    (unsigned char)((unsigned int)col - run_start);

                run_index++;
            }
        }
    }

    frame_run_start[frames] = (unsigned short)run_index;

    if (run_index != (unsigned int)run_count)
        return -1;

    return 0;
}

static int nanox_upload_sprite_to_pixmap(
    GR_WINDOW_ID pix,
    const unsigned char *pixels,
    int w,
    int h,
    int frames,
    int transparent)
{
    unsigned int frame_size;
    int frame;

    if (!pix || pixels == 0)
        return -1;

    frame_size = (unsigned int)w * (unsigned int)h;

    for (frame = 0; frame < frames; frame++) {
        const unsigned char *fp;
        int row;

        fp = pixels + (unsigned int)frame * frame_size;

        for (row = 0; row < h; row++) {
            int col;

            for (col = 0; col < w; col++) {
                unsigned int offset;
                unsigned char c;

                offset =
                    (unsigned int)row * (unsigned int)w +
                    (unsigned int)col;

                c = fp[offset];

                /*
                 * Transparent pixels need not be initialized because
                 * compiled transparent drawing copies only opaque spans.
                 */
                if (nanox_pixel_is_transparent(transparent, c))
                    continue;

                nanox_pixmap_point(pix,
                                   col,
                                   frame * h + row,
                                   c);
            }
        }
    }

    nanox_mark_commands_pending();
    return 0;
}

static int nanox_blit_compiled_area(GR_WINDOW_ID src,
                                    int src_x,
                                    int src_y,
                                    int dst_x,
                                    int dst_y,
                                    int w,
                                    int h)
{
    int cx;
    int cy;
    int cw;
    int ch;

    if (!nx_opened || !nx_win || !nx_gc || !src)
        return 0;

    cx = dst_x;
    cy = dst_y;
    cw = w;
    ch = h;

    if (!clip_rect(&cx, &cy, &cw, &ch))
        return 0;

    src_x += cx - dst_x;
    src_y += cy - dst_y;

    GrCopyArea(nx_win, nx_gc,
               cx, cy, cw, ch,
               src,
               src_x, src_y,
               MWROP_COPY);

    nanox_mark_commands_pending();
    return 1;
}

void nanox_free_compiled_sprite(int id)
{
    struct nanox_compiled_sprite *cs;

    if (id < 0 || id >= GFX_MAX_SPRITES)
        return;

    cs = &nx_compiled[id];

    if (!cs->used)
        return;

    if (cs->pixmap)
        GrDestroyWindow(cs->pixmap);

    if (cs->runs)
        free(cs->runs);

    if (cs->frame_run_start)
        free(cs->frame_run_start);

    if (cs->run_count <= nx_compiled_run_used)
        nx_compiled_run_used =
            (unsigned short)(nx_compiled_run_used - cs->run_count);

    cs->used = 0;
    cs->w = 0;
    cs->h = 0;
    cs->frames = 0;
    cs->transparent = 0;
    cs->pixmap = 0;
    cs->runs = 0;
    cs->frame_run_start = 0;
    cs->run_count = 0;
}

static void nanox_free_all_compiled_sprites(void)
{
    int id;

    for (id = 0; id < GFX_MAX_SPRITES; id++)
        nanox_free_compiled_sprite(id);

    nx_compiled_run_used = 0;
}

int nanox_compile_sprite(int id,
                         const unsigned char *pixels,
                         int w,
                         int h,
                         int frames,
                         int transparent)
{
    struct nanox_compiled_sprite *cs;
    unsigned int pix_h;
    unsigned int pixel_count;
    unsigned int counted_runs;
    unsigned int run_budget;
    unsigned int run_bytes;
    unsigned int frame_index_count;
    unsigned int index_bytes;
    unsigned short run_count;
    GR_WINDOW_ID pixmap;
    struct nanox_opaque_run *runs;
    unsigned short *frame_run_start;

    if (id < 0 || id >= GFX_MAX_SPRITES) {
        nx_err = "compiled sprite id out of range";
        return -1;
    }

    if (!nx_opened || !nx_gc) {
        nx_err = "Nano-X is not open";
        return -1;
    }

    if (pixels == 0) {
        nx_err = "compiled sprite has no pixels";
        return -1;
    }

    if (transparent < 0 || transparent > 255) {
        nx_err = "invalid sprite transparency value";
        return -1;
    }

    if (!nanox_sprite_sizes_ok(w,
                               h,
                               frames,
                               &pix_h,
                               &pixel_count)) {
        nx_err = "compiled sprite dimensions are too large";
        return -1;
    }

    /*
     * pixel_count is produced as part of the overflow validation above.
     */
    (void)pixel_count;

    cs = &nx_compiled[id];

    /*
     * gfx_define_sprite() releases a compiled representation whenever
     * the sprite definition changes. Therefore matching metadata here
     * represents an unchanged sprite.
     */
    if (cs->used &&
        cs->pixmap &&
        cs->w == (unsigned char)w &&
        cs->h == (unsigned char)h &&
        cs->frames == (unsigned char)frames &&
        cs->transparent == (unsigned char)transparent) {
        if (transparent == GFX_NO_TRANSPARENT ||
            (cs->frame_run_start != 0 &&
             (cs->run_count == 0 || cs->runs != 0))) {
            nx_err = "no error";
            return 0;
        }
    }

    if (transparent == GFX_NO_TRANSPARENT) {
        counted_runs = 0;
    } else {
        counted_runs =
            nanox_count_opaque_runs(pixels,
                                    w,
                                    h,
                                    frames,
                                    transparent);
    }

    if (counted_runs > NANOX_COMPILED_MAX_RUNS) {
        nx_err = "compiled sprite run limit exceeded";
        return -1;
    }

    if (nx_compiled_run_used > NANOX_COMPILED_MAX_RUNS) {
        nx_err = "compiled sprite run accounting error";
        return -1;
    }

    run_budget =
        (unsigned int)NANOX_COMPILED_MAX_RUNS -
        (unsigned int)nx_compiled_run_used;

    /*
     * The old representation for this ID will be released only after
     * the replacement has been built successfully.
     */
    if (cs->used) {
        if (cs->run_count > nx_compiled_run_used) {
            nx_err = "compiled sprite run accounting error";
            return -1;
        }

        run_budget += (unsigned int)cs->run_count;
    }

    if (counted_runs > run_budget) {
        nx_err = "compiled sprite run limit exceeded";
        return -1;
    }

    run_count = (unsigned short)counted_runs;
    runs = 0;
    frame_run_start = 0;
    pixmap = 0;

    /*
     * Transparent sprites always need the frame index, even when every
     * frame is completely transparent and run_count is zero.
     */
    if (transparent != GFX_NO_TRANSPARENT) {
        frame_index_count = (unsigned int)frames + 1U;

        if (frame_index_count >
            65535U / (unsigned int)sizeof(unsigned short)) {
            nx_err = "compiled sprite frame index overflow";
            return -1;
        }

        index_bytes =
            frame_index_count *
            (unsigned int)sizeof(unsigned short);

        frame_run_start =
            (unsigned short *)malloc((size_t)index_bytes);

        if (frame_run_start == 0) {
            nx_err = "cannot allocate compiled sprite frame index";
            return -1;
        }

        if (run_count > 0) {
            if ((unsigned int)run_count >
                65535U /
                (unsigned int)sizeof(struct nanox_opaque_run)) {
                free(frame_run_start);
                nx_err = "compiled sprite run allocation overflow";
                return -1;
            }

            run_bytes =
                (unsigned int)run_count *
                (unsigned int)sizeof(struct nanox_opaque_run);

            runs =
                (struct nanox_opaque_run *)malloc((size_t)run_bytes);

            if (runs == 0) {
                free(frame_run_start);
                nx_err = "cannot allocate compiled sprite runs";
                return -1;
            }
        }

        if (nanox_build_opaque_runs(runs,
                                    frame_run_start,
                                    pixels,
                                    w,
                                    h,
                                    frames,
                                    transparent,
                                    run_count) != 0) {
            if (runs != 0)
                free(runs);

            free(frame_run_start);

            nx_err = "compiled sprite span build failed";
            return -1;
        }
    }

    pixmap = GrNewPixmapEx(w, (int)pix_h, 0, 0);

    if (!pixmap) {
        if (runs != 0)
            free(runs);

        if (frame_run_start != 0)
            free(frame_run_start);

        nx_err = "cannot allocate compiled sprite pixmap";
        return -1;
    }

    if (nanox_upload_sprite_to_pixmap(pixmap,
                                      pixels,
                                      w,
                                      h,
                                      frames,
                                      transparent) != 0) {
        GrDestroyWindow(pixmap);

        if (runs != 0)
            free(runs);

        if (frame_run_start != 0)
            free(frame_run_start);

        nx_err = "compiled sprite upload failed";
        return -1;
    }

    /*
     * Everything succeeded. Only now replace the old representation.
     */
    nanox_free_compiled_sprite(id);

    cs->used = 1;
    cs->w = (unsigned char)w;
    cs->h = (unsigned char)h;
    cs->frames = (unsigned char)frames;
    cs->transparent = (unsigned char)transparent;
    cs->pixmap = pixmap;
    cs->runs = runs;
    cs->frame_run_start = frame_run_start;
    cs->run_count = run_count;

    nx_compiled_run_used =
        (unsigned short)((unsigned int)nx_compiled_run_used +
                         (unsigned int)run_count);

    nx_err = "no error";
    return 0;
}

int nanox_draw_compiled_sprite(int id,
                               int x,
                               int y,
                               int frame,
                               int flip_x)
{
    struct nanox_compiled_sprite *cs;
    int src_y;

    /*
     * No second server-side horizontally flipped copy is allocated.
     * The graphics layer will fall back to nanox_draw_bitmap().
     */
    if (flip_x)
        return -1;

    if (id < 0 || id >= GFX_MAX_SPRITES)
        return -1;

    if (!nx_opened || !nx_win || !nx_gc)
        return -1;

    cs = &nx_compiled[id];

    if (!cs->used || !cs->pixmap)
        return -1;

    if (frame < 0 || frame >= (int)cs->frames)
        frame = 0;

    /*
     * A completely off-screen compiled sprite is still a successful
     * compiled draw. Do not invoke the slow bitmap fallback.
     */
    if (x >= nx_w ||
        y >= nx_h ||
        x + (int)cs->w <= 0 ||
        y + (int)cs->h <= 0) {
        return 0;
    }

    src_y = frame * (int)cs->h;

    if (cs->transparent == GFX_NO_TRANSPARENT) {
        /*
         * nanox_blit_compiled_area() performs window-edge clipping.
         * A clipped-away copy is still a valid compiled no-op.
         */
        nanox_blit_compiled_area(cs->pixmap,
                                 0,
                                 src_y,
                                 x,
                                 y,
                                 (int)cs->w,
                                 (int)cs->h);

        return 0;
    }

    if (cs->frame_run_start == 0)
        return -1;

    {
        unsigned short start;
        unsigned short end;
        unsigned short i;

        start = cs->frame_run_start[frame];
        end = cs->frame_run_start[frame + 1];

        if (start > end || end > cs->run_count)
            return -1;

        /*
         * A fully transparent frame contains no runs. It is a successful
         * compiled no-op, not a reason to use the bitmap fallback.
         */
        if (start == end)
            return 0;

        if (cs->runs == 0)
            return -1;

        for (i = start; i < end; i++) {
            const struct nanox_opaque_run *run;
            int dst_x;
            int dst_y;
            int src_x;
            int src_row_y;

            run = &cs->runs[i];

            dst_x = x + (int)run->x;
            dst_y = y + (int)run->y;

            src_x = (int)run->x;
            src_row_y = src_y + (int)run->y;

            nanox_blit_compiled_area(cs->pixmap,
                                     src_x,
                                     src_row_y,
                                     dst_x,
                                     dst_y,
                                     (int)run->length,
                                     1);
        }
    }

    return 0;
}

static int nanox_redraw_from_background(void)
{
    if (!nx_opened || !nx_win || !nx_gc || !nx_bg)
        return 0;

    GrCopyArea(nx_win, nx_gc,
               0, 0, nx_w, nx_h,
               nx_bg,
               0, 0,
               MWROP_COPY);

    nanox_mark_commands_pending();
    return 1;
}

static void nanox_handle_event(GR_EVENT *ev)
{
    if (ev == 0)
        return;

    switch (ev->type) {
    case GR_EVENT_TYPE_EXPOSURE:
        /*
         * Restore immediately only when a complete cached background
         * exists. In the usual low-memory configuration nx_bg is zero,
         * so the next Lua frame repaints the window.
         */
        if (nanox_redraw_from_background())
            nanox_flush_pending();
        break;

    case GR_EVENT_TYPE_CLOSE_REQ:
        /*
         * Preserve the close request until gfx.keypressed() reads it.
         * nanox_get_key() will expose it as MWKEY_ESCAPE.
         */
        nx_close_pending = 1;
        break;

    case GR_EVENT_TYPE_KEY_DOWN:
        /*
         * Lua decides what Escape, Q and every other key mean.
         * Do not convert them into an error here.
         */
        nanox_queue_key(ev->keystroke.ch);
        break;

    default:
        break;
    }
}

static void nanox_drain_events(void)
{
    GR_EVENT ev;

    if (!nx_opened)
        return;

    for (;;) {
        memset(&ev, 0, sizeof(ev));

        GrGetNextEventTimeout(&ev, GR_TIMEOUT_POLL);

        if (ev.type == GR_EVENT_TYPE_NONE ||
            ev.type == GR_EVENT_TYPE_TIMEOUT) {
            break;
        }

        nanox_handle_event(&ev);
    }
}

static void nanox_process_events(unsigned int timeout_ms)
{
    GR_EVENT ev;
    unsigned long start_ms;
    unsigned long now_ms;
    unsigned long elapsed_ms;
    unsigned long remaining_ms;

    if (!nx_opened)
        return;

    /*
     * Zero means poll and buffer everything currently available without
     * waiting.
     */
    if (timeout_ms == 0) {
        nanox_drain_events();
        return;
    }

    start_ms = nanox_current_ms();
    remaining_ms = (unsigned long)timeout_ms;

    for (;;) {
        memset(&ev, 0, sizeof(ev));

        GrGetNextEventTimeout(&ev, remaining_ms);

        /*
         * A timeout means the remaining delay has completed.
         * Drain events that may have arrived at the timeout boundary.
         */
        if (ev.type == GR_EVENT_TYPE_NONE ||
            ev.type == GR_EVENT_TYPE_TIMEOUT) {
            nanox_drain_events();
            return;
        }

        nanox_handle_event(&ev);

        /*
         * Buffer every other event that arrived with the first one.
         */
        nanox_drain_events();

        now_ms = nanox_current_ms();
        elapsed_ms = now_ms - start_ms;

        if (elapsed_ms >= (unsigned long)timeout_ms)
            return;

        remaining_ms =
            (unsigned long)timeout_ms - elapsed_ms;
    }
}

int nanox_open(int w, int h)
{
    if (w <= 0)
        w = 320;

    if (h <= 0)
        h = 200;

    soft_palette_defaults();

    nanox_reset_input_queue();
    nx_commands_pending = 0;

    if (GrOpen() < 0) {
        nx_err = "cannot open Nano-X";
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

        nx_err = "cannot create Nano-X window";
        return -1;
    }

    nx_gc = GrNewGC();

    if (!nx_gc) {
        GrDestroyWindow(nx_win);
        GrClose();

        nx_win = 0;
        nx_err = "cannot create Nano-X GC";
        return -1;
    }

    /*
     * Avoid a full-screen background pixmap on ELKS. Moving sprites use
     * the smaller reusable save-under pixmap instead.
     */
    nx_bg = 0;

    GrSelectEvents(nx_win,
                   GR_EVENT_MASK_EXPOSURE |
                   GR_EVENT_MASK_KEY_DOWN |
                   GR_EVENT_MASK_CLOSE_REQ);

    GrMapWindow(nx_win);

    /*
     * Window mapping must be sent immediately.
     */
    GrFlush();

    nx_w = w;
    nx_h = h;
    nx_opened = 1;

    nx_fg_valid = 0;
    nx_fg_color = 0;
	
    /*
     * GrFlush() has sent the window-map request, and there are no
     * unflushed application drawing commands at this point.
     */
    nx_commands_pending = 0;

    /*
     * Allow Nano-X to complete the initial window mapping and exposure
     * before returning to Lua. Otherwise, Lua may draw its first frame
     * before the new window is ready, causing that frame not to appear.
     *
     * Any key presses or close request received during this short wait
     * are stored in the already-cleared input queue and remain available
     * to Lua.
     *
     * This fixed startup delay can be further optimized by waiting until
     * the initial exposure event is actually received, with a timeout to
     * avoid blocking indefinitely if no exposure event arrives.
     */
    nanox_process_events(NANOX_INITIAL_MAP_DELAY_MS);

    nx_err = "no error";

    return 0;
}

void nanox_close(void)
{
    /*
     * First send pending drawing and pixmap-upload commands while all
     * source and destination resources still exist.
     */
    nanox_flush_pending();

    /*
     * Then release permanent compiled sprite pixmaps and their client
     * metadata before closing the Nano-X connection.
     */
    nanox_free_all_compiled_sprites();

    if (nx_save_pix) {
        GrDestroyWindow(nx_save_pix);
        nx_save_pix = 0;
    }

    nx_save_alloc_w = 0;
    nx_save_alloc_h = 0;
    nx_save_valid = 0;

    if (nx_bg)
        GrDestroyWindow(nx_bg);

    if (nx_gc)
        GrDestroyGC(nx_gc);

    if (nx_win)
        GrDestroyWindow(nx_win);

    if (nx_opened)
        GrClose();

    nx_win = 0;
    nx_gc = 0;
    nx_bg = 0;

    nx_w = 0;
    nx_h = 0;
    nx_opened = 0;

    nx_fg_valid = 0;
    nx_fg_color = 0;

    nx_commands_pending = 0;
    nanox_reset_input_queue();

    nx_err = "no error";
}

const char *nanox_error(void)
{
    return nx_err;
}

int nanox_width(void)
{
    return nx_w;
}

int nanox_height(void)
{
    return nx_h;
}

void nanox_present(void)
{
    if (!nx_opened)
        return;

    /*
     * Nano-X is single-buffered. Present sends only commands accumulated
     * since the previous flush. It does not sleep or consume events.
     */
    nanox_flush_pending();
}

int nanox_get_key(GR_KEY *key)
{
    if (!nx_opened || key == 0)
        return 0;

    /*
     * Avoid polling Nano-X while a frame is being constructed.
     * Nano-X event polling may implicitly flush pending drawing requests.
     */
    if (!nx_close_pending &&
        nx_key_queue_count == 0 &&
        !nx_commands_pending) {
        nanox_process_events(0);
    }

    /*
     * A window close request has priority and is returned once as Escape.
     */
    if (nx_close_pending) {
        nx_close_pending = 0;
        *key = MWKEY_ESCAPE;
        return 1;
    }

    return nanox_pop_key(key);
}

int nanox_sleep_ms(unsigned int ms)
{
    if (!nx_opened)
        return 0;

    /*
     * This is normally already done by gfx.present(). It also supports
     * Lua code that draws and then calls gfx.sleep() without present().
     */
    nanox_flush_pending();

    /*
     * Wait for the complete requested duration while buffering keyboard,
     * exposure and close events.
     *
     * A zero delay performs only a nonblocking event drain.
     */
    nanox_process_events(ms);

    return 0;
}

void nanox_clear(int color)
{
    unsigned char c;

    if (!nx_opened)
        return;

    c = (unsigned char)color;
    nanox_set_fg(c);
    GrFillRect(nx_win, nx_gc, 0, 0, nx_w, nx_h);
    nanox_mark_commands_pending();
}

void nanox_pixel(int x, int y, int color)
{
    unsigned char c;

    if (!nx_opened)
        return;

    if (x < 0 || y < 0 || x >= nx_w || y >= nx_h)
        return;

    c = (unsigned char)color;
    nanox_set_fg(c);
    GrPoint(nx_win, nx_gc, x, y);
    nanox_mark_commands_pending();
}

void nanox_line(int x0, int y0, int x1, int y1, int color)
{
    unsigned char c;

    if (!nx_opened)
        return;

    c = (unsigned char)color;
    nanox_set_fg(c);
    GrLine(nx_win, nx_gc, x0, y0, x1, y1);
    nanox_mark_commands_pending();
}

void nanox_hline(int x, int y, int w, unsigned char c)
{
    if (!nx_opened)
        return;

    if (w <= 0 || y < 0 || y >= nx_h)
        return;

    if (x < 0) {
        w += x;
        x = 0;
    }

    if (x >= nx_w || w <= 0)
        return;

    if (x + w > nx_w)
        w = nx_w - x;

    nanox_set_fg(c);
    GrLine(nx_win, nx_gc, x, y, x + w - 1, y);
    nanox_mark_commands_pending();
}

static void nanox_vline(int x, int y, int h, unsigned char c)
{
    if (!nx_opened)
        return;

    if (h <= 0 || x < 0 || x >= nx_w)
        return;

    if (y < 0) {
        h += y;
        y = 0;
    }

    if (y >= nx_h || h <= 0)
        return;

    if (y + h > nx_h)
        h = nx_h - y;

    nanox_set_fg(c);
    GrLine(nx_win, nx_gc, x, y, x, y + h - 1);
    nanox_mark_commands_pending();
}

void nanox_rect(int x, int y, int w, int h, int color)
{
    unsigned char c;

    if (!nx_opened || w <= 0 || h <= 0)
        return;

    c = (unsigned char)color;

    nanox_hline(x, y, w, c);
    nanox_hline(x, y + h - 1, w, c);
    nanox_vline(x, y, h, c);
    nanox_vline(x + w - 1, y, h, c);
}

void nanox_fill(int x, int y, int w, int h, int color)
{
    unsigned char c;

    if (!nx_opened)
        return;

    if (!clip_rect(&x, &y, &w, &h))
        return;

    c = (unsigned char)color;
    nanox_set_fg(c);
    GrFillRect(nx_win, nx_gc, x, y, w, h);
    nanox_mark_commands_pending();
}

void nanox_draw_bitmap(const unsigned char *pixels,
                       int bw,
                       int bh,
                       int transparent,
                       int x,
                       int y,
                       int flip_x)
{
    int row;

    if (!nx_opened)
        return;

    if (pixels == 0 || bw <= 0 || bh <= 0)
        return;

    for (row = 0; row < bh; row++) {
        int sy;
        int col;

        sy = y + row;

        if (sy < 0 || sy >= nx_h)
            continue;

        col = 0;

        while (col < bw) {
            int src_col;
            int run_start;
            int run_len;
            unsigned int offset;
            unsigned char c;
            unsigned char run_color;

            src_col = flip_x ? (bw - 1 - col) : col;

            offset =
                (unsigned int)row * (unsigned int)bw +
                (unsigned int)src_col;

            c = pixels[offset];

            if (nanox_pixel_is_transparent(transparent, c)) {
                col++;
                continue;
            }

            run_start = col;
            run_len = 1;
            run_color = c;
            col++;

            while (col < bw) {
                src_col = flip_x ? (bw - 1 - col) : col;

                offset =
                    (unsigned int)row * (unsigned int)bw +
                    (unsigned int)src_col;

                c = pixels[offset];

                if (nanox_pixel_is_transparent(transparent, c) ||
                    c != run_color) {
                    break;
                }

                run_len++;
                col++;
            }

            nanox_hline(x + run_start,
                        sy,
                        run_len,
                        run_color);
        }
    }
}

void nanox_set_background(void)
{
    if (!nx_opened || !nx_win || !nx_gc || !nx_bg)
        return;

    /*
     * Copy current window contents into the background cache.
     * This is the Nano-X equivalent of:
     *   Mode X draw page -> PAGE2
     */
    GrCopyArea(nx_bg, nx_gc,
               0, 0, nx_w, nx_h,
               nx_win,
               0, 0,
               MWROP_COPY);

    nanox_mark_commands_pending();
}

void nanox_restore(int x, int y, int w, int h)
{
    if (!nx_opened || !nx_win || !nx_gc)
        return;

    /*
     * If a full background pixmap exists, use it.
     * Usually on ELKS nx_bg == 0, so use save-under instead.
     */
    if (!nx_bg) {
        nanox_restore_saved();
        return;
    }

    if (!clip_rect(&x, &y, &w, &h))
        return;

    GrCopyArea(nx_win, nx_gc,
               x, y, w, h,
               nx_bg,
               x, y,
               MWROP_COPY);

    nanox_mark_commands_pending();
}

void nanox_copy_rect(int src_page, int dst_page,
                     int x, int y, int w, int h)
{
    if (!nx_opened)
        return;

    /*
     * Nano-X has no real page flipping here.
     * We only emulate the useful cases:
     *
     *   BACKGROUND -> DRAW/VISIBLE : restore
     *   DRAW/VISIBLE -> BACKGROUND : cache
     */

    if (src_page == GFX_PAGE_BACKGROUND) {
        nanox_restore(x, y, w, h);
        return;
    }

    if (dst_page == GFX_PAGE_BACKGROUND) {
        nanox_set_background();
        return;
    }

    /*
     * Other page combinations are ignored in Nano-X backend.
     */
}

int nanox_save_under(int x, int y, int w, int h)
{
    if (!nx_opened || !nx_win || !nx_gc)
        return -1;

    /*
     * Nano-X is single-buffered.
     * Restore previous saved sprite area before saving the new one.
     */
    if (nx_save_valid)
        nanox_restore_saved();

    if (!clip_rect(&x, &y, &w, &h))
        return -1;

    if (!nx_save_pix || w > nx_save_alloc_w || h > nx_save_alloc_h) {
        if (nx_save_pix)
            GrDestroyWindow(nx_save_pix);

        nx_save_pix = GrNewPixmapEx(w, h, 0, 0);
        if (!nx_save_pix) {
            nx_save_valid = 0;
            nx_save_alloc_w = 0;
            nx_save_alloc_h = 0;
            return -1;
        }

        nx_save_alloc_w = w;
        nx_save_alloc_h = h;
    }

    GrCopyArea(nx_save_pix, nx_gc,
               0, 0, w, h,
               nx_win,
               x, y,
               MWROP_COPY);

    nanox_mark_commands_pending();

    nx_save_x = x;
    nx_save_y = y;
    nx_save_w = w;
    nx_save_h = h;
    nx_save_valid = 1;

    return 0;
}

void nanox_restore_saved(void)
{
    if (!nx_opened || !nx_win || !nx_gc)
        return;

    if (!nx_save_valid || !nx_save_pix)
        return;

    GrCopyArea(nx_win, nx_gc,
               nx_save_x,
               nx_save_y,
               nx_save_w,
               nx_save_h,
               nx_save_pix,
               0, 0,
               MWROP_COPY);

    nanox_mark_commands_pending();

    nx_save_valid = 0;
}
