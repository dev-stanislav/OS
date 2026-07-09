#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct shell_context shell_context_t;

typedef void (*shell_write_fn)(shell_context_t *context, const char *text);
typedef void (*shell_clear_fn)(shell_context_t *context);
typedef uint8_t (*shell_launch_fn)(shell_context_t *context, const char *id, char **args, uint8_t count);
typedef void (*shell_reboot_fn)(shell_context_t *context);

typedef enum {
    SHELL_RESULT_OK,
    SHELL_RESULT_EXIT
} shell_result_t;

struct shell_context {
    int current_dir;
    uint8_t gui;
    void *user;
    shell_write_fn write;
    shell_clear_fn clear;
    shell_launch_fn launch;
    shell_reboot_fn reboot;
};

shell_result_t shell_execute(shell_context_t *context, char *command);
uint8_t shell_text_eq(const char *left, const char *right);

#ifdef __cplusplus
}
#endif

#endif
