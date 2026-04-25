#include "uptime.h"
#include "../drivers/vga.h"
#include "../drivers/rtc.h"

/* We track uptime by storing boot time from RTC
   and comparing against current RTC time */

static uint8_t boot_hour   = 0;
static uint8_t boot_minute = 0;
static uint8_t boot_second = 0;
static uint8_t boot_day    = 0;
static uint8_t boot_month  = 0;
static uint8_t boot_year   = 0;

void uptime_init(void) {
    rtc_time_t t;
    rtc_read(&t);
    boot_hour   = t.hour;
    boot_minute = t.minute;
    boot_second = t.second;
    boot_day    = t.day;
    boot_month  = t.month;
    boot_year   = t.year;
}

void uptime_tick(void) {
    /* placeholder - uptime calculated from RTC diff */
}

unsigned uptime_seconds(void) {
    rtc_time_t t;
    rtc_read(&t);

    /* Convert boot time to total seconds */
    unsigned boot_total = (unsigned)boot_hour   * 3600
                        + (unsigned)boot_minute * 60
                        + (unsigned)boot_second;

    /* Convert current time to total seconds */
    unsigned now_total  = (unsigned)t.hour   * 3600
                        + (unsigned)t.minute * 60
                        + (unsigned)t.second;

    /* Handle midnight rollover */
    if(now_total < boot_total)
        now_total += 86400;

    return now_total - boot_total;
}

static void print_num(unsigned n) {
    if(n==0){terminal_putchar('0');return;}
    char buf[12]; int i=0;
    while(n>0){buf[i++]='0'+n%10;n/=10;}
    while(i--) terminal_putchar(buf[i]);
}
static void print2(unsigned n) {
    terminal_putchar('0'+n/10);
    terminal_putchar('0'+n%10);
}

void uptime_print(void) {
    unsigned secs  = uptime_seconds();
    unsigned hours = secs/3600;
    unsigned mins  = (secs%3600)/60;
    unsigned s     = secs%60;

    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_writeline("  SOS Runtime:");
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
    terminal_write("  ");
    print_num(hours);
    terminal_write(" hours, ");
    print_num(mins);
    terminal_write(" minutes, ");
    print_num(s);
    terminal_writeline(" seconds");

    terminal_setcolor(VGA_LIGHT_GREY, VGA_BLACK);
    terminal_write("  Boot time: ");
    print2(boot_hour);   terminal_putchar(':');
    print2(boot_minute); terminal_putchar(':');
    print2(boot_second);
    terminal_putchar('\n');
}
