#include "app.h"
#include "../fs.h"
#include "../gfx.h"
#include "../mouse.h"
#include "../keyboard.h"
#include "../libk.h"

static char body[FS_FILE_MAX+1],filename[80];static uint16_t length;static const char*notice;static uint8_t blink,last_left;
static void draw(void){gfx_clear(0x00F4F1FF);gfx_rect(0,0,800,60,0x009A6CF0);gfx_text(28,25,"FREE",0x00FFFFFF);gfx_text(64,25,"EDITOR",0x00EAE0FF);gfx_text(756,25,"X",0x00FFFFFF);gfx_rect(32,88,736,438,0x00FFFFFF);gfx_border(32,88,736,438,0x00DACFEE);gfx_text(56,110,filename,0x006C538F);gfx_text(570,110,"CTRL S SAVE",0x006C538F);int x=58,y=152;for(uint16_t i=0;i<length;i++){if(body[i]=='\n'||x>730){x=58;y+=14;if(y>500)break;if(body[i]=='\n')continue;}char s[2]={body[i],0};gfx_text(x,y,s,0x00202135);x+=6;}if(blink&&y<500)gfx_rect(x,y,2,9,0x009A6CF0);gfx_text(56,548,notice?notice:"WRITE SOMETHING BEAUTIFUL",0x006C538F);gfx_cursor(mouse_x(),mouse_y());}
void app_free_start(char **args,uint8_t count){const char *path=count?args[0]:"untitled.txt";kstrcpy(filename,path,sizeof(filename));length=0;notice="NEW DOCUMENT";int idx=fs_resolve(path,app_get_workdir());const fs_node_t*n=fs_node(idx);if(n&&n->type==FS_FILE){length=n->size;kmemcpy(body,n->data,length);notice="OPENED";}blink=1;last_left=0;gfx_init();draw();}
void app_free_tick(uint32_t ticks){uint8_t click=mouse_left();if(click&&!last_left&&mouse_x()>740&&mouse_y()<62){app_run("sproot",0,0);return;}last_left=click;if((ticks%20)==0){blink^=1;draw();}}
void app_free_key(uint16_t key){notice=0;if(key==KEY_SAVE){body[length]=0;notice=fs_write(filename,body,app_get_workdir())==FS_OK?"SAVED":"SAVE FAILED";}else if(key=='\b'){if(length)length--;}else if(key=='\n'){if(length<FS_FILE_MAX)body[length++]='\n';}else if(key>=32&&key<127&&length<FS_FILE_MAX)body[length++]=(char)key;blink=1;draw();}
