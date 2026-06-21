#include "mouse.h"
#include "io.h"
#include "gfx.h"
static uint8_t packet[3],index;
static int x=400,y=300;
static void wait_write(void){while(inb(0x64)&2);}
static void send(uint8_t value){wait_write();outb(0x64,0xD4);wait_write();outb(0x60,value);}
void mouse_init(void){wait_write();outb(0x64,0xA8);wait_write();outb(0x64,0x20);uint8_t status=inb(0x60)|2;wait_write();outb(0x64,0x60);wait_write();outb(0x60,status);send(0xF4);(void)inb(0x60);}
void mouse_irq_handler(void){packet[index++]=inb(0x60);if(index<3)return;index=0;if(!(packet[0]&8))return;x+=(int8_t)packet[1];y-=(int8_t)packet[2];if(x<0)x=0;if(y<0)y=0;if(x>=GFX_WIDTH)x=GFX_WIDTH-1;if(y>=GFX_HEIGHT)y=GFX_HEIGHT-1;gfx_cursor(x,y);}
