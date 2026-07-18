#include "nanox.h"
#include "graphics.h"

#include "nano-X.h"

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

    nx_commands_pending = 0;
    nanox_reset_input_queue();

    nx_err = "no error";

    return 0;
}

void nanox_close(void)
{
    /*
     * Send any final pending drawing commands before resources are
     * destroyed.
     */
    nanox_flush_pending();

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

            nanox_hline(x + run_start, sy, run_len, run_color);
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
