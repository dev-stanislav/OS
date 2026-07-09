extern "C" {
#include "app.h"
#include "../fs.h"
#include "../gfx.h"
#include "../keyboard.h"
#include "../libk.h"
#include "../mouse.h"
#include "../rtc.h"
#include "../timer.h"
#include "sys_settings.h"
}

namespace {

constexpr uint32_t Black = 0x00000000;
constexpr uint32_t White = 0x00FFFFFF;
constexpr uint32_t Panel = 0x00242A35;
constexpr uint32_t PanelLight = 0x00333A47;
constexpr uint32_t PanelDark = 0x00131820;
constexpr uint32_t Accent = 0x0000D6D6;
constexpr uint32_t AccentSoft = 0x00008F90;
constexpr uint32_t Pink = 0x00F2487A;
constexpr uint32_t Violet = 0x0053337C;
constexpr uint32_t NoHover = 255;
constexpr uint8_t ContextCount = 7;
constexpr int ContextHeight = 16 + ContextCount * 28;

struct Launcher {
    int x;
    int y;
    const char *label;
    const char *status;
    const char *app;
    uint8_t kind;
};

constexpr Launcher launchers[] = {
    {34, 62, "Files", "File Manager", "tbf", 0},
    {34, 134, "Editor", "Text Editor", "free", 1},
    {34, 206, "Paint", "Paint", "paint", 2},
    {34, 278, "Terminal", "Terminal", "terminal", 3},
    {34, 350, "Settings", "Settings", "settings", 4},
};

constexpr uint8_t launcher_count = static_cast<uint8_t>(sizeof(launchers) / sizeof(launchers[0]));
constexpr uint8_t MaxWindows = 8;
constexpr uint8_t NoWindow = 255;
constexpr uint8_t FileVisibleRows = 7;
constexpr uint8_t FileButtonCount = 6;
constexpr uint8_t TerminalRows = 5;
constexpr uint8_t TerminalCols = 58;
constexpr uint8_t PaintCols = 80;
constexpr uint8_t PaintRows = 40;
constexpr uint8_t PaintEmpty = 255;
constexpr uint8_t PaintColorCount = 5;

constexpr uint32_t PaintPalette[PaintColorCount] = {
    0x00000000,
    0x00FF3B30,
    0x000078FF,
    0x0000A86B,
    0x00FFCC00,
};

struct FileButton {
    int dx;
    int w;
    const char *label;
};

constexpr FileButton FileButtons[FileButtonCount] = {
    {24, 48, "Back"},
    {78, 48, "Open"},
    {132, 72, "New Dir"},
    {210, 72, "New Txt"},
    {288, 66, "Rename"},
    {360, 58, "Delete"},
};

struct DesktopWindow {
    uint8_t type;
    int x;
    int y;
    int w;
    int h;
    int drag_x;
    int drag_y;
    int directory;
    int file_index;
    uint8_t selected;
    uint8_t view_start;
    uint8_t rename_active;
    uint8_t line_len;
    uint16_t editor_len;
    uint8_t paint_color;
    uint8_t painting;
    int paint_last_col;
    int paint_last_row;
    char title[FS_NAME_MAX + 1];
    char line[48];
    char editor[FS_FILE_MAX + 1];
    char output[TerminalRows][TerminalCols];
    uint8_t paint[PaintRows][PaintCols];
};

uint8_t menu;
uint8_t last_left;
uint8_t last_right;
uint8_t hover_launcher;
uint8_t hover_menu;
uint8_t hover_dock;
uint8_t hover_context;
uint8_t context_menu;
int context_x;
int context_y;
uint32_t last_clock_second;
const char *notice;
DesktopWindow windows[MaxWindows];
uint8_t window_count;
uint8_t active_window;
uint8_t drag_window;

constexpr const char *context_labels[ContextCount] = {
    "New Folder",
    "New Text Document",
    "Open Files",
    "Open Terminal",
    "Open Settings",
    "Refresh",
    "Change Background",
};

uint32_t rgb(uint32_t r, uint32_t g, uint32_t b) {
    return (r << 16) | (g << 8) | b;
}

uint32_t blend(uint32_t a, uint32_t b, uint32_t step, uint32_t max) {
    int ar = static_cast<int>((a >> 16) & 255), ag = static_cast<int>((a >> 8) & 255), ab = static_cast<int>(a & 255);
    int br = static_cast<int>((b >> 16) & 255), bg = static_cast<int>((b >> 8) & 255), bb = static_cast<int>(b & 255);
    return rgb(static_cast<uint32_t>(ar + (br - ar) * static_cast<int>(step) / static_cast<int>(max)),
               static_cast<uint32_t>(ag + (bg - ag) * static_cast<int>(step) / static_cast<int>(max)),
               static_cast<uint32_t>(ab + (bb - ab) * static_cast<int>(step) / static_cast<int>(max)));
}

void text_shadow(int x, int y, const char *text, uint32_t color) {
    gfx_text_bold(x + 1, y + 1, text, Black);
    gfx_text_bold(x, y, text, color);
}

void two_digits(uint32_t value, char *out) {
    out[0] = static_cast<char>('0' + (value / 10u) % 10u);
    out[1] = static_cast<char>('0' + value % 10u);
}

void append_text(char *out, uint16_t cap, const char *text) {
    size_t length = kstrlen(out);
    kstrcpy(out + length, text, cap > length ? cap - length : 0);
}

uint32_t clock_seconds() {
    int32_t seconds = static_cast<int32_t>(timer_ticks() / TIMER_HZ);
    if (rtc_ready()) {
        seconds += static_cast<int32_t>(rtc_seconds_of_day());
        seconds += static_cast<int32_t>(rtc_timezone_minutes()) * 60;
    }
    while (seconds < 0) seconds += 86400;
    return static_cast<uint32_t>(seconds) % 86400u;
}

void clock_text(char *out) {
    uint32_t seconds = clock_seconds();
    two_digits(seconds / 3600u, out);
    out[2] = ':';
    two_digits((seconds / 60u) % 60u, out + 3);
    out[5] = ':';
    two_digits(seconds % 60u, out + 6);
    out[8] = '\0';
}

void circle(int cx, int cy, int radius, uint32_t color) {
    for (int y = -radius; y <= radius; y++) {
        int yy = y < 0 ? -y : y;
        int width = radius - (yy * yy) / radius;
        gfx_rect(cx - width, cy + y, width * 2 + 1, 1, color);
    }
}

void mountain(int cx, int base_y, int width, int height, uint32_t color) {
    for (int y = 0; y < height; y++) {
        int half = width * y / height;
        gfx_rect(cx - half, base_y - height + y, half * 2 + 1, 1, color);
    }
}

void stars() {
    const int points[][2] = {
        {74, 42}, {146, 86}, {238, 48}, {322, 72}, {488, 46}, {614, 84},
        {708, 52}, {740, 118}, {578, 136}, {420, 112}, {188, 122},
    };
    for (uint8_t i = 0; i < sizeof(points) / sizeof(points[0]); i++) {
        gfx_rect(points[i][0], points[i][1], 2, 2, Accent);
    }
}

void wallpaper() {
    uint8_t wallpaper_style = sys_settings_get()->wallpaper;
    uint32_t top = 0x000B344D;
    uint32_t mid = 0x002E2E68;
    uint32_t glow = 0x00F2487A;
    uint32_t ground = 0x000B2742;
    if (wallpaper_style == 1) {
        top = 0x00122A2A;
        mid = 0x00304E56;
        glow = 0x00E2C15E;
        ground = 0x00091E23;
    } else if (wallpaper_style == 2) {
        top = 0x00121B34;
        mid = 0x00442D5E;
        glow = 0x0000D6D6;
        ground = 0x000B182C;
    }
    for (int y = 0; y < GFX_HEIGHT; y++) {
        uint32_t color;
        if (y < 250) color = blend(top, mid, static_cast<uint32_t>(y), 250);
        else if (y < 420) color = blend(mid, glow, static_cast<uint32_t>(y - 250), 170);
        else color = blend(glow, ground, static_cast<uint32_t>(y - 420), 180);
        gfx_rect(0, y, GFX_WIDTH, 1, color);
    }
    stars();
    circle(522, 350, 112, 0x00FF5B86);
    circle(520, 350, 64, 0x00FFB260);
    gfx_rect(0, 430, GFX_WIDTH, 170, 0x0010213A);
    mountain(118, 464, 180, 118, 0x00352B67);
    mountain(314, 472, 230, 148, 0x00563483);
    mountain(538, 478, 250, 160, 0x00402E73);
    mountain(694, 468, 170, 110, 0x006E2F77);
    gfx_rect(0, 505, GFX_WIDTH, 95, 0x00091B2B);
    gfx_rect(0, 514, GFX_WIDTH, 3, 0x0000A6B8);
}

void icon_art(int x, int y, uint8_t kind, uint8_t large) {
    int s = large ? 4 : 3;
    if (kind == 0) {
        gfx_rect(x, y + 6, 10 * s, 8 * s, 0x00F4E9B5);
        gfx_rect(x + 2 * s, y, 5 * s, 3 * s, 0x00F4E9B5);
        gfx_border(x, y + 6, 10 * s, 8 * s, PanelDark);
        gfx_rect(x, y + 6, 10 * s, 2 * s, Accent);
    } else if (kind == 1) {
        gfx_rect(x + 2 * s, y, 8 * s, 11 * s, White);
        gfx_border(x + 2 * s, y, 8 * s, 11 * s, PanelDark);
        gfx_rect(x + 4 * s, y + 3 * s, 4 * s, s, Accent);
        gfx_rect(x + 4 * s, y + 6 * s, 4 * s, s, Violet);
        gfx_rect(x + 4 * s, y + 9 * s, 3 * s, s, Pink);
    } else if (kind == 2) {
        gfx_rect(x, y + 2 * s, 11 * s, 8 * s, White);
        gfx_border(x, y + 2 * s, 11 * s, 8 * s, PanelDark);
        gfx_rect(x + s, y + 4 * s, 9 * s, s, Pink);
        gfx_rect(x + s, y + 6 * s, 9 * s, s, Accent);
        gfx_rect(x + s, y + 8 * s, 7 * s, s, Violet);
    } else if (kind == 3) {
        gfx_rect(x, y + 2 * s, 11 * s, 8 * s, PanelDark);
        gfx_border(x, y + 2 * s, 11 * s, 8 * s, White);
        gfx_text_bold(x + 2 * s, y + 5 * s, ">", Accent);
    } else {
        gfx_rect(x + 2 * s, y + 2 * s, 8 * s, 8 * s, White);
        gfx_border(x + 2 * s, y + 2 * s, 8 * s, 8 * s, PanelDark);
        gfx_rect(x + 5 * s, y, 2 * s, 3 * s, Accent);
        gfx_rect(x + 5 * s, y + 9 * s, 2 * s, 3 * s, Accent);
        gfx_rect(x, y + 5 * s, 3 * s, 2 * s, Pink);
        gfx_rect(x + 9 * s, y + 5 * s, 3 * s, 2 * s, Pink);
        gfx_rect(x + 5 * s, y + 5 * s, 2 * s, 2 * s, Violet);
    }
}

void desktop_icon(const Launcher &launcher, uint8_t selected) {
    int x = launcher.x;
    int y = launcher.y;
    if (selected) {
        gfx_rect(x - 10, y - 7, 96, 58, AccentSoft);
        gfx_border(x - 10, y - 7, 96, 58, Accent);
    }
    icon_art(x + 12, y, launcher.kind, 0);
    text_shadow(x, y + 42, launcher.label, White);
}

uint8_t launcher_at(int x, int y) {
    for (uint8_t i = 0; i < launcher_count; i++) {
        const Launcher &launcher = launchers[i];
        if (x >= launcher.x - 12 && x < launcher.x + 96 &&
            y >= launcher.y - 8 && y < launcher.y + 64) return i;
    }
    return NoHover;
}

uint8_t dock_at(int x, int y) {
    if (y < 546 || y >= 596) return NoHover;
    for (uint8_t i = 0; i < launcher_count; i++) {
        int ix = 256 + i * 58;
        if (x >= ix && x < ix + 48) return i;
    }
    return NoHover;
}

uint8_t menu_at(int x, int y) {
    if (!menu || x < 8 || x >= 226) return NoHover;
    if (y >= 54 && y < 86) return 0;
    if (y >= 88 && y < 120) return 1;
    if (y >= 122 && y < 154) return 2;
    if (y >= 156 && y < 188) return 3;
    if (y >= 190 && y < 222) return 4;
    if (y >= 246 && y < 278) return launcher_count;
    return NoHover;
}

uint8_t context_at(int x, int y) {
    if (!context_menu) return NoHover;
    if (x < context_x || x >= context_x + 220) return NoHover;
    if (y < context_y + 8 || y >= context_y + 8 + ContextCount * 28) return NoHover;
    return static_cast<uint8_t>((y - context_y - 8) / 28);
}

const char *window_title(uint8_t type) {
    return type < launcher_count ? launchers[type].status : "Window";
}

uint8_t window_contains(const DesktopWindow &window, int x, int y) {
    return x >= window.x && x < window.x + window.w && y >= window.y && y < window.y + window.h;
}

uint8_t window_close_hit(const DesktopWindow &window, int x, int y) {
    return x >= window.x + 10 && x < window.x + 25 && y >= window.y + 9 && y < window.y + 24;
}

uint8_t window_title_hit(const DesktopWindow &window, int x, int y) {
    return x >= window.x && x < window.x + window.w && y >= window.y && y < window.y + 30;
}

uint8_t window_at(int x, int y) {
    for (int i = static_cast<int>(window_count) - 1; i >= 0; i--) {
        if (window_contains(windows[i], x, y)) return static_cast<uint8_t>(i);
    }
    return NoWindow;
}

void terminal_clear(DesktopWindow &window) {
    for (uint8_t row = 0; row < TerminalRows; row++) window.output[row][0] = '\0';
}

void terminal_print(DesktopWindow &window, const char *text) {
    for (uint8_t row = 0; row < TerminalRows; row++) {
        if (window.output[row][0] == '\0') {
            kstrcpy(window.output[row], text, TerminalCols);
            return;
        }
    }
    for (uint8_t row = 1; row < TerminalRows; row++) kstrcpy(window.output[row - 1], window.output[row], TerminalCols);
    kstrcpy(window.output[TerminalRows - 1], text, TerminalCols);
}

void paint_clear(DesktopWindow &window) {
    for (uint8_t row = 0; row < PaintRows; row++) {
        for (uint8_t col = 0; col < PaintCols; col++) window.paint[row][col] = PaintEmpty;
    }
}

void window_defaults(DesktopWindow &window, uint8_t type) {
    int offset = static_cast<int>(window_count) * 22;
    window.type = type;
    window.x = 156 + offset;
    window.y = 86 + offset;
    window.drag_x = window.drag_y = 0;
    window.directory = app_get_workdir();
    window.file_index = -1;
    window.selected = 0;
    window.view_start = 0;
    window.rename_active = 0;
    window.line_len = 0;
    window.editor_len = 0;
    window.paint_color = 0;
    window.painting = 0;
    window.paint_last_col = 0;
    window.paint_last_row = 0;
    kstrcpy(window.title, type == 1 ? "untitled.txt" : window_title(type), sizeof(window.title));
    window.line[0] = '\0';
    window.editor[0] = '\0';
    terminal_clear(window);
    paint_clear(window);
    if (type == 0) { window.w = 540; window.h = 360; }
    else if (type == 1) { window.w = 438; window.h = 306; }
    else if (type == 2) { window.w = 404; window.h = 318; }
    else if (type == 3) {
        window.w = 452;
        window.h = 318;
        terminal_print(window, "Luma Terminal v1");
        terminal_print(window, "type help for commands");
    } else {
        window.w = 420;
        window.h = 310;
    }
    if (window.x + window.w > GFX_WIDTH - 18) window.x = 118;
    if (window.y + window.h > 540) window.y = 76;
}

void bring_to_front(uint8_t index) {
    if (index >= window_count) return;
    DesktopWindow selected = windows[index];
    for (uint8_t i = index; i + 1 < window_count; i++) windows[i] = windows[i + 1];
    windows[window_count - 1] = selected;
    active_window = static_cast<uint8_t>(window_count - 1);
}

void close_window(uint8_t index) {
    if (index >= window_count) return;
    for (uint8_t i = index; i + 1 < window_count; i++) windows[i] = windows[i + 1];
    window_count--;
    active_window = window_count ? static_cast<uint8_t>(window_count - 1) : NoWindow;
    drag_window = NoWindow;
    notice = window_count ? "Window closed" : "Ready";
}

uint8_t open_window(uint8_t type) {
    if (type >= launcher_count) return NoWindow;
    if (window_count >= MaxWindows) {
        notice = "Too many windows";
        return NoWindow;
    }
    window_defaults(windows[window_count], type);
    active_window = window_count++;
    notice = launchers[type].status;
    return active_window;
}

void open_editor_file(uint8_t node_index) {
    const fs_node_t *node = fs_node(node_index);
    if (!node || node->type != FS_FILE) {
        notice = "Cannot open file";
        return;
    }
    uint8_t index = open_window(1);
    if (index == NoWindow) return;
    DesktopWindow &editor = windows[index];
    editor.file_index = node_index;
    editor.directory = node->parent;
    editor.editor_len = node->size;
    kstrcpy(editor.title, node->name, sizeof(editor.title));
    kmemcpy(editor.editor, node->data, editor.editor_len);
    editor.editor[editor.editor_len] = '\0';
    notice = "File opened";
}

void launch(uint8_t index) {
    open_window(index);
}

void make_candidate(char *out, const char *base, const char *ext, uint8_t number) {
    char text[8];
    kstrcpy(out, base, FS_NAME_MAX + 1);
    if (number > 1) {
        append_text(out, FS_NAME_MAX + 1, " ");
        kitoa(number, text, 10);
        append_text(out, FS_NAME_MAX + 1, text);
    }
    append_text(out, FS_NAME_MAX + 1, ext);
}

void unique_name_in(char *out, const char *base, const char *ext, int directory) {
    for (uint8_t number = 1; number < 99; number++) {
        make_candidate(out, base, ext, number);
        if (fs_resolve(out, directory) < 0) return;
    }
    make_candidate(out, base, ext, 99);
}

void unique_name(char *out, const char *base, const char *ext) {
    unique_name_in(out, base, ext, app_get_workdir());
}

void create_desktop_item(uint8_t folder) {
    char name[FS_NAME_MAX + 1];
    int cwd = app_get_workdir();
    if (folder) {
        unique_name(name, "New Folder", "");
        notice = fs_create(name, FS_DIR, cwd) == FS_OK ? "Folder created" : "Cannot create folder";
    } else {
        unique_name(name, "New File", ".txt");
        notice = fs_write(name, "", cwd) == FS_OK ? "Text document created" : "Cannot create file";
    }
}

void top_panel() {
    char time[9];
    clock_text(time);
    gfx_rect(0, 0, GFX_WIDTH, 32, Panel);
    gfx_rect(0, 31, GFX_WIDTH, 1, AccentSoft);
    gfx_rect(6, 5, 70, 22, menu ? AccentSoft : PanelLight);
    gfx_border(6, 5, 70, 22, menu ? Accent : PanelDark);
    gfx_text_bold(18, 12, "Luma", White);
    for (uint8_t i = 0; i < 4; i++) {
        int x = 92 + i * 22;
        gfx_rect(x, 8, 16, 16, i == 0 ? AccentSoft : PanelLight);
        char number[2] = {static_cast<char>('1' + i), 0};
        gfx_text_bold(x + 5, 13, number, White);
    }
    gfx_text_bold(338, 12, "Luma Desktop", White);
    gfx_text_bold(730, 12, time, Accent);
}

const char *status_text() {
    if (context_menu && hover_context < ContextCount) return context_labels[hover_context];
    if (menu && hover_menu < launcher_count) return launchers[hover_menu].status;
    if (menu && hover_menu == launcher_count) return "Exit to Shell";
    if (hover_dock < launcher_count) return launchers[hover_dock].status;
    if (hover_launcher < launcher_count) return launchers[hover_launcher].status;
    return notice ? notice : "Ready";
}

void dock() {
    gfx_rect(226, 546, 348, 50, PanelDark);
    gfx_border(226, 546, 348, 50, PanelLight);
    for (uint8_t i = 0; i < launcher_count; i++) {
        int x = 256 + i * 58;
        uint8_t active = hover_dock == i;
        gfx_rect(x, 552, 48, 38, active ? PanelLight : Panel);
        gfx_border(x, 552, 48, 38, active ? Accent : PanelDark);
        icon_art(x + 8, 558, launchers[i].kind, 0);
    }
    gfx_rect(610, 566, 156, 20, PanelDark);
    gfx_border(610, 566, 156, 20, PanelLight);
    gfx_text_bold(620, 573, status_text(), White);
}

void menu_row(int y, const char *label, uint8_t active) {
    gfx_rect(14, y, 204, 30, active ? AccentSoft : PanelLight);
    gfx_border(14, y, 204, 30, active ? Accent : Panel);
    gfx_text_bold(50, y + 12, label, White);
}

void start_menu() {
    if (!menu) return;
    gfx_rect(6, 40, 224, 252, Panel);
    gfx_border(6, 40, 224, 252, AccentSoft);
    gfx_rect(6, 40, 224, 10, Accent);
    gfx_text_bold(18, 58, "Applications", White);
    menu_row(54, "Files", hover_menu == 0);
    menu_row(88, "Text Editor", hover_menu == 1);
    menu_row(122, "Paint", hover_menu == 2);
    menu_row(156, "Terminal", hover_menu == 3);
    menu_row(190, "Settings", hover_menu == 4);
    gfx_rect(14, 234, 204, 2, AccentSoft);
    menu_row(246, "Exit", hover_menu == launcher_count);
}

void context_row(uint8_t index) {
    int y = context_y + 8 + index * 28;
    uint8_t active = hover_context == index;
    gfx_rect(context_x + 8, y, 204, 26, active ? AccentSoft : Panel);
    gfx_border(context_x + 8, y, 204, 26, active ? Accent : PanelLight);
    gfx_text_bold(context_x + 18, y + 10, context_labels[index], White);
}

void context_menu_draw() {
    if (!context_menu) return;
    gfx_rect(context_x, context_y, 220, ContextHeight, PanelDark);
    gfx_border(context_x, context_y, 220, ContextHeight, AccentSoft);
    for (uint8_t i = 0; i < ContextCount; i++) context_row(i);
}

uint8_t context_action(uint8_t index) {
    if (index == 0) create_desktop_item(1);
    else if (index == 1) create_desktop_item(0);
    else if (index == 2) {
        open_window(0);
    } else if (index == 3) {
        open_window(3);
    } else if (index == 4) {
        open_window(4);
    } else if (index == 5) notice = "Desktop refreshed";
    else if (index == 6) {
        sys_settings_next_wallpaper();
        notice = "Wallpaper changed";
    }
    return 0;
}

void open_context_menu(int x, int y) {
    context_x = x;
    context_y = y;
    if (context_x > GFX_WIDTH - 226) context_x = GFX_WIDTH - 226;
    if (context_y > GFX_HEIGHT - ContextHeight - 6) context_y = GFX_HEIGHT - ContextHeight - 6;
    if (context_x < 4) context_x = 4;
    if (context_y < 36) context_y = 36;
    context_menu = 1;
    menu = 0;
    hover_context = context_at(x, y);
}

void window_frame(const DesktopWindow &window, uint8_t active) {
    uint32_t edge = active ? sys_settings_accent_color() : 0x00626D7C;
    gfx_rect(window.x + 8, window.y + 10, window.w, window.h, 0x00333A47);
    gfx_rect(window.x, window.y, window.w, window.h, 0x001A1C26);
    gfx_border(window.x, window.y, window.w, window.h, edge);
    gfx_rect(window.x + 1, window.y + 1, window.w - 2, 28, 0x003B3D47);
    circle(window.x + 16, window.y + 16, 6, 0x00FF5F57);
    circle(window.x + 34, window.y + 16, 6, 0x00FFBD2E);
    circle(window.x + 52, window.y + 16, 6, 0x0028C840);
    gfx_text_bold(window.x + window.w / 2 - 42, window.y + 11, window_title(window.type), 0x00D7DAE4);
}

uint8_t files_count(const DesktopWindow &window) {
    uint8_t count = 0;
    for (int index = 0; index < FS_MAX_NODES; index++) {
        const fs_node_t *node = fs_node(index);
        if (node && node->parent == window.directory) count++;
    }
    return count;
}

uint8_t file_child_by_index(const DesktopWindow &window, uint8_t wanted) {
    uint8_t row = 0;
    for (int index = 0; index < FS_MAX_NODES; index++) {
        const fs_node_t *node = fs_node(index);
        if (!node || node->parent != window.directory) continue;
        if (row == wanted) return static_cast<uint8_t>(index);
        row++;
    }
    return NoWindow;
}

void ensure_files_visible(DesktopWindow &window) {
    uint8_t count = files_count(window);
    if (!count) {
        window.selected = 0;
        window.view_start = 0;
        return;
    }
    if (window.selected >= count) window.selected = static_cast<uint8_t>(count - 1);
    if (window.selected < window.view_start) window.view_start = window.selected;
    if (window.selected >= window.view_start + FileVisibleRows) {
        window.view_start = static_cast<uint8_t>(window.selected - FileVisibleRows + 1);
    }
}

void draw_file_button(const DesktopWindow &window, uint8_t index) {
    const FileButton &button = FileButtons[index];
    int x = window.x + button.dx;
    int y = window.y + 42;
    gfx_rect(x, y, button.w, 24, 0x00242633);
    gfx_border(x, y, button.w, 24, 0x004A5060);
    gfx_text_bold(x + 8, y + 9, button.label, 0x00D7DAE4);
}

void draw_files_window(DesktopWindow &window) {
    char path[80];
    fs_path(window.directory, path, sizeof(path));
    ensure_files_visible(window);
    for (uint8_t i = 0; i < FileButtonCount; i++) draw_file_button(window, i);
    gfx_rect(window.x + 14, window.y + 74, window.w - 28, 24, 0x00242633);
    gfx_border(window.x + 14, window.y + 74, window.w - 28, 24, 0x004A5060);
    gfx_text_bold(window.x + 24, window.y + 83, path, Accent);
    gfx_rect(window.x + 14, window.y + 108, window.w - 28, 178, 0x001F2230);
    gfx_border(window.x + 14, window.y + 108, window.w - 28, 178, 0x004A5060);

    uint8_t count = files_count(window);
    uint8_t visible = count > window.view_start ? static_cast<uint8_t>(count - window.view_start) : 0;
    if (visible > FileVisibleRows) visible = FileVisibleRows;
    for (uint8_t row = 0; row < visible; row++) {
        uint8_t absolute = static_cast<uint8_t>(window.view_start + row);
        uint8_t child = file_child_by_index(window, absolute);
        const fs_node_t *node = fs_node(child);
        if (!node) continue;
        int y = window.y + 124 + row * 24;
        if (absolute == window.selected) gfx_rect(window.x + 22, y - 5, window.w - 44, 20, 0x00324D6B);
        gfx_text_bold(window.x + 28, y, node->type == FS_DIR ? "[DIR]" : "[FILE]", node->type == FS_DIR ? 0x0000D6D6 : 0x00FFFFFF);
        gfx_text_bold(window.x + 86, y, node->name, 0x00FFFFFF);
    }
    if (!count) gfx_text_bold(window.x + 204, window.y + 190, "Empty Folder", 0x008A94A8);
    gfx_rect(window.x + 14, window.y + window.h - 42, window.w - 28, 26, 0x00242633);
    gfx_border(window.x + 14, window.y + window.h - 42, window.w - 28, 26, 0x004A5060);
    if (window.rename_active) {
        gfx_text_bold(window.x + 24, window.y + window.h - 32, "Rename:", Accent);
        gfx_text_bold(window.x + 94, window.y + window.h - 32, window.line, White);
        gfx_text_bold(window.x + window.w - 148, window.y + window.h - 32, "Enter OK", 0x008A94A8);
    } else {
        gfx_text_bold(window.x + 24, window.y + window.h - 32, notice ? notice : "Ready", White);
        gfx_text_bold(window.x + window.w - 202, window.y + window.h - 32, "Enter Open  Del Delete", 0x008A94A8);
    }
}

void draw_editor_window(const DesktopWindow &window) {
    gfx_rect(window.x + 16, window.y + 48, window.w - 32, window.h - 70, 0x00F5F7FB);
    gfx_border(window.x + 16, window.y + 48, window.w - 32, window.h - 70, 0x00AEB8CA);
    gfx_text_bold(window.x + 30, window.y + 66, window.title, 0x00242A35);
    int text_x = window.x + 30;
    int x = text_x;
    int y = window.y + 96;
    for (uint16_t i = 0; i < window.editor_len; i++) {
        if (window.editor[i] == '\n' || x > window.x + window.w - 38) {
            x = text_x;
            y += 14;
            if (y > window.y + window.h - 48) break;
            if (window.editor[i] == '\n') continue;
        }
        char text[2] = {window.editor[i], 0};
        gfx_text(x, y, text, 0x00242A35);
        x += 6;
    }
    if (!window.editor_len) gfx_text(window.x + 30, window.y + 96, "Empty file", 0x008A94A8);
}

int paint_canvas_x(const DesktopWindow &window) { return window.x + 18; }
int paint_canvas_y(const DesktopWindow &window) { return window.y + 48; }
int paint_canvas_w(const DesktopWindow &window) { return window.w - 36; }
int paint_canvas_h(const DesktopWindow &window) { return window.h - 104; }
int paint_swatch_y(const DesktopWindow &window) { return window.y + window.h - 44; }

void draw_paint_window(const DesktopWindow &window) {
    int cx = paint_canvas_x(window);
    int cy = paint_canvas_y(window);
    int cw = paint_canvas_w(window);
    int ch = paint_canvas_h(window);
    gfx_rect(cx, cy, cw, ch, 0x00FFFFFF);
    for (uint8_t row = 0; row < PaintRows; row++) {
        for (uint8_t col = 0; col < PaintCols; col++) {
            uint8_t value = window.paint[row][col];
            if (value >= PaintColorCount) continue;
            int px = cx + (static_cast<int>(col) * cw) / PaintCols;
            int py = cy + (static_cast<int>(row) * ch) / PaintRows;
            int px2 = cx + (static_cast<int>(col + 1) * cw) / PaintCols;
            int py2 = cy + (static_cast<int>(row + 1) * ch) / PaintRows;
            gfx_rect(px, py, px2 > px ? px2 - px : 1, py2 > py ? py2 - py : 1, PaintPalette[value]);
        }
    }
    gfx_border(cx, cy, cw, ch, 0x00AEB8CA);
    gfx_rect(window.x + 18, paint_swatch_y(window) - 8, window.w - 36, 42, 0x00242633);
    gfx_border(window.x + 18, paint_swatch_y(window) - 8, window.w - 36, 42, 0x004A5060);
    for (uint8_t i = 0; i < PaintColorCount; i++) {
        int sx = window.x + 36 + static_cast<int>(i) * 42;
        gfx_rect(sx, paint_swatch_y(window), 28, 18, PaintPalette[i]);
        gfx_border(sx - 3, paint_swatch_y(window) - 3, 34, 24, i == window.paint_color ? sys_settings_accent_color() : 0x00626D7C);
    }
    gfx_text_bold(window.x + 270, window.y + window.h - 38, "C Clear", 0x00D7DAE4);
}

void draw_terminal_window(DesktopWindow &window) {
    gfx_rect(window.x + 10, window.y + 34, window.w - 20, window.h - 44, 0x00161823);
    gfx_border(window.x + 10, window.y + 34, window.w - 20, window.h - 44, 0x003A4152);
    for (uint8_t row = 0; row < TerminalRows; row++) {
        gfx_text_bold(window.x + 20, window.y + 48 + row * 18, window.output[row], row ? 0x00FFFFFF : Accent);
    }
    gfx_text_bold(window.x + 20, window.y + window.h - 30, ">", Accent);
    gfx_text_bold(window.x + 34, window.y + window.h - 30, window.line, 0x00FFFFFF);
}

void draw_settings_window(const DesktopWindow &window) {
    const sys_settings_t *settings = sys_settings_get();
    uint32_t accent = sys_settings_accent_color();
    gfx_rect(window.x + 18, window.y + 48, 120, window.h - 66, 0x00242633);
    gfx_border(window.x + 18, window.y + 48, 120, window.h - 66, 0x004A5060);
    gfx_text_bold(window.x + 34, window.y + 70, "Appearance", accent);
    gfx_text_bold(window.x + 156, window.y + 62, "Wallpaper", 0x00FFFFFF);
    gfx_text_bold(window.x + 156, window.y + 86, settings->wallpaper == 0 ? "[Sky]" : " Sky ", settings->wallpaper == 0 ? accent : 0x00D7DAE4);
    gfx_text_bold(window.x + 226, window.y + 86, settings->wallpaper == 1 ? "[Mint]" : " Mint ", settings->wallpaper == 1 ? accent : 0x00D7DAE4);
    gfx_text_bold(window.x + 306, window.y + 86, settings->wallpaper == 2 ? "[Dusk]" : " Dusk ", settings->wallpaper == 2 ? accent : 0x00D7DAE4);
    gfx_text_bold(window.x + 156, window.y + 132, "Accent", 0x00FFFFFF);
    gfx_rect(window.x + 156, window.y + 158, 32, 22, 0x000078FF);
    gfx_rect(window.x + 204, window.y + 158, 32, 22, 0x0000D6D6);
    gfx_rect(window.x + 252, window.y + 158, 32, 22, 0x00F2487A);
    gfx_rect(window.x + 300, window.y + 158, 32, 22, 0x0000A86B);
    gfx_border(window.x + 153 + settings->accent * 48, window.y + 155, 38, 28, 0x00FFFFFF);
}

void draw_window(DesktopWindow &window, uint8_t active) {
    window_frame(window, active);
    if (window.type == 0) draw_files_window(window);
    else if (window.type == 1) draw_editor_window(window);
    else if (window.type == 2) draw_paint_window(window);
    else if (window.type == 3) draw_terminal_window(window);
    else draw_settings_window(window);
}

void draw_windows() {
    for (uint8_t i = 0; i < window_count; i++) draw_window(windows[i], i == active_window);
}

void draw() {
    gfx_begin_frame();
    wallpaper();
    top_panel();
    for (uint8_t i = 0; i < launcher_count; i++) desktop_icon(launchers[i], hover_launcher == i);
    draw_windows();
    start_menu();
    dock();
    context_menu_draw();
    gfx_present();
    gfx_cursor(mouse_x(), mouse_y());
}

void clamp_window(DesktopWindow &window) {
    if (window.x < 6) window.x = 6;
    if (window.y < 34) window.y = 34;
    if (window.x + window.w > GFX_WIDTH - 6) window.x = GFX_WIDTH - 6 - window.w;
    if (window.y + window.h > 540) window.y = 540 - window.h;
}

void start_drag(uint8_t index, int x, int y) {
    bring_to_front(index);
    drag_window = active_window;
    windows[drag_window].drag_x = x - windows[drag_window].x;
    windows[drag_window].drag_y = y - windows[drag_window].y;
}

void update_drag(int x, int y) {
    if (drag_window >= window_count) return;
    windows[drag_window].x = x - windows[drag_window].drag_x;
    windows[drag_window].y = y - windows[drag_window].drag_y;
    clamp_window(windows[drag_window]);
}

uint8_t file_child_at(DesktopWindow &window, uint8_t wanted_row) {
    if (wanted_row >= FileVisibleRows) return NoWindow;
    return file_child_by_index(window, static_cast<uint8_t>(window.view_start + wanted_row));
}

uint8_t files_selected_child(DesktopWindow &window) {
    ensure_files_visible(window);
    if (!files_count(window)) return NoWindow;
    return file_child_by_index(window, window.selected);
}

void files_select_name(DesktopWindow &window, const char *name) {
    uint8_t count = files_count(window);
    for (uint8_t index = 0; index < count; index++) {
        uint8_t child = file_child_by_index(window, index);
        const fs_node_t *node = fs_node(child);
        if (node && kstrcmp(node->name, name) == 0) {
            window.selected = index;
            ensure_files_visible(window);
            return;
        }
    }
}

void files_create_item(DesktopWindow &window, uint8_t folder) {
    char name[FS_NAME_MAX + 1];
    unique_name_in(name, folder ? "New Folder" : "New File", folder ? "" : ".txt", window.directory);
    fs_result_t result = folder ? fs_create(name, FS_DIR, window.directory) : fs_write(name, "", window.directory);
    if (result == FS_OK) {
        files_select_name(window, name);
        notice = folder ? "Folder created" : "Text file created";
    } else notice = folder ? "Cannot create folder" : "Cannot create file";
}

void files_go_back(DesktopWindow &window) {
    const fs_node_t *node = fs_node(window.directory);
    if (node && node->parent >= 0) {
        window.directory = node->parent;
        window.selected = 0;
        window.view_start = 0;
        window.rename_active = 0;
        app_set_workdir(window.directory);
        notice = "Parent folder";
    } else notice = "Already at root";
}

void files_open_selected(DesktopWindow &window) {
    uint8_t child = files_selected_child(window);
    if (child == NoWindow) return;
    const fs_node_t *node = fs_node(child);
    if (!node) return;
    if (node->type == FS_DIR) {
        window.directory = child;
        window.selected = 0;
        window.view_start = 0;
        window.rename_active = 0;
        app_set_workdir(window.directory);
        notice = "Folder opened";
    } else open_editor_file(child);
}

void files_delete_selected(DesktopWindow &window) {
    uint8_t child = files_selected_child(window);
    if (child == NoWindow) {
        notice = "Nothing selected";
        return;
    }
    const fs_node_t *node = fs_node(child);
    if (!node) return;
    if (fs_remove(node->name, window.directory, node->type == FS_DIR) == FS_OK) {
        window.rename_active = 0;
        ensure_files_visible(window);
        notice = "Deleted";
    } else notice = "Delete failed";
}

void files_begin_rename(DesktopWindow &window) {
    uint8_t child = files_selected_child(window);
    const fs_node_t *node = fs_node(child);
    if (!node) {
        notice = "Nothing selected";
        return;
    }
    kstrcpy(window.line, node->name, sizeof(window.line));
    window.line_len = static_cast<uint8_t>(kstrlen(window.line));
    window.rename_active = 1;
    notice = "Renaming";
}

void files_commit_rename(DesktopWindow &window) {
    uint8_t child = files_selected_child(window);
    const fs_node_t *node = fs_node(child);
    char new_name[FS_NAME_MAX + 1];
    if (!node || !window.line_len) {
        window.rename_active = 0;
        notice = "Rename cancelled";
        return;
    }
    kstrcpy(new_name, window.line, sizeof(new_name));
    if (fs_move(node->name, new_name, window.directory) == FS_OK) {
        window.rename_active = 0;
        files_select_name(window, new_name);
        notice = "Renamed";
    } else {
        window.rename_active = 0;
        notice = "Rename failed";
    }
}

uint8_t files_button_at(const DesktopWindow &window, int x, int y) {
    if (y < window.y + 42 || y >= window.y + 66) return NoHover;
    for (uint8_t i = 0; i < FileButtonCount; i++) {
        const FileButton &button = FileButtons[i];
        int bx = window.x + button.dx;
        if (x >= bx && x < bx + button.w) return i;
    }
    return NoHover;
}

void files_run_button(DesktopWindow &window, uint8_t button) {
    if (button == 0) files_go_back(window);
    else if (button == 1) files_open_selected(window);
    else if (button == 2) files_create_item(window, 1);
    else if (button == 3) files_create_item(window, 0);
    else if (button == 4) files_begin_rename(window);
    else if (button == 5) files_delete_selected(window);
}

void handle_files_click(DesktopWindow &window, int x, int y) {
    uint8_t button = files_button_at(window, x, y);
    if (button != NoHover) {
        files_run_button(window, button);
        return;
    }
    if (x < window.x + 14 || x >= window.x + window.w - 14) return;
    if (y < window.y + 114 || y >= window.y + 114 + FileVisibleRows * 24) return;
    uint8_t row = static_cast<uint8_t>((y - (window.y + 114)) / 24);
    uint8_t child = file_child_at(window, row);
    if (child == NoWindow) return;
    window.selected = static_cast<uint8_t>(window.view_start + row);
    ensure_files_visible(window);
    notice = "Selected";
}

void handle_settings_click(const DesktopWindow &window, int x, int y) {
    if (y >= window.y + 78 && y < window.y + 106) {
        uint8_t changed = 1;
        if (x >= window.x + 150 && x < window.x + 210) sys_settings_set_wallpaper(0);
        else if (x >= window.x + 220 && x < window.x + 292) sys_settings_set_wallpaper(1);
        else if (x >= window.x + 300 && x < window.x + 374) sys_settings_set_wallpaper(2);
        else changed = 0;
        if (changed) notice = "Wallpaper changed";
    } else if (y >= window.y + 152 && y < window.y + 186) {
        uint8_t changed = 1;
        if (x >= window.x + 150 && x < window.x + 194) sys_settings_set_accent(0);
        else if (x >= window.x + 198 && x < window.x + 242) sys_settings_set_accent(1);
        else if (x >= window.x + 246 && x < window.x + 290) sys_settings_set_accent(2);
        else if (x >= window.x + 294 && x < window.x + 338) sys_settings_set_accent(3);
        else changed = 0;
        if (changed) notice = "Accent changed";
    }
}

uint8_t paint_cell_at(const DesktopWindow &window, int x, int y, int *col, int *row) {
    int cx = paint_canvas_x(window);
    int cy = paint_canvas_y(window);
    int cw = paint_canvas_w(window);
    int ch = paint_canvas_h(window);
    if (x < cx || x >= cx + cw || y < cy || y >= cy + ch) return 0;
    *col = ((x - cx) * PaintCols) / cw;
    *row = ((y - cy) * PaintRows) / ch;
    return 1;
}

void paint_dot(DesktopWindow &window, int col, int row) {
    for (int yy = -1; yy <= 1; yy++) {
        for (int xx = -1; xx <= 1; xx++) {
            int target_col = col + xx;
            int target_row = row + yy;
            if (target_col < 0 || target_col >= PaintCols || target_row < 0 || target_row >= PaintRows) continue;
            window.paint[target_row][target_col] = window.paint_color;
        }
    }
}

void paint_stroke(DesktopWindow &window, int col0, int row0, int col1, int row1) {
    int dx = col1 - col0;
    int dy = row1 - row0;
    int steps = dx < 0 ? -dx : dx;
    int ady = dy < 0 ? -dy : dy;
    if (ady > steps) steps = ady;
    for (int i = 0; i <= steps; i++) {
        int col = col0 + (dx * i) / (steps ? steps : 1);
        int row = row0 + (dy * i) / (steps ? steps : 1);
        paint_dot(window, col, row);
    }
}

void handle_paint_motion(DesktopWindow &window, int x, int y) {
    int col = 0;
    int row = 0;
    if (!paint_cell_at(window, x, y, &col, &row)) {
        window.painting = 0;
        return;
    }
    if (!window.painting) {
        window.painting = 1;
        window.paint_last_col = col;
        window.paint_last_row = row;
    }
    paint_stroke(window, window.paint_last_col, window.paint_last_row, col, row);
    window.paint_last_col = col;
    window.paint_last_row = row;
    notice = "Painting";
}

void handle_paint_click(DesktopWindow &window, int x, int y) {
    int sy = paint_swatch_y(window);
    if (y >= sy - 8 && y < sy + 34) {
        for (uint8_t i = 0; i < PaintColorCount; i++) {
            int sx = window.x + 36 + static_cast<int>(i) * 42;
            if (x >= sx - 3 && x < sx + 31) {
                window.paint_color = i;
                window.painting = 0;
                notice = "Color selected";
                return;
            }
        }
        if (x >= window.x + 260 && x < window.x + 344) {
            paint_clear(window);
            window.painting = 0;
            notice = "Canvas cleared";
        }
        return;
    }
    handle_paint_motion(window, x, y);
}

void handle_window_click(uint8_t index, int x, int y) {
    bring_to_front(index);
    DesktopWindow &window = windows[active_window];
    if (window_close_hit(window, x, y)) {
        close_window(active_window);
        return;
    }
    if (window_title_hit(window, x, y)) {
        start_drag(active_window, x, y);
        return;
    }
    if (window.type == 0) handle_files_click(window, x, y);
    else if (window.type == 2) handle_paint_click(window, x, y);
    else if (window.type == 4) handle_settings_click(window, x, y);
}

void execute_terminal(DesktopWindow &window, const char *command) {
    if (!command || !*command) return;
    if (kstrcmp(command, "help") == 0) terminal_print(window, "help clear files editor paint terminal settings exit");
    else if (kstrcmp(command, "clear") == 0) terminal_clear(window);
    else if (kstrcmp(command, "files") == 0 || kstrcmp(command, "ls") == 0) open_window(0);
    else if (kstrcmp(command, "editor") == 0 || kstrcmp(command, "edit") == 0) open_window(1);
    else if (kstrcmp(command, "paint") == 0) open_window(2);
    else if (kstrcmp(command, "terminal") == 0) open_window(3);
    else if (kstrcmp(command, "settings") == 0) open_window(4);
    else if (kstrcmp(command, "exit") == 0) close_window(active_window);
    else terminal_print(window, "unknown command");
}

void handle_files_key(DesktopWindow &window, uint16_t key) {
    if (window.rename_active) {
        if (key == '\n') files_commit_rename(window);
        else if (key == '\b') {
            if (window.line_len) window.line[--window.line_len] = '\0';
        } else if (key >= 32 && key < 127 && key != '/' && window.line_len < FS_NAME_MAX) {
            window.line[window.line_len++] = static_cast<char>(key);
            window.line[window.line_len] = '\0';
        }
        return;
    }

    uint8_t count = files_count(window);
    if (key == '\b') files_go_back(window);
    else if (key == KEY_UP && count && window.selected) window.selected--;
    else if (key == KEY_DOWN && count && window.selected + 1 < count) window.selected++;
    else if (key == '\n') files_open_selected(window);
    else if (key == KEY_DELETE) files_delete_selected(window);
    else if (key == 'r' || key == 'R') files_begin_rename(window);
    else if (key == 'f' || key == 'F') files_create_item(window, 1);
    else if (key == 'n' || key == 'N') files_create_item(window, 0);
    ensure_files_visible(window);
}

void handle_window_key(uint16_t key) {
    if (active_window >= window_count) return;
    DesktopWindow &window = windows[active_window];
    if (window.type == 0) {
        handle_files_key(window, key);
    } else if (key == '\b' && window.type == 3 && window.line_len) {
        window.line[--window.line_len] = '\0';
    } else if (key == '\n' && window.type == 3) {
        char command[48];
        kstrcpy(command, window.line, sizeof(command));
        terminal_print(window, window.line);
        window.line_len = 0;
        window.line[0] = '\0';
        execute_terminal(window, command);
    } else if (window.type == 3 && key >= 32 && key < 127 && window.line_len < sizeof(window.line) - 1) {
        window.line[window.line_len++] = static_cast<char>(key);
        window.line[window.line_len] = '\0';
    } else if (window.type == 4 && key >= '1' && key <= '3') {
        sys_settings_set_wallpaper(static_cast<uint8_t>(key - '1'));
    } else if (window.type == 4 && key >= '4' && key <= '7') {
        sys_settings_set_accent(static_cast<uint8_t>(key - '4'));
    } else if (window.type == 2 && (key == 'c' || key == 'C')) {
        paint_clear(window);
        window.painting = 0;
        notice = "Canvas cleared";
    }
    draw();
}

}

