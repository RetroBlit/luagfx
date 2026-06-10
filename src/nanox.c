#include "nanox.h"
#include "graphics.h"

#include "nano-X.h"

#include <string.h>

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
static int nx_quit_requested = 0;

static const char *nx_err = "no error";

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

static void nanox_redraw_from_background(void)
{
    if (!nx_opened || !nx_win || !nx_gc || !nx_bg)
        return;

    GrCopyArea(nx_win, nx_gc,
               0, 0, nx_w, nx_h,
               nx_bg,
               0, 0,
               MWROP_COPY);
}

static void nanox_handle_event(GR_EVENT *ev)
{
    switch (ev->type) {
    case GR_EVENT_TYPE_EXPOSURE:
        /*
         * Old code cleared the window here.
         * That destroys the cached scene visually.
         * Restore from background instead.
         */
        nanox_redraw_from_background();
        GrFlush();
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

static int nanox_process_events(unsigned int timeout_ms)
{
    GR_EVENT ev;

    if (!nx_opened)
        return 0;

    memset(&ev, 0, sizeof(ev));
    GrGetNextEventTimeout(&ev, timeout_ms);

    if (ev.type != 0)
        nanox_handle_event(&ev);

    if (nx_quit_requested) {
        nx_err = "Interrupted";
        return -1;
    }

    return 0;
}

int nanox_open(int w, int h)
{
    if (w <= 0)
        w = 320;
    if (h <= 0)
        h = 200;

    soft_palette_defaults();

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
     * Background cache pixmap.
     * Used by nanox_set_background() and nanox_restore().
     */
    nx_bg = GrNewPixmap(w, h, 0);
    if (!nx_bg) {
        /*(nx_gc);
        GrDestroyWindow(nx_win);
        GrClose();
        nx_gc = 0;
        nx_win = 0;
        nx_err = "cannot create Nano-X background pixmap";
        return -1;*/
		nx_err = "Nano-X background pixmap disabled";
    }

    GrSelectEvents(nx_win,
                   GR_EVENT_MASK_EXPOSURE |
                   GR_EVENT_MASK_KEY_DOWN |
                   GR_EVENT_MASK_CLOSE_REQ);

    GrMapWindow(nx_win);
    GrFlush();

    nx_w = w;
    nx_h = h;
    nx_opened = 1;
    nx_quit_requested = 0;
    nx_err = "no error";

    return 0;
}

void nanox_close(void)
{
    if (nx_gc)
        GrDestroyGC(nx_gc);

    if (nx_bg)
        GrDestroyWindow(nx_bg);

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
    nx_quit_requested = 0;
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

    GrFlush();

    /*
     * Do not use timeout 0 here.
     * On this Nano-X/ELKS build it can block until an event.
     * Timeout 1 lets the game continue even without mouse movement.
     */
    nanox_process_events(1);
}

int nanox_sleep_ms(unsigned int ms)
{
    if (ms == 0)
        ms = 1;

    return nanox_process_events(ms);
}

void nanox_clear(int color)
{
    unsigned char c;

    if (!nx_opened)
        return;

    c = (unsigned char)color;
    GrSetGCForeground(nx_gc, nx_color(c));
    GrFillRect(nx_win, nx_gc, 0, 0, nx_w, nx_h);
}

void nanox_pixel(int x, int y, int color)
{
    unsigned char c;

    if (!nx_opened)
        return;

    if (x < 0 || y < 0 || x >= nx_w || y >= nx_h)
        return;

    c = (unsigned char)color;
    GrSetGCForeground(nx_gc, nx_color(c));
    GrPoint(nx_win, nx_gc, x, y);
}

void nanox_line(int x0, int y0, int x1, int y1, int color)
{
    unsigned char c;

    if (!nx_opened)
        return;

    c = (unsigned char)color;
    GrSetGCForeground(nx_gc, nx_color(c));
    GrLine(nx_win, nx_gc, x0, y0, x1, y1);
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

    GrSetGCForeground(nx_gc, nx_color(c));
    GrLine(nx_win, nx_gc, x, y, x + w - 1, y);
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

    GrSetGCForeground(nx_gc, nx_color(c));
    GrLine(nx_win, nx_gc, x, y, x, y + h - 1);
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
    GrSetGCForeground(nx_gc, nx_color(c));
    GrFillRect(nx_win, nx_gc, x, y, w, h);
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
}

void nanox_restore(int x, int y, int w, int h)
{
    if (!nx_opened || !nx_win || !nx_gc || !nx_bg)
        return;

    if (!clip_rect(&x, &y, &w, &h))
        return;

    /*
     * Restore rectangle from background pixmap to window.
     * This fixes the Nano-X sprite trail problem.
     */
    GrCopyArea(nx_win, nx_gc,
               x, y, w, h,
               nx_bg,
               x, y,
               MWROP_COPY);
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