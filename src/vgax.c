/*
Assembly style is OpenWatcom 16 bit
*/
#include "vgax.h"

#define VGA_SC_INDEX     0x3C4
#define VGA_SC_DATA      0x3C5
#define VGA_CRTC_INDEX   0x3D4
#define VGA_CRTC_DATA    0x3D5
#define VGA_INPUT_STAT   0x3DA
#define VGA_DAC_WRITE    0x3C8
#define VGA_DAC_DATA     0x3C9
#define VGA_GC_INDEX     0x3CE
#define VGA_GC_DATA      0x3CF

static unsigned char current_plane_mask = 0xFF;

static unsigned char inb(unsigned short port)
{
    unsigned char val = 0;
    _asm {
        push dx
        mov dx, [port]
        in  al, dx
        mov [val], al
        pop dx
    }
    return val;
}

static void outb(unsigned char val, unsigned short port)
{
    _asm {
        push ax
        push dx
        mov dx, [port]
        mov al, [val]
        out dx, al
        pop dx
        pop ax
    }
}

static void bios_set_video_mode(unsigned char mode)
{
    unsigned short mode_int;
    mode_int = (unsigned short)mode;
    _asm {
        push ax
        push bx
        push cx
        push dx
        push si
        push di
        push bp
        push es
        mov ax, [mode_int]
        int 10h
        pop es
        pop bp
        pop di
        pop si
        pop dx
        pop cx
        pop bx
        pop ax
    }
}

static void set_plane_mask(unsigned char mask)
{
    /* Read tracking variable directly out of Code Segment far memory */
    if (mask == current_plane_mask)
        return;

    outb(0x02, VGA_SC_INDEX);
    outb(mask, VGA_SC_DATA);
    current_plane_mask = mask;
}

static void set_write_plane(unsigned char plane)
{
    set_plane_mask((unsigned char)(1 << (plane & 3)));
}

static void set_read_plane(unsigned char plane)
{
    outb(0x04, VGA_GC_INDEX);
    outb((unsigned char)(plane & 3), VGA_GC_DATA);
}

static void vram_copy_bytes(unsigned short src,
                            unsigned short dst,
                            unsigned short len)
{
    if (len == 0 || src == dst)
        return;

    _asm {
        push ax
        push cx
        push si
        push di
        push ds
        push es

        cld

        mov si, [src]
        mov di, [dst]
        mov cx, [len]

        mov ax, 0A000h
        mov ds, ax
        mov es, ax

        rep movsb

        pop es
        pop ds
        pop di
        pop si
        pop cx
        pop ax
    }
}

static void vram_poke(unsigned short off, unsigned char val)
{
    _asm {
        push ax
        push di
        push es
        mov ax, 0A000h
        mov es, ax
        mov di, [off]
        mov al, [val]
        mov es:[di], al
        pop es
        pop di
        pop ax
    }
}

static void vram_memset(unsigned short off, unsigned short len, unsigned char val)
{
    if (len == 0)
        return;

    _asm {
        push ax
        push cx
        push di
        push es
        cld
        mov ax, 0A000h
        mov es, ax
        mov di, [off]
        mov cx, [len]
        mov al, [val]
        rep stosb
        pop es
        pop di
        pop cx
        pop ax
    }
}

static void vram_vline(unsigned short off, unsigned short h, unsigned char val)
{
    if (h == 0)
        return;

    _asm {
        push ax
        push cx
        push di
        push es
        cld
        mov ax, 0A000h
        mov es, ax
        mov di, [off]
        mov cx, [h]
        mov al, [val]
    vram_vline_loop:
        mov es:[di], al
        add di, 80          /* Mode X width stride line delta offset factor */
        loop vram_vline_loop
        pop es
        pop di
        pop cx
        pop ax
    }
}

void vgax_set_palette(unsigned char index,
                      unsigned char r,
                      unsigned char g,
                      unsigned char b)
{
    outb(index, VGA_DAC_WRITE);
    outb(r, VGA_DAC_DATA);
    outb(g, VGA_DAC_DATA);
    outb(b, VGA_DAC_DATA);
}

