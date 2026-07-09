#include "shell.h"
#include "apps/app.h"
#include "fs.h"
#include "heap.h"
#include "libk.h"
#include "net.h"
#include "proc.h"
#include "rtc.h"
#include "timer.h"
#include "users.h"

static char text_buffer[FS_FILE_MAX + 1];

static char lower_char(char value) {
    return value >= 'A' && value <= 'Z' ? (char)(value + ('a' - 'A')) : value;
}

uint8_t shell_text_eq(const char *left, const char *right) {
    while (*left && *right) {
        if (lower_char(*left++) != lower_char(*right++)) return 0;
    }
    return *left == '\0' && *right == '\0';
}

static void emit(shell_context_t *context, const char *text) {
    if (context && context->write) context->write(context, text);
}

static void append(char *out, uint16_t capacity, const char *text) {
    uint16_t pos = (uint16_t)kstrlen(out);
    if (pos >= capacity) return;
    while (*text && pos + 1 < capacity) out[pos++] = *text++;
    out[pos] = '\0';
}

static void append_uint(char *out, uint16_t capacity, uint32_t value) {
    char number[16];
    kitoa(value, number, 10);
    append(out, capacity, number);
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
        } else {
            while (*text && *text != ' ') text++;
        }
        if (*text) *text++ = '\0';
    }
    return count;
}

static void join_args(char **args, uint8_t start, uint8_t count, char *out, uint16_t capacity) {
    out[0] = '\0';
    for (uint8_t i = start; i < count; i++) {
        if (i > start) append(out, capacity, " ");
        append(out, capacity, args[i]);
    }
}

static uint8_t parse_u16(const char *text, uint16_t *out) {
    uint32_t value = 0;
    if (!text || !*text) return 0;
    while (*text) {
        if (*text < '0' || *text > '9') return 0;
        value = value * 10u + (uint32_t)(*text - '0');
        if (value > 65535u) return 0;
        text++;
    }
    *out = (uint16_t)value;
    return 1;
}

static uint8_t parse_octal(const char *text, uint16_t *out) {
    uint16_t value = 0;
    if (!text || !*text) return 0;
    while (*text) {
        if (*text < '0' || *text > '7') return 0;
        value = (uint16_t)(value * 8u + (uint16_t)(*text - '0'));
        text++;
    }
    *out = value;
    return 1;
}

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

static void timezone_text(int16_t minutes, char *out, uint8_t capacity) {
    uint16_t absolute = minutes < 0 ? (uint16_t)-minutes : (uint16_t)minutes;
    if (capacity < 10) return;
    out[0] = 'U'; out[1] = 'T'; out[2] = 'C';
    out[3] = minutes < 0 ? '-' : '+';
    out[4] = (char)('0' + (absolute / 60u) / 10u);
    out[5] = (char)('0' + (absolute / 60u) % 10u);
    out[6] = ':';
    out[7] = (char)('0' + (absolute % 60u) / 10u);
    out[8] = (char)('0' + (absolute % 60u) % 10u);
    out[9] = '\0';
}

static void clock_text(char *out) {
    int32_t seconds = (int32_t)(rtc_seconds_of_day() + timer_ticks() / TIMER_HZ) + (int32_t)rtc_timezone_minutes() * 60;
    while (seconds < 0) seconds += 86400;
    seconds %= 86400;
    out[0] = (char)('0' + ((uint32_t)seconds / 3600u) / 10u);
    out[1] = (char)('0' + ((uint32_t)seconds / 3600u) % 10u);
    out[2] = ':';
    out[3] = (char)('0' + (((uint32_t)seconds / 60u) % 60u) / 10u);
    out[4] = (char)('0' + (((uint32_t)seconds / 60u) % 60u) % 10u);
    out[5] = ':';
    out[6] = (char)('0' + ((uint32_t)seconds % 60u) / 10u);
    out[7] = (char)('0' + ((uint32_t)seconds % 60u) % 10u);
    out[8] = '\0';
}

static void mode_text(uint16_t mode, char *out) {
    out[0] = (char)('0' + ((mode >> 6) & 7));
    out[1] = (char)('0' + ((mode >> 3) & 7));
    out[2] = (char)('0' + (mode & 7));
    out[3] = '\0';
}

