#include "proc.h"
#include "libk.h"
#include "timer.h"
#include "vga.h"

#define PROC_MAX 12
#define PROC_NAME_MAX 24

typedef struct {
    uint8_t used;
    uint8_t protected;
    uint16_t pid;
    uint32_t started_ticks;
    uint32_t last_seen_ticks;
    char name[PROC_NAME_MAX + 1];
} proc_entry_t;

static proc_entry_t processes[PROC_MAX];
static uint16_t next_pid;

static void print_number(uint32_t value) {
    char text[16];
    kitoa(value, text, 10);
    vga_write(text, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}

static proc_entry_t *find_free(void) {
    for (uint8_t i = 0; i < PROC_MAX; i++) if (!processes[i].used) return &processes[i];
    return 0;
}

static proc_entry_t *find_pid(uint16_t pid) {
    for (uint8_t i = 0; i < PROC_MAX; i++) if (processes[i].used && processes[i].pid == pid) return &processes[i];
    return 0;
}

static void create_process(uint16_t pid, const char *name, uint8_t protected) {
    proc_entry_t *process = find_free();
    if (!process) return;
    process->used = 1;
    process->protected = protected;
    process->pid = pid;
    process->started_ticks = timer_ticks();
    process->last_seen_ticks = process->started_ticks;
    kstrcpy(process->name, name, sizeof(process->name));
}

void proc_init(void) {
    kmemset(processes, 0, sizeof(processes));
    next_pid = 2;
    create_process(1, "shell", 1);
}

uint16_t proc_spawn(const char *name) {
    proc_entry_t *process = find_free();
    if (!process || !name || !*name) return 0;
    uint16_t pid = next_pid++;
    if (next_pid == 0) next_pid = 2;
    process->used = 1;
    process->protected = 0;
    process->pid = pid;
    process->started_ticks = timer_ticks();
    process->last_seen_ticks = process->started_ticks;
    kstrcpy(process->name, name, sizeof(process->name));
    return pid;
}

proc_result_t proc_kill(uint16_t pid) {
    proc_entry_t *process = find_pid(pid);
    if (!process) return PROC_RESULT_NOT_FOUND;
    if (process->protected) return PROC_RESULT_PROTECTED;
    kmemset(process, 0, sizeof(*process));
    return PROC_RESULT_OK;
}

void proc_list(void) {
    uint32_t now = timer_ticks();
    vga_write("PID  STATE    TIME  NAME\n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    for (uint8_t i = 0; i < PROC_MAX; i++) {
        if (!processes[i].used) continue;
        print_number(processes[i].pid);
        if (processes[i].pid < 10) vga_write("    ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        else if (processes[i].pid < 100) vga_write("   ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        else vga_write("  ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_write("running  ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        print_number((now - processes[i].started_ticks) / TIMER_HZ);
        vga_write("s", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        if (((now - processes[i].started_ticks) / TIMER_HZ) < 10) vga_write("     ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        else if (((now - processes[i].started_ticks) / TIMER_HZ) < 100) vga_write("    ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        else vga_write("   ", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_write(processes[i].name, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_write("\n", VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    }
}

void proc_poll(void) {
    uint32_t now = timer_ticks();
    for (uint8_t i = 0; i < PROC_MAX; i++) if (processes[i].used) processes[i].last_seen_ticks = now;
}