void vgax_set_default_palette(void)
{
    /* Explicitly mapping colors manually to prevent large array memory overheads */
    vgax_set_palette(0,  0,  0,  0);   /* transparent / black */
    vgax_set_palette(1, 22, 42, 63);   /* sky blue */
    vgax_set_palette(2,  8, 45, 10);   /* grass */
    vgax_set_palette(3, 34, 18,  7);   /* dirt */
    vgax_set_palette(4, 45, 12,  8);   /* brick red */
    vgax_set_palette(5, 63, 43, 25);   /* skin */
    vgax_set_palette(6, 58,  4,  4);   /* hero red */
    vgax_set_palette(7,  6, 16, 50);   /* overalls blue */
    vgax_set_palette(8, 63, 55,  5);   /* yellow */
    vgax_set_palette(9, 20,  9,  2);   /* dark brown */
    vgax_set_palette(10, 63, 63, 63);  /* white */
    vgax_set_palette(11,  5,  5,  5);  /* outline */
    vgax_set_palette(12, 34, 34, 34);  /* gray */
    vgax_set_palette(13,  4, 25,  5);  /* dark green */
    vgax_set_palette(14, 15,  8,  3);  /* shadow brown */
    vgax_set_palette(15, 63, 63, 32);  /* highlight */
}

void vgax_wait_vsync(void)
{
    while (inb(VGA_INPUT_STAT) & 0x08)
        ;
    while (!(inb(VGA_INPUT_STAT) & 0x08))
        ;
}

void vgax_flip(unsigned short page)
{
    outb(0x0C, VGA_CRTC_INDEX);
    outb((unsigned char)(page >> 8), VGA_CRTC_DATA);

    outb(0x0D, VGA_CRTC_INDEX);
    outb((unsigned char)(page & 0xFF), VGA_CRTC_DATA);
}

void vgax_init(void)
{
    bios_set_video_mode(0x13);

    /* Disable chain-4. This turns mode 13h into planar Mode X memory. */
    outb(0x04, VGA_SC_INDEX);
    outb(0x06, VGA_SC_DATA);

    current_plane_mask = 0xFF;
    set_plane_mask(0x0F);

    /* Unlock CRTC register 0x11. */
    outb(0x11, VGA_CRTC_INDEX);
    outb((unsigned char)(inb(VGA_CRTC_DATA) & 0x7F), VGA_CRTC_DATA);

    /* 320x240 Mode X CRTC timing values. */
    outb(0x06, VGA_CRTC_INDEX); outb(0x0D, VGA_CRTC_DATA);
    outb(0x07, VGA_CRTC_INDEX); outb(0x3E, VGA_CRTC_DATA);
    outb(0x09, VGA_CRTC_INDEX); outb(0x41, VGA_CRTC_DATA);
    outb(0x10, VGA_CRTC_INDEX); outb(0xEA, VGA_CRTC_DATA);
    outb(0x11, VGA_CRTC_INDEX); outb(0xAC, VGA_CRTC_DATA);
    outb(0x12, VGA_CRTC_INDEX); outb(0xDF, VGA_CRTC_DATA);
    outb(0x13, VGA_CRTC_INDEX); outb(0x28, VGA_CRTC_DATA);
    outb(0x14, VGA_CRTC_INDEX); outb(0x00, VGA_CRTC_DATA);
    outb(0x15, VGA_CRTC_INDEX); outb(0xE7, VGA_CRTC_DATA);
    outb(0x16, VGA_CRTC_INDEX); outb(0x06, VGA_CRTC_DATA);
    outb(0x17, VGA_CRTC_INDEX); outb(0xE3, VGA_CRTC_DATA);

    vgax_set_default_palette();

    vgax_clear_page(VGAX_PAGE0, 0);
    vgax_clear_page(VGAX_PAGE1, 0);
    vgax_clear_page(VGAX_PAGE2, 0);
    vgax_flip(VGAX_PAGE0);
}

void vgax_text_mode(void)
{
    bios_set_video_mode(0x03);
}

void vgax_clear_page(unsigned short page, unsigned char color)
{
    set_plane_mask(0x0F);
    vram_memset(page, VGAX_PAGE_BYTES, color);
}

void vgax_plot(unsigned short page, int x, int y, unsigned char color)
{
    unsigned short off;

    if (x < 0 || x >= VGAX_W || y < 0 || y >= VGAX_H)
        return;

    off = (unsigned short)(page + y * VGAX_BYTES_PER_LINE + (x >> 2));
    set_write_plane((unsigned char)(x & 3));
    vram_poke(off, color);
}