static void print_result(shell_context_t *context, fs_result_t result) {
    if (result == FS_OK) emit(context, "ok");
    else if (result == FS_NOT_FOUND) emit(context, "not found");
    else if (result == FS_EXISTS) emit(context, "already exists");
    else if (result == FS_FULL) emit(context, "filesystem full");
    else if (result == FS_NOT_DIR) emit(context, "not a directory");
    else if (result == FS_NOT_EMPTY) emit(context, "directory is not empty");
    else if (result == FS_TOO_LARGE) emit(context, "file too large");
    else emit(context, "invalid path or command");
}

static void command_help(shell_context_t *context) {
    emit(context, "system: help clear about uname uptime mem minifetch");
    emit(context, "files:  pwd ls [-l] cd mkdir rmdir touch cat");
    emit(context, "files:  write append cp mv rename rm stat df");
    emit(context, "apps:   run luma settings files editor paint terminal");
    emit(context, "proc:   ps jobs bg APP kill PID");
    emit(context, "users:  user list|add|login, whoami");
    emit(context, "net:    net info | net ping [ip]");
}

static void command_minifetch(shell_context_t *context) {
    char line[96];
    char time[9];
    char zone[10];
    clock_text(time);
    timezone_text(rtc_timezone_minutes(), zone, sizeof(zone));
    emit(context, "MiniOS i686 / Luma shell");
    emit(context, "Kernel: protected mode experimental");
    line[0] = '\0';
    append(line, sizeof(line), "Files: ");
    append_uint(line, sizeof(line), fs_used_count());
    append(line, sizeof(line), "/32 nodes");
    emit(context, line);
    line[0] = '\0';
    append(line, sizeof(line), "Uptime: ");
    append_uint(line, sizeof(line), timer_ticks() / TIMER_HZ);
    append(line, sizeof(line), " seconds");
    emit(context, line);
    line[0] = '\0';
    append(line, sizeof(line), "Time: ");
    append(line, sizeof(line), time);
    append(line, sizeof(line), " ");
    append(line, sizeof(line), zone);
    emit(context, line);
}

static void command_mem(shell_context_t *context) {
    char line[96];
    line[0] = '\0';
    append(line, sizeof(line), "Disk FS nodes: ");
    append_uint(line, sizeof(line), fs_used_count());
    append(line, sizeof(line), "/32");
    emit(context, line);
    line[0] = '\0';
    append(line, sizeof(line), "Disk FS data: ");
    append_uint(line, sizeof(line), fs_used_bytes());
    append(line, sizeof(line), "/");
    append_uint(line, sizeof(line), fs_capacity_bytes());
    append(line, sizeof(line), " bytes");
    emit(context, line);
    line[0] = '\0';
    append(line, sizeof(line), "Kernel heap: ");
    append_uint(line, sizeof(line), heap_used());
    append(line, sizeof(line), "/");
    append_uint(line, sizeof(line), heap_capacity());
    append(line, sizeof(line), " bytes");
    emit(context, line);
}

static void command_time(shell_context_t *context, char **args, uint8_t count) {
    char line[96];
    char time[9];
    char zone[10];
    if (count > 3 && shell_text_eq(args[1], "set") &&
        (shell_text_eq(args[2], "utc") || shell_text_eq(args[2], "timezone") || shell_text_eq(args[2], "tz"))) {
        int16_t minutes;
        char saved[10];
        fs_result_t result;
        if (!users_is_root()) {
            emit(context, "only root can set system time zone");
            return;
        }
        if (!parse_timezone(args[3], &minutes)) {
            emit(context, "usage: system set utc +03:00");
            return;
        }
        rtc_set_timezone_minutes(minutes);
        timezone_text(minutes, saved, sizeof(saved));
        result = fs_write("/system/timezone", saved + 3, fs_root());
        if (result != FS_OK) {
            emit(context, "timezone changed but not saved");
            print_result(context, result);
            return;
        }
        line[0] = '\0';
        append(line, sizeof(line), "timezone set to ");
        append(line, sizeof(line), saved);
        emit(context, line);
        return;
    }
    clock_text(time);
    timezone_text(rtc_timezone_minutes(), zone, sizeof(zone));
    line[0] = '\0';
    append(line, sizeof(line), "Local: ");
    append(line, sizeof(line), time);
    append(line, sizeof(line), " ");
    append(line, sizeof(line), zone);
    emit(context, line);
    emit(context, rtc_ready() ? "RTC: ready" : "RTC: unavailable");
}

