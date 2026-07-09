#include <stdint.h>
#include "apic.h"
#include "console.h"
#include "cpu.h"
#include "fs.h"
#include "io.h"
#include "irq.h"
#include "keyboard.h"
#include "libk.h"
#include "panic.h"
#include "rtc.h"
#include "shell.h"
#include "timer.h"
#include "vga.h"
#include "apps/app.h"
#include "net.h"
#include "proc.h"
#include "users.h"

#define LINE_MAX 160
#define HISTORY_MAX 16

static char line[LINE_MAX + 1];
static uint16_t line_length, line_cursor;
static char history[HISTORY_MAX][LINE_MAX + 1];
static uint8_t history_count, history_view;
static int current_dir;
static size_t input_row, input_col;
static uint8_t game_active, secret, games_played;
static uint8_t fetch_active, fetch_save;
static char fetch_save_path[80];

static void print_number(uint32_t value) { char text[16]; kitoa(value, text, 10); vga_write(text, VGA_COLOR_WHITE, VGA_COLOR_BLACK); }

static uint8_t starts_with_utc(const char *text) {
    return text && (text[0] == 'U' || text[0] == 'u') && (text[1] == 'T' || text[1] == 't') && (text[2] == 'C' || text[2] == 'c');
}

static uint8_t parse_timezone(const char *text, int16_t *minutes) {
    const char *cursor = text;
    int16_t sign = 1;
    uint16_t hours = 0, mins = 0;
    uint8_t digits = 0;
    if (!cursor || !*cursor) return 0;
    if (starts_with_utc(cursor)) cursor += 3;
    if (*cursor == '+') cursor++;
    else if (*cursor == '-') { sign = -1; cursor++; }
    while (*cursor >= '0' && *cursor <= '9') {
        hours = (uint16_t)(hours * 10 + (uint16_t)(*cursor - '0'));
        cursor++;
        digits++;
        if (hours > 14) return 0;
    }
    if (!digits) return 0;
    if (*cursor == ':') {
        cursor++;
        digits = 0;
        while (*cursor >= '0' && *cursor <= '9') {
            mins = (uint16_t)(mins * 10 + (uint16_t)(*cursor - '0'));
            cursor++;
            digits++;
            if (mins > 59 || digits > 2) return 0;
        }
        if (!digits) return 0;
    }
    if (*cursor) return 0;
    int16_t total = (int16_t)(sign * (int16_t)(hours * 60u + mins));
    if (total < -720 || total > 840) return 0;
    *minutes = total;
    return 1;
}