void vgax_hline(unsigned short page, int x, int y, int w, unsigned char color)
{
    int last;
    int plane;

    if (w <= 0 || y < 0 || y >= VGAX_H)
        return;

    if (x < 0) {
        w += x;
        x = 0;
    }

    if (x >= VGAX_W || w <= 0)
        return;

    if (x + w > VGAX_W)
        w = VGAX_W - x;

    last = x + w - 1;

    for (plane = 0; plane < 4; plane++) {
        int first;
        int count;
        unsigned short off;

        first = x + ((plane - (x & 3) + 4) & 3);
        if (first > last)
            continue;

        count = ((last - first) >> 2) + 1;
        off = (unsigned short)(page + y * VGAX_BYTES_PER_LINE + (first >> 2));

        set_write_plane((unsigned char)plane);
        vram_memset(off, (unsigned short)count, color);
    }
}

void vgax_vline(unsigned short page, int x, int y, int h, unsigned char color)
{
    unsigned short off;

    if (h <= 0 || x < 0 || x >= VGAX_W)
        return;

    if (y < 0) {
        h += y;
        y = 0;
    }

    if (y >= VGAX_H || h <= 0)
        return;

    if (y + h > VGAX_H)
        h = VGAX_H - y;

    off = (unsigned short)(page + y * VGAX_BYTES_PER_LINE + (x >> 2));
    set_write_plane((unsigned char)(x & 3));
    vram_vline(off, (unsigned short)h, color);
}

void vgax_fill_rect(unsigned short page, int x, int y,
                    int w, int h, unsigned char color)
{
    int last;
    int plane;

    if (w <= 0 || h <= 0)
        return;

    if (x < 0) {
        w += x;
        x = 0;
    }

    if (y < 0) {
        h += y;
        y = 0;
    }

    if (x >= VGAX_W || y >= VGAX_H || w <= 0 || h <= 0)
        return;

    if (x + w > VGAX_W)
        w = VGAX_W - x;

    if (y + h > VGAX_H)
        h = VGAX_H - y;

    last = x + w - 1;

    for (plane = 0; plane < 4; plane++) {
        int first;
        int count;
        int row;
        unsigned short off;

        first = x + ((plane - (x & 3) + 4) & 3);
        if (first > last)
            continue;

        count = ((last - first) >> 2) + 1;
        off = (unsigned short)(page + y * VGAX_BYTES_PER_LINE + (first >> 2));

        set_write_plane((unsigned char)plane);
        for (row = 0; row < h; row++) {
            vram_memset((unsigned short)(off + row * VGAX_BYTES_PER_LINE),
                        (unsigned short)count,
                        color);
        }
    }
}

static int vgax_abs(int x)
{
    return x < 0 ? -x : x;
}

