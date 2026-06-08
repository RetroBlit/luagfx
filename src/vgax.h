#ifndef VGAX_INCLUDED
#define VGAX_INCLUDED

/*
 * vgax.h - tiny VGA Mode X library for ELKS / OWC
 *
 * Target mode: 320x240, 256 colors, planar Mode X.
 * Page size:   320*240/4 = 19200 bytes per VGA plane.
 * Pages:       PAGE0, PAGE1, PAGE2 fit in 64 KiB VGA aperture.
 *
 * This is deliberately small: init, page flipping, primitives,
 * tiles and sprites.  It avoids malloc and floating point.
 */

#define VGAX_W 320
#define VGAX_H 240
#define VGAX_BYTES_PER_LINE 80
#define VGAX_PAGE_BYTES 19200

#define VGAX_PAGE0 0
#define VGAX_PAGE1 VGAX_PAGE_BYTES
#define VGAX_PAGE2 (VGAX_PAGE_BYTES * 2)

#define VGAX_NO_TRANSPARENT 255

struct vgax_sprite {
    unsigned char w;
    unsigned char h;
    unsigned char transparent;
    const unsigned char *pixels;      /* linear, row-major, one byte per pixel */
};

struct vgax_tileset {
    unsigned char tile_w;
    unsigned char tile_h;
    unsigned char count;
    unsigned char transparent;
    const unsigned char *pixels;      /* count * tile_w * tile_h */
};

void vgax_init(void);
void vgax_text_mode(void);

void vgax_wait_vsync(void);
void vgax_flip(unsigned short page);

void vgax_set_palette(unsigned char index,
                      unsigned char r,
                      unsigned char g,
                      unsigned char b);
void vgax_set_default_palette(void);

void vgax_clear_page(unsigned short page, unsigned char color);

void vgax_plot(unsigned short page, int x, int y, unsigned char color);
void vgax_hline(unsigned short page, int x, int y, int w, unsigned char color);
void vgax_vline(unsigned short page, int x, int y, int h, unsigned char color);
void vgax_line(unsigned short page, int x0, int y0,
               int x1, int y1, unsigned char color);
void vgax_fill_rect(unsigned short page, int x, int y,
                    int w, int h, unsigned char color);

void vgax_draw_bitmap(unsigned short page,
                      const unsigned char *pixels,
                      unsigned char bw,
                      unsigned char bh,
                      unsigned char transparent,
                      int x,
                      int y,
                      int flip_x);

void vgax_draw_sprite(unsigned short page,
                      const struct vgax_sprite *spr,
                      int x,
                      int y,
                      int flip_x);

void vgax_draw_tile(unsigned short page,
                    const struct vgax_tileset *ts,
                    unsigned char tile_index,
                    int x,
                    int y);

void vgax_draw_tilemap(unsigned short page,
                       const struct vgax_tileset *ts,
                       const unsigned char *map,
                       unsigned char map_w,
                       unsigned char map_h,
                       int scroll_x,
                       int scroll_y);
					   
void vgax_copy_rect(unsigned short src_page,
                    unsigned short dst_page,
                    int x,
                    int y,
                    int w,
                    int h);

#endif /* VGAX_H */
