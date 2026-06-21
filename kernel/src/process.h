#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

void process_init(void);
void process_run_user_demo(void);
void process_list(void);
uint32_t syscall_handler(uint32_t number, uint32_t argument);

#endif
