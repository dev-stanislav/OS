#ifndef GFX_H
#define GFX_H
#include <stdint.h>
#define GFX_WIDTH 800
#define GFX_HEIGHT 600
void gfx_init(void);
void gfx_shutdown(void);
void gfx_clear(uint32_t color);
void gfx_rect(int x,int y,int w,int h,uint32_t color);
void gfx_border(int x,int y,int w,int h,uint32_t color);
void gfx_bevel(int x,int y,int w,int h,uint8_t raised);
void gfx_pixel(int x,int y,uint32_t color);
void gfx_text(int x,int y,const char *text,uint32_t color);
void gfx_text_bold(int x,int y,const char *text,uint32_t color);
void gfx_set_font(uint8_t style);
uint8_t gfx_font(void);
void gfx_begin_frame(void);
void gfx_present(void);
void gfx_cursor(int x,int y);
void gfx_cursor_hide(void);
#endif
