#include "app.h"
#include "../keyboard.h"
#include "../gfx.h"
#include "../mouse.h"

#define TEAL 0x00008080
#define FACE 0x00C0C0C0
#define BLUE 0x00000080

static uint8_t menu,last_left;
static void button(int x,int y,int w,int h,const char *label){gfx_rect(x,y,w,h,FACE);gfx_bevel(x,y,w,h,1);gfx_text(x+8,y+9,label,0x00000000);}
static void desktop_icon(int x,int y,const char *label,uint8_t kind){if(kind==0){gfx_rect(x+7,y+5,34,25,0x00FFFFFF);gfx_border(x+7,y+5,34,25,0x00000000);gfx_rect(x+7,y+5,34,5,BLUE);}else if(kind==1){gfx_rect(x+11,y+2,25,31,0x00FFFFFF);gfx_border(x+11,y+2,25,31,0x00000000);gfx_rect(x+15,y+10,16,2,BLUE);gfx_rect(x+15,y+16,16,2,BLUE);gfx_rect(x+15,y+22,12,2,BLUE);}else if(kind==2){gfx_rect(x+5,y+5,36,28,0x00FFFFFF);gfx_border(x+5,y+5,36,28,0x00000000);gfx_rect(x+8,y+9,30,3,0x00FF0000);gfx_rect(x+8,y+15,30,3,0x0000FF00);gfx_rect(x+8,y+21,30,3,0x000000FF);}else{gfx_rect(x+5,y+5,36,28,0x00000000);gfx_border(x+5,y+5,36,28,0x00FFFFFF);gfx_text(x+11,y+13,">_",0x00FFFFFF);}gfx_text(x,y+42,label,0x00FFFFFF);}
static void draw(void){gfx_clear(TEAL);desktop_icon(36,34,"TBF",0);desktop_icon(36,104,"FREE",1);desktop_icon(36,174,"PAINT",2);desktop_icon(36,244,"TERM",3);gfx_rect(0,572,800,28,FACE);gfx_bevel(0,572,800,28,1);button(4,576,82,21,"START");gfx_rect(94,577,180,19,0x00808080);gfx_bevel(94,577,180,19,0);gfx_text(104,583,"SPROOT DESKTOP",0x00FFFFFF);gfx_text(690,583,"12 00",0x00000000);if(menu){gfx_rect(2,348,250,224,FACE);gfx_bevel(2,348,250,224,1);gfx_rect(6,352,34,216,BLUE);gfx_text(17,480,"SPROOT",0x00FFFFFF);gfx_text(52,370,"PROGRAMS",0x00000000);gfx_text(52,402,"TBF FILE MANAGER",0x00000000);gfx_text(52,434,"FREE TEXT EDITOR",0x00000000);gfx_text(52,466,"PAINT",0x00000000);gfx_text(52,498,"TERMINAL",0x00000000);gfx_rect(44,522,202,2,0x00808080);gfx_text(52,540,"EXIT TO SHELL",0x00000000);}gfx_cursor(mouse_x(),mouse_y());}
void app_sproot_start(char **args,uint8_t count){(void)args;(void)count;gfx_init();menu=last_left=0;draw();}
void app_sproot_tick(uint32_t ticks){(void)ticks;uint8_t click=mouse_left();if(click&&!last_left){int x=mouse_x(),y=mouse_y();if(y>=572&&x<90)menu^=1;else if(x>=30&&x<95&&y>=25&&y<90)app_run("tbf",0,0);else if(x>=30&&x<95&&y>=95&&y<160)app_run("free",0,0);else if(x>=30&&x<95&&y>=165&&y<230)app_run("paint",0,0);else if(x>=30&&x<95&&y>=235&&y<300)app_run("terminal",0,0);else if(menu&&x>44&&x<250){if(y>=388&&y<420)app_run("tbf",0,0);else if(y>=420&&y<452)app_run("free",0,0);else if(y>=452&&y<484)app_run("paint",0,0);else if(y>=484&&y<516)app_run("terminal",0,0);else if(y>=522&&y<562){app_exit_gui();return;}else draw();}else draw();}last_left=click;}
void app_sproot_key(uint16_t key){if(key=='s'||key=='\n'||key==' ') {menu^=1;draw();}}
