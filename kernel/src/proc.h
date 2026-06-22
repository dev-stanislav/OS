#ifndef PROC_H
#define PROC_H

#include <stdint.h>

typedef enum {
    PROC_RESULT_OK,
    PROC_RESULT_FULL,
    PROC_RESULT_NOT_FOUND,
    PROC_RESULT_PROTECTED
} proc_result_t;

void proc_init(void);
uint16_t proc_spawn(const char *name);
proc_result_t proc_kill(uint16_t pid);
void proc_list(void);
void proc_poll(void);

#endif
