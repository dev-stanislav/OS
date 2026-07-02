#include "syscall.h"

static syscall_handler_t handlers[SYSCALL_MAX];

static uint32_t sys_ping(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e) {
    (void)a; (void)b; (void)c; (void)d; (void)e;
    return 0x4D4F5301u;
}

void syscall_init(void) {
    for (uint8_t i=0;i<SYSCALL_MAX;i++) handlers[i] = 0;
    syscall_register(0, sys_ping);
}

uint8_t syscall_register(uint32_t number, syscall_handler_t handler) {
    if (number >= SYSCALL_MAX) return 0;
    handlers[number] = handler;
    return 1;
}

void syscall_dispatch(interrupt_frame_t *frame) {
    if (!frame) return;
    uint32_t number = frame->eax;
    if (number >= SYSCALL_MAX || !handlers[number]) {
        frame->eax = 0xFFFFFFFFu;
        return;
    }
    frame->eax = handlers[number](frame->ebx, frame->ecx, frame->edx, frame->esi, frame->edi);
}
