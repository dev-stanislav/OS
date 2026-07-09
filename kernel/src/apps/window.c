#include "window.h"
#include "sys_settings.h"
#include "../gfx.h"

static uint32_t rgb(uint32_t r, uint32_t g, uint32_t b) {
    return (r << 16) | (g << 8) | b;
}

static uint32_t blend(uint32_t a, uint32_t b, uint32_t step, uint32_t max) {
    int ar = (int)((a >> 16) & 255), ag = (int)((a >> 8) & 255), ab = (int)(a & 255);
    int br = (int)((b >> 16) & 255), bg = (int)((b >> 8) & 255), bb = (int)(b & 255);
    return rgb((uint32_t)(ar + (br - ar) * (int)step / (int)max),
               (uint32_t)(ag + (bg - ag) * (int)step / (int)max),
               (uint32_t)(ab + (bb - ab) * (int)step / (int)max));
}

static void round_rect(int x, int y, int w, int h, int radius, uint32_t color) {
    if (radius < 1 || w <= radius * 2 || h <= radius * 2) {
        gfx_rect(x, y, w, h, color);
        return;
    }
    for (int row = 0; row < radius; row++) {
        int dy = radius - row;
        int inset = (dy * dy) / (radius * 2);
        gfx_rect(x + inset, y + row, w - inset * 2, 1, color);
        gfx_rect(x + inset, y + h - 1 - row, w - inset * 2, 1, color);
    }
    gfx_rect(x, y + radius, w, h - radius * 2, color);
}

static void round_box(int x, int y, int w, int h, int radius, uint32_t border, uint32_t fill) {
    round_rect(x, y, w, h, radius, border);
    round_rect(x + 2, y + 2, w - 4, h - 4, radius - 2, fill);
}

void app_window_background(void) {
    const sys_settings_t *settings = sys_settings_get();
    uint32_t top = 0x00BBDCF6, mid = 0x00DCD6FF, low = 0x00F7B6C9, bottom = 0x007FCFEA;
    if (settings->wallpaper == 1) {
        top = 0x00C7F5DD; mid = 0x00D7F8F1; low = 0x00F7E6C6; bottom = 0x008ED7B2;
    } else if (settings->wallpaper == 2) {
        top = 0x00BFC8FF; mid = 0x00E1D1FF; low = 0x00FFB7DE; bottom = 0x0095B8FF;
    }
    for (int y = 0; y < GFX_HEIGHT; y++) {
        uint32_t color;
        if (y < 220) color = blend(top, mid, (uint32_t)y, 220);
        else if (y < 430) color = blend(mid, low, (uint32_t)(y - 220), 210);
        else color = blend(low, bottom, (uint32_t)(y - 430), 170);
        gfx_rect(0, y, GFX_WIDTH, 1, color);
    }
    gfx_rect(0, 0, GFX_WIDTH, 28, 0x00EEF6FF);
    gfx_text_bold(18, 10, "Luma", 0x002A3654);
    gfx_text_bold(710, 10, "Ready", 0x002A3654);
    round_rect(324, 584, 152, 7, 4, 0x007899B8);
}

void app_window_draw(const app_window_t *window) {
    uint32_t accent = sys_settings_accent_color();
    app_window_background();
    round_rect(window->x + 12, window->y + 16, window->w, window->h, 28, 0x004B5F75);
    round_rect(window->x + 6, window->y + 8, window->w, window->h, 28, 0x00788CA5);
    round_box(window->x, window->y, window->w, window->h, 28, accent, 0x00F5F8FC);
    round_rect(window->x + 12, window->y + 10, window->w - 24, 42, 20, 0x00FFFFFF);
    gfx_text_bold(window->x + 28, window->y + 25, window->title, 0x00242A35);
    round_rect(window->x + window->w - 46, window->y + 16, 28, 24, 12, 0x00F2487A);
    gfx_text_bold(window->x + window->w - 36, window->y + 25, "X", 0x00FFFFFF);
}

uint8_t app_window_close_hit(const app_window_t *window, int x, int y) {
    return x >= window->x + window->w - 46 && x < window->x + window->w - 18 &&
           y >= window->y + 16 && y < window->y + 40;
}

uint8_t app_window_title_hit(const app_window_t *window, int x, int y) {
    return x >= window->x + 12 && x < window->x + window->w - 56 &&
           y >= window->y + 10 && y < window->y + 54;
}

uint8_t app_window_drag(app_window_t *window, app_window_drag_t *drag, uint8_t down, int x, int y) {
    int nx, ny;
    if (!down) {
        if (drag->active) {
            drag->active = 0;
            return 1;
        }
        return 0;
    }
    if (!drag->active) {
        if (!app_window_title_hit(window, x, y) || app_window_close_hit(window, x, y)) return 0;
        drag->active = 1;
        drag->offset_x = x - window->x;
        drag->offset_y = y - window->y;
        return 1;
    }
    nx = x - drag->offset_x;
    ny = y - drag->offset_y;
    if (nx < 6) nx = 6;
    if (ny < 34) ny = 34;
    if (nx + window->w > GFX_WIDTH - 6) nx = GFX_WIDTH - 6 - window->w;
    if (ny + window->h > GFX_HEIGHT - 10) ny = GFX_HEIGHT - 10 - window->h;
    if (nx == window->x && ny == window->y) return 0;
    window->x = nx;
    window->y = ny;
    return 1;
}
