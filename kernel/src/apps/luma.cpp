extern "C" {
#include "app.h"
#include "../gfx.h"
#include "../keyboard.h"
#include "../mouse.h"
#include "../rtc.h"
#include "../timer.h"
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
};

constexpr uint8_t launcher_count = static_cast<uint8_t>(sizeof(launchers) / sizeof(launchers[0]));

uint8_t menu;
uint8_t last_left;
uint8_t hover_launcher;
uint8_t hover_menu;
uint8_t hover_dock;
uint32_t last_clock_second;

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
    for (int y = 0; y < GFX_HEIGHT; y++) {
        uint32_t color;
        if (y < 250) color = blend(0x000B344D, 0x002E2E68, static_cast<uint32_t>(y), 250);
        else if (y < 420) color = blend(0x002E2E68, 0x00F2487A, static_cast<uint32_t>(y - 250), 170);
        else color = blend(0x00B82163, 0x000B2742, static_cast<uint32_t>(y - 420), 180);
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
    } else {
        gfx_rect(x, y + 2 * s, 11 * s, 8 * s, PanelDark);
        gfx_border(x, y + 2 * s, 11 * s, 8 * s, White);
        gfx_text_bold(x + 2 * s, y + 5 * s, ">", Accent);
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
        int ix = 286 + i * 58;
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
    if (y >= 212 && y < 244) return 4;
    return NoHover;
}

void launch(uint8_t index) {
    if (index >= launcher_count) return;
    app_run(launchers[index].app, 0, 0);
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
    if (menu && hover_menu < launcher_count) return launchers[hover_menu].status;
    if (menu && hover_menu == 4) return "Exit to Shell";
    if (hover_dock < launcher_count) return launchers[hover_dock].status;
    if (hover_launcher < launcher_count) return launchers[hover_launcher].status;
    return "Ready";
}

void dock() {
    gfx_rect(246, 546, 308, 50, PanelDark);
    gfx_border(246, 546, 308, 50, PanelLight);
    for (uint8_t i = 0; i < launcher_count; i++) {
        int x = 286 + i * 58;
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
    gfx_rect(6, 40, 224, 216, Panel);
    gfx_border(6, 40, 224, 216, AccentSoft);
    gfx_rect(6, 40, 224, 10, Accent);
    gfx_text_bold(18, 58, "Applications", White);
    menu_row(54, "Files", hover_menu == 0);
    menu_row(88, "Text Editor", hover_menu == 1);
    menu_row(122, "Paint", hover_menu == 2);
    menu_row(156, "Terminal", hover_menu == 3);
    gfx_rect(14, 200, 204, 2, AccentSoft);
    menu_row(212, "Exit", hover_menu == 4);
}

void draw() {
    gfx_cursor_hide();
    wallpaper();
    top_panel();
    for (uint8_t i = 0; i < launcher_count; i++) desktop_icon(launchers[i], hover_launcher == i);
    start_menu();
    dock();
    gfx_cursor(mouse_x(), mouse_y());
}

}

extern "C" void app_luma_start(char **args, uint8_t count) {
    (void)args;
    (void)count;
    gfx_init();
    menu = last_left = 0;
    hover_launcher = hover_menu = hover_dock = NoHover;
    last_clock_second = 0xFFFFFFFFu;
    draw();
}

extern "C" void app_luma_tick(uint32_t ticks) {
    (void)ticks;
    int x = mouse_x();
    int y = mouse_y();
    uint8_t next_launcher = launcher_at(x, y);
    uint8_t next_dock = dock_at(x, y);
    uint8_t next_menu = menu_at(x, y);
    uint32_t second = clock_seconds();
    uint8_t redraw = 0;
    uint8_t click = mouse_left();

    if (next_launcher != hover_launcher || next_dock != hover_dock ||
        next_menu != hover_menu || second != last_clock_second) {
        hover_launcher = next_launcher;
        hover_dock = next_dock;
        hover_menu = next_menu;
        last_clock_second = second;
        redraw = 1;
    }

    if (click && !last_left) {
        if (y < 32 && x < 82) {
            menu ^= 1;
            hover_menu = menu_at(x, y);
            redraw = 1;
        } else if (menu && hover_menu != NoHover) {
            if (hover_menu == 4) {
                app_exit_gui();
                last_left = click;
                return;
            }
            launch(hover_menu);
            last_left = click;
            return;
        } else if (hover_dock != NoHover) {
            launch(hover_dock);
            last_left = click;
            return;
        } else if (hover_launcher != NoHover) {
            launch(hover_launcher);
            last_left = click;
            return;
        } else if (menu) {
            menu = 0;
            redraw = 1;
        }
    }

    if (redraw) draw();
    else gfx_cursor(x, y);
    last_left = click;
}

extern "C" void app_luma_key(uint16_t key) {
    if (key == 's' || key == 'S' || key == '\n' || key == ' ') {
        menu ^= 1;
        draw();
    } else if (key >= '1' && key <= '4') {
        launch(static_cast<uint8_t>(key - '1'));
    } else if (key == 'q' || key == 'Q') app_exit_gui();
}
