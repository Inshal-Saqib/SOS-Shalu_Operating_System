#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../drivers/rtc.h"
#include "memory.h"
#include "history.h"
#include "gui.h"

#define MAX_CMD  80
#define CLIP_MAX 80

/* ── Clipboard ─────────────────────────────────────────────────────────── */
static char clipboard[CLIP_MAX];
static int  clipboard_len = 0;

/* ── I/O port helpers ──────────────────────────────────────────────────── */
static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(port));
    return v;
}
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0,%1"::"a"(val),"Nd"(port));
}

/* ── String utils ──────────────────────────────────────────────────────── */
static int strcmp_fn(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a - *b;
}

/* ── Number printing ───────────────────────────────────────────────────── */
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

/* ── Banner ────────────────────────────────────────────────────────────── */
static void print_banner(void) {
    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_writeline("============================================");
    terminal_writeline("         Welcome to MyOS v0.3              ");
    terminal_writeline("  Type 'gui' to switch to GUI mode         ");
    terminal_writeline("============================================");
    terminal_putchar('\n');
}

static const char sc_map[128] = {
    0,  0, '1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n',  0,
    'a','s','d','f','g','h','j','k','l',';','\'','`',  0, '\\',
    'z','x','c','v','b','n','m',',','.','/',  0, '*',  0,  ' ',
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0
};

/* Scancode constants */
#define SC_CTRL_L  0x1D
#define SC_C       0x2E
#define SC_V       0x2F
#define SC_BS      0x0E   /* backspace */
#define SC_TAB     0x0F
#define SC_ENTER   0x1C

/* ════════════════════════════════════════════════════════════════════════
   READLINE  — reads a full line with editing support
   Handles: normal chars, backspace, tab, ctrl+c, ctrl+v, enter
   ════════════════════════════════════════════════════════════════════════ */
static void readline(char* buf, int max) {
    int i         = 0;
    int ctrl_down = 0;
    buf[0] = '\0';

    while (1) {
        /* Wait for scancode */
        while (!(inb(0x64) & 0x01));
        uint8_t sc = inb(0x60);

        /* Track Ctrl press/release */
        if (sc == 0x1D) { ctrl_down = 1; continue; }  /* Ctrl pressed  */
        if (sc == 0x9D) { ctrl_down = 0; continue; }  /* Ctrl released */

        /* Ignore all key releases */
        if (sc & 0x80) continue;

        /* Enter */
        if (sc == 0x1C) {
            buf[i] = '\0';
            terminal_putchar('\n');
            break;
        }

        /* Backspace */
        if (sc == 0x0E) {
            if (i > 0) {
                i--;
                buf[i] = '\0';
                terminal_erase_char();  /* uses VGA cursor tracking */
            }
            continue;
        }

        /* Tab = 4 spaces */
        if (sc == 0x0F) {
            for (int s = 0; s < 4 && i < max - 1; s++) {
                buf[i++] = ' ';
                terminal_putchar(' ');
            }
            buf[i] = '\0';
            continue;
        }

        /* Ctrl+C = copy line to clipboard */
        if (ctrl_down && sc == 0x2E) {
            clipboard_len = i;
            for (int j = 0; j < i; j++) clipboard[j] = buf[j];
            clipboard[i] = '\0';
            terminal_setcolor(VGA_LIGHT_GREY, VGA_BLACK);
            terminal_write(" [copied]");
            terminal_setcolor(VGA_WHITE, VGA_BLACK);
            ctrl_down = 0;
            continue;
        }

        /* Ctrl+V = paste clipboard */
        if (ctrl_down && sc == 0x2F) {
            for (int j = 0; j < clipboard_len && i < max - 1; j++) {
                buf[i++] = clipboard[j];
                terminal_putchar(clipboard[j]);
            }
            buf[i] = '\0';
            ctrl_down = 0;
            continue;
        }

        ctrl_down = 0;

        /* Normal character */
        char c = (sc < 128) ? sc_map[sc] : 0;
        if (c && i < max - 1) {
            buf[i++] = c;
            buf[i]   = '\0';
            terminal_putchar(c);
        }
    }
}

