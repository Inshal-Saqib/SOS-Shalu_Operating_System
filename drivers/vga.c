#include "vga.h"

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY (uint16_t*)0xB8000

static uint16_t* vga_buf;
static size_t    term_row;
static size_t    term_col;
static uint8_t   term_color;

static uint8_t make_color(vga_color fg, vga_color bg) {
    return fg | bg << 4;
}

static uint16_t make_entry(char c, uint8_t color) {
    return (uint16_t)c | (uint16_t)color << 8;
}

void terminal_init(void) {
    vga_buf    = VGA_MEMORY;
    term_row   = 0;
    term_col   = 0;
    term_color = make_color(VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_clear();
}

void terminal_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            vga_buf[y * VGA_WIDTH + x] = make_entry(' ', term_color);
    term_row = 0;
    term_col = 0;
}

void terminal_setcolor(uint8_t fg, uint8_t bg) {
    term_color = make_color((vga_color)fg, (vga_color)bg);
}

static void terminal_scroll(void) {
    for (size_t y = 1; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            vga_buf[(y-1) * VGA_WIDTH + x] = vga_buf[y * VGA_WIDTH + x];
    for (size_t x = 0; x < VGA_WIDTH; x++)
        vga_buf[(VGA_HEIGHT-1) * VGA_WIDTH + x] = make_entry(' ', term_color);
    term_row = VGA_HEIGHT - 1;
}

void terminal_putchar(char c) {
    if (c == '\n') {
        term_col = 0;
        if (++term_row == VGA_HEIGHT) terminal_scroll();
        return;
    }
    vga_buf[term_row * VGA_WIDTH + term_col] = make_entry(c, term_color);
    if (++term_col == VGA_WIDTH) {
        term_col = 0;
        if (++term_row == VGA_HEIGHT) terminal_scroll();
    }
}

void terminal_write(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++)
        terminal_putchar(str[i]);
}

void terminal_writeline(const char* str) {
    terminal_write(str);
    terminal_putchar('\n');
}
