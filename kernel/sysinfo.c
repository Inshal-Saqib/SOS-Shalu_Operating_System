#include "sysinfo.h"
#include "../drivers/vga.h"
#include "../drivers/rtc.h"
#include "memory.h"
#include "uptime.h"

static void print_num(size_t n) {
    if(n==0){terminal_putchar('0');return;}
    char buf[20]; int i=0;
    while(n>0){buf[i++]='0'+n%10;n/=10;}
    while(i--) terminal_putchar(buf[i]);
}
static void print2(uint8_t n) {
    terminal_putchar('0'+n/10);
    terminal_putchar('0'+n%10);
}

/* Box is 76 chars wide to use full 80-col screen */
static void divider(void) {
    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_writeline("+--------------------------------------------------------------------------+");
}
static void empty_row(void) {
    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_writeline("|                                                                          |");
}
static void section(const char* title) {
    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_write("|  ");
    terminal_setcolor(VGA_LIGHT_BROWN, VGA_BLACK);
    terminal_write(title);
    terminal_putchar('\n');
}
static void row(const char* label, const char* val) {
    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_write("|    ");
    terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_write(label);
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
    terminal_writeline(val);
}

void sysinfo_print(void) {
    terminal_putchar('\n');
    divider();

    /* Title */
    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_writeline("|              SOS v1.0  -  Shalu Operating System  |  Status                  |");
    divider();
    empty_row();

    /* OS */
    section("  OS & Kernel                                                          |");
    row("OS Name      :  ", "SOS v1.0");
    row("Architecture :  ", "x86 32-bit Protected Mode");
    row("Kernel Type  :  ", "Monolithic (no external libraries)");
    row("Bootloader   :  ", "GRUB Multiboot Specification");
    row("Build Tools  :  ", "GCC -m32 -ffreestanding, NASM, LD");
    empty_row();

    /* CPU */
    section("  Processor                                                             |");
    row("CPU Family   :  ", "x86 (i386 compatible)");
    row("Mode         :  ", "32-bit Protected Mode");
    row("Stack Size   :  ", "16 KB (defined in boot.asm)");
    empty_row();

    /* Memory */
    size_t used, free_mem, total;
    memory_stats(&used, &free_mem, &total);

    section("  Memory                                                                |");

    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_write("|    ");
    terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_write("Heap Total   :  ");
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
    print_num(total/1024);
    terminal_writeline(" KB");

    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_write("|    ");
    terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_write("Heap Used    :  ");
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
    print_num(used/1024);
    terminal_writeline(" KB");

    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_write("|    ");
    terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_write("Heap Free    :  ");
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
    print_num(free_mem/1024);
    terminal_writeline(" KB");

    empty_row();

    /* Time */
    rtc_time_t t; rtc_read(&t);
    section("  Date & Time                                                           |");

    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_write("|    ");
    terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_write("Date         :  ");
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
    print2(t.day);   terminal_putchar('/');
    print2(t.month); terminal_write("/20");
    print2(t.year);  terminal_putchar('\n');

    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_write("|    ");
    terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_write("Time         :  ");
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
    print2(t.hour);   terminal_putchar(':');
    print2(t.minute); terminal_putchar(':');
    print2(t.second); terminal_putchar('\n');

    unsigned secs = uptime_seconds();
    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_write("|    ");
    terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_write("Uptime       :  ");
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
    print_num(secs/3600);       terminal_write(" hrs  ");
    print_num((secs%3600)/60);  terminal_write(" min  ");
    print_num(secs%60);         terminal_writeline(" sec");

    empty_row();

    /* Drivers */
    section("  Drivers & Features                                                    |");
    row("Display      :  ", "VGA Text Mode 80x25 @ 0xB8000");
    row("Keyboard     :  ", "PS/2 Controller - Polling Mode");
    row("Clock        :  ", "CMOS RTC via I/O ports 0x70/0x71");
    row("Memory Mgr   :  ", "Linked-list heap (kmalloc/kfree)");
    row("Shell        :  ", "SOS terminal with logbook & clipboard");
    row("GUI          :  ", "VGA text-mode menu (type 'gui')");
    row("Security     :  ", "SOS login guard (3 attempts)");
    row("Commands     :  ", "sos-help time date memstat memcheck");
    row("             :  ", "say status runtime compute splash desk");

    empty_row();
    divider();
    terminal_putchar('\n');
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
}
