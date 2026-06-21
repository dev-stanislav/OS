#include "app.h"
#include "../fs.h"
#include "../gfx.h"
#include "../mouse.h"
#include "../keyboard.h"

static int directory,entries[FS_MAX_NODES];static uint8_t count,selected,last_left;
static void refresh(void){count=0;for(int i=0;i<FS_MAX_NODES;i++){const fs_node_t*n=fs_node(i);if(n&&n->parent==directory)entries[count++]=i;}if(selected>=count)selected=count?count-1:0;}
static void text(int x,int y,const char*s,uint32_t c){gfx_text(x,y,s,c);}
static void draw(void){char path[80];fs_path(directory,path,sizeof(path));gfx_clear(0x00C0C0C0);gfx_rect(20,28,760,540,0x00C0C0C0);gfx_border(20,28,760,540,0x00000000);gfx_bevel(22,30,756,536,1);gfx_rect(28,36,744,22,0x00000080);text(34,43,"TBF - THE BEST FILE",0x00FFFFFF);gfx_rect(746,39,20,16,0x00C0C0C0);gfx_bevel(746,39,20,16,1);text(752,43,"X",0x00000000);gfx_rect(28,64,744,24,0x00C0C0C0);text(36,72,"FILE  VIEW  HELP",0x00000000);gfx_rect(32,94,736,430,0x00FFFFFF);gfx_bevel(30,92,740,434,0);text(42,106,path,0x00000000);text(42,132,"NAME",0x00000000);text(530,132,"TYPE",0x00000000);gfx_rect(38,146,724,2,0x00808080);text(50,162,"..",0x00000080);for(uint8_t i=0;i<count&&i<9;i++){const fs_node_t*n=fs_node(entries[i]);int y=190+i*30;if(i==selected){gfx_rect(40,y-5,716,20,0x00000080);}text(50,y,n->name,i==selected?0x00FFFFFF:0x00000000);text(530,y,n->type==FS_DIR?"FOLDER":"FILE",i==selected?0x00FFFFFF:0x00000000);}gfx_rect(28,532,744,26,0x00C0C0C0);gfx_bevel(28,532,744,26,0);text(38,540,"CLICK FOLDER TO OPEN",0x00000000);gfx_cursor(mouse_x(),mouse_y());}
void app_tbf_start(char **args,uint8_t n){(void)args;(void)n;gfx_init();directory=app_get_workdir();selected=0;last_left=0;refresh();draw();}
void app_tbf_tick(uint32_t t){(void)t;uint8_t click=mouse_left();if(click&&!last_left){int y=mouse_y(),x=mouse_x();if(x>740&&y>=36&&y<60){app_run("sproot",0,0);return;}if(x>40&&x<756&&y>=182&&y<182+(int)count*30){selected=(y-182)/30;const fs_node_t*n=fs_node(entries[selected]);if(n->type==FS_DIR){directory=entries[selected];selected=0;refresh();}draw();}}last_left=click;}
void app_tbf_key(uint16_t key){if(key=='\b'){const fs_node_t*n=fs_node(directory);if(n->parent>=0){directory=n->parent;refresh();}}else if(key==KEY_UP&&selected)selected--;else if(key==KEY_DOWN&&selected+1<count)selected++;draw();}
