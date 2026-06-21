#include "app.h"
#include "../fs.h"
#include "../gfx.h"
#include "../mouse.h"
#include "../keyboard.h"

static int directory,entries[FS_MAX_NODES];static uint8_t count,selected,last_left;
static void refresh(void){count=0;for(int i=0;i<FS_MAX_NODES;i++){const fs_node_t*n=fs_node(i);if(n&&n->parent==directory)entries[count++]=i;}if(selected>=count)selected=count?count-1:0;}
static void text(int x,int y,const char*s,uint32_t c){gfx_text(x,y,s,c);}
static void draw(void){char path[80];fs_path(directory,path,sizeof(path));gfx_clear(0x00EAF0FA);gfx_rect(0,0,800,58,0x004B7BEC);text(28,23,"TBF",0x00FFFFFF);text(58,23,"FILE MANAGER",0x00DDE8FF);text(756,23,"X",0x00FFFFFF);gfx_rect(32,88,736,460,0x00FFFFFF);gfx_border(32,88,736,460,0x00CED8E8);text(58,112,path,0x004B6388);text(58,150,"NAME",0x0071819D);text(530,150,"TYPE",0x0071819D);gfx_rect(52,166,696,1,0x00DFE6F1);text(68,184,"..",0x004B7BEC);for(uint8_t i=0;i<count&&i<9;i++){const fs_node_t*n=fs_node(entries[i]);int y=218+i*34;if(i==selected)gfx_rect(52,y-8,696,27,0x00E8F0FF);text(68,y,n->name,i==selected?0x001D59C9:0x00141B2D);text(530,y,n->type==FS_DIR?"FOLDER":"FILE",0x0071819D);gfx_rect(52,y+25,696,1,0x00EEF2F8);}text(58,570,"CLICK A FOLDER TO OPEN   BACKSPACE TO GO UP",0x005D6D87);gfx_cursor(mouse_x(),mouse_y());}
void app_tbf_start(char **args,uint8_t n){(void)args;(void)n;gfx_init();directory=app_get_workdir();selected=0;last_left=0;refresh();draw();}
void app_tbf_tick(uint32_t t){(void)t;uint8_t click=mouse_left();if(click&&!last_left){int y=mouse_y(),x=mouse_x();if(x>740&&y<62){app_run("sproot",0,0);return;}if(x>52&&x<748&&y>=210&&y<210+(int)count*34){selected=(y-210)/34;const fs_node_t*n=fs_node(entries[selected]);if(n->type==FS_DIR){directory=entries[selected];selected=0;refresh();}draw();}}last_left=click;}
void app_tbf_key(uint16_t key){if(key=='\b'){const fs_node_t*n=fs_node(directory);if(n->parent>=0){directory=n->parent;refresh();}}else if(key==KEY_UP&&selected)selected--;else if(key==KEY_DOWN&&selected+1<count)selected++;draw();}
