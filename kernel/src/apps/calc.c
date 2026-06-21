#include "app.h"
#include "../vga.h"
#include "../libk.h"

static int parse(const char *text, int *value) { int sign=1; int result=0; if(*text=='-'){sign=-1;text++;} if(!*text)return 0; while(*text){if(*text<'0'||*text>'9')return 0;result=result*10+(*text++-'0');}*value=result*sign;return 1; }
void app_calc_start(char **args, uint8_t count) {
    int left,right,result; char text[16];
    if(count!=3||!parse(args[0],&left)||!parse(args[2],&right)){vga_write("usage: run calc 12 * 7\n",VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK);return;}
    if(args[1][1]){vga_write("one operator required\n",VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK);return;}
    switch(args[1][0]){case '+':result=left+right;break;case '-':result=left-right;break;case '*':result=left*right;break;case '/':if(!right){vga_write("division by zero\n",VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK);return;}result=left/right;break;default:vga_write("operators: + - * /\n",VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK);return;}
    if(result<0){vga_write("-",VGA_COLOR_WHITE,VGA_COLOR_BLACK);kitoa((uint32_t)(-result),text,10);}else kitoa((uint32_t)result,text,10); vga_write(text,VGA_COLOR_LIGHT_GREEN,VGA_COLOR_BLACK);vga_write("\n",VGA_COLOR_WHITE,VGA_COLOR_BLACK);
}
