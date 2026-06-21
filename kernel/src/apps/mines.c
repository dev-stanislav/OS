#include "app.h"
#include "../keyboard.h"
#include "../vga.h"

#define SIZE 8
static uint8_t mines[SIZE][SIZE], open_cells[SIZE][SIZE], flags[SIZE][SIZE], cursor_x,cursor_y,finished;
static uint8_t near(uint8_t x,uint8_t y){uint8_t n=0;for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){int xx=x+dx,yy=y+dy;if(xx>=0&&xx<SIZE&&yy>=0&&yy<SIZE&&mines[yy][xx])n++;}return n;}
static void draw(void){vga_clear(VGA_COLOR_WHITE,VGA_COLOR_BLACK);vga_write("MiniMines: arrows move, Space open, F flag, Esc exit\n\n",VGA_COLOR_LIGHT_CYAN,VGA_COLOR_BLACK);for(uint8_t y=0;y<SIZE;y++){for(uint8_t x=0;x<SIZE;x++){char c='#';if(flags[y][x])c='F';if(open_cells[y][x]){uint8_t n=near(x,y);c=mines[y][x]?'*':(n?(char)('0'+n):'.');}uint8_t color=(x==cursor_x&&y==cursor_y)?VGA_COLOR_BLACK:VGA_COLOR_WHITE;uint8_t bg=(x==cursor_x&&y==cursor_y)?VGA_COLOR_LIGHT_GREEN:VGA_COLOR_BLACK;vga_putchar(c,color,bg);vga_putchar(' ',color,bg);}vga_write("\n",VGA_COLOR_WHITE,VGA_COLOR_BLACK);}if(finished)vga_write("Game over. Esc to exit.\n",VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK);}
void app_mines_start(char **args,uint8_t count){static const uint8_t positions[10][2]={{1,0},{6,0},{3,1},{0,2},{5,2},{2,4},{7,4},{4,5},{1,7},{6,7}};(void)args;(void)count;for(uint8_t y=0;y<SIZE;y++)for(uint8_t x=0;x<SIZE;x++){mines[y][x]=0;open_cells[y][x]=0;flags[y][x]=0;}for(uint8_t i=0;i<10;i++)mines[positions[i][1]][positions[i][0]]=1;cursor_x=cursor_y=finished=0;draw();}
void app_mines_key(uint16_t key){if(finished){draw();return;}if(key==KEY_LEFT&&cursor_x)cursor_x--;else if(key==KEY_RIGHT&&cursor_x+1<SIZE)cursor_x++;else if(key==KEY_UP&&cursor_y)cursor_y--;else if(key==KEY_DOWN&&cursor_y+1<SIZE)cursor_y++;else if(key=='f'||key=='F')flags[cursor_y][cursor_x]^=1;else if(key==' '&&!flags[cursor_y][cursor_x]){open_cells[cursor_y][cursor_x]=1;if(mines[cursor_y][cursor_x])finished=1;}draw();}
