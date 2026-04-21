#include "../drivers/vga.h"
#include "../drivers/keyboard.h"

#define MAX_CMD 80

static int strcmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a - *b;
}

static void readline(char* buf, int max) {
    int i = 0;
    while (i < max - 1) {
        char c = keyboard_getchar();
        if (c == '\n') break;
        if (c == '\b') {
            if (i > 0) {
                i--; buf[i] = '\0';
                terminal_putchar('\b');
                terminal_putchar(' ');
                terminal_putchar('\b');
            }
        } else {
            buf[i++] = c;
            terminal_putchar(c);
        }
    }
    buf[i] = '\0';
    terminal_putchar('\n');
}

static void run_command(const char* cmd) {
    if (strcmp(cmd, "help") == 0) {
        terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
        terminal_writeline("Available commands:");
        terminal_setcolor(VGA_WHITE, VGA_BLACK);
        terminal_writeline("  help    - Show this help");
        terminal_writeline("  clear   - Clear the screen");
        terminal_writeline("  about   - About this OS");
        terminal_writeline("  halt    - Halt the system");
    } else if (strcmp(cmd, "clear") == 0) {
        terminal_clear();
    } else if (strcmp(cmd, "about") == 0) {
        terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
        terminal_writeline("MyOS v0.1 - Built from scratch in C & Assembly");
        terminal_writeline("Features: VGA, Keyboard polling, Mini Shell");
        terminal_setcolor(VGA_WHITE, VGA_BLACK);
    } else if (strcmp(cmd, "halt") == 0) {
        terminal_setcolor(VGA_LIGHT_RED, VGA_BLACK);
        terminal_writeline("System halting...");
        __asm__ volatile("cli; hlt");
    } else if (cmd[0] == '\0') {
    } else {
        terminal_setcolor(VGA_LIGHT_RED, VGA_BLACK);
        terminal_write("Unknown command: ");
        terminal_writeline(cmd);
        terminal_setcolor(VGA_WHITE, VGA_BLACK);
        terminal_writeline("Type 'help' for available commands.");
    }
}

void kernel_main(void) {
    terminal_init();

    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_writeline("============================================");
    terminal_writeline("         Welcome to MyOS v0.1              ");
    terminal_writeline("============================================");
    terminal_putchar('\n');

    terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_writeline("[OK] Kernel loaded");
    terminal_writeline("[OK] VGA driver ready");

    keyboard_init();
    terminal_writeline("[OK] Keyboard ready (polling mode)");
    terminal_putchar('\n');

    terminal_setcolor(VGA_WHITE, VGA_BLACK);
    terminal_writeline("Type 'help' to see available commands.");
    terminal_putchar('\n');

    char cmd[MAX_CMD];
    while (1) {
        terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
        terminal_write("myos> ");
        terminal_setcolor(VGA_WHITE, VGA_BLACK);
        readline(cmd, MAX_CMD);
        run_command(cmd);
    }
}
