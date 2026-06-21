#include "app.h"
#include "../gfx.h"
#include "../mouse.h"

static uint32_t color;static int last_x,last_y;static uint8_t drawing,last_left;
static void dot(int x,int y){for(int yy=-3;yy<=3;yy++)for(int xx=-3;xx<=3;xx++)if(xx*xx+yy*yy<=9)gfx_pixel(x+xx,y+yy,color);}
static void stroke(int x0,int y0,int x1,int y1){int dx=x1-x0,dy=y1-y0,steps=dx<0?-dx:dx,ady=dy<0?-dy:dy;if(ady>steps)steps=ady;for(int i=0;i<=steps;i++)dot(x0+(dx*i)/(steps?steps:1),y0+(dy*i)/(steps?steps:1));}
static void frame(void){gfx_clear(0x00C0C0C0);gfx_rect(20,28,760,540,0x00C0C0C0);gfx_border(20,28,760,540,0x00000000);gfx_bevel(22,30,756,536,1);gfx_rect(28,36,744,22,0x00000080);gfx_text(34,43,"PAINT",0x00FFFFFF);gfx_rect(746,39,20,16,0x00C0C0C0);gfx_bevel(746,39,20,16,1);gfx_text(752,43,"X",0x00000000);gfx_rect(28,64,744,24,0x00C0C0C0);gfx_text(36,72,"FILE  EDIT  VIEW  IMAGE  HELP",0x00000000);gfx_rect(36,98,728,400,0x00FFFFFF);gfx_bevel(34,96,732,404,0);uint32_t swatches[]={0x00000000,0x00FF0000,0x000000FF,0x00008000,0x00FFFF00};for(int i=0;i<5;i++){gfx_rect(42+i*42,518,30,20,swatches[i]);if(color==swatches[i])gfx_border(40+i*42,516,34,24,0x00000000);}gfx_rect(28,548,744,10,0x00C0C0C0);gfx_bevel(28,548,744,10,0);}
void app_paint_start(char **args,uint8_t n){(void)args;(void)n;gfx_init();color=0x00000000;drawing=last_left=0;frame();gfx_cursor(mouse_x(),mouse_y());}
void app_paint_tick(uint32_t t){(void)t;int x=mouse_x(),y=mouse_y();uint8_t click=mouse_left();if(click&&!last_left&&x>740&&y>=36&&y<60){app_run("sproot",0,0);return;}if(click&&y>=516&&y<542&&x>=42&&x<252){color=((uint32_t[]){0x00000000,0x00FF0000,0x000000FF,0x00008000,0x00FFFF00})[(x-42)/42];drawing=0;frame();gfx_cursor(x,y);}else if(click&&x>37&&x<763&&y>99&&y<497){if(!drawing){last_x=x;last_y=y;drawing=1;}stroke(last_x,last_y,x,y);last_x=x;last_y=y;}else drawing=0;last_left=click;}
void app_paint_key(uint16_t key){if(key=='c'||key=='C')frame();}
