#ifndef SYS_SETTINGS_H
#define SYS_SETTINGS_H

#include <stdint.h>

typedef struct {
    uint8_t wallpaper;
    uint8_t accent;
} sys_settings_t;

const sys_settings_t *sys_settings_get(void);
void sys_settings_set_wallpaper(uint8_t value);
void sys_settings_next_wallpaper(void);
void sys_settings_set_accent(uint8_t value);
uint32_t sys_settings_accent_color(void);

#endif
