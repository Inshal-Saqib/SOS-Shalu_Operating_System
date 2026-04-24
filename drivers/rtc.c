#include "rtc.h"
#include "vga.h"
#include <stdint.h>

static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static uint8_t bcd_to_bin(uint8_t bcd) {
    return (bcd & 0x0F) + ((bcd >> 4) * 10);
}

static uint8_t rtc_reg(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

static int is_updating(void) {
    outb(0x70, 0x0A);
    return inb(0x71) & 0x80;
}

void rtc_read(rtc_time_t* t) {
    while (is_updating());
    t->second = bcd_to_bin(rtc_reg(0x00));
    t->minute = bcd_to_bin(rtc_reg(0x02));
    t->hour   = bcd_to_bin(rtc_reg(0x04));
    t->day    = bcd_to_bin(rtc_reg(0x07));
    t->month  = bcd_to_bin(rtc_reg(0x08));
    t->year   = bcd_to_bin(rtc_reg(0x09));
}

static void print2(uint8_t n) {
    terminal_putchar('0' + n / 10);
    terminal_putchar('0' + n % 10);
}

static void print_num(int n) {
    if (n == 0) { terminal_putchar('0'); return; }
    char buf[10]; int i = 0;
    while (n > 0) { buf[i++] = '0' + n % 10; n /= 10; }
    while (i--) terminal_putchar(buf[i]);
}

static const char* month_name(uint8_t m) {
    const char* months[] = {
        "???","January","February","March","April","May","June",
        "July","August","September","October","November","December"
    };
    return (m >= 1 && m <= 12) ? months[m] : months[0];
}

static const char* day_name(int d) {
    const char* days[] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
    return days[d % 7];
}

/* Zeller's congruence to get day of week */
static int day_of_week(int d, int m, int y) {
    if (m < 3) { m += 12; y--; }
    int k = y % 100, j = y / 100;
    int h = (d + (13*(m+1))/5 + k + k/4 + j/4 + 5*j) % 7;
    return (h + 6) % 7;
}

static int days_in_month(int m, int y) {
    int days[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    if (m == 2 && (y % 4 == 0)) return 29;
    return days[m];
}

void rtc_print_time(void) {
    rtc_time_t t;
    rtc_read(&t);
    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_write("  Current Time : ");
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
    print2(t.hour); terminal_putchar(':');
    print2(t.minute); terminal_putchar(':');
    print2(t.second);
    terminal_write("  (");
    print2(t.day); terminal_putchar('/');
    print2(t.month); terminal_write("/20");
    print2(t.year);
    terminal_writeline(")");
}

void rtc_print_date(void) {
    rtc_time_t t;
    rtc_read(&t);
    int year = 2000 + t.year;
    int dow  = day_of_week(t.day, t.month, year);

    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_writeline("  Current Date:");
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
    terminal_write("  ");
    terminal_write(day_name(dow));
    terminal_write(", ");
    terminal_write(month_name(t.month));
    terminal_putchar(' ');
    print_num(t.day);
    terminal_write(", 20");
    print2(t.year);
    terminal_putchar('\n');
}

void rtc_print_calendar(void) {
    rtc_time_t t;
    rtc_read(&t);
    int year  = 2000 + t.year;
    int month = t.month;
    int today = t.day;
    int dim   = days_in_month(month, year);
    int start = day_of_week(1, month, year);

    terminal_putchar('\n');
    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_write("      ");
    terminal_write(month_name(month));
    terminal_write(" ");
    print_num(year);
    terminal_putchar('\n');
    terminal_setcolor(VGA_LIGHT_GREY, VGA_BLACK);
    terminal_writeline("  Su Mo Tu We Th Fr Sa");

    terminal_setcolor(VGA_WHITE, VGA_BLACK);
    terminal_write("  ");
    for (int i = 0; i < start; i++) terminal_write("   ");

    for (int d = 1; d <= dim; d++) {
        int col = (start + d - 1) % 7;

        if (d == today) {
            terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
        } else {
            terminal_setcolor(VGA_WHITE, VGA_BLACK);
        }

        if (d < 10) terminal_putchar(' ');
        print_num(d);

        if (col == 6) {
            terminal_putchar('\n');
            terminal_write("  ");
        } else {
            terminal_putchar(' ');
        }
    }
    terminal_putchar('\n');
    terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_writeline("  (today is highlighted)");
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
}
