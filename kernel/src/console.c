#include <stdint.h>
#include "console.h"
#include "fs.h"
#include "io.h"
#include "keyboard.h"
#include "libk.h"
#include "timer.h"
#include "vga.h"
#include "apps/app.h"
#include "heap.h"
#include "net.h"
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

static void command_help(void) {
    vga_write("system: help clear about uname uptime mem pwd reboot\n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write("files:  ls cd mkdir rmdir touch cat write rm\n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write("apps:   minipkg run minifetch game\nnet:    net info | net ping | fetch URL [file]\nusers:  user add|list|login, whoami\n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write("pkg:    minipkg install ID [url] asks y/n and downloads\n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
}

static void command_ls(const char *path) {
    int directory = fs_resolve(path, current_dir);
    const fs_node_t *node = fs_node(directory);
    if (!node) { vga_write("not found\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK); return; }
    if (node->type != FS_DIR) { vga_write(node->name, VGA_COLOR_WHITE, VGA_COLOR_BLACK); vga_write("\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK); return; }
    for (int i=0;i<FS_MAX_NODES;i++) {
        const fs_node_t *child = fs_node(i);
        if (child && child->parent == directory) {
            vga_write(child->type == FS_DIR ? "[dir]  " : "[file] ", child->type == FS_DIR ? VGA_COLOR_LIGHT_CYAN : VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            vga_write(child->name, VGA_COLOR_WHITE, VGA_COLOR_BLACK); vga_write("\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        }
    }
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

static void execute_command(char *command) {
    char *args[8]; uint8_t count = parse_args(command, args, 8);
    if (!count) return;
    if (game_active) { execute_game(args, count); return; }
    if (kstrcmp(args[0], "help") == 0) command_help();
    else if (kstrcmp(args[0], "clear") == 0) vga_clear(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    else if (kstrcmp(args[0], "about") == 0 || kstrcmp(args[0], "uname") == 0) vga_write("MiniOS i686 v1 experimental kernel\n", VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    else if (kstrcmp(args[0], "uptime") == 0) { vga_write("uptime: ", VGA_COLOR_WHITE, VGA_COLOR_BLACK); print_number(timer_ticks() / TIMER_HZ); vga_write(" s\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK); }
    else if (kstrcmp(args[0], "mem") == 0) { vga_write("Disk FS nodes: ", VGA_COLOR_WHITE, VGA_COLOR_BLACK); print_number(fs_used_count()); vga_write("/32\nDisk FS data: ",VGA_COLOR_WHITE,VGA_COLOR_BLACK);print_number(fs_used_bytes());vga_write("/",VGA_COLOR_WHITE,VGA_COLOR_BLACK);print_number(fs_capacity_bytes());vga_write(" bytes (image: 4 MiB)\nKernel heap: ", VGA_COLOR_WHITE, VGA_COLOR_BLACK); print_number(heap_used()); vga_write("/",VGA_COLOR_WHITE,VGA_COLOR_BLACK); print_number(heap_capacity()); vga_write(" bytes\n",VGA_COLOR_WHITE,VGA_COLOR_BLACK); }
    else if (kstrcmp(args[0], "net") == 0 && count > 1 && kstrcmp(args[1], "info") == 0) {
        if(!net_ready())vga_write("network: RTL8139 not found\n",VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK);
        else if(net_ping_ok())vga_write("network: online, gateway ping replied\n",VGA_COLOR_LIGHT_GREEN,VGA_COLOR_BLACK);
        else if(net_gateway_known())vga_write("network: gateway resolved, ping pending\n",VGA_COLOR_LIGHT_BROWN,VGA_COLOR_BLACK);
        else vga_write("network: adapter ready, resolving gateway\n",VGA_COLOR_LIGHT_BROWN,VGA_COLOR_BLACK);
    }
    else if (kstrcmp(args[0], "net") == 0 && count > 1 && kstrcmp(args[1], "ping") == 0) {
        if(count > 2) { if(net_ping_ip(args[2]))vga_write("ping sent; use net info\n",VGA_COLOR_LIGHT_CYAN,VGA_COLOR_BLACK);else vga_write("usage: net ping 8.8.8.8\n",VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK); }
        else { net_ping_gateway(); vga_write("ping sent to 10.0.2.2; use net info\n",VGA_COLOR_LIGHT_CYAN,VGA_COLOR_BLACK); }
    }
    else if (kstrcmp(args[0], "fetch") == 0 && count > 1) command_fetch(args[1], count > 2 ? args[2] : 0);
    else if (kstrcmp(args[0], "whoami") == 0) { vga_write(users_current_name(),VGA_COLOR_LIGHT_GREEN,VGA_COLOR_BLACK);vga_write(" (",VGA_COLOR_WHITE,VGA_COLOR_BLACK);vga_write(users_current_role(),VGA_COLOR_LIGHT_BROWN,VGA_COLOR_BLACK);vga_write(")\n",VGA_COLOR_WHITE,VGA_COLOR_BLACK); }
    else if (kstrcmp(args[0], "user") == 0 && count > 1) {
        if(kstrcmp(args[1],"list")==0) users_list();
        else if(kstrcmp(args[1],"add")==0 && count>3) users_add(args[2],args[3]);
        else if(kstrcmp(args[1],"login")==0 && count>2) { if(users_login(args[2]))current_dir=fs_resolve(users_current_home(),fs_root()); }
        else vga_write("usage: user list | user add NAME root|user | user login NAME\n",VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK);
    }
    else if (kstrcmp(args[0], "minifetch") == 0) app_run("minifetch",0,0);
    else if (kstrcmp(args[0], "tbf") == 0) { app_set_workdir(current_dir); app_run("tbf",0,0); }
    else if (kstrcmp(args[0], "sproot") == 0) { app_set_workdir(current_dir); app_run("sproot",0,0); }
    else if (kstrcmp(args[0], "minipkg") == 0 && count > 1) {
        if (kstrcmp(args[1], "list") == 0) app_list(0);
        else if (kstrcmp(args[1], "installed") == 0) app_list(1);
        else if (kstrcmp(args[1], "info") == 0 && count > 2) app_info(args[2]);
        else if (kstrcmp(args[1], "install") == 0 && count > 2) app_install(args[2], count > 3 ? args[3] : 0);
        else if (kstrcmp(args[1], "remove") == 0 && count > 2) app_remove(args[2]);
        else vga_write("usage: minipkg list|installed|info|install|remove <id> [url]\n",VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK);
    }
    else if (kstrcmp(args[0], "run") == 0 && count > 1) { app_set_workdir(current_dir); app_run(args[1],args+2,(uint8_t)(count-2)); }
    else if (kstrcmp(args[0], "pwd") == 0) { char path[80]; fs_path(current_dir,path,sizeof(path)); vga_write(path,VGA_COLOR_WHITE,VGA_COLOR_BLACK); vga_write("\n",VGA_COLOR_WHITE,VGA_COLOR_BLACK); }
    else if (kstrcmp(args[0], "ls") == 0) command_ls(count > 1 ? args[1] : ".");
    else if (kstrcmp(args[0], "cd") == 0) { int target = fs_resolve(count > 1 ? args[1] : "/", current_dir); const fs_node_t *node = fs_node(target); if (node && node->type == FS_DIR) current_dir = target; else vga_write("directory not found\n",VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK); }
    else if (kstrcmp(args[0], "mkdir") == 0 && count > 1) { fs_result_t r=fs_create(args[1],FS_DIR,current_dir); if(r!=FS_OK)print_result(r); }
    else if (kstrcmp(args[0], "touch") == 0 && count > 1) { fs_result_t r=fs_create(args[1],FS_FILE,current_dir); if(r!=FS_OK && r!=FS_EXISTS)print_result(r); }
    else if (kstrcmp(args[0], "cat") == 0 && count > 1) { int f=fs_resolve(args[1],current_dir); const fs_node_t*n=fs_node(f); if(n&&n->type==FS_FILE&&fs_can_read(f)){vga_write(n->data,VGA_COLOR_WHITE,VGA_COLOR_BLACK);vga_write("\n",VGA_COLOR_WHITE,VGA_COLOR_BLACK);}else vga_write("file not found or permission denied\n",VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK); }
    else if (kstrcmp(args[0], "write") == 0 && count > 1) { fs_result_t r=fs_write(args[1],count>2?args[2]:"",current_dir); if(r!=FS_OK)print_result(r); }
    else if (kstrcmp(args[0], "rm") == 0 && count > 1) { fs_result_t r=fs_remove(args[1],current_dir,0); if(r!=FS_OK)print_result(r); }
    else if (kstrcmp(args[0], "rmdir") == 0 && count > 1) { fs_result_t r=fs_remove(args[1],current_dir,1); if(r!=FS_OK)print_result(r); }
    else if (kstrcmp(args[0], "chmod") == 0 && count > 2) { uint16_t mode=0;for(uint8_t i=0;args[1][i];i++){if(args[1][i]<'0'||args[1][i]>'7'){mode=0;break;}mode=(uint16_t)(mode*8+args[1][i]-'0');}fs_chmod(args[2],mode,current_dir); }
    else if (kstrcmp(args[0], "chown") == 0 && count > 2) { int uid=users_find_uid(args[1]);if(uid<0)vga_write("user not found\n",VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK);else if(!users_is_root())vga_write("only root can chown\n",VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK);else fs_chown(args[2],(uint8_t)uid,current_dir); }
    else if (kstrcmp(args[0], "game") == 0) { secret=(uint8_t)(((++games_played*3)%9)+1);game_active=1;vga_write("guess a number from 1 to 9\n",VGA_COLOR_LIGHT_CYAN,VGA_COLOR_BLACK); }
    else if (kstrcmp(args[0], "reboot") == 0) { vga_write("rebooting...\n",VGA_COLOR_LIGHT_BROWN,VGA_COLOR_BLACK); outb(0x64,0xFE); }
    else vga_write("unknown command; type help\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
}

void console_init(void) { fs_init(); users_init(); current_dir=fs_resolve(users_current_home(),fs_root()); app_init(); vga_write("MiniOS terminal v1 - type help\n",VGA_COLOR_LIGHT_CYAN,VGA_COLOR_BLACK); prompt(); }

void console_update(void) {
    net_poll();
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
