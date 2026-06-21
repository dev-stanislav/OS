#include "app.h"
#include "../keyboard.h"
#include "../gfx.h"
#include "../mouse.h"

#define NAVY 0x00111B35
#define GLASS 0xDDF3F6FF
#define INK 0x00141B2D

static uint8_t menu,last_left;
static void panel(int x,int y,int w,int h,uint32_t fill){gfx_rect(x,y,w,h,0x002A3655);gfx_rect(x+2,y+2,w-4,h-4,fill);}
static void icon(int x,int y,uint32_t color,const char *label,uint8_t kind){panel(x,y,84,84,color);if(kind==0){gfx_rect(x+19,y+20,46,34,0x00FFFFFF);gfx_rect(x+19,y+20,46,7,0x005B8CFF);gfx_rect(x+26,y+34,20,3,0x006C7A92);}else if(kind==1){gfx_rect(x+24,y+17,36,50,0x00FFFFFF);gfx_rect(x+30,y+29,23,3,0x00717E95);gfx_rect(x+30,y+38,23,3,0x00717E95);gfx_rect(x+30,y+47,16,3,0x00717E95);}else{gfx_rect(x+16,y+19,52,45,0x00FFFFFF);gfx_rect(x+19,y+22,46,39,0x00FFF7E8);gfx_rect(x+23,y+26,38,3,0x00FF6E7A);gfx_rect(x+23,y+34,38,3,0x004BB9FF);gfx_rect(x+23,y+42,38,3,0x0067D391);}gfx_text(x+(84-6*6)/2,y+94,label,0x00FFFFFF);}
static void draw(void){
    gfx_clear(NAVY);gfx_rect(0,0,800,72,0x0019233F);gfx_text(32,26,"SPROOT",0x00FFFFFF);gfx_text(118,26,"DESKTOP",0x007FA3FF);
    gfx_text(600,26,"EXPERIMENTAL OS",0x009CB3D7);gfx_rect(0,72,800,2,0x003D4E73);
    gfx_text(42,112,"YOUR SPACE",0x008FA7D8);icon(44,142,0x004B7BEC,"TBF",0);icon(152,142,0x009A6CF0,"FREE",1);icon(260,142,0x00FF7693,"PAINT",2);icon(368,142,0x00385F78,"TERM",0);
    gfx_text(44,270,"A calmer desktop for small ideas.",0x009CB3D7);
    gfx_rect(0,536,800,64,0x0019233F);panel(26,548,138,38,0x00334B7E);gfx_text(45,563,"LAUNCHPAD",0x00FFFFFF);gfx_text(610,563,"SPROOT 2.0",0x009CB3D7);
    if(menu){panel(26,292,250,224,0x00F4F7FF);gfx_text(48,316,"LAUNCH APPS",INK);gfx_text(48,349,"TBF FILES",0x004B7BEC);gfx_text(48,383,"FREE EDITOR",0x009A6CF0);gfx_text(48,417,"PAINT CANVAS",0x00E85B78);gfx_text(48,451,"TERMINAL",0x00385F78);gfx_text(48,489,"EXIT TO SHELL",0x006C7890);}
    gfx_cursor(mouse_x(),mouse_y());
}
void app_sproot_start(char **args,uint8_t count){(void)args;(void)count;gfx_init();menu=0;last_left=0;draw();}
void app_sproot_tick(uint32_t ticks){(void)ticks;uint8_t click=mouse_left();if(click&&!last_left){int x=mouse_x(),y=mouse_y();if(y>536&&x<180)menu^=1;else if(x>=44&&x<128&&y>=142&&y<226)app_run("tbf",0,0);else if(x>=152&&x<236&&y>=142&&y<226)app_run("free",0,0);else if(x>=260&&x<344&&y>=142&&y<226)app_run("paint",0,0);else if(x>=368&&x<452&&y>=142&&y<226)app_run("terminal",0,0);else if(menu&&x>26&&x<276){if(y>=332&&y<366)app_run("tbf",0,0);else if(y>=366&&y<400)app_run("free",0,0);else if(y>=400&&y<434)app_run("paint",0,0);else if(y>=434&&y<468)app_run("terminal",0,0);else if(y>=468&&y<512){app_exit_gui();return;}else draw();}else draw();}last_left=click;}
void app_sproot_key(uint16_t key){if(key==27){app_exit_gui();return;}if(key=='s'||key=='\n'||key==' ') {menu^=1;draw();}}
