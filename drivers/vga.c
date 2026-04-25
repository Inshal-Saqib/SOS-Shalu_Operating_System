#include "vga.h"
#include <stdint.h>

#define VGA_WIDTH   80
#define VGA_HEIGHT  25
#define VGA_MEMORY  ((uint16_t*)0xB8000)

/* Scroll buffer - stores 200 lines of history */
#define BUF_LINES   200
#define BUF_SIZE    (BUF_LINES * VGA_WIDTH)

static uint16_t screen_buf[BUF_SIZE];  /* full scroll buffer    */
static int      buf_lines   = 0;       /* total lines written   */
static int      scroll_top  = 0;       /* topmost visible line  */
static uint8_t  term_col    = 0;       /* current column        */
static uint8_t  term_color  = 0x07;

/* I/O for hardware cursor */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0,%1"::"a"(val),"Nd"(port));
}

/* ── Buffer helpers ─────────────────────────────────────────────────────── */
static uint16_t* buf_row(int line) {
    return &screen_buf[(line % BUF_LINES) * VGA_WIDTH];
}

static void buf_new_line(void) {
    /* Clear the new line */
    uint16_t blank = (uint16_t)' ' | ((uint16_t)term_color << 8);
    uint16_t* row = buf_row(buf_lines);
    for (int x = 0; x < VGA_WIDTH; x++) row[x] = blank;
    buf_lines++;
    term_col = 0;
    /* Auto-scroll: keep view at bottom */
    if (buf_lines > VGA_HEIGHT)
        scroll_top = buf_lines - VGA_HEIGHT;
    else
        scroll_top = 0;
}

static void buf_put(char c) {
    if (buf_lines == 0) buf_new_line();
    uint16_t entry = (uint16_t)c | ((uint16_t)term_color << 8);
    buf_row(buf_lines - 1)[term_col] = entry;
    term_col++;
    if (term_col >= VGA_WIDTH) buf_new_line();
}

/* ── Render visible window to VGA ───────────────────────────────────────── */
static void render(void) {
    uint16_t* vga = VGA_MEMORY;
    uint16_t blank = (uint16_t)' ' | ((uint16_t)term_color << 8);

    for (int row = 0; row < VGA_HEIGHT; row++) {
        int line = scroll_top + row;
        if (line < buf_lines) {
            uint16_t* src = buf_row(line);
            for (int x = 0; x < VGA_WIDTH; x++)
                vga[row * VGA_WIDTH + x] = src[x];
        } else {
            for (int x = 0; x < VGA_WIDTH; x++)
                vga[row * VGA_WIDTH + x] = blank;
        }
    }

    /* Update hardware cursor to current write position */
    int cur_row = (buf_lines - 1) - scroll_top;
    if (cur_row >= 0 && cur_row < VGA_HEIGHT) {
        uint16_t pos = (uint16_t)(cur_row * VGA_WIDTH + term_col);
        outb(0x3D4, 0x0F); outb(0x3D5, (uint8_t)(pos & 0xFF));
        outb(0x3D4, 0x0E); outb(0x3D5, (uint8_t)(pos >> 8));
    }
}

/* ── Public API ─────────────────────────────────────────────────────────── */
void terminal_init(void) {
    buf_lines  = 0;
    scroll_top = 0;
    term_col   = 0;
    term_color = 0x07;
    /* Clear buffer */
    for (int i = 0; i < BUF_SIZE; i++)
        screen_buf[i] = (uint16_t)' ' | ((uint16_t)term_color << 8);
    buf_new_line();
    render();
}

void terminal_clear(void) {
    buf_lines  = 0;
    scroll_top = 0;
    term_col   = 0;
    for (int i = 0; i < BUF_SIZE; i++)
        screen_buf[i] = (uint16_t)' ' | ((uint16_t)term_color << 8);
    buf_new_line();
    render();
}

void terminal_setcolor(uint8_t fg, uint8_t bg) {
    term_color = (uint8_t)(fg | (bg << 4));
}

void terminal_putchar(char c) {
    if (c == '\n') {
        buf_new_line();
        render();
        return;
    }
    if (c == '\b') {
        if (term_col > 0) {
            term_col--;
            uint16_t blank = (uint16_t)' ' | ((uint16_t)term_color << 8);
            buf_row(buf_lines - 1)[term_col] = blank;
            render();
        }
        return;
    }
    if (c == '\r') { term_col = 0; render(); return; }
    if (c < 32) return;
    buf_put(c);
    render();
}

void terminal_write(const char* str) {
    for (int i = 0; str[i]; i++) terminal_putchar(str[i]);
}

void terminal_writeline(const char* str) {
    terminal_write(str);
    terminal_putchar('\n');
}

void terminal_erase_char(void) {
    if (term_col > 0) {
        term_col--;
        uint16_t blank = (uint16_t)' ' | ((uint16_t)term_color << 8);
        buf_row(buf_lines - 1)[term_col] = blank;
        render();
    }
}

/* ── Scroll API (called from kernel readline on PgUp/PgDn) ─────────────── */
void terminal_scroll_up(int lines) {
    scroll_top -= lines;
    if (scroll_top < 0) scroll_top = 0;
    render();
}

void terminal_scroll_down(int lines) {
    int max = buf_lines - VGA_HEIGHT;
    if (max < 0) max = 0;
    scroll_top += lines;
    if (scroll_top > max) scroll_top = max;
    render();
}

void terminal_scroll_bottom(void) {
    int max = buf_lines - VGA_HEIGHT;
    scroll_top = max < 0 ? 0 : max;
    render();
}
