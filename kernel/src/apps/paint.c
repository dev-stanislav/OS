#include "app.h"
#include "../gfx.h"
#include "../mouse.h"

static uint32_t color;static int last_x,last_y;static uint8_t drawing,last_left;
static void dot(int x,int y){for(int yy=-3;yy<=3;yy++)for(int xx=-3;xx<=3;xx++)if(xx*xx+yy*yy<=9)gfx_pixel(x+xx,y+yy,color);}
static void stroke(int x0,int y0,int x1,int y1){int dx=x1-x0,dy=y1-y0,steps=dx<0?-dx:dx,ady=dy<0?-dy:dy;if(ady>steps)steps=ady;for(int i=0;i<=steps;i++)dot(x0+(dx*i)/(steps?steps:1),y0+(dy*i)/(steps?steps:1));}
static void frame(void){gfx_clear(0x00FFF7ED);gfx_rect(0,0,800,62,0x00FF7693);gfx_text(28,26,"PAINT",0x00FFFFFF);gfx_text(70,26,"CANVAS",0x00FFE7ED);gfx_text(756,26,"X",0x00FFFFFF);gfx_rect(22,78,756,480,0x00FFFFFF);gfx_border(22,78,756,480,0x00E7D6DB);uint32_t swatches[]={0x00202135,0x00FF5D70,0x004B7BEC,0x0067C68C,0x00F3B63F};for(int i=0;i<5;i++){gfx_rect(38+i*42,570,30,20,swatches[i]);if(color==swatches[i])gfx_border(36+i*42,568,34,24,0x00202135);}gfx_text(270,576,"LEFT MOUSE BUTTON DRAWS",0x00846170);}
void app_paint_start(char **args,uint8_t n){(void)args;(void)n;gfx_init();color=0x00202135;drawing=last_left=0;frame();gfx_cursor(mouse_x(),mouse_y());}
void app_paint_tick(uint32_t t){(void)t;int x=mouse_x(),y=mouse_y();uint8_t click=mouse_left();if(click&&!last_left&&x>748&&y<62){app_run("sproot",0,0);return;}if(click&&y>=568&&y<594&&x>=38&&x<248){color=((uint32_t[]){0x00202135,0x00FF5D70,0x004B7BEC,0x0067C68C,0x00F3B63F})[(x-38)/42];drawing=0;frame();gfx_cursor(x,y);}else if(click&&x>25&&x<775&&y>81&&y<555){if(!drawing){last_x=x;last_y=y;drawing=1;}stroke(last_x,last_y,x,y);last_x=x;last_y=y;}else drawing=0;last_left=click;}
void app_paint_key(uint16_t key){if(key=='c'||key=='C')frame();}
