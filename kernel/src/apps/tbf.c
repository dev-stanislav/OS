#include "app.h"
#include "../fs.h"
#include "../gfx.h"
#include "../keyboard.h"
#include "../libk.h"
#include "../mouse.h"

#define WIN_X 38
#define WIN_Y 48
#define WIN_W 724
#define WIN_H 500
#define GRID_X 64
#define GRID_Y 170
#define TILE_W 130
#define TILE_H 76
#define GRID_COLS 5
#define GRID_ROWS 4
#define VISIBLE_ITEMS (GRID_COLS * GRID_ROWS)
#define NO_BUTTON 255

typedef struct {
    int x;
    int y;
    int w;
    const char *label;
} toolbar_button_t;

static const toolbar_button_t buttons[] = {
    {62, 98, 62, "Back"},
    {132, 98, 62, "Open"},
    {202, 98, 110, "New Folder"},
    {320, 98, 94, "New File"},
    {422, 98, 82, "Rename"},
};

static int directory;
static int entries[FS_MAX_NODES];
static uint8_t count;
static uint8_t selected;
static uint8_t view_start;
static uint8_t last_left;
static uint8_t rename_active;
static char rename_buffer[FS_NAME_MAX + 1];
static uint8_t rename_length;
static const char *notice;
static uint32_t notice_color;

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

static void append_text(char *out, uint16_t cap, const char *text) {
    size_t length = kstrlen(out);
    kstrcpy(out + length, text, cap > length ? cap - length : 0);
}

static void set_notice(const char *text, uint32_t color) {
    notice = text;
    notice_color = color;
}

static uint8_t has_selection(void) {
    return count && selected < count;
}

static void ensure_visible(void) {
    if (!count) {
        view_start = 0;
        return;
    }
    if (selected < view_start) view_start = selected;
    if (selected >= view_start + VISIBLE_ITEMS) view_start = (uint8_t)(selected - VISIBLE_ITEMS + 1);
}

static int compare_entries(int left, int right) {
    const fs_node_t *a = fs_node(left);
    const fs_node_t *b = fs_node(right);
    if (!a || !b) return 0;
    if (a->type != b->type) return a->type == FS_DIR ? -1 : 1;
    return kstrcmp(a->name, b->name);
}

static void refresh(void) {
    count = 0;
    for (int index = 0; index < FS_MAX_NODES; index++) {
        const fs_node_t *node = fs_node(index);
        if (node && node->parent == directory) entries[count++] = index;
    }
    for (uint8_t a = 0; a < count; a++) {
        for (uint8_t b = (uint8_t)(a + 1); b < count; b++) {
            if (compare_entries(entries[a], entries[b]) > 0) {
                int tmp = entries[a];
                entries[a] = entries[b];
                entries[b] = tmp;
            }
        }
    }
    if (selected >= count) selected = count ? (uint8_t)(count - 1) : 0;
    ensure_visible();
}

static void select_name(const char *name) {
    for (uint8_t index = 0; index < count; index++) {
        const fs_node_t *node = fs_node(entries[index]);
        if (node && kstrcmp(node->name, name) == 0) {
            selected = index;
            ensure_visible();
            return;
        }
    }
}

static void backdrop(void) {
    for (int y = 0; y < GFX_HEIGHT; y++) {
        uint32_t color = y < 330
            ? blend(0x000B2238, 0x00273155, (uint32_t)y, 330)
            : blend(0x00273155, 0x00091320, (uint32_t)(y - 330), 270);
        gfx_rect(0, y, GFX_WIDTH, 1, color);
    }
    gfx_rect(0, 515, GFX_WIDTH, 85, 0x00071314);
    gfx_rect(0, 512, GFX_WIDTH, 2, 0x0000D6D6);
    gfx_rect(88, 92, 2, 2, 0x0000D6D6);
    gfx_rect(168, 58, 2, 2, 0x00F2487A);
    gfx_rect(640, 86, 2, 2, 0x0000D6D6);
    gfx_rect(718, 134, 2, 2, 0x00FFFFFF);
}

static void dot(int cx, int cy, int radius, uint32_t color) {
    for (int y = -radius; y <= radius; y++) {
        int yy = y < 0 ? -y : y;
        int width = radius - (yy * yy) / radius;
        gfx_rect(cx - width, cy + y, width * 2 + 1, 1, color);
    }
}

