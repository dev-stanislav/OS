#include "app.h"
#include "../fs.h"
#include "../keyboard.h"
#include "../libk.h"
#include "../vga.h"

#define EDITOR_ROWS 21
#define EDITOR_TOP 2

static char text[FS_FILE_MAX + 1];
static uint16_t length, cursor;
static char filename[80];
static const char *message;

static void erase_at(uint16_t index) { for(uint16_t i=index;i<length;i++)text[i]=text[i+1];length--; }
static void insert_at(uint16_t index,char c) { if(length>=FS_FILE_MAX){message="file limit: 1024 bytes";return;}for(uint16_t i=length;i>index;i--)text[i]=text[i-1];text[index]=c;length++; }

static void move_up_down(int direction) {
    uint16_t start=cursor; while(start&&text[start-1]!='\n')start--; uint16_t column=cursor-start;
    if(direction<0){if(start==0)return;uint16_t target=start-1;while(target&&text[target-1]!='\n')target--;cursor=target;while(column&&cursor<length&&text[cursor]!='\n'){cursor++;column--;}}
    else {uint16_t end=cursor;while(end<length&&text[end]!='\n')end++;if(end==length)return;cursor=end+1;while(column&&cursor<length&&text[cursor]!='\n'){cursor++;column--;}}
}

static void draw(void) {
    vga_clear(VGA_COLOR_WHITE,VGA_COLOR_BLACK);
    vga_write("Free: ",VGA_COLOR_LIGHT_CYAN,VGA_COLOR_BLACK);vga_write(filename,VGA_COLOR_LIGHT_GREEN,VGA_COLOR_BLACK);vga_write("  Ctrl+S save  Ctrl+C/Esc exit\n",VGA_COLOR_LIGHT_GREY,VGA_COLOR_BLACK);
    for(uint8_t y=EDITOR_TOP;y<EDITOR_TOP+EDITOR_ROWS;y++)for(uint8_t x=0;x<VGA_WIDTH;x++)vga_put_at(y,x,' ',VGA_COLOR_WHITE,VGA_COLOR_BLACK);
    uint8_t row=EDITOR_TOP,col=0; size_t cursor_row=row,cursor_col=col;
    for(uint16_t i=0;i<=length;i++) {
        if(i==cursor){cursor_row=row;cursor_col=col;}
        if(i==length)break;
        if(text[i]=='\n'){row++;col=0;if(row>=EDITOR_TOP+EDITOR_ROWS)break;continue;}
        vga_put_at(row,col,text[i],VGA_COLOR_WHITE,VGA_COLOR_BLACK);
        if(++col>=VGA_WIDTH){col=0;if(++row>=EDITOR_TOP+EDITOR_ROWS)break;}
    }
    vga_write("\n",VGA_COLOR_WHITE,VGA_COLOR_BLACK);
    vga_set_position(EDITOR_TOP+EDITOR_ROWS+1,0);
    vga_write(message?message:"",message?VGA_COLOR_LIGHT_BROWN:VGA_COLOR_WHITE,VGA_COLOR_BLACK);
    vga_set_position(cursor_row,cursor_col);
}

void app_free_start(char **args,uint8_t count) {
    const char *path=count?args[0]:"untitled.txt";kstrcpy(filename,path,sizeof(filename));message=0;length=cursor=0;
    int index=fs_resolve(path,app_get_workdir());const fs_node_t *node=fs_node(index);
    if(node&&node->type==FS_FILE){length=node->size;kmemcpy(text,node->data,length);text[length]='\0';message="opened";}
    else if(node&&node->type==FS_DIR)message="cannot edit a directory";
    else message="new file";
    draw();
}

void app_free_key(uint16_t key) {
    message=0;
    if(key==KEY_LEFT){if(cursor)cursor--;}
    else if(key==KEY_RIGHT){if(cursor<length)cursor++;}
    else if(key==KEY_UP)move_up_down(-1);
    else if(key==KEY_DOWN)move_up_down(1);
    else if(key==KEY_HOME){while(cursor&&text[cursor-1]!='\n')cursor--;}
    else if(key==KEY_END){while(cursor<length&&text[cursor]!='\n')cursor++;}
    else if(key==KEY_DELETE){if(cursor<length)erase_at(cursor);}
    else if(key=='\b'){if(cursor){cursor--;erase_at(cursor);}}
    else if(key=='\n')insert_at(cursor++,'\n');
    else if(key==KEY_SAVE){text[length]='\0';fs_result_t result=fs_write(filename,text,app_get_workdir());message=result==FS_OK?"saved": "save failed";}
    else if(key<127&&key>=32)insert_at(cursor++,(char)key);
    draw();
}
