#include "app.h"
#include "../keyboard.h"
#include "../vga.h"

static uint8_t menu;
static void put(size_t row,size_t col,const char *text,uint8_t fg,uint8_t bg){while(*text)vga_put_at(row,col++,*text++,fg,bg);}
static void draw(void){vga_clear(VGA_COLOR_WHITE,VGA_COLOR_BLUE);put(0,0," SPROOT Desktop                                      _ [] X ",VGA_COLOR_WHITE,VGA_COLOR_DARK_GREY);put(3,4,"[ ]",VGA_COLOR_WHITE,VGA_COLOR_BLUE);put(4,4,"TBF",VGA_COLOR_WHITE,VGA_COLOR_BLUE);put(7,4,"[ ]",VGA_COLOR_WHITE,VGA_COLOR_BLUE);put(8,3,"Free",VGA_COLOR_WHITE,VGA_COLOR_BLUE);put(22,0," Start ",VGA_COLOR_BLACK,VGA_COLOR_LIGHT_GREY);put(22,10,"MiniOS",VGA_COLOR_WHITE,VGA_COLOR_DARK_GREY);put(22,67,"SPROOT 1.0",VGA_COLOR_BLACK,VGA_COLOR_LIGHT_GREY);if(menu){for(size_t y=13;y<22;y++)for(size_t x=0;x<27;x++)vga_put_at(y,x,' ',VGA_COLOR_BLACK,VGA_COLOR_LIGHT_GREY);put(14,2,"SPROOT",VGA_COLOR_WHITE,VGA_COLOR_BLUE);put(16,2,"TBF File Manager",VGA_COLOR_BLACK,VGA_COLOR_LIGHT_GREY);put(18,2,"Free Text Editor",VGA_COLOR_BLACK,VGA_COLOR_LIGHT_GREY);put(20,2,"Esc - return to shell",VGA_COLOR_BLACK,VGA_COLOR_LIGHT_GREY);}vga_set_position(1,0);}
void app_sproot_start(char **args,uint8_t count){(void)args;(void)count;menu=0;draw();}
void app_sproot_key(uint16_t key){if(key=='s'||key=='\n'||key==' ')menu^=1;draw();}
