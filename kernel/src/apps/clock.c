#include "app.h"
#include "../keyboard.h"
#include "../libk.h"
#include "../timer.h"
#include "../vga.h"

static void draw(uint32_t ticks) { char text[16]; vga_clear(VGA_COLOR_WHITE,VGA_COLOR_BLACK); vga_write("MiniOS Clock\n\nUptime: ",VGA_COLOR_LIGHT_CYAN,VGA_COLOR_BLACK); kitoa(ticks/TIMER_HZ,text,10);vga_write(text,VGA_COLOR_LIGHT_GREEN,VGA_COLOR_BLACK);vga_write(" seconds\n\nEsc: exit\n",VGA_COLOR_WHITE,VGA_COLOR_BLACK); }
void app_clock_start(char **args,uint8_t count){(void)args;(void)count;draw(timer_ticks());}
void app_clock_key(uint16_t key){(void)key;}
void app_clock_tick(uint32_t ticks){if(ticks%TIMER_HZ==0)draw(ticks);}
