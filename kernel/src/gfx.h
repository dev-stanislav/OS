#ifndef GFX_H
#define GFX_H
#include <stdint.h>
#define GFX_WIDTH 800
#define GFX_HEIGHT 600
void gfx_init(void);
void gfx_clear(uint32_t color);
void gfx_rect(int x,int y,int w,int h,uint32_t color);
void gfx_border(int x,int y,int w,int h,uint32_t color);
void gfx_cursor(int x,int y);
#endif