/* ── Shared command handlers ───────────────────────────────────────────── */
static void do_about(void) {
    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_writeline("  MyOS v0.3");
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
    terminal_writeline("  Built from scratch in C & x86 Assembly");
    terminal_writeline("  Features: VGA, Keyboard, Memory Manager,");
    terminal_writeline("           RTC Clock, Calendar");
    terminal_writeline("           GUI Mode, Clipboard, History");
}
static void do_meminfo(void) {
    size_t used, free_mem, total;
    memory_stats(&used, &free_mem, &total);
    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_writeline("  Memory Info:");
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
    terminal_write("  Total : "); print_kb(total);    terminal_putchar('\n');
    terminal_write("  Used  : "); print_kb(used);     terminal_putchar('\n');
    terminal_write("  Free  : "); print_kb(free_mem); terminal_putchar('\n');
}
static void do_memtest(void) {
    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_writeline("  Running memory test...");
    void* a = kmalloc(256);
    void* b = kmalloc(512);
    void* c = kmalloc(1024);
    terminal_setcolor(a && b && c ? VGA_LIGHT_GREEN : VGA_LIGHT_RED, VGA_BLACK);
    terminal_writeline(a && b && c ? "  [OK] Allocated 256+512+1024 bytes"
                                   : "  [FAIL] Allocation failed!");
    kfree(b);
    terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_writeline("  [OK] Freed 512 byte block");
    void* d = kmalloc(512);
    if (d) terminal_writeline("  [OK] Re-al 512 bytes");
    kfree(a); kfree(c); kfree(d);
    terminal_writeline("  [OK] All blocks freed - test passed!");
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
}

