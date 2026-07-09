#ifndef APP_WINDOW_H
#define APP_WINDOW_H

#include <stdint.h>

typedef struct {
    int x;
    int y;
    int w;
    int h;
    const char *title;
} app_window_t;

void app_window_background(void);
void app_window_draw(const app_window_t *window);
uint8_t app_window_close_hit(const app_window_t *window, int x, int y);

#endif
