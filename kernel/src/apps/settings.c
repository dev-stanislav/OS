#include "app.h"
#include "sys_settings.h"
#include "window.h"
#include "../gfx.h"
#include "../mouse.h"

static app_window_t win = {146, 78, 508, 404, "Settings"};
static app_window_drag_t drag;
static uint8_t last_left;
static const char *notice;

static void button(int x, int y, int w, const char *label, uint8_t active) {
    uint32_t accent = sys_settings_accent_color();
    gfx_rect(x, y, w, 28, active ? 0x00D8E9FF : 0x00FFFFFF);
    gfx_border(x, y, w, 28, active ? accent : 0x00C7D3DF);
    gfx_text_bold(x + 12, y + 11, label, active ? accent : 0x00242A35);
}

static void row_title(int x, int y, const char *title, const char *hint) {
    gfx_text_bold(x, y, title, 0x00242A35);
    gfx_text_bold(x, y + 18, hint, 0x00607082);
}

static void swatch(int x, int y, uint32_t color, uint8_t active) {
    gfx_rect(x, y, 34, 26, color);
    gfx_border(x - 3, y - 3, 40, 32, active ? 0x00242A35 : 0x00C7D3DF);
}

static void draw(void) {
    const sys_settings_t *settings = sys_settings_get();
    int x = win.x + 34;
    int y = win.y + 76;
    gfx_cursor_hide();
    app_window_draw(&win);
    gfx_rect(win.x + 24, win.y + 62, 118, win.h - 92, 0x00EAF1F8);
    gfx_border(win.x + 24, win.y + 62, 118, win.h - 92, 0x00D1DDE8);
    gfx_text_bold(win.x + 42, win.y + 86, "Appearance", sys_settings_accent_color());
    gfx_text_bold(win.x + 42, win.y + 118, "Desktop", 0x00242A35);
    gfx_text_bold(win.x + 42, win.y + 150, "System", 0x00242A35);

    x = win.x + 166;
    row_title(x, y, "Wallpaper", "Click a style");
    button(x, y + 36, 72, "Sky", settings->wallpaper == 0);
    button(x + 82, y + 36, 72, "Mint", settings->wallpaper == 1);
    button(x + 164, y + 36, 72, "Dusk", settings->wallpaper == 2);

    y += 104;
    row_title(x, y, "Accent", "Window and controls");
    swatch(x, y + 40, 0x000078FF, settings->accent == 0);
    swatch(x + 54, y + 40, 0x0000D6D6, settings->accent == 1);
    swatch(x + 108, y + 40, 0x00F2487A, settings->accent == 2);
    swatch(x + 162, y + 40, 0x0000A86B, settings->accent == 3);

    y += 112;
    row_title(x, y, "Font", "System text style");
    button(x, y + 36, 76, "Pixel", settings->font == 0);
    button(x + 86, y + 36, 76, "Clean", settings->font == 1);
    button(x + 172, y + 36, 72, "Bold", settings->font == 2);

    gfx_rect(win.x + 34, win.y + win.h - 42, win.w - 68, 24, 0x00EAF1F8);
    gfx_border(win.x + 34, win.y + win.h - 42, win.w - 68, 24, 0x00D1DDE8);
    gfx_text_bold(win.x + 48, win.y + win.h - 33, notice ? notice : "Settings ready", 0x00242A35);
    gfx_cursor(mouse_x(), mouse_y());
}

static uint8_t hit(int x, int y, int bx, int by, int bw, int bh) {
    return x >= bx && x < bx + bw && y >= by && y < by + bh;
}

void app_settings_start(char **args, uint8_t count) {
    (void)args;
    (void)count;
    gfx_init();
    last_left = 0;
    drag.active = 0;
    notice = "Settings ready";
    draw();
}

void app_settings_tick(uint32_t ticks) {
    (void)ticks;
    uint8_t click = mouse_left();
    int mx = mouse_x(), my = mouse_y();
    int x = win.x + 166, y = win.y + 76;
    if (click && !last_left && app_window_close_hit(&win, mx, my)) {
        app_run("luma", 0, 0);
        return;
    }
    if (app_window_drag(&win, &drag, click, mx, my)) {
        draw();
        last_left = click;
        return;
    }
    if (click && !last_left) {
        if (hit(mx, my, x, y + 36, 72, 28)) { sys_settings_set_wallpaper(0); notice = "Wallpaper Sky"; draw(); }
        else if (hit(mx, my, x + 82, y + 36, 72, 28)) { sys_settings_set_wallpaper(1); notice = "Wallpaper Mint"; draw(); }
        else if (hit(mx, my, x + 164, y + 36, 72, 28)) { sys_settings_set_wallpaper(2); notice = "Wallpaper Dusk"; draw(); }
        else if (hit(mx, my, x - 3, y + 141, 40, 32)) { sys_settings_set_accent(0); notice = "Accent Blue"; draw(); }
        else if (hit(mx, my, x + 51, y + 141, 40, 32)) { sys_settings_set_accent(1); notice = "Accent Cyan"; draw(); }
        else if (hit(mx, my, x + 105, y + 141, 40, 32)) { sys_settings_set_accent(2); notice = "Accent Pink"; draw(); }
        else if (hit(mx, my, x + 159, y + 141, 40, 32)) { sys_settings_set_accent(3); notice = "Accent Green"; draw(); }
        else if (hit(mx, my, x, y + 252, 76, 28)) { sys_settings_set_font(0); notice = "Font Pixel"; draw(); }
        else if (hit(mx, my, x + 86, y + 252, 76, 28)) { sys_settings_set_font(1); notice = "Font Clean"; draw(); }
        else if (hit(mx, my, x + 172, y + 252, 72, 28)) { sys_settings_set_font(2); notice = "Font Bold"; draw(); }
    }
    last_left = click;
}

void app_settings_key(uint16_t key) {
    if (key >= '1' && key <= '3') {
        sys_settings_set_wallpaper((uint8_t)(key - '1'));
        notice = "Wallpaper changed";
        draw();
    } else if (key >= '4' && key <= '7') {
        sys_settings_set_accent((uint8_t)(key - '4'));
        notice = "Accent changed";
        draw();
    } else if (key >= '8' && key <= '9') {
        sys_settings_set_font((uint8_t)(key - '8'));
        notice = "Font changed";
        draw();
    } else if (key == '0') {
        sys_settings_set_font(2);
        notice = "Font changed";
        draw();
    }
}