static void draw_button(uint8_t index, uint8_t active) {
    const toolbar_button_t *button = &buttons[index];
    gfx_rect(button->x, button->y, button->w, 26, active ? 0x00333A47 : 0x001C2430);
    gfx_border(button->x, button->y, button->w, 26, active ? 0x0000D6D6 : 0x004B5362);
    gfx_text_bold(button->x + 10, button->y + 10, button->label, active ? 0x0000D6D6 : 0x00FFFFFF);
}

static void folder_icon(int x, int y) {
    gfx_rect(x + 2, y + 14, 46, 30, 0x00F4C95C);
    gfx_rect(x + 8, y + 7, 22, 10, 0x00FFE18D);
    gfx_border(x + 2, y + 14, 46, 30, 0x006B4D14);
    gfx_rect(x + 4, y + 18, 42, 6, 0x00FFE18D);
}

static void file_icon(int x, int y) {
    gfx_rect(x + 9, y + 4, 34, 44, 0x00F2F5FA);
    gfx_border(x + 9, y + 4, 34, 44, 0x00333A47);
    gfx_rect(x + 15, y + 15, 22, 3, 0x0000D6D6);
    gfx_rect(x + 15, y + 24, 22, 3, 0x00F2487A);
    gfx_rect(x + 15, y + 33, 16, 3, 0x0053337C);
}

static void short_name(char *out, const char *name, uint8_t max_chars) {
    uint8_t length = 0;
    while (name[length] && length < max_chars) {
        out[length] = name[length];
        length++;
    }
    if (name[length] && max_chars > 3) {
        out[max_chars - 3] = '.';
        out[max_chars - 2] = '.';
        out[max_chars - 1] = '.';
        out[max_chars] = '\0';
    } else {
        out[length] = '\0';
    }
}

static void draw_tile(uint8_t visible_index, uint8_t entry_index) {
    const fs_node_t *node = fs_node(entries[entry_index]);
    if (!node) return;
    int x = GRID_X + (visible_index % GRID_COLS) * TILE_W;
    int y = GRID_Y + (visible_index / GRID_COLS) * TILE_H;
    uint8_t active = entry_index == selected;
    char label[21];

    gfx_rect(x, y, TILE_W - 12, TILE_H - 8, active ? 0x00244756 : 0x0018212C);
    gfx_border(x, y, TILE_W - 12, TILE_H - 8, active ? 0x0000D6D6 : 0x00333A47);
    if (node->type == FS_DIR) folder_icon(x + 34, y + 8);
    else file_icon(x + 36, y + 6);
    short_name(label, node->name, 18);
    gfx_text_bold(x + 10, y + 58, label, active ? 0x0000D6D6 : 0x00FFFFFF);
}

static void draw_status(void) {
    gfx_rect(WIN_X + 16, WIN_Y + 452, WIN_W - 32, 34, 0x001C2430);
    gfx_border(WIN_X + 16, WIN_Y + 452, WIN_W - 32, 34, 0x00333A47);
    if (rename_active) {
        gfx_text_bold(WIN_X + 28, WIN_Y + 464, "Rename", 0x00FFFFFF);
        gfx_rect(WIN_X + 92, WIN_Y + 459, 246, 22, 0x000D121C);
        gfx_border(WIN_X + 92, WIN_Y + 459, 246, 22, 0x0000D6D6);
        gfx_text_bold(WIN_X + 102, WIN_Y + 466, rename_buffer, 0x0000D6D6);
        gfx_text_bold(WIN_X + 354, WIN_Y + 466, "Enter OK  Backspace Edit", 0x00B8C0CC);
    } else {
        gfx_text_bold(WIN_X + 28, WIN_Y + 464, notice ? notice : "Ready", notice_color);
        gfx_text_bold(WIN_X + 438, WIN_Y + 464, "Enter Open  R Rename  Del Delete", 0x00B8C0CC);
    }
}

