#include "app.h"
#include "../fs.h"
#include "../keyboard.h"
#include "../vga.h"

static int directory, entries[FS_MAX_NODES];
static uint8_t entry_count, selected, preview;

static void refresh(void) {
    entry_count=0;
    for(int i=0;i<FS_MAX_NODES;i++){const fs_node_t*n=fs_node(i);if(n&&n->parent==directory)entries[entry_count++]=i;}
    if(selected>entry_count)selected=entry_count;
}
static void draw_list(void) {
    char path[80];fs_path(directory,path,sizeof(path));vga_clear(VGA_COLOR_WHITE,VGA_COLOR_BLACK);
    vga_write("TBF - The Best File  ",VGA_COLOR_LIGHT_CYAN,VGA_COLOR_BLACK);vga_write(path,VGA_COLOR_LIGHT_GREEN,VGA_COLOR_BLACK);vga_write("\nStorage: ",VGA_COLOR_LIGHT_GREY,VGA_COLOR_BLACK);char used[8],capacity[8];uint32_t value=fs_used_bytes();uint8_t len=0;if(!value)used[len++]='0';while(value){used[len++]=(char)('0'+value%10);value/=10;}uint8_t out=len;while(out)vga_putchar(used[--out],VGA_COLOR_LIGHT_GREY,VGA_COLOR_BLACK);vga_write("/",VGA_COLOR_LIGHT_GREY,VGA_COLOR_BLACK);value=fs_capacity_bytes();len=0;while(value){capacity[len++]=(char)('0'+value%10);value/=10;}out=len;while(out)vga_putchar(capacity[--out],VGA_COLOR_LIGHT_GREY,VGA_COLOR_BLACK);vga_write(" bytes\n",VGA_COLOR_LIGHT_GREY,VGA_COLOR_BLACK);
    vga_write("Arrows: select  Enter: open  Backspace: up  Space: view  Esc: exit\n\n",VGA_COLOR_LIGHT_GREY,VGA_COLOR_BLACK);
    vga_write(selected==0?"> [..]\n":"  [..]\n",selected==0?VGA_COLOR_LIGHT_GREEN:VGA_COLOR_WHITE,VGA_COLOR_BLACK);
    for(uint8_t i=0;i<entry_count;i++){const fs_node_t*n=fs_node(entries[i]);uint8_t color=(selected==i+1)?VGA_COLOR_LIGHT_GREEN:VGA_COLOR_WHITE;vga_write(selected==i+1?"> ":"  ",color,VGA_COLOR_BLACK);vga_write(n->type==FS_DIR?"[DIR]  ":"[FILE] ",n->type==FS_DIR?VGA_COLOR_LIGHT_CYAN:color,VGA_COLOR_BLACK);vga_write(n->name,color,VGA_COLOR_BLACK);if(n->type==FS_FILE){vga_write("  ",color,VGA_COLOR_BLACK);char size[8];uint16_t value=n->size;uint8_t len=0;if(!value)size[len++]='0';while(value){size[len++]=(char)('0'+value%10);value/=10;}while(len)vga_putchar(size[--len],color,VGA_COLOR_BLACK);vga_write(" B",color,VGA_COLOR_BLACK);}vga_write("\n",color,VGA_COLOR_BLACK);}
}
static void draw_preview(int index) {
    const fs_node_t*n=fs_node(index);vga_clear(VGA_COLOR_WHITE,VGA_COLOR_BLACK);vga_write("TBF viewer: ",VGA_COLOR_LIGHT_CYAN,VGA_COLOR_BLACK);vga_write(n->name,VGA_COLOR_LIGHT_GREEN,VGA_COLOR_BLACK);vga_write("\nBackspace: return  Esc: exit\n\n",VGA_COLOR_LIGHT_GREY,VGA_COLOR_BLACK);vga_write(n->data,VGA_COLOR_WHITE,VGA_COLOR_BLACK);
}
void app_tbf_start(char **args,uint8_t count){(void)args;(void)count;directory=app_get_workdir();selected=0;preview=0;refresh();draw_list();}
void app_tbf_key(uint16_t key) {
    if(preview){if(key=='\b'){preview=0;draw_list();}return;}
    if(key==KEY_UP&&selected)selected--;else if(key==KEY_DOWN&&selected<entry_count)selected++;
    else if(key=='\b'){const fs_node_t*n=fs_node(directory);if(n->parent>=0){directory=n->parent;selected=0;refresh();}}
    else if((key=='\n'||key==' ')&&selected==0){const fs_node_t*n=fs_node(directory);if(n->parent>=0){directory=n->parent;selected=0;refresh();}}
    else if((key=='\n'||key==' ')&&selected){const fs_node_t*n=fs_node(entries[selected-1]);if(n->type==FS_DIR&&key=='\n'){directory=entries[selected-1];selected=0;refresh();}else if(n->type==FS_FILE){preview=1;draw_preview(entries[selected-1]);return;}}
    draw_list();
}