static void print_node(shell_context_t *context, const fs_node_t *node, uint8_t long_format) {
    char line[96];
    char mode[4];
    if (!node) return;
    line[0] = '\0';
    if (long_format) {
        mode_text(node->mode, mode);
        append(line, sizeof(line), node->type == FS_DIR ? "d " : "f ");
        append(line, sizeof(line), mode);
        append(line, sizeof(line), " ");
        append_uint(line, sizeof(line), node->size);
        append(line, sizeof(line), " ");
        append(line, sizeof(line), node->name);
    } else {
        append(line, sizeof(line), node->type == FS_DIR ? "[dir]  " : "[file] ");
        append(line, sizeof(line), node->name);
    }
    emit(context, line);
}

static void command_ls(shell_context_t *context, char **args, uint8_t count) {
    uint8_t long_format = count > 1 && shell_text_eq(args[1], "-l");
    const char *path = long_format ? (count > 2 ? args[2] : ".") : (count > 1 ? args[1] : ".");
    int index = fs_resolve(path, context->current_dir);
    const fs_node_t *node = fs_node(index);
    uint8_t found = 0;
    if (!node) {
        emit(context, "not found");
        return;
    }
    if (node->type != FS_DIR) {
        print_node(context, node, long_format);
        return;
    }
    if (long_format) emit(context, "T MODE SIZE NAME");
    for (int i = 0; i < FS_MAX_NODES; i++) {
        const fs_node_t *child = fs_node(i);
        if (child && child->parent == index) {
            print_node(context, child, long_format);
            found = 1;
        }
    }
    if (!found) emit(context, "empty");
}

static void command_stat(shell_context_t *context, const char *path) {
    int index = fs_resolve(path, context->current_dir);
    const fs_node_t *node = fs_node(index);
    char line[96];
    char mode[4];
    char full_path[80];
    if (!node) {
        emit(context, "not found");
        return;
    }
    fs_path(index, full_path, sizeof(full_path));
    mode_text(node->mode, mode);
    line[0] = '\0';
    append(line, sizeof(line), "Path: ");
    append(line, sizeof(line), full_path);
    emit(context, line);
    emit(context, node->type == FS_DIR ? "Type: directory" : "Type: file");
    line[0] = '\0';
    append(line, sizeof(line), "Size: ");
    append_uint(line, sizeof(line), node->size);
    append(line, sizeof(line), " bytes");
    emit(context, line);
    line[0] = '\0';
    append(line, sizeof(line), "Mode: ");
    append(line, sizeof(line), mode);
    emit(context, line);
}

static void command_df(shell_context_t *context) {
    char line[96];
    line[0] = '\0';
    append(line, sizeof(line), "Nodes: ");
    append_uint(line, sizeof(line), fs_used_count());
    append(line, sizeof(line), "/");
    append_uint(line, sizeof(line), FS_MAX_NODES);
    append(line, sizeof(line), " used");
    emit(context, line);
    line[0] = '\0';
    append(line, sizeof(line), "Data: ");
    append_uint(line, sizeof(line), fs_used_bytes());
    append(line, sizeof(line), "/");
    append_uint(line, sizeof(line), fs_capacity_bytes());
    append(line, sizeof(line), " bytes used");
    emit(context, line);
}

static void command_ps(shell_context_t *context) {
    proc_info_t list[12];
    uint8_t count = proc_snapshot(list, (uint8_t)(sizeof(list) / sizeof(list[0])));
    uint32_t now = timer_ticks();
    emit(context, "PID TIME NAME");
    for (uint8_t i = 0; i < count; i++) {
        char line[96];
        line[0] = '\0';
        append_uint(line, sizeof(line), list[i].pid);
        append(line, sizeof(line), " ");
        append_uint(line, sizeof(line), (now - list[i].started_ticks) / TIMER_HZ);
        append(line, sizeof(line), "s ");
        append(line, sizeof(line), list[i].is_protected ? "*" : "");
        append(line, sizeof(line), list[i].name);
        emit(context, line);
    }
}

static void command_user(shell_context_t *context, char **args, uint8_t count) {
    if (count > 1 && shell_text_eq(args[1], "list")) {
        user_info_t users[8];
        uint8_t total = users_snapshot(users, (uint8_t)(sizeof(users) / sizeof(users[0])));
        emit(context, "NAME ROLE");
        for (uint8_t i = 0; i < total; i++) {
            char line[64];
            line[0] = '\0';
            append(line, sizeof(line), users[i].current ? "*" : " ");
            append(line, sizeof(line), users[i].name);
            append(line, sizeof(line), " ");
            append(line, sizeof(line), users[i].root ? "root" : "user");
            emit(context, line);
        }
    } else if (count > 3 && shell_text_eq(args[1], "add")) {
        users_result_t result = users_add_result(args[2], args[3]);
        emit(context, users_result_text(result));
    } else if (count > 2 && shell_text_eq(args[1], "login")) {
        if (users_login_result(args[2])) {
            context->current_dir = fs_resolve(users_current_home(), fs_root());
            emit(context, "logged in");
        } else emit(context, "user not found");
    } else emit(context, "usage: user list | user add NAME root|user | user login NAME");
}