static void draw(void) {
    char path[80];
    gfx_cursor_hide();
    fs_path(directory, path, sizeof(path));
    backdrop();
    gfx_rect(WIN_X, WIN_Y, WIN_W, WIN_H, 0x00131820);
    gfx_border(WIN_X, WIN_Y, WIN_W, WIN_H, 0x004B5362);
    gfx_rect(WIN_X, WIN_Y, WIN_W, 36, 0x00242A35);
    dot(WIN_X + 20, WIN_Y + 18, 6, 0x00F2487A);
    dot(WIN_X + 40, WIN_Y + 18, 6, 0x00FFCA5C);
    dot(WIN_X + 60, WIN_Y + 18, 6, 0x0000D6D6);
    gfx_text_bold(WIN_X + 300, WIN_Y + 15, "File Manager", 0x00FFFFFF);
    gfx_rect(WIN_X + WIN_W - 34, WIN_Y + 8, 22, 20, 0x00F2487A);
    gfx_border(WIN_X + WIN_W - 34, WIN_Y + 8, 22, 20, 0x006C1430);
    gfx_text_bold(WIN_X + WIN_W - 26, WIN_Y + 15, "X", 0x00FFFFFF);

    for (uint8_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); i++) draw_button(i, 0);
    gfx_rect(62, 132, 640, 24, 0x000D121C);
    gfx_border(62, 132, 640, 24, 0x00333A47);
    gfx_text_bold(72, 141, "Location", 0x00B8C0CC);
    gfx_text_bold(144, 141, path, 0x0000D6D6);

    gfx_rect(56, 162, 662, 324, 0x000D121C);
    gfx_border(56, 162, 662, 324, 0x00333A47);
    if (!count) {
        gfx_text_bold(326, 318, "Empty Folder", 0x00B8C0CC);
    } else {
        uint8_t visible = (uint8_t)(count - view_start);
        if (visible > VISIBLE_ITEMS) visible = VISIBLE_ITEMS;
        for (uint8_t i = 0; i < visible; i++) draw_tile(i, (uint8_t)(view_start + i));
    }
    draw_status();
    gfx_cursor(mouse_x(), mouse_y());
}

static uint8_t button_at(int x, int y) {
    for (uint8_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); i++) {
        const toolbar_button_t *button = &buttons[i];
        if (x >= button->x && x < button->x + button->w && y >= button->y && y < button->y + 26) return i;
    }
    return NO_BUTTON;
}

static uint8_t item_at(int x, int y) {
    uint8_t visible = count > view_start ? (uint8_t)(count - view_start) : 0;
    if (visible > VISIBLE_ITEMS) visible = VISIBLE_ITEMS;
    for (uint8_t i = 0; i < visible; i++) {
        int tx = GRID_X + (i % GRID_COLS) * TILE_W;
        int ty = GRID_Y + (i / GRID_COLS) * TILE_H;
        if (x >= tx && x < tx + TILE_W - 12 && y >= ty && y < ty + TILE_H - 8) return (uint8_t)(view_start + i);
    }
    return 255;
}

static void make_candidate(char *out, const char *base, const char *ext, uint8_t number) {
    char text[8];
    kstrcpy(out, base, FS_NAME_MAX + 1);
    if (number > 1) {
        append_text(out, FS_NAME_MAX + 1, " ");
        kitoa(number, text, 10);
        append_text(out, FS_NAME_MAX + 1, text);
    }
    append_text(out, FS_NAME_MAX + 1, ext);
}

static void unique_name(char *out, const char *base, const char *ext) {
    for (uint8_t number = 1; number < 99; number++) {
        make_candidate(out, base, ext, number);
        if (fs_resolve(out, directory) < 0) return;
    }
    make_candidate(out, base, ext, 99);
}

static void create_folder(void) {
    char name[FS_NAME_MAX + 1];
    unique_name(name, "New Folder", "");
    if (fs_create(name, FS_DIR, directory) == FS_OK) {
        refresh();
        select_name(name);
        set_notice("Folder created", 0x0000D6D6);
    } else set_notice("Cannot create folder", 0x00F2487A);
}

static void create_file(void) {
    char name[FS_NAME_MAX + 1];
    unique_name(name, "New File", ".txt");
    if (fs_write(name, "", directory) == FS_OK) {
        refresh();
        select_name(name);
        set_notice("File created", 0x0000D6D6);
    } else set_notice("Cannot create file", 0x00F2487A);
}

static uint8_t open_selected(void) {
    const fs_node_t *node;
    char *args[1];
    if (!has_selection()) return 0;
    node = fs_node(entries[selected]);
    if (!node) return 0;
    if (node->type == FS_DIR) {
        directory = entries[selected];
        app_set_workdir(directory);
        selected = 0;
        view_start = 0;
        refresh();
        set_notice("Folder opened", 0x0000D6D6);
        return 0;
    } else {
        app_set_workdir(directory);
        args[0] = (char *)node->name;
        app_run("free", args, 1);
        return 1;
    }
}

static void go_back(void) {
    const fs_node_t *node = fs_node(directory);
    if (node && node->parent >= 0) {
        directory = node->parent;
        app_set_workdir(directory);
        selected = 0;
        view_start = 0;
        refresh();
        set_notice("Parent folder", 0x0000D6D6);
    }
}