extern "C" void app_luma_start(char **args, uint8_t count) {
    (void)args;
    (void)count;
    gfx_init();
    menu = last_left = last_right = context_menu = 0;
    hover_launcher = hover_menu = hover_dock = hover_context = NoHover;
    window_count = 0;
    active_window = drag_window = NoWindow;
    last_clock_second = 0xFFFFFFFFu;
    notice = "Ready";
    draw();
}

extern "C" void app_luma_tick(uint32_t ticks) {
    (void)ticks;
    int x = mouse_x();
    int y = mouse_y();
    uint8_t click = mouse_left();
    uint8_t right = mouse_right();
    if (drag_window != NoWindow) {
        if (click) {
            update_drag(x, y);
            draw();
            last_left = click;
            last_right = right;
            return;
        }
        drag_window = NoWindow;
    }
    if (active_window != NoWindow && active_window < window_count && windows[active_window].type == 2) {
        if (click && windows[active_window].painting) {
            handle_paint_motion(windows[active_window], x, y);
            draw();
            last_left = click;
            last_right = right;
            return;
        }
        if (!click) windows[active_window].painting = 0;
    }
    uint8_t over_window = (context_menu || menu) ? NoWindow : window_at(x, y);
    uint8_t next_launcher = (context_menu || over_window != NoWindow) ? NoHover : launcher_at(x, y);
    uint8_t next_dock = (context_menu || over_window != NoWindow) ? NoHover : dock_at(x, y);
    uint8_t next_menu = context_menu ? NoHover : menu_at(x, y);
    uint8_t next_context = context_at(x, y);
    uint32_t second = clock_seconds();
    uint8_t redraw = 0;

    if (next_launcher != hover_launcher || next_dock != hover_dock ||
        next_menu != hover_menu || next_context != hover_context || second != last_clock_second) {
        hover_launcher = next_launcher;
        hover_dock = next_dock;
        hover_menu = next_menu;
        hover_context = next_context;
        last_clock_second = second;
        redraw = 1;
    }

    if (right && !last_right && over_window == NoWindow) {
        open_context_menu(x, y);
        redraw = 1;
    }

    if (click && !last_left) {
        if (context_menu) {
            if (hover_context != NoHover) {
                uint8_t launched = context_action(hover_context);
                context_menu = 0;
                last_left = click;
                last_right = right;
                if (launched) return;
                draw();
                return;
            }
            context_menu = 0;
            redraw = 1;
        } else if (y < 32 && x < 82) {
            menu ^= 1;
            hover_menu = menu_at(x, y);
            redraw = 1;
        } else if (menu && hover_menu != NoHover) {
            if (hover_menu == launcher_count) {
                app_exit_gui();
                last_left = click;
                last_right = right;
                return;
            }
            launch(hover_menu);
            menu = 0;
            last_left = click;
            last_right = right;
            draw();
            return;
        } else if (hover_dock != NoHover) {
            launch(hover_dock);
            last_left = click;
            last_right = right;
            draw();
            return;
        } else if (over_window != NoWindow) {
            handle_window_click(over_window, x, y);
            last_left = click;
            last_right = right;
            draw();
            return;
        } else if (hover_launcher != NoHover) {
            launch(hover_launcher);
            last_left = click;
            last_right = right;
            draw();
            return;
        } else if (menu) {
            menu = 0;
            redraw = 1;
        }
    }

    if (redraw) draw();
    else gfx_cursor(x, y);
    last_left = click;
    last_right = right;
}

extern "C" void app_luma_key(uint16_t key) {
    if (active_window != NoWindow && active_window < window_count) {
        handle_window_key(key);
        return;
    }
    if (key == 's' || key == 'S' || key == '\n' || key == ' ') {
        menu ^= 1;
        draw();
    } else if (key >= '1' && key <= '5') {
        launch(static_cast<uint8_t>(key - '1'));
    } else if (key == 'q' || key == 'Q') app_exit_gui();
}