static const char *app_alias(const char *id) {
    if (shell_text_eq(id, "files")) return "tbf";
    if (shell_text_eq(id, "editor") || shell_text_eq(id, "edit")) return "free";
    if (shell_text_eq(id, "neofetch")) return "minifetch";
    return id;
}

static void command_run(shell_context_t *context, char **args, uint8_t count) {
    const char *id;
    if (count < 2) {
        emit(context, "usage: run APP");
        return;
    }
    id = app_alias(args[1]);
    if (!context->launch || !context->launch(context, id, args + 2, (uint8_t)(count - 2))) {
        emit(context, "cannot run app here");
    }
}

static void command_bg(shell_context_t *context, const char *id) {
    uint16_t pid;
    char line[96];
    if (!app_can_run(id)) {
        emit(context, "package is not installed");
        return;
    }
    pid = proc_spawn(id);
    if (!pid) {
        emit(context, "process table full");
        return;
    }
    line[0] = '\0';
    append(line, sizeof(line), "[");
    append_uint(line, sizeof(line), pid);
    append(line, sizeof(line), "] ");
    append(line, sizeof(line), id);
    append(line, sizeof(line), " started");
    emit(context, line);
}

static uint8_t command_app_alias(shell_context_t *context, const char *id) {
    id = app_alias(id);
    if (shell_text_eq(id, "tbf") || shell_text_eq(id, "free") || shell_text_eq(id, "paint") ||
        shell_text_eq(id, "terminal") || shell_text_eq(id, "settings") || shell_text_eq(id, "luma") ||
        shell_text_eq(id, "sproot") || shell_text_eq(id, "minifetch")) {
        char *args[1] = {0};
        return context->launch && context->launch(context, id, args, 0);
    }
    return 0;
}

