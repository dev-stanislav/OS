#include "sys_settings.h"
#include "../gfx.h"

static sys_settings_t settings = {0, 0, 0};
static const uint32_t accents[] = {0x000078FF, 0x0000D6D6, 0x00F2487A, 0x0000A86B};

const sys_settings_t *sys_settings_get(void) {
    return &settings;
}

void sys_settings_set_wallpaper(uint8_t value) {
    settings.wallpaper = (uint8_t)(value % 3);
}

void sys_settings_next_wallpaper(void) {
    sys_settings_set_wallpaper((uint8_t)(settings.wallpaper + 1));
}

void sys_settings_set_accent(uint8_t value) {
    settings.accent = (uint8_t)(value % (sizeof(accents) / sizeof(accents[0])));
}

uint32_t sys_settings_accent_color(void) {
    return accents[settings.accent];
}

void sys_settings_set_font(uint8_t value) {
    settings.font = (uint8_t)(value % 3);
    gfx_set_font(settings.font);
}

void sys_settings_next_font(void) {
    sys_settings_set_font((uint8_t)(settings.font + 1));
}
