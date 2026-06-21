#ifndef APP_H
#define APP_H

#include <stdint.h>

typedef void (*app_start_fn)(char **args, uint8_t count);
typedef void (*app_key_fn)(uint16_t key);
typedef void (*app_tick_fn)(uint32_t ticks);

typedef struct {
    const char *id;
    const char *name;
    const char *version;
    const char *description;
    app_start_fn start;
    app_key_fn key;
    app_tick_fn tick;
} app_package_t;

void app_init(void);
void app_list(uint8_t installed_only);
void app_info(const char *id);
void app_install(const char *id);
void app_remove(const char *id);
void app_run(const char *id, char **args, uint8_t count);
void app_set_workdir(int directory);
int app_get_workdir(void);
uint8_t app_is_active(void);
void app_handle_key(uint16_t key);
void app_handle_tick(uint32_t ticks);
void app_exit_gui(void);

#endif
