#include "app.h"
#include "../gfx.h"
#include "../mouse.h"
#include "../fs.h"
#include "../libk.h"

#define OUT_ROWS 12
#define OUT_COLS 100
static char line[48],out[OUT_ROWS][OUT_COLS];static uint8_t length,last_left;static int current_dir;

static void print(const char *text){for(uint8_t i=1;i<OUT_ROWS;i++)kstrcpy(out[i-1],out[i],OUT_COLS);kstrcpy(out[OUT_ROWS-1],text,OUT_COLS);}
static void clear_output(void){for(uint8_t i=0;i<OUT_ROWS;i++)out[i][0]=0;}
static void draw(void){char path[80];fs_path(current_dir,path,sizeof(path));gfx_clear(0x00101620);gfx_rect(0,0,800,60,0x00385F78);gfx_text(28,25,"TERMINAL",0x00FFFFFF);gfx_text(100,25,"SPROOT SHELL",0x00C8E7F4);gfx_text(756,25,"X",0x00FFFFFF);gfx_rect(32,76,736,466,0x000A111D);gfx_border(32,76,736,466,0x004D7187);gfx_text(56,96,path,0x006CE1A5);for(uint8_t i=0;i<OUT_ROWS;i++)gfx_text(56,126+i*24,out[i],0x00D8ECF4);gfx_text(56,510,">",0x006CE1A5);gfx_text(70,510,line,0x00FFFFFF);gfx_cursor(mouse_x(),mouse_y());}
static char *next(char **p){while(**p==' ')*p+=1;if(!**p)return 0;char *word=*p;while(**p&&**p!=' ')*p+=1;if(**p)*(*p)++=0;return word;}
static void list(void){uint8_t found=0;for(int i=0;i<FS_MAX_NODES;i++){const fs_node_t*n=fs_node(i);if(n&&n->parent==current_dir){char item[OUT_COLS];kstrcpy(item,n->type==FS_DIR?"DIR ":"FILE ",sizeof(item));kstrcpy(item+kstrlen(item),n->name,sizeof(item)-kstrlen(item));print(item);found=1;}}if(!found)print("EMPTY");}
static void run(char *cmd,char *arg,char *rest){if(kstrcmp(cmd,"help")==0)print("HELP LS CD PWD CAT TOUCH MKDIR WRITE RUN CLEAR EXIT");else if(kstrcmp(cmd,"clear")==0)clear_output();else if(kstrcmp(cmd,"pwd")==0){char path[80];fs_path(current_dir,path,sizeof(path));print(path);}else if(kstrcmp(cmd,"ls")==0)list();else if(kstrcmp(cmd,"cd")==0){int target=fs_resolve(arg?arg:"/",current_dir);const fs_node_t*n=fs_node(target);if(n&&n->type==FS_DIR){current_dir=target;print("OK");}else print("DIRECTORY NOT FOUND");}else if(kstrcmp(cmd,"cat")==0&&arg){int index=fs_resolve(arg,current_dir);const fs_node_t*n=fs_node(index);if(n&&n->type==FS_FILE){print(n->data);}else print("FILE NOT FOUND");}else if(kstrcmp(cmd,"touch")==0&&arg){print(fs_create(arg,FS_FILE,current_dir)==FS_OK?"CREATED":"CANNOT CREATE");}else if(kstrcmp(cmd,"mkdir")==0&&arg){print(fs_create(arg,FS_DIR,current_dir)==FS_OK?"CREATED":"CANNOT CREATE");}else if(kstrcmp(cmd,"write")==0&&arg){print(fs_write(arg,rest?rest:"",current_dir)==FS_OK?"SAVED":"CANNOT SAVE");}else if(kstrcmp(cmd,"run")==0&&arg){app_set_workdir(current_dir);app_run(arg,0,0);return;}else if(kstrcmp(cmd,"sproot")==0||kstrcmp(cmd,"exit")==0){app_run("sproot",0,0);return;}else print("UNKNOWN COMMAND - TYPE HELP");length=0;line[0]=0;draw();}
static void execute(void){char copy[48];kstrcpy(copy,line,sizeof(copy));char *cursor=copy;char *cmd=next(&cursor);char *arg=next(&cursor);while(*cursor==' ')cursor++;if(cmd)run(cmd,arg,cursor);else {length=0;line[0]=0;draw();}}
void app_terminal_start(char **args,uint8_t count){(void)args;(void)count;gfx_init();current_dir=app_get_workdir();length=0;line[0]=0;last_left=0;clear_output();print("MINIOS SHELL - TYPE HELP");draw();}
void app_terminal_tick(uint32_t ticks){(void)ticks;uint8_t click=mouse_left();if(click&&!last_left&&mouse_x()>740&&mouse_y()<62){app_run("sproot",0,0);return;}last_left=click;}
void app_terminal_key(uint16_t key){if(key=='\n')execute();else if(key=='\b'&&length){line[--length]=0;draw();}else if(key>=32&&key<127&&length<47){line[length++]=(char)key;line[length]=0;draw();}}
