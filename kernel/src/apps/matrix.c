#include "app.h"
#include "../timer.h"
#include "../vga.h"

static uint8_t drops[VGA_WIDTH];
static uint32_t last_tick;
static uint32_t seed=0x12345678;
static uint32_t random_next(void){seed=seed*1103515245u+12345u;return seed;}
void app_matrix_start(char **args,uint8_t count){(void)args;(void)count;vga_clear(VGA_COLOR_BLACK,VGA_COLOR_BLACK);for(uint8_t i=0;i<VGA_WIDTH;i++)drops[i]=(uint8_t)(random_next()%VGA_HEIGHT);last_tick=0;}
void app_matrix_key(uint16_t key){(void)key;}
void app_matrix_tick(uint32_t ticks){if(ticks-last_tick<5)return;last_tick=ticks;for(uint8_t x=0;x<VGA_WIDTH;x++){uint8_t y=drops[x];vga_put_at(y,x,' ',VGA_COLOR_BLACK,VGA_COLOR_BLACK);y=(uint8_t)((y+1)%VGA_HEIGHT);drops[x]=y;vga_put_at(y,x,(char)('0'+random_next()%10),VGA_COLOR_LIGHT_GREEN,VGA_COLOR_BLACK);}vga_update_cursor();}
