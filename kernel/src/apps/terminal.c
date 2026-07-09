#include "app.h"
#include "window.h"
#include "../fs.h"
#include "../gfx.h"
#include "../libk.h"
#include "../mouse.h"

#define OUT_ROWS 12
#define OUT_COLS 92

static app_window_t win = {82, 78, 636, 430, "Terminal"};
static app_window_drag_t drag;
static char line[48], out[OUT_ROWS][OUT_COLS];
static uint8_t length, last_left;
static int current_dir;

static int term_x(void) { return win.x + 30; }
static int term_y(void) { return win.y + 88; }
static int term_w(void) { return win.w - 60; }
static int term_h(void) { return 286; }

static void print(const char *text) {
    for (uint8_t i = 1; i < OUT_ROWS; i++) kstrcpy(out[i - 1], out[i], OUT_COLS);
    kstrcpy(out[OUT_ROWS - 1], text, OUT_COLS);
}

static void clear_output(void) {
    for (uint8_t i = 0; i < OUT_ROWS; i++) out[i][0] = 0;
}

static void draw(void) {
    char path[80];
    fs_path(current_dir, path, sizeof(path));
    gfx_cursor_hide();
    app_window_draw(&win);
    gfx_rect(term_x(), term_y(), term_w(), term_h(), 0x00131820);
    gfx_border(term_x(), term_y(), term_w(), term_h(), 0x00354559);
    gfx_text_bold(term_x() + 12, term_y() + 14, path, 0x0000D6D6);
    for (uint8_t i = 0; i < OUT_ROWS; i++) gfx_text_bold(term_x() + 12, term_y() + 42 + i * 18, out[i], 0x00FFFFFF);
    gfx_text_bold(term_x() + 12, term_y() + term_h() - 22, ">", 0x0000D6D6);
    gfx_text_bold(term_x() + 26, term_y() + term_h() - 22, line, 0x00FFFFFF);
    gfx_rect(win.x + 30, win.y + win.h - 42, win.w - 60, 24, 0x00EAF1F8);
    gfx_border(win.x + 30, win.y + win.h - 42, win.w - 60, 24, 0x00D1DDE8);
    gfx_text_bold(win.x + 44, win.y + win.h - 33, "Type Help", 0x00242A35);
    gfx_cursor(mouse_x(), mouse_y());
}

static char *next(char **p) {
    while (**p == ' ') *p += 1;
    if (!**p) return 0;
    char *word = *p;
    while (**p && **p != ' ') *p += 1;
    if (**p) *(*p)++ = 0;
    return word;
}

static void list(void) {
    uint8_t found = 0;
    for (int i = 0; i < FS_MAX_NODES; i++) {
        const fs_node_t *node = fs_node(i);
        if (node && node->parent == current_dir) {
            char item[OUT_COLS];
            kstrcpy(item, node->type == FS_DIR ? "DIR " : "FILE ", sizeof(item));
            kstrcpy(item + kstrlen(item), node->name, sizeof(item) - kstrlen(item));
            print(item);
            found = 1;
        }
    }
    if (!found) print("EMPTY");
}

static void run(char *cmd, char *arg, char *rest) {
    if (kstrcmp(cmd, "help") == 0) print("HELP LS CD PWD CAT TOUCH MKDIR WRITE SETTINGS RUN CLEAR EXIT");
    else if (kstrcmp(cmd, "clear") == 0) clear_output();
    else if (kstrcmp(cmd, "pwd") == 0) {
        char path[80];
        fs_path(current_dir, path, sizeof(path));
        print(path);
    } else if (kstrcmp(cmd, "ls") == 0) list();
    else if (kstrcmp(cmd, "cd") == 0) {
        int target = fs_resolve(arg ? arg : "/", current_dir);
        const fs_node_t *node = fs_node(target);
        if (node && node->type == FS_DIR) {
            current_dir = target;
            print("OK");
        } else print("DIRECTORY NOT FOUND");
    } else if (kstrcmp(cmd, "cat") == 0 && arg) {
        int index = fs_resolve(arg, current_dir);
        const fs_node_t *node = fs_node(index);
        if (node && node->type == FS_FILE) print(node->data);
        else print("FILE NOT FOUND");
    } else if (kstrcmp(cmd, "touch") == 0 && arg) print(fs_create(arg, FS_FILE, current_dir) == FS_OK ? "CREATED" : "CANNOT CREATE");
    else if (kstrcmp(cmd, "mkdir") == 0 && arg) print(fs_create(arg, FS_DIR, current_dir) == FS_OK ? "CREATED" : "CANNOT CREATE");
    else if (kstrcmp(cmd, "write") == 0 && arg) print(fs_write(arg, rest ? rest : "", current_dir) == FS_OK ? "SAVED" : "CANNOT SAVE");
    else if (kstrcmp(cmd, "run") == 0 && arg) {
        app_set_workdir(current_dir);
        app_run(arg, 0, 0);
        return;
    } else if (kstrcmp(cmd, "settings") == 0) {
        app_run("settings", 0, 0);
        return;
    } else if (kstrcmp(cmd, "luma") == 0 || kstrcmp(cmd, "sproot") == 0 || kstrcmp(cmd, "exit") == 0) {
        app_run("luma", 0, 0);
        return;
    } else print("UNKNOWN COMMAND - TYPE HELP");
    length = 0;
    line[0] = 0;
    draw();
}

static void execute(void) {
    char copy[48];
    kstrcpy(copy, line, sizeof(copy));
    char *cursor = copy;
    char *cmd = next(&cursor);
    char *arg = next(&cursor);
    while (*cursor == ' ') cursor++;
    if (cmd) run(cmd, arg, cursor);
    else {
        length = 0;
        line[0] = 0;
        draw();
    }
}

void app_terminal_start(char **args, uint8_t count) {
    (void)args;
    (void)count;
    gfx_init();
    current_dir = app_get_workdir();
    length = 0;
    line[0] = 0;
    last_left = 0;
    drag.active = 0;
    clear_output();
    print("MINIOS SHELL - TYPE HELP");
    draw();
}

void app_terminal_tick(uint32_t ticks) {
    (void)ticks;
    uint8_t click = mouse_left();
    if (click && !last_left && app_window_close_hit(&win, mouse_x(), mouse_y())) {
        app_run("luma", 0, 0);
        return;
    }
    if (app_window_drag(&win, &drag, click, mouse_x(), mouse_y())) {
        draw();
        last_left = click;
        return;
    }
    last_left = click;
}

void app_terminal_key(uint16_t key) {
    if (key == '\n') execute();
    else if (key == '\b' && length) {
        line[--length] = 0;
        draw();
    } else if (key >= 32 && key < 127 && length < 47) {
        line[length++] = (char)key;
        line[length] = 0;
        draw();
    }
}
