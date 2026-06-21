#include "app.h"
#include "../keyboard.h"
#include "../vga.h"
#include "../gfx.h"
#include "../mouse.h"

static uint8_t menu;
static void put(size_t row,size_t col,const char *text,uint8_t fg,uint8_t bg){while(*text)vga_put_at(row,col++,*text++,fg,bg);}
static void draw(void){vga_clear(VGA_COLOR_WHITE,VGA_COLOR_BLUE);put(0,0," SPROOT Desktop                                      _ [] X ",VGA_COLOR_WHITE,VGA_COLOR_DARK_GREY);put(3,4,"[ ]",VGA_COLOR_WHITE,VGA_COLOR_BLUE);put(4,4,"TBF",VGA_COLOR_WHITE,VGA_COLOR_BLUE);put(7,4,"[ ]",VGA_COLOR_WHITE,VGA_COLOR_BLUE);put(8,3,"Free",VGA_COLOR_WHITE,VGA_COLOR_BLUE);put(22,0," Start ",VGA_COLOR_BLACK,VGA_COLOR_LIGHT_GREY);put(22,10,"MiniOS",VGA_COLOR_WHITE,VGA_COLOR_DARK_GREY);put(22,67,"SPROOT 1.0",VGA_COLOR_BLACK,VGA_COLOR_LIGHT_GREY);if(menu){for(size_t y=13;y<22;y++)for(size_t x=0;x<27;x++)vga_put_at(y,x,' ',VGA_COLOR_BLACK,VGA_COLOR_LIGHT_GREY);put(14,2,"SPROOT",VGA_COLOR_WHITE,VGA_COLOR_BLUE);put(16,2,"TBF File Manager",VGA_COLOR_BLACK,VGA_COLOR_LIGHT_GREY);put(18,2,"Free Text Editor",VGA_COLOR_BLACK,VGA_COLOR_LIGHT_GREY);put(20,2,"Esc - return to shell",VGA_COLOR_BLACK,VGA_COLOR_LIGHT_GREY);}vga_set_position(1,0);}
static void gui_draw(void){gfx_clear(0x008080);gfx_rect(0,560,800,40,0x00C0C0C0);gfx_rect(4,564,90,30,0x00E0E0E0);gfx_rect(20,40,210,120,0x00C0C0C0);gfx_border(20,40,210,120,0x00000080);gfx_cursor(mouse_x(),mouse_y());}
void app_sproot_start(char **args,uint8_t count){(void)args;(void)count;gfx_init();menu=0;gui_draw();draw();}
void app_sproot_tick(uint32_t ticks){(void)ticks;}
void app_sproot_key(uint16_t key){if(key=='s'||key=='\n'||key==' ')menu^=1;draw();}
