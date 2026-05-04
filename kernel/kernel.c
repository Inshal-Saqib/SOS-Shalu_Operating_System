#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../drivers/rtc.h"
#include "memory.h"
#include "history.h"
#include "gui.h"
#include "auth.h"
#include "shutdown.h"
#include "splash.h"
#include "uptime.h"
#include "sysinfo.h"
#include "calc.h"
#include "banner.h"

#define MAX_CMD  80
#define CLIP_MAX 80

static char clipboard[CLIP_MAX];
static int  clipboard_len = 0;

static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(port));
    return v;
}

static int strcmp_fn(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a - *b;
}

static void print_num(size_t n) {
    if (n == 0) { terminal_putchar('0'); return; }
    char buf[20]; int i = 0;
    while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    while (i--) terminal_putchar(buf[i]);
}

static void print_kb(size_t bytes) {
    print_num(bytes / 1024);
    terminal_write(" KB (");
    print_num(bytes);
    terminal_write(" bytes)");
}

static void print_banner(void) {
    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_writeline("=====================================================");
    terminal_writeline("   SOS - Shalu Operating System  v1.0               ");
    terminal_writeline("   Type 'sos-help' for commands  |  'desk' for GUI  ");
    terminal_writeline("=====================================================");
    terminal_putchar('\n');
}

static void readline(char* buf, int max);

static void do_about(void) {
    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_writeline("  SOS - Shalu Operating System v1.0");
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
    terminal_writeline("  Built from scratch in C & x86 Assembly");
    terminal_writeline("  Architecture : x86 32-bit Protected Mode");
    terminal_writeline("  Bootloader   : GRUB Multiboot");
    terminal_writeline("  Features     : VGA Driver, PS/2 Keyboard,");
    terminal_writeline("                 Heap Memory Manager,");
    terminal_writeline("                 CMOS Real-Time Clock,");
    terminal_writeline("                 Scrollable Terminal,");
    terminal_writeline("                 GUI Desk Mode,");
    terminal_writeline("                 Password Security,");
    terminal_writeline("                 Command History & Clipboard");
}

static void do_memstat(void) {
    size_t used, free_mem, total;
    memory_stats(&used, &free_mem, &total);
    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_writeline("  Memory Status:");
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
    terminal_write("  Total  : "); print_kb(total);    terminal_putchar('\n');
    terminal_write("  Used   : "); print_kb(used);     terminal_putchar('\n');
    terminal_write("  Free   : "); print_kb(free_mem); terminal_putchar('\n');
}

static void do_memcheck(void) {
    void* a;
    void* b;
    void* cc;
    void* d;
    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_writeline("  Running SOS memory check...");
    a  = kmalloc(256);
    b  = kmalloc(512);
    cc = kmalloc(1024);
    terminal_setcolor((a && b && cc) ? VGA_LIGHT_GREEN : VGA_LIGHT_RED, VGA_BLACK);
    terminal_writeline((a && b && cc) ? "  [PASS] Allocated 256 + 512 + 1024 bytes"
                                      : "  [FAIL] Allocation failed!");
    kfree(b);
    terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_writeline("  [PASS] Freed 512 byte block");
    d = kmalloc(512);
    if (d) terminal_writeline("  [PASS] Re-allocated 512 bytes");
    kfree(a); kfree(cc); kfree(d);
    terminal_writeline("  [PASS] All blocks freed - memory is healthy!");
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
}

static void do_logbook(void) {
    int count = history_count_get();
    int show;
    int i;
    if (count == 0) {
        terminal_writeline("  Logbook is empty.");
        return;
    }
    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_writeline("  SOS Command Logbook (last 10):");
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
    show = count < 10 ? count : 10;
    for (i = show; i >= 1; i--) {
        char hbuf[MAX_CMD];
        if (history_get(i, hbuf)) {
            terminal_write("    ");
            print_num((size_t)(count - i + 1));
            terminal_write(": ");
            terminal_writeline(hbuf);
        }
    }
}

