#include "process.h"
#include "gdt.h"
#include "vga.h"

#define SYS_WRITE 1
#define SYS_EXIT 2

extern void enter_user_mode(void);
extern uint8_t user_kernel_stack_top;

static uint8_t user_started;
static uint8_t user_running;

void process_init(void) { gdt_set_kernel_stack((uint32_t)&user_kernel_stack_top); }

void process_run_user_demo(void) {
    if(user_running){vga_write("user process is already running\n",VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK);return;}
    user_started=1;user_running=1;
    vga_write("starting userdemo (ring 3)...\n",VGA_COLOR_LIGHT_CYAN,VGA_COLOR_BLACK);
    enter_user_mode();
    user_running=0;
    vga_write("userdemo exited to kernel\n",VGA_COLOR_LIGHT_GREEN,VGA_COLOR_BLACK);
}

void process_list(void) {
    vga_write("PID  MODE    STATE    NAME\n",VGA_COLOR_LIGHT_CYAN,VGA_COLOR_BLACK);
    vga_write("0    kernel  running  shell\n",VGA_COLOR_WHITE,VGA_COLOR_BLACK);
    if(user_started)vga_write(user_running?"1    user    running  userdemo\n":"1    user    exited   userdemo\n",user_running?VGA_COLOR_LIGHT_GREEN:VGA_COLOR_LIGHT_GREY,VGA_COLOR_BLACK);
}

uint32_t syscall_handler(uint32_t number, uint32_t argument) {
    if(number==SYS_WRITE) {
        const char *text=(const char *)argument;
        vga_write("[user] ",VGA_COLOR_LIGHT_MAGENTA,VGA_COLOR_BLACK);
        for(uint16_t i=0;text[i]&&i<120;i++)vga_putchar(text[i],VGA_COLOR_WHITE,VGA_COLOR_BLACK);
        return 0;
    }
    if(number==SYS_EXIT)return 1;
    return 0;
}
