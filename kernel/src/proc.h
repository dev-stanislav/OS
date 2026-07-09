#ifndef PROC_H
#define PROC_H

#include <stdint.h>

typedef enum {
    PROC_RESULT_OK,
    PROC_RESULT_FULL,
    PROC_RESULT_NOT_FOUND,
    PROC_RESULT_PROTECTED
} proc_result_t;

#define PROC_NAME_MAX 24

typedef struct {
    uint8_t is_protected;
    uint16_t pid;
    uint32_t started_ticks;
    char name[PROC_NAME_MAX + 1];
} proc_info_t;

void proc_init(void);
uint16_t proc_spawn(const char *name);
proc_result_t proc_kill(uint16_t pid);
void proc_list(void);
uint8_t proc_snapshot(proc_info_t *out, uint8_t maximum);
void proc_poll(void);

#endif