shell_result_t shell_execute(shell_context_t *context, char *command) {
    char *args[8];
    uint8_t count = parse_args(command, args, (uint8_t)(sizeof(args) / sizeof(args[0])));
    if (!count) return SHELL_RESULT_OK;

    if (count > 0 && shell_text_eq(args[count - 1], "&")) {
        if (count > 2 && shell_text_eq(args[0], "run")) command_bg(context, args[1]);
        else emit(context, "background mode supports: run APP &");
        return SHELL_RESULT_OK;
    }

    if (shell_text_eq(args[0], "help")) command_help(context);
    else if (shell_text_eq(args[0], "clear")) { if (context->clear) context->clear(context); }
    else if (shell_text_eq(args[0], "about") || shell_text_eq(args[0], "uname")) emit(context, "MiniOS i686 v1 experimental kernel");
    else if (shell_text_eq(args[0], "uptime")) {
        char line[64];
        line[0] = '\0';
        append(line, sizeof(line), "uptime: ");
        append_uint(line, sizeof(line), timer_ticks() / TIMER_HZ);
        append(line, sizeof(line), " s");
        emit(context, line);
    } else if (shell_text_eq(args[0], "mem")) command_mem(context);
    else if (shell_text_eq(args[0], "minifetch") || shell_text_eq(args[0], "neofetch")) command_minifetch(context);
    else if (shell_text_eq(args[0], "system") || shell_text_eq(args[0], "date")) command_time(context, args, count);
    else if (shell_text_eq(args[0], "whoami")) {
        char line[64];
        line[0] = '\0';
        append(line, sizeof(line), users_current_name());
        append(line, sizeof(line), " (");
        append(line, sizeof(line), users_current_role());
        append(line, sizeof(line), ")");
        emit(context, line);
    } else if (shell_text_eq(args[0], "user")) command_user(context, args, count);
    else if (shell_text_eq(args[0], "pwd")) {
        char path[80];
        fs_path(context->current_dir, path, sizeof(path));
        emit(context, path);
    } else if (shell_text_eq(args[0], "ls")) command_ls(context, args, count);
    else if (shell_text_eq(args[0], "cd")) {
        int target = fs_resolve(count > 1 ? args[1] : "/", context->current_dir);
        const fs_node_t *node = fs_node(target);
        if (node && node->type == FS_DIR) { context->current_dir = target; emit(context, "ok"); }
        else emit(context, "directory not found");
    } else if (shell_text_eq(args[0], "mkdir") && count > 1) print_result(context, fs_create(args[1], FS_DIR, context->current_dir));
    else if (shell_text_eq(args[0], "touch") && count > 1) {
        fs_result_t result = fs_create(args[1], FS_FILE, context->current_dir);
        print_result(context, result == FS_EXISTS ? FS_OK : result);
    } else if (shell_text_eq(args[0], "cat") && count > 1) {
        int index = fs_resolve(args[1], context->current_dir);
        const fs_node_t *node = fs_node(index);
        if (node && node->type == FS_FILE && fs_can_read(index)) emit(context, node->data);
        else emit(context, "file not found or permission denied");
    } else if (shell_text_eq(args[0], "write") && count > 1) {
        join_args(args, 2, count, text_buffer, sizeof(text_buffer));
        print_result(context, fs_write(args[1], text_buffer, context->current_dir));
    } else if (shell_text_eq(args[0], "append") && count > 2) {
        join_args(args, 2, count, text_buffer, sizeof(text_buffer));
        print_result(context, fs_append(args[1], text_buffer, context->current_dir));
    } else if (shell_text_eq(args[0], "cp") && count > 2) print_result(context, fs_copy(args[1], args[2], context->current_dir));
    else if ((shell_text_eq(args[0], "mv") || shell_text_eq(args[0], "rename")) && count > 2) print_result(context, fs_move(args[1], args[2], context->current_dir));
    else if (shell_text_eq(args[0], "stat") && count > 1) command_stat(context, args[1]);
    else if (shell_text_eq(args[0], "df")) command_df(context);
    else if (shell_text_eq(args[0], "rm") && count > 1) print_result(context, fs_remove(args[1], context->current_dir, 0));
    else if (shell_text_eq(args[0], "rmdir") && count > 1) print_result(context, fs_remove(args[1], context->current_dir, 1));
    else if (shell_text_eq(args[0], "chmod") && count > 2) {
        uint16_t mode;
        if (!parse_octal(args[1], &mode)) emit(context, "usage: chmod 644 file");
        else { fs_chmod(args[2], mode, context->current_dir); emit(context, "ok"); }
    } else if (shell_text_eq(args[0], "chown") && count > 2) {
        int uid = users_find_uid(args[1]);
        if (uid < 0) emit(context, "user not found");
        else if (!users_is_root()) emit(context, "only root can chown");
        else { fs_chown(args[2], (uint8_t)uid, context->current_dir); emit(context, "ok"); }
    } else if (shell_text_eq(args[0], "ps") || shell_text_eq(args[0], "jobs")) command_ps(context);
    else if (shell_text_eq(args[0], "bg") && count > 1) command_bg(context, args[1]);
    else if (shell_text_eq(args[0], "kill") && count > 1) {
        uint16_t pid;
        if (!parse_u16(args[1], &pid)) emit(context, "usage: kill PID");
        else {
            proc_result_t result = proc_kill(pid);
            if (result == PROC_RESULT_OK) emit(context, "process killed");
            else if (result == PROC_RESULT_PROTECTED) emit(context, "cannot kill system process");
            else emit(context, "process not found");
        }
    } else if (shell_text_eq(args[0], "net") && count > 1 && shell_text_eq(args[1], "info")) {
        if (!net_ready()) emit(context, "network: RTL8139 not found");
        else if (net_ping_ok()) emit(context, "network: online, gateway ping replied");
        else if (net_gateway_known()) emit(context, "network: gateway resolved, ping pending");
        else emit(context, "network: adapter ready, resolving gateway");
    } else if (shell_text_eq(args[0], "net") && count > 1 && shell_text_eq(args[1], "ping")) {
        if (count > 2) emit(context, net_ping_ip(args[2]) ? "ping sent; use net info" : "usage: net ping 8.8.8.8");
        else { net_ping_gateway(); emit(context, "ping sent to 10.0.2.2; use net info"); }
    } else if (shell_text_eq(args[0], "run")) command_run(context, args, count);
    else if (shell_text_eq(args[0], "exit")) return SHELL_RESULT_EXIT;
    else if (shell_text_eq(args[0], "reboot")) {
        emit(context, "rebooting...");
        if (context->reboot) context->reboot(context);
    } else if (!command_app_alias(context, args[0])) emit(context, "unknown command; type help");

    return SHELL_RESULT_OK;
}