void vgax_line(unsigned short page, int x0, int y0,
               int x1, int y1, unsigned char color)
{
    int dx;
    int dy;
    int sx;
    int sy;
    int err;

    if (y0 == y1) {
        if (x1 < x0)
            vgax_hline(page, x1, y0, x0 - x1 + 1, color);
        else
            vgax_hline(page, x0, y0, x1 - x0 + 1, color);
        return;
    }

    if (x0 == x1) {
        if (y1 < y0)
            vgax_vline(page, x0, y1, y0 - y1 + 1, color);
        else
            vgax_vline(page, x0, y0, y1 - y0 + 1, color);
        return;
    }

    dx = vgax_abs(x1 - x0);
    sx = x0 < x1 ? 1 : -1;
    dy = -vgax_abs(y1 - y0);
    sy = y0 < y1 ? 1 : -1;
    err = dx + dy;

    for (;;) {
        int e2;

        vgax_plot(page, x0, y0, color);

        if (x0 == x1 && y0 == y1)
            break;

        e2 = err << 1;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void vgax_draw_bitmap(unsigned short page,
                      const unsigned char *pixels,
                      unsigned char bw,
                      unsigned char bh,
                      unsigned char transparent,
                      int x,
                      int y,
                      int flip_x)
{
    int row;

    if (pixels == 0 || bw == 0 || bh == 0)
        return;

    for (row = 0; row < (int)bh; row++) {
        int screen_y;
        int col;

        screen_y = y + row;
        if (screen_y < 0 || screen_y >= VGAX_H)
            continue;

        col = 0;
        while (col < (int)bw) {
            int src_col;
            int run_start;
            int run_len;
            unsigned char c;
            unsigned char run_color;

            src_col = flip_x ? ((int)bw - 1 - col) : col;
            
            c = pixels[row * (int)bw + src_col];

            if (c == transparent) {
                col++;
                continue;
            }

            run_start = col;
            run_len = 1;
            run_color = c;
            col++;

            while (col < (int)bw) {
                src_col = flip_x ? ((int)bw - 1 - col) : col;
                c = pixels[row * (int)bw + src_col];
                if (c == transparent || c != run_color)
                    break;
                run_len++;
                col++;
            }

            vgax_hline(page, x + run_start, screen_y, run_len, run_color);
        }
    }
}

int vgax_compile_sprite(struct vgax_compiled_sprite *out,
                        const unsigned char *pixels,
                        unsigned char w,
                        unsigned char h,
                        unsigned char frames,
                        unsigned char transparent,
                        struct vgax_sprite_phase *phase_buf,
                        struct vgax_sprite_run *run_buf,
                        unsigned short max_runs)
{
    unsigned short run_count;
    int frame;
    int phase;

    if (out == 0 || pixels == 0 || phase_buf == 0 || run_buf == 0)
        return -1;
    if (w == 0 || h == 0 || frames == 0)
        return -1;

    run_count = 0;

    for (frame = 0; frame < (int)frames; frame++) {
        unsigned int frame_base;

        frame_base = (unsigned int)frame * (unsigned int)w * (unsigned int)h;

        for (phase = 0; phase < 4; phase++) {
            struct vgax_sprite_phase *ph;
            int plane;

            ph = &phase_buf[frame * 4 + phase];

            for (plane = 0; plane < 4; plane++) {
                int row;

                ph->start[plane] = run_count;

                for (row = 0; row < (int)h; row++) {
                    int col;

                    col = (plane - phase + 4) & 3;

                    while (col < (int)w) {
                        unsigned char c;
                        unsigned char len;
                        unsigned char xbyte;
                        int next_col;

                        c = pixels[frame_base + row * (int)w + col];

                        if (c == transparent) {
                            col += 4;
                            continue;
                        }

                        xbyte = (unsigned char)((phase + col) >> 2);
                        len = 1;
                        next_col = col + 4;

                        while (next_col < (int)w) {
                            unsigned char c2;

                            c2 = pixels[frame_base + row * (int)w + next_col];

                            if (c2 == transparent || c2 != c)
                                break;

                            len++;
                            next_col += 4;
                        }

                        if (run_count >= max_runs)
                            return -1;

                        run_buf[run_count].y = (unsigned char)row;
                        run_buf[run_count].xbyte = xbyte;
                        run_buf[run_count].len = len;
                        run_buf[run_count].color = c;
                        run_count++;

                        col = next_col;
                    }
                }

                ph->count[plane] = (unsigned short)(run_count - ph->start[plane]);
            }
        }
    }

    out->w = w;
    out->h = h;
    out->frames = frames;
    out->transparent = transparent;
    out->compiled = 1;
    out->phases = phase_buf;
    out->runs = run_buf;
    out->run_count = run_count;

    return 0;
}

void vgax_draw_compiled_sprite(unsigned short page,
                               const struct vgax_compiled_sprite *spr,
                               int x,
                               int y,
                               int frame)
{
    int phase;
    int plane;
    int xbase;

    if (spr == 0 || !spr->compiled)
        return;

    if (frame < 0 || frame >= (int)spr->frames)
        frame = 0;

    /* This fast path assumes the whole sprite is visible.
       Clipped sprites should fall back to vgax_draw_bitmap(). */
    if (x < 0 || y < 0)
        return;
    if (x + (int)spr->w > VGAX_W || y + (int)spr->h > VGAX_H)
        return;

    phase = x & 3;
    xbase = x >> 2;

    for (plane = 0; plane < 4; plane++) {
        const struct vgax_sprite_phase *ph;
        unsigned short i;
        unsigned short start;
        unsigned short count;

        ph = &spr->phases[frame * 4 + phase];

        start = ph->start[plane];
        count = ph->count[plane];

        set_write_plane((unsigned char)plane);

        for (i = 0; i < count; i++) {
            const struct vgax_sprite_run *r;
            unsigned short off;

            r = &spr->runs[start + i];

            off = (unsigned short)(page +
                                   (y + (int)r->y) * VGAX_BYTES_PER_LINE +
                                   xbase +
                                   (int)r->xbyte);

            vram_memset(off, r->len, r->color);
        }
    }
}

void vgax_draw_sprite(unsigned short page,
                      const struct vgax_sprite *spr,
                      int x,
                      int y,
                      int flip_x)
{
    if (spr == 0)
        return;

    vgax_draw_bitmap(page, spr->pixels, spr->w, spr->h,
                     spr->transparent, x, y, flip_x);
}

void vgax_draw_tile(unsigned short page,
                    const struct vgax_tileset *ts,
                    unsigned char tile_index,
                    int x,
                    int y)
{
    unsigned int tile_size;
    const unsigned char *tile_pixels;

    if (ts == 0 || ts->pixels == 0)
        return;

    if (tile_index >= ts->count)
        return;

    tile_size = (unsigned int)ts->tile_w * (unsigned int)ts->tile_h;
    tile_pixels = ts->pixels + (unsigned int)tile_index * tile_size;

    vgax_draw_bitmap(page, tile_pixels, ts->tile_w, ts->tile_h,
                     ts->transparent, x, y, 0);
}

void vgax_draw_tilemap(unsigned short page,
                       const struct vgax_tileset *ts,
                       const unsigned char *map,
                       unsigned char map_w,
                       unsigned char map_h,
                       int scroll_x,
                       int scroll_y)
{
    int tw;
    int th;
    int start_col;
    int start_row;
    int end_col;
    int end_row;
    int row;

    if (ts == 0 || map == 0 || ts->tile_w == 0 || ts->tile_h == 0)
        return;

    if (scroll_x < 0)
        scroll_x = 0;
    if (scroll_y < 0)
        scroll_y = 0;

    tw = ts->tile_w;
    th = ts->tile_h;

    start_col = scroll_x / tw;
    start_row = scroll_y / th;
    end_col = (scroll_x + VGAX_W + tw - 1) / tw;
    end_row = (scroll_y + VGAX_H + th - 1) / th;

    if (end_col > (int)map_w)
        end_col = map_w;
    if (end_row > (int)map_h)
        end_row = map_h;

    for (row = start_row; row < end_row; row++) {
        int col;
        for (col = start_col; col < end_col; col++) {
            unsigned char tile;
            int sx;
            int sy;

            tile = map[row * (int)map_w + col];
            sx = col * tw - scroll_x;
            sy = row * th - scroll_y;
            vgax_draw_tile(page, ts, tile, sx, sy);
        }
    }
}

void vgax_copy_rect(unsigned short src_page,
                    unsigned short dst_page,
                    int x,
                    int y,
                    int w,
                    int h)
{
    int last;
    int plane;

    if (w <= 0 || h <= 0)
        return;

    /* Same source/destination page and same coordinates: nothing to do. */
    if (src_page == dst_page)
        return;

    if (x < 0) {
        w += x;
        x = 0;
    }

    if (y < 0) {
        h += y;
        y = 0;
    }

    if (x >= VGAX_W || y >= VGAX_H || w <= 0 || h <= 0)
        return;

    if (x + w > VGAX_W)
        w = VGAX_W - x;

    if (y + h > VGAX_H)
        h = VGAX_H - y;

    last = x + w - 1;

    for (plane = 0; plane < 4; plane++) {
        int first;
        int count;
        int row;
        unsigned short src_off;
        unsigned short dst_off;

        first = x + ((plane - (x & 3) + 4) & 3);
        if (first > last)
            continue;

        count = ((last - first) >> 2) + 1;

        src_off = (unsigned short)(src_page +
                                   y * VGAX_BYTES_PER_LINE +
                                   (first >> 2));

        dst_off = (unsigned short)(dst_page +
                                   y * VGAX_BYTES_PER_LINE +
                                   (first >> 2));

        set_read_plane((unsigned char)plane);
        set_write_plane((unsigned char)plane);

        for (row = 0; row < h; row++) {
            vram_copy_bytes((unsigned short)(src_off + row * VGAX_BYTES_PER_LINE),
                            (unsigned short)(dst_off + row * VGAX_BYTES_PER_LINE),
                            (unsigned short)count);
        }
    }
}
