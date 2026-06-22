#include "app.h"
#include "../fs.h"
#include "../keyboard.h"
#include "../libk.h"
#include "../net.h"
#include "../vga.h"
#include "../gfx.h"

extern void app_minifetch_start(char **args, uint8_t count);
extern void app_calc_start(char **args, uint8_t count);
extern void app_clock_start(char **args, uint8_t count);
extern void app_files_start(char **args, uint8_t count);
extern void app_matrix_start(char **args, uint8_t count);
extern void app_mines_start(char **args, uint8_t count);
extern void app_free_start(char **args, uint8_t count);
extern void app_tbf_start(char **args, uint8_t count);
extern void app_sproot_start(char **args, uint8_t count);
extern void app_clock_key(uint16_t key); extern void app_clock_tick(uint32_t ticks);
extern void app_files_key(uint16_t key);
extern void app_matrix_key(uint16_t key); extern void app_matrix_tick(uint32_t ticks);
extern void app_mines_key(uint16_t key);
extern void app_free_key(uint16_t key);
extern void app_tbf_key(uint16_t key);
extern void app_sproot_key(uint16_t key);
extern void app_sproot_tick(uint32_t ticks);
extern void app_tbf_tick(uint32_t ticks);
extern void app_free_tick(uint32_t ticks);
extern void app_paint_start(char **args, uint8_t count);
extern void app_paint_key(uint16_t key);
extern void app_paint_tick(uint32_t ticks);
extern void app_terminal_start(char **args, uint8_t count);
extern void app_terminal_key(uint16_t key);
extern void app_terminal_tick(uint32_t ticks);

#define PACKAGE(id, name, version, description, start, key, tick) {#id, name, version, description, start, key, tick},
static const app_package_t packages[] = {
#include "../../../apps/catalog.def"
};
#undef PACKAGE

#define PACKAGE_COUNT (sizeof(packages) / sizeof(packages[0]))
#define MINIPKG_BASE_URL "https://raw.githubusercontent.com/dev-stanislav/OS/main/apps/"
#define MINIPKG_INDEX_URL "https://raw.githubusercontent.com/dev-stanislav/OS/main/apps/index.txt"

typedef enum { PKG_IDLE, PKG_CONFIRM, PKG_DOWNLOAD, PKG_LIST, PKG_INFO } pkg_state_t;

static uint8_t installed[PACKAGE_COUNT];
static const app_package_t *active;
static int working_dir;
static pkg_state_t pkg_state;
static int pkg_pending_index;
static char pkg_pending_path[40];
static char pkg_pending_url[192];

static void text_append(char *out, uint16_t cap, const char *text) {
    size_t len = kstrlen(out);
    kstrcpy(out + len, text, cap > len ? cap - len : 0);
}

static void package_path(char *path, const char *id) {
    kstrcpy(path, "/apps/", 40);
    kstrcpy(path + kstrlen(path), id, 40 - kstrlen(path));
    kstrcpy(path + kstrlen(path), ".pkg", 40 - kstrlen(path));
}

static int package_index(const char *id) {
    for (uint8_t i=0;i<PACKAGE_COUNT;i++) if (kstrcmp(packages[i].id,id)==0) return i;
    return -1;
}

static uint8_t builtin_package(const char *id) {
    return kstrcmp(id,"tbf")==0||kstrcmp(id,"sproot")==0||kstrcmp(id,"free")==0||kstrcmp(id,"paint")==0||kstrcmp(id,"terminal")==0;
}

static void package_url(char *url, const char *id) {
    kstrcpy(url, MINIPKG_BASE_URL, 192);
    text_append(url, 192, id);
    text_append(url, 192, "/manifest.txt");
}

