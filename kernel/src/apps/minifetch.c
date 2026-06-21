#include "app.h"
#include "../fs.h"
#include "../libk.h"
#include "../timer.h"
#include "../vga.h"

static void number(uint32_t value) { char text[16]; kitoa(value,text,10); vga_write(text,VGA_COLOR_WHITE,VGA_COLOR_BLACK); }
void app_minifetch_start(char **args, uint8_t count) {
    (void)args; (void)count;
    vga_write("       __  __ _       _  ___  ____\n",VGA_COLOR_LIGHT_CYAN,VGA_COLOR_BLACK);
    vga_write("      |  \\/  (_)_ __ (_)/ _ \\ / ___|\n",VGA_COLOR_LIGHT_CYAN,VGA_COLOR_BLACK);
    vga_write("      | |\\/| | | '_ \\| | | | \\___ \\ \n",VGA_COLOR_LIGHT_CYAN,VGA_COLOR_BLACK);
    vga_write("      | |  | | | | | | | |_| |___) |\n",VGA_COLOR_LIGHT_CYAN,VGA_COLOR_BLACK);
    vga_write("      |_|  |_|_|_| |_|_|\\___/|____/\n\n",VGA_COLOR_LIGHT_CYAN,VGA_COLOR_BLACK);
    vga_write("OS:       MiniOS experimental kernel\nKernel:   i686 protected mode\nTerminal: MiniOS shell + MiniPkg\nFiles:    RAM FS, ",VGA_COLOR_WHITE,VGA_COLOR_BLACK);
    number(fs_used_count()); vga_write("/32 nodes used\nUptime:   ",VGA_COLOR_WHITE,VGA_COLOR_BLACK); number(timer_ticks()/TIMER_HZ); vga_write(" seconds\n",VGA_COLOR_WHITE,VGA_COLOR_BLACK);
}
