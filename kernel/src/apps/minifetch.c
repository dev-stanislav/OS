#include "app.h"
#include "../fs.h"
#include "../libk.h"
#include "../rtc.h"
#include "../timer.h"
#include "../vga.h"

static void number(uint32_t value) { char text[16]; kitoa(value,text,10); vga_write(text,VGA_COLOR_WHITE,VGA_COLOR_BLACK); }

static void two_digits(uint32_t value) {
    if (value < 10) vga_write("0",VGA_COLOR_WHITE,VGA_COLOR_BLACK);
    number(value);
}

static void print_time(void) {
    if (!rtc_ready()) {
        vga_write("RTC unavailable\n",VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK);
        return;
    }
    int32_t seconds = (int32_t)(rtc_seconds_of_day() + timer_ticks() / TIMER_HZ) + (int32_t)rtc_timezone_minutes() * 60;
    while (seconds < 0) seconds += 86400;
    seconds %= 86400;
    two_digits((uint32_t)seconds / 3600u);
    vga_write(":",VGA_COLOR_WHITE,VGA_COLOR_BLACK);
    two_digits(((uint32_t)seconds / 60u) % 60u);
    vga_write(":",VGA_COLOR_WHITE,VGA_COLOR_BLACK);
    two_digits((uint32_t)seconds % 60u);
    int16_t timezone = rtc_timezone_minutes();
    uint16_t absolute = timezone < 0 ? (uint16_t)-timezone : (uint16_t)timezone;
    vga_write(" UTC",VGA_COLOR_WHITE,VGA_COLOR_BLACK);
    vga_write(timezone < 0 ? "-" : "+",VGA_COLOR_WHITE,VGA_COLOR_BLACK);
    two_digits(absolute / 60u);
    vga_write(":",VGA_COLOR_WHITE,VGA_COLOR_BLACK);
    two_digits(absolute % 60u);
    vga_write("\n",VGA_COLOR_WHITE,VGA_COLOR_BLACK);
}

void app_minifetch_start(char **args, uint8_t count) {
    (void)args; (void)count;
    vga_write("       __  __ _       _  ___  ____\n",VGA_COLOR_LIGHT_CYAN,VGA_COLOR_BLACK);
    vga_write("      |  \\/  (_)_ __ (_)/ _ \\ / ___|\n",VGA_COLOR_LIGHT_CYAN,VGA_COLOR_BLACK);
    vga_write("      | |\\/| | | '_ \\| | | | \\___ \\ \n",VGA_COLOR_LIGHT_CYAN,VGA_COLOR_BLACK);
    vga_write("      | |  | | | | | | | |_| |___) |\n",VGA_COLOR_LIGHT_CYAN,VGA_COLOR_BLACK);
    vga_write("      |_|  |_|_|_| |_|_|\\___/|____/\n\n",VGA_COLOR_LIGHT_CYAN,VGA_COLOR_BLACK);
    vga_write("OS:       MiniOS experimental kernel\nKernel:   i686 protected mode\nTerminal: MiniOS shell + MiniPkg\nFiles:    RAM FS, ",VGA_COLOR_WHITE,VGA_COLOR_BLACK);
    number(fs_used_count()); vga_write("/32 nodes used\nUptime:   ",VGA_COLOR_WHITE,VGA_COLOR_BLACK); number(timer_ticks()/TIMER_HZ); vga_write(" seconds\n",VGA_COLOR_WHITE,VGA_COLOR_BLACK);
    vga_write("Time:     ",VGA_COLOR_WHITE,VGA_COLOR_BLACK); print_time();
    vga_write("Storage:  ",VGA_COLOR_WHITE,VGA_COLOR_BLACK);number(fs_used_bytes());vga_write(" / ",VGA_COLOR_WHITE,VGA_COLOR_BLACK);number(fs_capacity_bytes());vga_write(" bytes used (disk image: 4 MiB)\n",VGA_COLOR_WHITE,VGA_COLOR_BLACK);
}