static void print_hex(uint32_t value) {
    char text[2];
    vga_write("0x", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    for (int shift=28;shift>=0;shift-=4) {
        uint8_t digit = (uint8_t)((value >> shift) & 0x0F);
        text[0] = (char)(digit < 10 ? '0' + digit : 'A' + digit - 10);
        text[1] = '\0';
        vga_write(text, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    }
}

static void print_result(fs_result_t result) {
    if (result == FS_NOT_FOUND) vga_write("not found\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    else if (result == FS_EXISTS) vga_write("already exists\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    else if (result == FS_FULL) vga_write("RAM filesystem is full\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    else if (result == FS_NOT_DIR) vga_write("not a directory\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    else if (result == FS_NOT_EMPTY) vga_write("directory is not empty\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    else if (result == FS_TOO_LARGE) vga_write("file is too large (1 KiB max)\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    else vga_write("invalid path or command\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
}

static void redraw_line(void) {
    size_t visible = input_col < VGA_WIDTH ? VGA_WIDTH - input_col : 0;
    uint16_t start = 0;
    if (visible && line_cursor >= visible) start = (uint16_t)(line_cursor - visible + 1);
    for (size_t i = 0; i < visible; i++) vga_put_at(input_row, input_col + i, ' ', VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    for (size_t i = 0; i < visible && start + i < line_length; i++) vga_put_at(input_row, input_col + i, line[start + i], VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_set_position(input_row, input_col + (line_cursor >= start ? line_cursor - start : 0));
}

static void prompt(void) {
    char path[80];
    fs_path(current_dir, path, sizeof(path));
    vga_write(users_current_name(), VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write("@minios:", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    size_t path_length = kstrlen(path);
    if (path_length > 24) {
        vga_write("...", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_write(path + path_length - 21, VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    } else vga_write(path, VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write(" > ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    input_row = vga_get_row(); input_col = vga_get_col();
    line_length = line_cursor = 0; line[0] = '\0'; history_view = history_count;
    redraw_line();
}

static uint8_t parse_args(char *text, char **args, uint8_t maximum) {
    uint8_t count = 0;
    while (*text && count < maximum) {
        while (*text == ' ') text++;
        if (!*text) break;
        args[count++] = text;
        if (*text == '"') {
            args[count - 1] = ++text;
            while (*text && *text != '"') text++;
        } else while (*text && *text != ' ') text++;
        if (*text) *text++ = '\0';
    }
    return count;
}

static void add_history(const char *command) {
    if (!*command) return;
    if (history_count < HISTORY_MAX) { kstrcpy(history[history_count++], command, LINE_MAX + 1); return; }
    for (uint8_t i=1;i<HISTORY_MAX;i++) kstrcpy(history[i-1], history[i], LINE_MAX + 1);
    kstrcpy(history[HISTORY_MAX-1], command, LINE_MAX + 1);
}

static void print_feature(const char *name, uint8_t enabled) {
    vga_write(name, enabled ? VGA_COLOR_LIGHT_GREEN : VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    vga_write(enabled ? " " : "- ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}

static void command_debug_cpu(void) {
    const cpu_info_t *info = cpu_info();
    vga_write("CPU: ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_write(info->vendor, VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write(" family ", VGA_COLOR_WHITE, VGA_COLOR_BLACK); print_number(info->family);
    vga_write(" model ", VGA_COLOR_WHITE, VGA_COLOR_BLACK); print_number(info->model);
    vga_write(" stepping ", VGA_COLOR_WHITE, VGA_COLOR_BLACK); print_number(info->stepping);
    vga_write("\nFeatures: ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    print_feature("FPU", cpu_has(CPU_FEATURE_FPU));
    print_feature("TSC", cpu_has(CPU_FEATURE_TSC));
    print_feature("MSR", cpu_has(CPU_FEATURE_MSR));
    print_feature("PAE", cpu_has(CPU_FEATURE_PAE));
    print_feature("NX", cpu_has(CPU_FEATURE_NX));
    print_feature("APIC", cpu_has(CPU_FEATURE_APIC));
    print_feature("SEP", cpu_has(CPU_FEATURE_SEP));
    print_feature("SSE", cpu_has(CPU_FEATURE_SSE));
    print_feature("FXSR", cpu_has(CPU_FEATURE_FXSR));
    vga_write("\nFeature mask: ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    print_hex(info->features);
    vga_write("\nAPIC runtime: ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_write(apic_available() ? "available\n" : "not available\n", apic_available() ? VGA_COLOR_LIGHT_GREEN : VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
}

static void command_debug_irq(void) {
    vga_write("IRQ  MASK COUNT SPURIOUS\n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    for (uint8_t irq=0;irq<16;irq++) {
        print_number(irq);
        if (irq < 10) vga_write("    ", VGA_COLOR_WHITE, VGA_COLOR_BLACK); else vga_write("   ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_write(irq_is_masked(irq) ? "yes  " : "no   ", irq_is_masked(irq) ? VGA_COLOR_LIGHT_BROWN : VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        print_number(irq_count(irq));
        vga_write("     ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        print_number(irq_spurious_count(irq));
        vga_write("\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    }
}

static void debug_crash_div0(void) {
    volatile uint32_t zero = 0;
    volatile uint32_t value = 1 / zero;
    (void)value;
}

static void debug_crash_gpf(void) {
    __asm__ volatile ("movw $0x1234, %%ax; movw %%ax, %%ds" ::: "ax");
}

static void debug_crash_pf(void) {
    volatile uint32_t *bad = (uint32_t*)0xFFFFFFFFu;
    *bad = 0xBADCAFEu;
}

static void command_debug(char **args, uint8_t count) {
    if (count > 1 && kstrcmp(args[1], "cpu") == 0) command_debug_cpu();
    else if (count > 1 && kstrcmp(args[1], "irq") == 0) command_debug_irq();
    else if (count > 2 && kstrcmp(args[1], "crash") == 0 && kstrcmp(args[2], "div0") == 0) debug_crash_div0();
    else if (count > 2 && kstrcmp(args[1], "crash") == 0 && kstrcmp(args[2], "gpf") == 0) debug_crash_gpf();
    else if (count > 2 && kstrcmp(args[1], "crash") == 0 && kstrcmp(args[2], "pf") == 0) debug_crash_pf();
    else if (count > 2 && kstrcmp(args[1], "crash") == 0 && kstrcmp(args[2], "panic") == 0) panic("debug panic", 0);
    else vga_write("usage: debug cpu | irq | crash div0|gpf|pf|panic\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
}

static void load_timezone_config(void) {
    int index = fs_resolve("/system/timezone", fs_root());
    const fs_node_t *node = fs_node(index);
    int16_t minutes;
    if (node && node->type == FS_FILE && parse_timezone(node->data, &minutes)) rtc_set_timezone_minutes(minutes);
}

static void execute_game(char **args, uint8_t count) {
    if (count > 0 && kstrcmp(args[0], "exit") == 0) { game_active = 0; vga_write("game ended\n", VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK); return; }
    if (count == 1 && args[0][1] == '\0' && args[0][0] >= '1' && args[0][0] <= '9') {
        uint8_t guess = (uint8_t)(args[0][0] - '0');
        if (guess == secret) { vga_write("you won!\n", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK); game_active = 0; }
        else vga_write(guess < secret ? "too low\n" : "too high\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    } else vga_write("guess 1-9 or type exit\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
}

static void command_fetch(const char *url, const char *path) {
    if (fetch_active) { vga_write("fetch already running\n", VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK); return; }
    fetch_save = path && *path;
    if (fetch_save) kstrcpy(fetch_save_path, path, sizeof(fetch_save_path));
    else fetch_save_path[0] = '\0';
    if (!net_fetch_start(url)) {
        vga_write("fetch: ", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write(net_fetch_error(), VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write("\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        return;
    }
    fetch_active = 1;
    vga_write("fetch started\n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
}

static void finish_fetch(void) {
    net_fetch_status_t status = net_fetch_status();
    if (status == NET_FETCH_DONE) {
        const char *body = net_fetch_body();
        uint16_t length = net_fetch_body_length();
        if (fetch_save) {
            if (length > FS_FILE_MAX) vga_write("fetch: response too large for file\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            else {
                fs_result_t result = fs_write(fetch_save_path, body, current_dir);
                if (result == FS_OK) vga_write("fetch saved\n", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
                else print_result(result);
            }
        } else {
            vga_write(body, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            if (!length || body[length - 1] != '\n') vga_write("\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        }
    } else if (status == NET_FETCH_ERROR) {
        vga_write("fetch: ", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write(net_fetch_error(), VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_write("\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    }
    fetch_active = 0;
    prompt();
}

static void shell_vga_write(shell_context_t *context, const char *text) {
    (void)context;
    vga_write(text, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    if (!*text || text[kstrlen(text) - 1] != '\n') vga_write("\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}

static void shell_vga_clear(shell_context_t *context) {
    (void)context;
    vga_clear(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}

static uint8_t shell_vga_launch(shell_context_t *context, const char *id, char **args, uint8_t count) {
    if (shell_text_eq(id, "sproot")) id = "luma";
    if (!app_can_run(id)) {
        shell_vga_write(context, "package is not installed");
        return 1;
    }
    app_set_workdir(context->current_dir);
    app_run(id, args, count);
    return 1;
}

static void shell_vga_reboot(shell_context_t *context) {
    (void)context;
    outb(0x64, 0xFE);
}

static void execute_command(char *command) {
    char parsed[LINE_MAX + 1];
    char shell_line[LINE_MAX + 1];
    char *args[8];
    uint8_t count;
    shell_context_t shell;
    if (!command || !*command) return;

    kstrcpy(parsed, command, sizeof(parsed));
    count = parse_args(parsed, args, 8);
    if (!count) return;
    if (game_active) { execute_game(args, count); return; }
    if (shell_text_eq(args[0], "debug")) { command_debug(args, count); return; }
    if (shell_text_eq(args[0], "fetch") && count > 1) { command_fetch(args[1], count > 2 ? args[2] : 0); return; }
    if (shell_text_eq(args[0], "minipkg") && count > 1) {
        if (shell_text_eq(args[1], "list")) app_list(0);
        else if (shell_text_eq(args[1], "installed")) app_list(1);
        else if (shell_text_eq(args[1], "info") && count > 2) app_info(args[2]);
        else if (shell_text_eq(args[1], "install") && count > 2) app_install(args[2], count > 3 ? args[3] : 0);
        else if (shell_text_eq(args[1], "remove") && count > 2) app_remove(args[2]);
        else vga_write("usage: minipkg list|installed|info|install|remove <id> [url]\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        return;
    }
    if (shell_text_eq(args[0], "game")) {
        secret = (uint8_t)(((++games_played * 3) % 9) + 1);
        game_active = 1;
        vga_write("guess a number from 1 to 9\n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        return;
    }

    shell.current_dir = current_dir;
    shell.gui = 0;
    shell.user = 0;
    shell.write = shell_vga_write;
    shell.clear = shell_vga_clear;
    shell.launch = shell_vga_launch;
    shell.reboot = shell_vga_reboot;
    kstrcpy(shell_line, command, sizeof(shell_line));
    (void)shell_execute(&shell, shell_line);
    current_dir = shell.current_dir;
}

void console_init(void) { fs_init(); users_init(); load_timezone_config(); current_dir=fs_resolve(users_current_home(),fs_root()); app_init(); proc_init(); vga_write("MiniOS terminal v1 - type help\n",VGA_COLOR_LIGHT_CYAN,VGA_COLOR_BLACK); prompt(); }

void console_update(void) {
    net_poll();
    proc_poll();
    uint16_t key = keyboard_pop_event();
    if (fetch_active) {
        if (key == KEY_INTERRUPT) {
            net_fetch_cancel();
            vga_write("^C\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        } else net_fetch_poll();
        if (net_fetch_status() != NET_FETCH_BUSY) finish_fetch();
        return;
    }
    if (app_pkg_busy()) {
        if (key) app_pkg_handle_key(key);
        app_pkg_poll();
        if (!app_pkg_busy()) prompt();
        return;
    }
    if (app_is_active()) {
        app_handle_tick(timer_ticks());
        if (key) app_handle_key(key);
        if (!app_is_active()) prompt();
        return;
    }
    if (!key) return;
    if (key == KEY_INTERRUPT) {
        vga_set_position(input_row, input_col + line_length);
        vga_write("^C\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        prompt();
        return;
    }
    if (key == KEY_LEFT && line_cursor) { line_cursor--; redraw_line(); }
    else if (key == KEY_RIGHT && line_cursor < line_length) { line_cursor++; redraw_line(); }
    else if (key == KEY_HOME) { line_cursor=0; redraw_line(); }
    else if (key == KEY_END) { line_cursor=line_length; redraw_line(); }
    else if (key == KEY_DELETE && line_cursor < line_length) { for(uint16_t i=line_cursor;i<line_length;i++)line[i]=line[i+1];line_length--;redraw_line(); }
    else if (key == KEY_UP && history_count && history_view) { history_view--;kstrcpy(line,history[history_view],sizeof(line));line_length=line_cursor=(uint16_t)kstrlen(line);redraw_line(); }
    else if (key == KEY_DOWN && history_view < history_count) { history_view++;if(history_view==history_count)line[0]='\0';else kstrcpy(line,history[history_view],sizeof(line));line_length=line_cursor=(uint16_t)kstrlen(line);redraw_line(); }
    else if (key == '\b' && line_cursor) { for(uint16_t i=(uint16_t)(line_cursor-1);i<line_length;i++)line[i]=line[i+1];line_cursor--;line_length--;redraw_line(); }
    else if (key == '\n') { vga_set_position(input_row,input_col+line_length);vga_newline();line[line_length]='\0';add_history(line);execute_command(line);if(!app_is_active()&&!fetch_active&&!app_pkg_busy())prompt(); }
    else if (key < 128 && line_length < LINE_MAX) { for(uint16_t i=line_length;i>line_cursor;i--)line[i]=line[i-1];line[line_cursor++]=(char)key;line_length++;line[line_length]='\0';redraw_line(); }
}
