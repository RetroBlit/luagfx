#ifndef NANOX_BACKEND_H
#define NANOX_BACKEND_H

int nanox_open(int w, int h);
void nanox_close(void);

const char *nanox_error(void);

int nanox_width(void);
int nanox_height(void);

void nanox_present(void);
int nanox_sleep_ms(unsigned int ms);

void nanox_clear(int color);
void nanox_pixel(int x, int y, int color);
void nanox_line(int x0, int y0, int x1, int y1, int color);
void nanox_hline(int x, int y, int w, unsigned char color);
void nanox_rect(int x, int y, int w, int h, int color);
void nanox_fill(int x, int y, int w, int h, int color);

void nanox_draw_bitmap(const unsigned char *pixels,
                       int bw,
                       int bh,
                       int transparent,
                       int x,
                       int y,
                       int flip_x);

/* Background cache / dirty rectangle support */
void nanox_set_background(void);
void nanox_restore(int x, int y, int w, int h);
void nanox_copy_rect(int src_page, int dst_page,
                     int x, int y, int w, int h);
int  nanox_save_under(int x, int y, int w, int h);
void nanox_restore_saved(void);

#endif
