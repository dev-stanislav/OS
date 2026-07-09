#include "app.h"
#include "window.h"
#include "../fs.h"
#include "../gfx.h"
#include "../keyboard.h"
#include "../libk.h"
#include "../mouse.h"

static const app_window_t win = {104, 76, 592, 438, "Text Editor"};
static char body[FS_FILE_MAX + 1], filename[80];
static uint16_t length;
static const char *notice;
static uint8_t blink, last_left;

static void draw(void) {
    int body_x = win.x + 28, body_y = win.y + 92, body_w = win.w - 56, body_h = 270;
    int x = body_x + 12, y = body_y + 14;
    gfx_cursor_hide();
    app_window_draw(&win);
    gfx_text_bold(win.x + 30, win.y + 62, filename, 0x00006D9A);
    gfx_rect(body_x, body_y, body_w, body_h, 0x00FFFFFF);
    gfx_border(body_x, body_y, body_w, body_h, 0x00C9D7E6);
    for (uint16_t i = 0; i < length; i++) {
        if (body[i] == '\n' || x > body_x + body_w - 18) {
            x = body_x + 12;
            y += 14;
            if (y > body_y + body_h - 18) break;
            if (body[i] == '\n') continue;
        }
        char text[2] = {body[i], 0};
        gfx_text(x, y, text, 0x00131820);
        x += 6;
    }
    if (blink && y < body_y + body_h - 10) gfx_rect(x, y, 2, 9, 0x00131820);
    gfx_rect(win.x + 28, win.y + win.h - 54, win.w - 56, 30, 0x00EAF1F8);
    gfx_border(win.x + 28, win.y + win.h - 54, win.w - 56, 30, 0x00D1DDE8);
    gfx_text_bold(win.x + 42, win.y + win.h - 43, notice ? notice : "Ctrl S to Save", 0x00242A35);
    gfx_cursor(mouse_x(), mouse_y());
}

void app_free_start(char **args, uint8_t count) {
    const char *path = count ? args[0] : "untitled.txt";
    kstrcpy(filename, path, sizeof(filename));
    length = 0;
    notice = "NEW DOCUMENT";
    int idx = fs_resolve(path, app_get_workdir());
    const fs_node_t *node = fs_node(idx);
    if (node && node->type == FS_FILE) {
        length = node->size;
        kmemcpy(body, node->data, length);
        notice = "OPENED";
    }
    blink = 1;
    last_left = 0;
    gfx_init();
    draw();
}

void app_free_tick(uint32_t ticks) {
    uint8_t click = mouse_left();
    if (click && !last_left && app_window_close_hit(&win, mouse_x(), mouse_y())) {
        app_run("luma", 0, 0);
        return;
    }
    last_left = click;
    if ((ticks % 20) == 0) {
        blink ^= 1;
        draw();
    }
}

void app_free_key(uint16_t key) {
    notice = 0;
    if (key == KEY_SAVE) {
        body[length] = 0;
        notice = fs_write(filename, body, app_get_workdir()) == FS_OK ? "SAVED" : "SAVE FAILED";
    } else if (key == '\b') {
        if (length) length--;
    } else if (key == '\n') {
        if (length < FS_FILE_MAX) body[length++] = '\n';
    } else if (key >= 32 && key < 127 && length < FS_FILE_MAX) body[length++] = (char)key;
    blink = 1;
    draw();
}
