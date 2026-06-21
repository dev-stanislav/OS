#include "gfx.h"
#include "io.h"

#define VBE_INDEX 0x01CE
#define VBE_DATA 0x01CF
#define VBE_XRES 1
#define VBE_YRES 2
#define VBE_BPP 3
#define VBE_ENABLE 4
#define VBE_ENABLED 1
#define VBE_LFB 0x40

static volatile uint32_t *fb=(volatile uint32_t*)0xE0000000;
static void vbe(uint16_t index,uint16_t value){outw(VBE_INDEX,index);outw(VBE_DATA,value);}
void gfx_init(void){vbe(VBE_ENABLE,0);vbe(VBE_XRES,GFX_WIDTH);vbe(VBE_YRES,GFX_HEIGHT);vbe(VBE_BPP,32);vbe(VBE_ENABLE,VBE_ENABLED|VBE_LFB);}
void gfx_clear(uint32_t color){for(uint32_t i=0;i<GFX_WIDTH*GFX_HEIGHT;i++)fb[i]=color;}
void gfx_rect(int x,int y,int w,int h,uint32_t color){if(x<0){w+=x;x=0;}if(y<0){h+=y;y=0;}if(x+w>GFX_WIDTH)w=GFX_WIDTH-x;if(y+h>GFX_HEIGHT)h=GFX_HEIGHT-y;for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++)fb[(y+yy)*GFX_WIDTH+x+xx]=color;}
void gfx_border(int x,int y,int w,int h,uint32_t color){gfx_rect(x,y,w,2,color);gfx_rect(x,y+h-2,w,2,color);gfx_rect(x,y,2,h,color);gfx_rect(x+w-2,y,2,h,color);}