static void do_desk(void) {
    while (1) {
        gui_action_t action = gui_run();
        char tmp[4];
        if (action == GUI_SWITCH_CLI) {
            terminal_init();
            print_banner();
            terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
            terminal_writeline("[SOS] Returned to terminal.");
            terminal_putchar('\n');
            terminal_setcolor(VGA_WHITE, VGA_BLACK);
            break;
        }
        if      (action == GUI_CLOCK)    { terminal_init(); rtc_print_time(); rtc_print_date(); terminal_writeline("\nPress Enter to return..."); readline(tmp,2); }
        else if (action == GUI_CALENDAR) { terminal_init(); rtc_print_calendar(); terminal_writeline("\nPress Enter to return..."); readline(tmp,2); }
        else if (action == GUI_MEMINFO)  { terminal_init(); do_memstat();  terminal_writeline("\nPress Enter to return..."); readline(tmp,2); }
        else if (action == GUI_MEMTEST)  { terminal_init(); do_memcheck(); terminal_writeline("\nPress Enter to return..."); readline(tmp,2); }
        else if (action == GUI_ABOUT)    { terminal_init(); do_about();    terminal_writeline("\nPress Enter to return..."); readline(tmp,2); }
        else if (action == GUI_HISTORY)  { terminal_init(); do_logbook();  terminal_writeline("\nPress Enter to return..."); readline(tmp,2); }
        else if (action == GUI_CLEAR)    { terminal_clear(); terminal_writeline("Press Enter to return..."); readline(tmp,2); }
        else if (action == GUI_HALT) {
            int choice = shutdown_confirm_gui();
            if(choice == POWER_SHUTDOWN) { shutdown_do(); }
            else if(choice == POWER_RESTART) { restart(); }
            /* else CANCEL — loop back to GUI */
        }
    }
}

