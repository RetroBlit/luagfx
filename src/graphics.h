#ifndef GRAPHICS_H
#define GRAPHICS_H

/*
 * graphics.h - small Lua graphics backend API.
 *
 * Runtime selection is intentionally removed:
 *   - default build              : fullscreen VGA Mode X backend
 *   - build with USE_NANOX_BACKEND: Nano-X window backend
 *
 * Lua uses gfx.open(w, h).  For Mode X the physical mode is 320x240;
 * w/h are used as the logical clipping size, clamped to 320x240.
 */

#define GFX_BACKEND_NONE   0
#define GFX_BACKEND_NANOX  1
#define GFX_BACKEND_MODEX  2

#define GFX_CAP_PAGEFLIP   0x0001
#define GFX_CAP_VSYNC      0x0002
#define GFX_CAP_BACKPAGE   0x0004
#define GFX_CAP_COPYRECT   0x0008
#define GFX_CAP_SPRITES    0x0010
#define GFX_CAP_TILES      0x0020
#define GFX_CAP_TILEMAP    0x0040
#define GFX_CAP_WINDOWED   0x0080

#define GFX_PAGE0           0
#define GFX_PAGE1           1
#define GFX_PAGE2           2
#define GFX_PAGE_DRAW       10
#define GFX_PAGE_VISIBLE    11
#define GFX_PAGE_BACKGROUND 12

#define GFX_NO_TRANSPARENT  255

#define GFX_MAX_SPRITES     32
#define GFX_MAX_TILESETS    16
#define GFX_MAX_TILEMAPS    16

int gfx_open(int w, int h);
void gfx_close(void);
int gfx_is_open(void);
const char *gfx_error(void);
const char *gfx_backend_name(void);
int gfx_backend_id(void);
int gfx_width(void);
int gfx_height(void);
unsigned int gfx_caps(void);

void gfx_clear(int color);
void gfx_pixel(int x, int y, int color);
void gfx_line(int x0, int y0, int x1, int y1, int color);
void gfx_rect(int x, int y, int w, int h, int color);
void gfx_fill(int x, int y, int w, int h, int color);

void gfx_present(void);
int gfx_sleep_ms(unsigned int ms);

int gfx_define_sprite(int id, int w, int h, int frames,
                      int transparent, const unsigned char *pixels);
void gfx_draw_sprite(int id, int x, int y, int frame, int flip_x);

int gfx_define_tileset(int id, int tile_w, int tile_h, int count,
                       int transparent, const unsigned char *pixels);
void gfx_draw_tile(int tileset_id, int tile_id, int x, int y);

int gfx_define_tilemap(int id, int map_w, int map_h, int tileset_id,
                       const unsigned char *map);
void gfx_draw_tilemap(int map_id, int scroll_x, int scroll_y);

void gfx_set_background(void);
void gfx_restore(int x, int y, int w, int h);
void gfx_copy_rect(int src_page, int dst_page, int x, int y, int w, int h);

#endif /* GRAPHICS_H */