static void begin_rename(void) {
    const fs_node_t *node;
    if (!has_selection()) {
        set_notice("Nothing selected", 0x00F2487A);
        return;
    }
    node = fs_node(entries[selected]);
    if (!node) return;
    kstrcpy(rename_buffer, node->name, sizeof(rename_buffer));
    rename_length = (uint8_t)kstrlen(rename_buffer);
    rename_active = 1;
    set_notice("Renaming", 0x0000D6D6);
}

static void commit_rename(void) {
    const fs_node_t *node;
    char new_name[FS_NAME_MAX + 1];
    if (!rename_length || !has_selection()) {
        rename_active = 0;
        set_notice("Rename cancelled", 0x00B8C0CC);
        return;
    }
    node = fs_node(entries[selected]);
    if (!node) return;
    kstrcpy(new_name, rename_buffer, sizeof(new_name));
    if (fs_move(node->name, new_name, directory) == FS_OK) {
        rename_active = 0;
        refresh();
        select_name(new_name);
        set_notice("Renamed", 0x0000D6D6);
    } else {
        rename_active = 0;
        set_notice("Rename failed", 0x00F2487A);
    }
}

static void delete_selected(void) {
    const fs_node_t *node;
    if (!has_selection()) {
        set_notice("Nothing selected", 0x00F2487A);
        return;
    }
    node = fs_node(entries[selected]);
    if (!node) return;
    if (fs_remove(node->name, directory, node->type == FS_DIR) == FS_OK) {
        refresh();
        set_notice("Deleted", 0x0000D6D6);
    } else set_notice("Delete failed", 0x00F2487A);
}

void app_tbf_start(char **args, uint8_t n) {
    (void)args;
    (void)n;
    gfx_init();
    directory = app_get_workdir();
    selected = 0;
    view_start = 0;
    last_left = 0;
    rename_active = 0;
    set_notice("Ready", 0x00B8C0CC);
    refresh();
    draw();
}

void app_tbf_tick(uint32_t ticks) {
    (void)ticks;
    uint8_t click = mouse_left();
    if (click && !last_left) {
        int x = mouse_x();
        int y = mouse_y();
        uint8_t button;
        uint8_t item;
        if (x >= WIN_X + WIN_W - 34 && x < WIN_X + WIN_W - 12 && y >= WIN_Y + 8 && y < WIN_Y + 28) {
            app_run("luma", 0, 0);
            return;
        }
        if (!rename_active) {
            button = button_at(x, y);
            if (button != NO_BUTTON) {
                if (button == 0) go_back();
                else if (button == 1 && open_selected()) {
                    last_left = click;
                    return;
                }
                else if (button == 2) create_folder();
                else if (button == 3) create_file();
                else if (button == 4) begin_rename();
                draw();
                last_left = click;
                return;
            }
            item = item_at(x, y);
            if (item != 255) {
                selected = item;
                ensure_visible();
                set_notice("Selected", 0x00B8C0CC);
                draw();
            }
        }
    }
    last_left = click;
}

void app_tbf_key(uint16_t key) {
    if (rename_active) {
        if (key == '\n') commit_rename();
        else if (key == '\b') {
            if (rename_length) rename_buffer[--rename_length] = '\0';
        } else if (key == KEY_ESCAPE) {
            rename_active = 0;
            set_notice("Rename cancelled", 0x00B8C0CC);
        } else if (key >= 32 && key < 127 && key != '/' && rename_length < FS_NAME_MAX) {
            rename_buffer[rename_length++] = (char)key;
            rename_buffer[rename_length] = '\0';
        }
        draw();
        return;
    }

    if (key == '\b') go_back();
    else if (key == KEY_UP && has_selection() && selected >= GRID_COLS) selected = (uint8_t)(selected - GRID_COLS);
    else if (key == KEY_DOWN && has_selection() && selected + GRID_COLS < count) selected = (uint8_t)(selected + GRID_COLS);
    else if (key == KEY_LEFT && has_selection() && selected) selected--;
    else if (key == KEY_RIGHT && has_selection() && selected + 1 < count) selected++;
    else if (key == '\n' && open_selected()) return;
    else if (key == 'r' || key == 'R') begin_rename();
    else if (key == 'n' || key == 'N') create_file();
    else if (key == 'f' || key == 'F') create_folder();
    else if (key == KEY_DELETE) delete_selected();
    ensure_visible();
    draw();
}