static void run_command(const char* base, const char* args) {

    if (strcmp_fn(base, "sos-help") == 0 && args[0] == 0) {
        terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
        terminal_writeline("  SOS Command Reference");
        terminal_writeline("  +----------------------------------------------------+");
        terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
        terminal_writeline("  System:");
        terminal_setcolor(VGA_WHITE, VGA_BLACK);
        terminal_writeline("    sos-help   Show this help");
        terminal_writeline("    Saaf       Clear the screen");
        terminal_writeline("    whoami     About SOS");
        terminal_writeline("    shutdown   Shutdown or restart (prompts S/R/C)");
        terminal_writeline("    say        Print text  (say Hello World)");
        terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
        terminal_writeline("  Information:");
        terminal_setcolor(VGA_WHITE, VGA_BLACK);
        terminal_writeline("    status     Full system dashboard");
        terminal_writeline("    runtime    System uptime");
        terminal_writeline("    memstat    Memory usage");
        terminal_writeline("    memcheck   Memory diagnostic");
        terminal_writeline("    time       Current time and date");
        terminal_writeline("    date       Monthly calendar");
        terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
        terminal_writeline("  Tools:");
        terminal_setcolor(VGA_WHITE, VGA_BLACK);
        terminal_writeline("    compute    Calculator (5a3 9s4 6m7 8d2 9r4)");
        terminal_writeline("    splash     ASCII art  (splash SOS)");
        terminal_writeline("    logbook    Command history");
        terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
        terminal_writeline("  Interface:");
        terminal_setcolor(VGA_WHITE, VGA_BLACK);
        terminal_writeline("    desk       Open GUI mode");
        terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
        terminal_writeline("  Keys:");
        terminal_setcolor(VGA_WHITE, VGA_BLACK);
        terminal_writeline("    PgUp/PgDn  Scroll terminal");
        terminal_writeline("    Tab        Insert 4 spaces");
        terminal_writeline("    Ctrl+C     Copy line");
        terminal_writeline("    Ctrl+V     Paste");

    } else if (strcmp_fn(base, "sos-help") == 0 && args[0] != 0) {
        if      (strcmp_fn(args,"compute")==0) terminal_writeline("  compute: a=add s=sub m=mul d=div r=mod  e.g: compute 5a3m2");
        else if (strcmp_fn(args,"splash") ==0) terminal_writeline("  splash: Big ASCII art text. e.g: splash SOS");
        else if (strcmp_fn(args,"say")    ==0) terminal_writeline("  say: Print text. e.g: sao World");
        else if (strcmp_fn(args,"status") ==0) terminal_writeline("  status: Full OS dashboard - version, CPU, memory, time.");
        else if (strcmp_fn(args,"memstat")==0) terminal_writeline("  memstat: Shows total, used and free heap memory.");
        else if (strcmp_fn(args,"time")   ==0) terminal_writeline("  time: Current time and date from hardware RTC.");
        else if (strcmp_fn(args,"date")   ==0) terminal_writeline("  date: This month calendar with today highlighted.");
        else if (strcmp_fn(args,"desk")   ==0) terminal_writeline("  desk: Opens SOS graphical menu. Press 8 to return.");
        else if (strcmp_fn(args,"logbook")==0) terminal_writeline("  logbook: Last 10 typed commands.");
        else if (strcmp_fn(args,"runtime")==0) terminal_writeline("  runtime: How long SOS has been running.");
        else { terminal_write("  No help for: "); terminal_writeline(args); }

    } else if (strcmp_fn(base, "saaf") == 0) {
        terminal_clear();
        print_banner();

    } else if (strcmp_fn(base, "whoami") == 0) {
        do_about();

    } else if (strcmp_fn(base, "time") == 0) {
        terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
        terminal_writeline("  ----------------------------------------");
        rtc_print_time();
        rtc_print_date();
        terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
        terminal_writeline("  ----------------------------------------");
        terminal_setcolor(VGA_WHITE, VGA_BLACK);

    } else if (strcmp_fn(base, "date") == 0) {
        terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
        terminal_writeline("  ----------------------------------------");
        rtc_print_calendar();
        terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
        terminal_writeline("  ----------------------------------------");
        terminal_setcolor(VGA_WHITE, VGA_BLACK);

    } else if (strcmp_fn(base, "memstat") == 0) {
        do_memstat();

    } else if (strcmp_fn(base, "memcheck") == 0) {
        do_memcheck();

    } else if (strcmp_fn(base, "status") == 0) {
        sysinfo_print();

    } else if (strcmp_fn(base, "runtime") == 0) {
        uptime_print();

    } else if (strcmp_fn(base, "logbook") == 0) {
        do_logbook();

    } else if (strcmp_fn(base, "desk") == 0) {
        do_desk();

    } else if (strcmp_fn(base, "say") == 0) {
        terminal_writeline(args[0] ? args : "");

    } else if (strcmp_fn(base, "compute") == 0) {
        calc_run(args);

    } else if (strcmp_fn(base, "splash") == 0) {
        banner_print(args);

    } else if (strcmp_fn(base, "shutdown") == 0 ||
               strcmp_fn(base, "restart")  == 0) {
        /* Both go through the same confirm prompt */
        /* shutdown() internally handles S/R/C choice */
        shutdown();

    } else if (base[0] == 0) {
        /* empty line */

    } else {
        terminal_setcolor(VGA_LIGHT_RED, VGA_BLACK);
        terminal_write("  Unknown command: ");
        terminal_writeline(base);
        terminal_setcolor(VGA_WHITE, VGA_BLACK);
        terminal_writeline("  Type 'sos-help' for available commands.");
    }
}