static void print_number(uint32_t value) {
    char text[16];
    kitoa(value, text, 10);
    vga_write(text, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}

static uint8_t starts_with(const char *text, const char *prefix) {
    while (*prefix) if (*text++ != *prefix++) return 0;
    return 1;
}

static uint8_t manifest_matches_id(const char *manifest, const char *id) {
    char needle[40];
    kstrcpy(needle, "id=", sizeof(needle));
    text_append(needle, sizeof(needle), id);
    while (*manifest) {
        if (starts_with(manifest, needle)) {
            char next = manifest[kstrlen(needle)];
            if (next == '\0' || next == '\n' || next == '\r') return 1;
        }
        while (*manifest && *manifest != '\n') manifest++;
        if (*manifest == '\n') manifest++;
    }
    return 0;
}

static void status(const char *message, uint8_t color) { vga_write(message, color, VGA_COLOR_BLACK); vga_write("\n", color, VGA_COLOR_BLACK); }

void app_init(void) {
    active=0; working_dir=fs_root(); pkg_state=PKG_IDLE; pkg_pending_index=-1;
    (void)fs_create("/apps",FS_DIR,fs_root());
    for (uint8_t i=0;i<PACKAGE_COUNT;i++) { char path[40]; package_path(path,packages[i].id); installed[i]=fs_resolve(path,fs_root())>=0; if(builtin_package(packages[i].id))installed[i]=1; }
}

void app_set_workdir(int directory) { working_dir = directory; }
int app_get_workdir(void) { return working_dir; }

void app_list(uint8_t installed_only) {
    if(!installed_only) {
        if(pkg_state!=PKG_IDLE){status("minipkg is busy",VGA_COLOR_LIGHT_BROWN);return;}
        if(!net_fetch_start(MINIPKG_INDEX_URL)) {
            vga_write("minipkg: ",VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK);
            vga_write(net_fetch_error(),VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK);
            vga_write("\n",VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK);
            return;
        }
        status("loading app library...",VGA_COLOR_LIGHT_CYAN);
        pkg_state=PKG_LIST;
        return;
    }
    for (uint8_t i=0;i<PACKAGE_COUNT;i++) if (!installed_only || installed[i]) {
        vga_write(installed[i] ? "[installed] " : "[available] ", installed[i] ? VGA_COLOR_LIGHT_GREEN : VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        vga_write(packages[i].id, VGA_COLOR_WHITE, VGA_COLOR_BLACK); vga_write(" - ",VGA_COLOR_WHITE,VGA_COLOR_BLACK);
        vga_write(packages[i].description,VGA_COLOR_LIGHT_GREY,VGA_COLOR_BLACK); vga_write("\n",VGA_COLOR_WHITE,VGA_COLOR_BLACK);
    }
}

void app_info(const char *id) {
    int index=package_index(id); if(index<0){status("package not found",VGA_COLOR_LIGHT_RED);return;}
    if(pkg_state!=PKG_IDLE){status("minipkg is busy",VGA_COLOR_LIGHT_BROWN);return;}
    pkg_pending_index=index;
    package_url(pkg_pending_url,id);
    if(!net_fetch_start(pkg_pending_url)) {
        vga_write("minipkg: ",VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK);
        vga_write(net_fetch_error(),VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK);
        vga_write("\n",VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK);
        return;
    }
    status("loading package info...",VGA_COLOR_LIGHT_CYAN);
    pkg_state=PKG_INFO;
}

void app_install(const char *id, const char *url) {
    int index=package_index(id); if(index<0){status("package not found",VGA_COLOR_LIGHT_RED);return;} if(installed[index]){status("already installed",VGA_COLOR_LIGHT_BROWN);return;} if(pkg_state!=PKG_IDLE){status("minipkg is busy",VGA_COLOR_LIGHT_BROWN);return;}
    const app_package_t *p=&packages[index];
    pkg_pending_index=index;
    package_path(pkg_pending_path,id);
    if(url&&*url) kstrcpy(pkg_pending_url,url,sizeof(pkg_pending_url)); else package_url(pkg_pending_url,id);
    vga_write("MiniPkg install\n",VGA_COLOR_LIGHT_CYAN,VGA_COLOR_BLACK);
    vga_write("package: ",VGA_COLOR_WHITE,VGA_COLOR_BLACK);vga_write(p->id,VGA_COLOR_LIGHT_GREEN,VGA_COLOR_BLACK);vga_write(" ",VGA_COLOR_WHITE,VGA_COLOR_BLACK);vga_write(p->version,VGA_COLOR_WHITE,VGA_COLOR_BLACK);vga_write("\n",VGA_COLOR_WHITE,VGA_COLOR_BLACK);
    vga_write("description: ",VGA_COLOR_WHITE,VGA_COLOR_BLACK);vga_write(p->description,VGA_COLOR_LIGHT_GREY,VGA_COLOR_BLACK);vga_write("\n",VGA_COLOR_WHITE,VGA_COLOR_BLACK);
    vga_write("source: ",VGA_COLOR_WHITE,VGA_COLOR_BLACK);vga_write(pkg_pending_url,VGA_COLOR_LIGHT_GREY,VGA_COLOR_BLACK);vga_write("\n",VGA_COLOR_WHITE,VGA_COLOR_BLACK);
    vga_write("Download and install? [y/n] ",VGA_COLOR_LIGHT_BROWN,VGA_COLOR_BLACK);
    pkg_state=PKG_CONFIRM;
}

void app_remove(const char *id) {
    int index=package_index(id); if(index<0){status("package not found",VGA_COLOR_LIGHT_RED);return;} if(builtin_package(id)){status("built-in app cannot be removed",VGA_COLOR_LIGHT_BROWN);return;} if(!installed[index]){status("not installed",VGA_COLOR_LIGHT_BROWN);return;}
    char path[40]; package_path(path,id);
    (void)fs_remove(path,fs_root(),0); installed[index]=0; status("removed",VGA_COLOR_LIGHT_GREEN);
}

uint8_t app_can_run(const char *id) {
    int index = package_index(id);
    return index >= 0 && installed[index];
}

void app_run(const char *id, char **args, uint8_t count) {
    int index=package_index(id); if(index<0){status("package not found",VGA_COLOR_LIGHT_RED);return;} if(!installed[index]){status("package is not installed",VGA_COLOR_LIGHT_RED);return;}
    active=&packages[index]; active->start(args,count);
    if (!active->key && !active->tick) active=0;
}

uint8_t app_is_active(void) { return active != 0; }
uint8_t app_pkg_busy(void) { return pkg_state != PKG_IDLE; }

void app_pkg_handle_key(uint16_t key) {
    if(pkg_state==PKG_IDLE)return;
    if(key==KEY_INTERRUPT) {
        if(pkg_state==PKG_DOWNLOAD||pkg_state==PKG_LIST||pkg_state==PKG_INFO) net_fetch_cancel();
        vga_write("^C\n",VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK);
        status("minipkg cancelled",VGA_COLOR_LIGHT_BROWN);
        pkg_state=PKG_IDLE;
        return;
    }
    if(pkg_state!=PKG_CONFIRM)return;
    if(key=='y'||key=='Y') {
        vga_write("y\n",VGA_COLOR_LIGHT_GREEN,VGA_COLOR_BLACK);
        if(!net_fetch_start(pkg_pending_url)) {
            vga_write("minipkg: ",VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK);
            vga_write(net_fetch_error(),VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK);
            vga_write("\n",VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK);
            pkg_state=PKG_IDLE;
            return;
        }
        status("downloading package manifest...",VGA_COLOR_LIGHT_CYAN);
        pkg_state=PKG_DOWNLOAD;
    } else if(key=='n'||key=='N'||key==KEY_ESCAPE) {
        vga_write("n\n",VGA_COLOR_LIGHT_BROWN,VGA_COLOR_BLACK);
        status("install cancelled",VGA_COLOR_LIGHT_BROWN);
        pkg_state=PKG_IDLE;
    }
}

void app_pkg_poll(void) {
    if(pkg_state!=PKG_DOWNLOAD&&pkg_state!=PKG_LIST&&pkg_state!=PKG_INFO)return;
    net_fetch_poll();
    net_fetch_status_t fetch_status=net_fetch_status();
    if(fetch_status==NET_FETCH_BUSY)return;
    if(fetch_status==NET_FETCH_ERROR) {
        vga_write("minipkg: ",VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK);
        vga_write(net_fetch_error(),VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK);
        vga_write("\n",VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK);
        pkg_state=PKG_IDLE;
        return;
    }
    const char *body=net_fetch_body();
    uint16_t length=net_fetch_body_length();
    if(pkg_state==PKG_LIST||pkg_state==PKG_INFO) {
        vga_write(body,VGA_COLOR_WHITE,VGA_COLOR_BLACK);
        if(!length||body[length-1]!='\n') vga_write("\n",VGA_COLOR_WHITE,VGA_COLOR_BLACK);
        if(pkg_state==PKG_INFO) vga_write(installed[pkg_pending_index]?"status=installed\n":"status=available\n",installed[pkg_pending_index]?VGA_COLOR_LIGHT_GREEN:VGA_COLOR_LIGHT_BROWN,VGA_COLOR_BLACK);
        pkg_state=PKG_IDLE;
        return;
    }
    const app_package_t *p=&packages[pkg_pending_index];
    if(length==0||length>FS_FILE_MAX) {
        status("minipkg: package manifest is too large",VGA_COLOR_LIGHT_RED);
        pkg_state=PKG_IDLE;
        return;
    }
    if(!manifest_matches_id(body,p->id)) {
        status("minipkg: package id mismatch",VGA_COLOR_LIGHT_RED);
        pkg_state=PKG_IDLE;
        return;
    }
    fs_result_t result=fs_write(pkg_pending_path,body,fs_root());
    if(result!=FS_OK) {
        status("minipkg: cannot write package",VGA_COLOR_LIGHT_RED);
        pkg_state=PKG_IDLE;
        return;
    }
    installed[pkg_pending_index]=1;
    vga_write("installed ",VGA_COLOR_LIGHT_GREEN,VGA_COLOR_BLACK);
    vga_write(p->id,VGA_COLOR_WHITE,VGA_COLOR_BLACK);
    vga_write(" (",VGA_COLOR_WHITE,VGA_COLOR_BLACK);
    print_number(length);
    vga_write(" bytes)\n",VGA_COLOR_WHITE,VGA_COLOR_BLACK);
    pkg_state=PKG_IDLE;
}

void app_handle_key(uint16_t key) { if(!active)return; if(key==KEY_ESCAPE || key==KEY_INTERRUPT){app_exit_gui();return;} if(active->key)active->key(key); }
void app_handle_tick(uint32_t ticks) { if(active && active->tick)active->tick(ticks); }
void app_exit_gui(void) { gfx_shutdown(); vga_text_mode(); active=0; vga_clear(VGA_COLOR_WHITE,VGA_COLOR_BLACK); }