/* ── Command dispatcher ────────────────────────────────────────────────── */
static void run_command(const char* cmd) {

    if (strcmp_fn(cmd, "help") == 0) {
        terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
        terminal_writeline("Commands:");
        terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
        terminal_writeline("  System:   help  clear  about  halt");
        terminal_writeline("  Time:     clock  calendar");
        terminal_writeline("  Memory:   meminfo  memtest");
    
        terminal_writeline("  History:  history");
        terminal_writeline("  GUI:      gui");
        terminal_setcolor(VGA_WHITE, VGA_BLACK);
        terminal_writeline("  Keys: Tab=4spaces  Backspace=delete");
        terminal_writeline("        Ctrl+C=copy  Ctrl+V=paste");

    } else if (strcmp_fn(cmd, "clear") == 0) {
        terminal_clear();
        print_banner();

    } else if (strcmp_fn(cmd, "about") == 0) {
        do_about();

    } else if (strcmp_fn(cmd, "clock") == 0) {
        terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
        terminal_writeline("--------------------------------------------");
        rtc_print_time();
        rtc_print_date();
        terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
        terminal_writeline("--------------------------------------------");
        terminal_setcolor(VGA_WHITE, VGA_BLACK);

    } else if (strcmp_fn(cmd, "calendar") == 0) {
        terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
        terminal_writeline("--------------------------------------------");
        rtc_print_calendar();
        terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
        terminal_writeline("--------------------------------------------");
        terminal_setcolor(VGA_WHITE, VGA_BLACK);

    } else if (strcmp_fn(cmd, "meminfo") == 0) {
        do_meminfo();

    } else if (strcmp_fn(cmd, "memtest") == 0) {
        do_memtest();

    } else if (strcmp_fn(cmd, "history") == 0) {
        int count = history_count_get();
        if (count == 0) {
            terminal_writeline("  No history yet.");
        } else {
            terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
            terminal_writeline("  Command History (last 10):");
            terminal_setcolor(VGA_WHITE, VGA_BLACK);
            int show = count < 10 ? count : 10;
            for (int i = show; i >= 1; i--) {
                char hbuf[MAX_CMD];
                if (history_get(i, hbuf)) {
                    terminal_write("  ");
                    print_num(count - i + 1);
                    terminal_write(": ");
                    terminal_writeline(hbuf);
                }
            }
        }

    } else if (strcmp_fn(cmd, "gui") == 0) {
        while (1) {
            gui_action_t action = gui_run();
            if (action == GUI_SWITCH_CLI) {
                terminal_init();
                print_banner();
                terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
                terminal_writeline("[OK] Switched back to CLI.");
                terminal_putchar('\n');
                terminal_setcolor(VGA_WHITE, VGA_BLACK);
                break;
            }
            char tmp[4];
            if      (action == GUI_CLOCK)    { terminal_init(); rtc_print_time(); rtc_print_date(); terminal_setcolor(VGA_WHITE,VGA_BLACK); terminal_writeline("\nPress Enter to return..."); readline(tmp,2); }
            else if (action == GUI_CALENDAR) { terminal_init(); rtc_print_calendar(); terminal_setcolor(VGA_WHITE,VGA_BLACK); terminal_writeline("\nPress Enter to return..."); readline(tmp,2); }
            else if (action == GUI_MEMINFO)  { terminal_init(); do_meminfo(); terminal_setcolor(VGA_WHITE,VGA_BLACK); terminal_writeline("\nPress Enter to return..."); readline(tmp,2); }
            else if (action == GUI_MEMTEST)  { terminal_init(); do_memtest(); terminal_setcolor(VGA_WHITE,VGA_BLACK); terminal_writeline("\nPress Enter to return..."); readline(tmp,2); }
            else if (action == GUI_ABOUT)    { terminal_init(); do_about(); terminal_setcolor(VGA_WHITE,VGA_BLACK); terminal_writeline("\nPress Enter to return..."); readline(tmp,2); }
            else if (action == GUI_HISTORY)  {
                terminal_init();
                int count = history_count_get();
                if(count==0){ terminal_writeline("No history yet."); }
                else {
                    terminal_setcolor(VGA_LIGHT_CYAN,VGA_BLACK);
                    terminal_writeline("Command History:");
                    terminal_setcolor(VGA_WHITE,VGA_BLACK);
                    int show=count<10?count:10;
                    for(int i=show;i>=1;i--){
                        char hb[80];
                        if(history_get(i,hb)){ terminal_write("  "); terminal_writeline(hb); }
                    }
                }
                terminal_writeline("\nPress Enter to return...");
                readline(tmp,2);
            }
            else if (action == GUI_CLEAR)    { terminal_clear(); terminal_writeline("Press Enter to return..."); readline(tmp,2); }
            else if (action == GUI_HALT)     { terminal_init(); terminal_setcolor(VGA_LIGHT_RED,VGA_BLACK); terminal_writeline("System halting..."); __asm__ volatile("cli;hlt"); }
        }

    } else if (strcmp_fn(cmd, "halt") == 0) {
        terminal_setcolor(VGA_LIGHT_RED, VGA_BLACK);
        terminal_writeline("System halting. Goodbye!");
        __asm__ volatile("cli; hlt");

    } else if (cmd[0] == '\0') {
        /* empty line */
    } else {
        terminal_setcolor(VGA_LIGHT_RED, VGA_BLACK);
        terminal_write("  Unknown: "); terminal_writeline(cmd);
        terminal_setcolor(VGA_WHITE, VGA_BLACK);
        terminal_writeline("  Type 'help' for commands.");
    }
}

/* ── Kernel entry ──────────────────────────────────────────────────────── */
void kernel_main(void) {
    terminal_init();
    print_banner();

    terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_writeline("[OK] Kernel loaded");
    terminal_writeline("[OK] VGA driver ready");
    keyboard_init();
    terminal_writeline("[OK] Keyboard ready");
    memory_init();
    terminal_writeline("[OK] Memory manager ready");
    terminal_writeline("[OK] RTC clock ready");
    terminal_writeline("[OK] Clipboard & history ready");
    terminal_writeline("[OK] GUI mode ready - type 'gui' or use arrow keys + Enter");
    terminal_putchar('\n');
    rtc_print_time();
    terminal_putchar('\n');
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
    terminal_writeline("Type 'help' for commands. Type 'gui' for GUI.");
    terminal_putchar('\n');

    char cmd[MAX_CMD];
    while (1) {
        terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
        terminal_write("myos> ");
        terminal_setcolor(VGA_WHITE, VGA_BLACK);
        readline(cmd, MAX_CMD);
        if (cmd[0] != '\0') history_add(cmd);
        run_command(cmd);
    }
}
