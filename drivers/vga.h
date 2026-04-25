#ifndef VGA_DRIVER_H
#define VGA_DRIVER_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    VGA_BLACK = 0, VGA_BLUE, VGA_GREEN, VGA_CYAN,
    VGA_RED, VGA_MAGENTA, VGA_BROWN, VGA_LIGHT_GREY,
    VGA_DARK_GREY, VGA_LIGHT_BLUE, VGA_LIGHT_GREEN,
    VGA_LIGHT_CYAN, VGA_LIGHT_RED, VGA_LIGHT_MAGENTA,
    VGA_LIGHT_BROWN, VGA_WHITE,
} vga_color;

void terminal_init(void);
void terminal_clear(void);
void terminal_setcolor(uint8_t fg, uint8_t bg);
void terminal_putchar(char c);
void terminal_write(const char* str);
void terminal_writeline(const char* str);
void terminal_erase_char(void);
void terminal_scroll_up(int lines);
void terminal_scroll_down(int lines);
void terminal_scroll_bottom(void);

#endif
