#include "app.h"
#include "../fs.h"
#include "../keyboard.h"
#include "../vga.h"

static int current, entries[FS_MAX_NODES], count, selected;
static void refresh(void) {
    count=0; for(int i=0;i<FS_MAX_NODES;i++){const fs_node_t*n=fs_node(i);if(n&&n->parent==current)entries[count++]=i;} if(selected>=count)selected=count?count-1:0;
}
static void draw(void) {
    char path[80];fs_path(current,path,sizeof(path));vga_clear(VGA_COLOR_WHITE,VGA_COLOR_BLACK);vga_write("Files: ",VGA_COLOR_LIGHT_CYAN,VGA_COLOR_BLACK);vga_write(path,VGA_COLOR_LIGHT_GREEN,VGA_COLOR_BLACK);vga_write("\n\n",VGA_COLOR_WHITE,VGA_COLOR_BLACK);
    vga_write("[..]\n",VGA_COLOR_LIGHT_GREY,VGA_COLOR_BLACK);
    for(int i=0;i<count;i++){const fs_node_t*n=fs_node(entries[i]);vga_write(i==selected?"> ":"  ",i==selected?VGA_COLOR_LIGHT_GREEN:VGA_COLOR_WHITE,VGA_COLOR_BLACK);vga_write(n->type==FS_DIR?"[dir]  ":"[file] ",n->type==FS_DIR?VGA_COLOR_LIGHT_CYAN:VGA_COLOR_WHITE,VGA_COLOR_BLACK);vga_write(n->name,VGA_COLOR_WHITE,VGA_COLOR_BLACK);vga_write("\n",VGA_COLOR_WHITE,VGA_COLOR_BLACK);}vga_write("\nArrows: select  Enter: open  Esc: exit\n",VGA_COLOR_LIGHT_GREY,VGA_COLOR_BLACK);
}
void app_files_start(char **args,uint8_t count){(void)args;(void)count;current=fs_root();selected=0;refresh();draw();}
void app_files_key(uint16_t key){if(key==KEY_UP&&selected>0)selected--;else if(key==KEY_DOWN&&selected+1<count)selected++;else if(key=='\n'&&count){const fs_node_t*n=fs_node(entries[selected]);if(n->type==FS_DIR){current=entries[selected];selected=0;refresh();}}else if(key=='\b'){const fs_node_t*n=fs_node(current);if(n->parent>=0){current=n->parent;selected=0;refresh();}}draw();}