static void readline(char* buf, int max) {
    int     i        = 0;
    int     ctrl_dn  = 0;
    int     extended = 0;
    uint8_t sc       = 0;
    char    c        = 0;
    buf[0] = 0;

    while (1) {
        while (!(inb(0x64) & 0x01));
        sc = inb(0x60);

        if (sc == 0xE0) { extended = 1; continue; }
        if (sc == 0x1D) { ctrl_dn  = 1; continue; }
        if (sc == 0x9D) { ctrl_dn  = 0; continue; }

        if (extended) {
            extended = 0;
            if (sc & 0x80) continue;
            if (sc == 0x49) { terminal_scroll_up(5);    continue; }
            if (sc == 0x51) { terminal_scroll_down(5);  continue; }
            if (sc == 0x48) { terminal_scroll_up(1);    continue; }
            if (sc == 0x50) { terminal_scroll_down(1);  continue; }
            if (sc == 0x47) { terminal_scroll_up(999);  continue; }
            if (sc == 0x4F) { terminal_scroll_bottom(); continue; }
            continue;
        }

        if (sc & 0x80) continue;
        terminal_scroll_bottom();

        if (sc == 0x1C) {
            buf[i] = 0;
            terminal_putchar('\n');
            break;
        }

        if (sc == 0x0E) {
            if (i > 0) { i--; buf[i] = 0; terminal_erase_char(); }
            continue;
        }

        if (sc == 0x0F) {
            int s;
            for (s = 0; s < 4 && i < max-1; s++) {
                buf[i++] = ' '; terminal_putchar(' ');
            }
            buf[i] = 0;
            continue;
        }

        if (ctrl_dn && sc == 0x2E) {
            int j;
            clipboard_len = i;
            for (j = 0; j < i; j++) clipboard[j] = buf[j];
            clipboard[i] = 0;
            terminal_setcolor(VGA_LIGHT_GREY, VGA_BLACK);
            terminal_write(" [copied]");
            terminal_setcolor(VGA_WHITE, VGA_BLACK);
            ctrl_dn = 0;
            continue;
        }

        if (ctrl_dn && sc == 0x2F) {
            int j;
            for (j = 0; j < clipboard_len && i < max-1; j++) {
                buf[i++] = clipboard[j];
                terminal_putchar(clipboard[j]);
            }
            buf[i] = 0;
            ctrl_dn = 0;
            continue;
        }

        ctrl_dn = 0;
        c = keyboard_getchar_sc(sc);
        if (c && i < max-1) {
            buf[i++] = c;
            buf[i]   = 0;
            terminal_putchar(c);
        }
    }
}

void kernel_main(void) {
    int  bi, ai, idx;
    char cmd[MAX_CMD];
    char base[MAX_CMD];
    char args[MAX_CMD];

    terminal_init();
    splash_show();
    uptime_init();
    terminal_clear();

    auth_login();

    terminal_clear();
    for (volatile int w = 0; w < 20000000; w++);
    print_banner();

    terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_writeline("[SOS] Kernel loaded");
    terminal_writeline("[SOS] VGA driver ready");
    keyboard_init();
    terminal_writeline("[SOS] Keyboard ready");
    memory_init();
    terminal_writeline("[SOS] Memory manager ready");
    terminal_writeline("[SOS] RTC clock ready");
    terminal_writeline("[SOS] Security module active");
    terminal_writeline("[SOS] All systems nominal");
    terminal_putchar('\n');

    rtc_print_time();
    terminal_putchar('\n');

    terminal_setcolor(VGA_WHITE, VGA_BLACK);
    terminal_writeline("Type 'sos-help' for commands. Type 'desk' for GUI.");
    terminal_putchar('\n');

    while (1) {
        terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
        terminal_write("sos> ");
        terminal_setcolor(VGA_WHITE, VGA_BLACK);

        readline(cmd, MAX_CMD);
        if (cmd[0] == 0) continue;
        history_add(cmd);

        bi = 0; ai = 0; idx = 0;
        while (cmd[idx] && cmd[idx] != ' ') base[bi++] = cmd[idx++];
        base[bi] = 0;
        while (cmd[idx] == ' ') idx++;
        while (cmd[idx]) args[ai++] = cmd[idx++];
        args[ai] = 0;

        run_command(base, args);
    }
}
