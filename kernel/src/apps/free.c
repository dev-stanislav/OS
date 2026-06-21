#include "app.h"
#include "../fs.h"
#include "../gfx.h"
#include "../mouse.h"
#include "../keyboard.h"
#include "../libk.h"

static char body[FS_FILE_MAX+1],filename[80];static uint16_t length;static const char*notice;static uint8_t blink,last_left;
static void draw(void){gfx_clear(0x00C0C0C0);gfx_rect(20,28,760,540,0x00C0C0C0);gfx_border(20,28,760,540,0x00000000);gfx_bevel(22,30,756,536,1);gfx_rect(28,36,744,22,0x00000080);gfx_text(34,43,"FREE - ",0x00FFFFFF);gfx_text(82,43,filename,0x00FFFFFF);gfx_rect(746,39,20,16,0x00C0C0C0);gfx_bevel(746,39,20,16,1);gfx_text(752,43,"X",0x00000000);gfx_rect(28,64,744,24,0x00C0C0C0);gfx_text(36,72,"FILE  EDIT  SEARCH  HELP",0x00000000);gfx_rect(32,94,736,430,0x00FFFFFF);gfx_bevel(30,92,740,434,0);int x=42,y=106;for(uint16_t i=0;i<length;i++){if(body[i]=='\n'||x>750){x=42;y+=14;if(y>510)break;if(body[i]=='\n')continue;}char s[2]={body[i],0};gfx_text(x,y,s,0x00000000);x+=6;}if(blink&&y<510)gfx_rect(x,y,2,9,0x00000000);gfx_rect(28,532,744,26,0x00C0C0C0);gfx_bevel(28,532,744,26,0);gfx_text(38,540,notice?notice:"CTRL S TO SAVE",0x00000000);gfx_cursor(mouse_x(),mouse_y());}
void app_free_start(char **args,uint8_t count){const char *path=count?args[0]:"untitled.txt";kstrcpy(filename,path,sizeof(filename));length=0;notice="NEW DOCUMENT";int idx=fs_resolve(path,app_get_workdir());const fs_node_t*n=fs_node(idx);if(n&&n->type==FS_FILE){length=n->size;kmemcpy(body,n->data,length);notice="OPENED";}blink=1;last_left=0;gfx_init();draw();}
void app_free_tick(uint32_t ticks){uint8_t click=mouse_left();if(click&&!last_left&&mouse_x()>740&&mouse_y()>=36&&mouse_y()<60){app_run("sproot",0,0);return;}last_left=click;if((ticks%20)==0){blink^=1;draw();}}
void app_free_key(uint16_t key){notice=0;if(key==KEY_SAVE){body[length]=0;notice=fs_write(filename,body,app_get_workdir())==FS_OK?"SAVED":"SAVE FAILED";}else if(key=='\b'){if(length)length--;}else if(key=='\n'){if(length<FS_FILE_MAX)body[length++]='\n';}else if(key>=32&&key<127&&length<FS_FILE_MAX)body[length++]=(char)key;blink=1;draw();}
