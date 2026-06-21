#include "gfx.h"
#include "io.h"
#include "paging.h"

#define VBE_INDEX 0x01CE
#define VBE_DATA 0x01CF
#define VBE_XRES 1
#define VBE_YRES 2
#define VBE_BPP 3
#define VBE_ENABLE 4
#define VBE_ENABLED 1
#define VBE_LFB 0x40

static volatile uint32_t *fb;
static uint32_t cursor_saved[23];
static int cursor_x, cursor_y;
static uint8_t cursor_visible;
static void vbe(uint16_t index,uint16_t value){outw(VBE_INDEX,index);outw(VBE_DATA,value);}
void gfx_init(void){fb=(volatile uint32_t*)paging_framebuffer_address();cursor_visible=0;vbe(VBE_ENABLE,0);vbe(VBE_XRES,GFX_WIDTH);vbe(VBE_YRES,GFX_HEIGHT);vbe(VBE_BPP,32);vbe(VBE_ENABLE,VBE_ENABLED|VBE_LFB);}
void gfx_shutdown(void){vbe(VBE_ENABLE,0);cursor_visible=0;}
void gfx_clear(uint32_t color){for(uint32_t i=0;i<GFX_WIDTH*GFX_HEIGHT;i++)fb[i]=color;}
void gfx_rect(int x,int y,int w,int h,uint32_t color){if(x<0){w+=x;x=0;}if(y<0){h+=y;y=0;}if(x+w>GFX_WIDTH)w=GFX_WIDTH-x;if(y+h>GFX_HEIGHT)h=GFX_HEIGHT-y;for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++)fb[(y+yy)*GFX_WIDTH+x+xx]=color;}
void gfx_border(int x,int y,int w,int h,uint32_t color){gfx_rect(x,y,w,2,color);gfx_rect(x,y+h-2,w,2,color);gfx_rect(x,y,2,h,color);gfx_rect(x+w-2,y,2,h,color);}
void gfx_bevel(int x,int y,int w,int h,uint8_t raised){uint32_t top=raised?0x00FFFFFF:0x00808080,bottom=raised?0x00808080:0x00FFFFFF;gfx_rect(x,y,w,2,top);gfx_rect(x,y,2,h,top);gfx_rect(x,y+h-2,w,2,bottom);gfx_rect(x+w-2,y,2,h,bottom);}
void gfx_pixel(int x,int y,uint32_t color){if(x>=0&&x<GFX_WIDTH&&y>=0&&y<GFX_HEIGHT)fb[y*GFX_WIDTH+x]=color;}
static const uint8_t font[][5]={{0,0,0,0,0},{31,36,68,36,31},{127,73,73,73,54},{62,65,65,65,34},{127,65,65,34,28},{127,73,73,73,65},{127,72,72,72,64},{62,65,73,73,47},{127,8,8,8,127},{0,65,127,65,0},{2,1,1,1,126},{127,8,20,34,65},{127,1,1,1,1},{127,32,16,32,127},{127,32,16,8,127},{62,65,65,65,62},{127,72,72,72,48},{62,65,69,66,61},{127,72,76,74,49},{49,73,73,73,70},{64,64,127,64,64},{126,1,1,1,126},{124,2,1,2,124},{126,1,14,1,126},{99,20,8,20,99},{96,16,15,16,96},{67,69,73,81,97},{0,0,0,0,0},{0,66,127,64,0},{66,97,81,73,70},{33,65,69,75,49},{24,20,18,127,16},{39,69,69,69,57},{60,74,73,73,48},{1,113,9,5,3},{54,73,73,73,54},{6,73,73,41,30},{0,0,0,0,0}};
static const uint8_t *glyph(char c){if(c>='A'&&c<='Z')return font[c-'A'+1];if(c>='a'&&c<='z')return font[c-'a'+1];if(c>='0'&&c<='9')return font[c-'0'+27];return font[0];}
void gfx_text(int x,int y,const char *text,uint32_t color){while(*text){const uint8_t*g=glyph(*text++);for(int col=0;col<5;col++)for(int row=0;row<7;row++)if(g[col]&(1<<row))gfx_pixel(x+col,y+6-row,color);x+=6;}}
void gfx_cursor(int x,int y){if(cursor_visible){for(int i=0;i<12;i++)if(cursor_x+i<GFX_WIDTH&&cursor_y<GFX_HEIGHT)fb[cursor_y*GFX_WIDTH+cursor_x+i]=cursor_saved[i];for(int i=1;i<12;i++)if(cursor_x<GFX_WIDTH&&cursor_y+i<GFX_HEIGHT)fb[(cursor_y+i)*GFX_WIDTH+cursor_x]=cursor_saved[11+i];}cursor_x=x;cursor_y=y;for(int i=0;i<12;i++)cursor_saved[i]=(x+i<GFX_WIDTH&&y<GFX_HEIGHT)?fb[y*GFX_WIDTH+x+i]:0;for(int i=1;i<12;i++)cursor_saved[11+i]=(x<GFX_WIDTH&&y+i<GFX_HEIGHT)?fb[(y+i)*GFX_WIDTH+x]:0;for(int i=0;i<12;i++){if(x+i<GFX_WIDTH&&y<GFX_HEIGHT)fb[y*GFX_WIDTH+x+i]=0xFFFFFF;if(x<GFX_WIDTH&&y+i<GFX_HEIGHT)fb[(y+i)*GFX_WIDTH+x]=0xFFFFFF;}cursor_visible=1;}
