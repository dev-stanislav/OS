#include "app.h"
#include "../fs.h"
#include "../keyboard.h"
#include "../libk.h"
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
#include "../../../packages/catalog.def"
};
#undef PACKAGE

#define PACKAGE_COUNT (sizeof(packages) / sizeof(packages[0]))
static uint8_t installed[PACKAGE_COUNT];
static const app_package_t *active;
static int working_dir;

static void package_path(char *path, const char *id) {
    kstrcpy(path, "/apps/", 40);
    kstrcpy(path + kstrlen(path), id, 40 - kstrlen(path));
    kstrcpy(path + kstrlen(path), ".pkg", 40 - kstrlen(path));
}

static int package_index(const char *id) {
    for (uint8_t i=0;i<PACKAGE_COUNT;i++) if (kstrcmp(packages[i].id,id)==0) return i;
    return -1;
}

static void status(const char *message, uint8_t color) { vga_write(message, color, VGA_COLOR_BLACK); vga_write("\n", color, VGA_COLOR_BLACK); }

void app_init(void) {
    active=0; working_dir=fs_root();
    (void)fs_create("/apps",FS_DIR,fs_root());
    for (uint8_t i=0;i<PACKAGE_COUNT;i++) { char path[40]; package_path(path,packages[i].id); installed[i]=fs_resolve(path,fs_root())>=0; if(kstrcmp(packages[i].id,"tbf")==0||kstrcmp(packages[i].id,"sproot")==0||kstrcmp(packages[i].id,"free")==0||kstrcmp(packages[i].id,"paint")==0||kstrcmp(packages[i].id,"terminal")==0)installed[i]=1; }
}

void app_set_workdir(int directory) { working_dir = directory; }
int app_get_workdir(void) { return working_dir; }

void app_list(uint8_t installed_only) {
    for (uint8_t i=0;i<PACKAGE_COUNT;i++) if (!installed_only || installed[i]) {
        vga_write(installed[i] ? "[installed] " : "[available] ", installed[i] ? VGA_COLOR_LIGHT_GREEN : VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        vga_write(packages[i].id, VGA_COLOR_WHITE, VGA_COLOR_BLACK); vga_write(" - ",VGA_COLOR_WHITE,VGA_COLOR_BLACK);
        vga_write(packages[i].description,VGA_COLOR_LIGHT_GREY,VGA_COLOR_BLACK); vga_write("\n",VGA_COLOR_WHITE,VGA_COLOR_BLACK);
    }
}

void app_info(const char *id) {
    int index=package_index(id); if(index<0){status("package not found",VGA_COLOR_LIGHT_RED);return;}
    const app_package_t *p=&packages[index];
    vga_write(p->name,VGA_COLOR_LIGHT_CYAN,VGA_COLOR_BLACK);vga_write(" ",VGA_COLOR_WHITE,VGA_COLOR_BLACK);vga_write(p->version,VGA_COLOR_WHITE,VGA_COLOR_BLACK);vga_write("\n",VGA_COLOR_WHITE,VGA_COLOR_BLACK);
    vga_write(p->description,VGA_COLOR_WHITE,VGA_COLOR_BLACK);vga_write("\n",VGA_COLOR_WHITE,VGA_COLOR_BLACK);
    vga_write(installed[index]?"status: installed\n":"status: available\n",installed[index]?VGA_COLOR_LIGHT_GREEN:VGA_COLOR_LIGHT_BROWN,VGA_COLOR_BLACK);
}

void app_install(const char *id) {
    int index=package_index(id); if(index<0){status("package not found",VGA_COLOR_LIGHT_RED);return;} if(installed[index]){status("already installed",VGA_COLOR_LIGHT_BROWN);return;}
    char path[40]; package_path(path,id);
    fs_result_t result=fs_write(path,"MiniPkg metadata",fs_root()); if(result!=FS_OK){status("cannot install package",VGA_COLOR_LIGHT_RED);return;}
    installed[index]=1; status("installed",VGA_COLOR_LIGHT_GREEN);
}

void app_remove(const char *id) {
    int index=package_index(id); if(index<0){status("package not found",VGA_COLOR_LIGHT_RED);return;} if(kstrcmp(id,"tbf")==0||kstrcmp(id,"sproot")==0||kstrcmp(id,"free")==0||kstrcmp(id,"paint")==0||kstrcmp(id,"terminal")==0){status("built-in app cannot be removed",VGA_COLOR_LIGHT_BROWN);return;} if(!installed[index]){status("not installed",VGA_COLOR_LIGHT_BROWN);return;}
    char path[40]; package_path(path,id);
    (void)fs_remove(path,fs_root(),0); installed[index]=0; status("removed",VGA_COLOR_LIGHT_GREEN);
}

void app_run(const char *id, char **args, uint8_t count) {
    int index=package_index(id); if(index<0){status("package not found",VGA_COLOR_LIGHT_RED);return;} if(!installed[index]){status("package is not installed",VGA_COLOR_LIGHT_RED);return;}
    active=&packages[index]; active->start(args,count);
    if (!active->key && !active->tick) active=0;
}

uint8_t app_is_active(void) { return active != 0; }
void app_handle_key(uint16_t key) { if(!active)return; if(key==KEY_ESCAPE || key==KEY_INTERRUPT){app_exit_gui();return;} if(active->key)active->key(key); }
void app_handle_tick(uint32_t ticks) { if(active && active->tick)active->tick(ticks); }
void app_exit_gui(void) { gfx_shutdown(); active=0; vga_clear(VGA_COLOR_WHITE,VGA_COLOR_BLACK); }
