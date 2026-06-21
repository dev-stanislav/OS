#include <stdint.h>
#include "console.h"
#include "keyboard.h"
#include "vga.h"

#define COMMAND_MAX 63

static char command[COMMAND_MAX + 1];
static uint8_t command_length;
static uint8_t game_active;
static uint8_t secret;
static uint8_t games_played;

static int equals(const char *left, const char *right) {
    while (*left && *right && *left == *right) {
        left++;
        right++;
    }
    return *left == *right;
}

static void prompt(void) {
    vga_write(game_active ? "guess> " : "minios> ", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
}

static void print_help(void) {
    vga_write("Commands: help, clear, about, game\n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write("In game: type a number 1-9, or exit.\n", VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

static void run_command(void) {
    if (game_active) {
        if (equals(command, "exit")) {
            game_active = 0;
            vga_write("Game ended.\n", VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
        } else if (command[1] == '\0' && command[0] >= '1' && command[0] <= '9') {
            uint8_t guess = (uint8_t)(command[0] - '0');
            if (guess == secret) {
                vga_write("You won! Tiny kernel, big brain.\n", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
                game_active = 0;
            } else if (guess < secret) {
                vga_write("Too low. Try again.\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            } else {
                vga_write("Too high. Try again.\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            }
        } else {
            vga_write("Enter one number from 1 to 9, or exit.\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        }
    } else if (equals(command, "help")) {
        print_help();
    } else if (equals(command, "clear")) {
        vga_clear(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    } else if (equals(command, "about")) {
        vga_write("MiniOS: a tiny 32-bit x86 kernel.\n", VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    } else if (equals(command, "game")) {
        games_played++;
        secret = (uint8_t)(((games_played * 3) % 9) + 1);
        game_active = 1;
        vga_write("Guess the number from 1 to 9. Type exit to leave.\n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    } else if (command_length != 0) {
        vga_write("Unknown command. Type help.\n", VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    }
}

void console_init(void) {
    vga_write("MiniOS shell — type help.\n", VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    prompt();
}

void console_update(void) {
    char character = keyboard_read_char();
    if (character == 0 || character == '\t') return;

    if (character == '\n') {
        vga_newline();
        command[command_length] = '\0';
        run_command();
        command_length = 0;
        prompt();
    } else if (character == '\b') {
        if (command_length > 0) {
            command_length--;
            vga_backspace(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        }
    } else if (command_length < COMMAND_MAX) {
        command[command_length++] = character;
        vga_putchar(character, VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    }
}
