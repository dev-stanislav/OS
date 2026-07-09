#include "app.h"
#include "window.h"
#include "../gfx.h"
#include "../mouse.h"

static const app_window_t win = {88, 70, 624, 456, "Paint"};
static uint32_t color;
static int last_x, last_y;
static uint8_t drawing, last_left;

static int canvas_x(void) { return win.x + 32; }
static int canvas_y(void) { return win.y + 92; }
static int canvas_w(void) { return win.w - 64; }
static int canvas_h(void) { return 286; }
static int swatch_y(void) { return win.y + win.h - 58; }

static void dot(int x, int y) {
    for (int yy = -3; yy <= 3; yy++)
        for (int xx = -3; xx <= 3; xx++)
            if (xx * xx + yy * yy <= 9) gfx_pixel(x + xx, y + yy, color);
}

static void stroke(int x0, int y0, int x1, int y1) {
    int dx = x1 - x0, dy = y1 - y0, steps = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
    if (ady > steps) steps = ady;
    for (int i = 0; i <= steps; i++) dot(x0 + (dx * i) / (steps ? steps : 1), y0 + (dy * i) / (steps ? steps : 1));
}

static void draw_swatches(void) {
    uint32_t swatches[] = {0x00000000, 0x00FF3B30, 0x000078FF, 0x0000A86B, 0x00FFCC00};
    gfx_rect(win.x + 32, swatch_y() - 8, win.w - 64, 42, 0x00EAF1F8);
    gfx_border(win.x + 32, swatch_y() - 8, win.w - 64, 42, 0x00D1DDE8);
    for (int i = 0; i < 5; i++) {
        int x = win.x + 48 + i * 44;
        gfx_rect(x, swatch_y(), 30, 22, swatches[i]);
        gfx_border(x - 2, swatch_y() - 2, 34, 26, color == swatches[i] ? 0x000078FF : 0x00B8C4D1);
    }
}

static void frame(void) {
    gfx_cursor_hide();
    app_window_draw(&win);
    gfx_rect(canvas_x(), canvas_y(), canvas_w(), canvas_h(), 0x00FFFFFF);
    gfx_border(canvas_x(), canvas_y(), canvas_w(), canvas_h(), 0x00C9D7E6);
    draw_swatches();
    gfx_text_bold(win.x + 300, swatch_y() + 8, "C Clear", 0x00242A35);
    gfx_cursor(mouse_x(), mouse_y());
}

void app_paint_start(char **args, uint8_t n) {
    (void)args;
    (void)n;
    gfx_init();
    color = 0x00000000;
    drawing = last_left = 0;
    frame();
}

void app_paint_tick(uint32_t t) {
    (void)t;
    int x = mouse_x(), y = mouse_y();
    uint8_t click = mouse_left();
    if (click && !last_left && app_window_close_hit(&win, x, y)) {
        app_run("luma", 0, 0);
        return;
    }
    if (click && y >= swatch_y() - 4 && y < swatch_y() + 28 && x >= win.x + 48 && x < win.x + 48 + 5 * 44) {
        color = ((uint32_t[]){0x00000000, 0x00FF3B30, 0x000078FF, 0x0000A86B, 0x00FFCC00})[(x - (win.x + 48)) / 44];
        drawing = 0;
        draw_swatches();
        gfx_cursor(x, y);
    } else if (click && x > canvas_x() && x < canvas_x() + canvas_w() && y > canvas_y() && y < canvas_y() + canvas_h()) {
        if (!drawing) {
            last_x = x;
            last_y = y;
            drawing = 1;
        }
        stroke(last_x, last_y, x, y);
        last_x = x;
        last_y = y;
    } else drawing = 0;
    last_left = click;
}

void app_paint_key(uint16_t key) {
    if (key == 'c' || key == 'C') frame();
}
