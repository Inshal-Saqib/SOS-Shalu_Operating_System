#include "shutdown.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"

#define SCREEN_W 80
#define SCREEN_H 25
#define VGA_BUF ((volatile uint16_t*)0xB8000)

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0,%1"::"a"(val),"Nd"(port));
}
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0,%1"::"a"(val),"Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(port));
    return v;
}

static void vput(int x, int y, char c, uint8_t col) {
    if(x<0||x>=SCREEN_W||y<0||y>=SCREEN_H) return;
    VGA_BUF[y*SCREEN_W+x]=(uint16_t)c|((uint16_t)col<<8);
}
static void vstr(int x, int y, const char* s, uint8_t col) {
    while(*s) vput(x++,y,*s++,col);
}
static void vfill(int x, int y, int w, char c, uint8_t col) {
    for(int i=0;i<w;i++) vput(x+i,y,c,col);
}
static void vstr_center(int y, const char* s, uint8_t col) {
    int len=0; while(s[len]) len++;
    vstr((SCREEN_W-len)/2,y,s,col);
}
static void vclear(uint8_t col) {
    for(int y=0;y<SCREEN_H;y++) vfill(0,y,SCREEN_W,' ',col);
}

/* Wait for key-down scancode */
static uint8_t wait_scan(void) {
    uint8_t sc;
    for(;;) {
        while(!(inb(0x64)&1));
        sc = inb(0x60);
        if(!(sc & 0x80)) return sc;
    }
}

/* Hardware CPU reset via keyboard controller */
static void do_cpu_reset(void) {
    uint8_t v;
    do { v = inb(0x64); } while(v & 0x02);
    outb(0x64, 0xFE);
    for(volatile int w=0;w<2000000;w++);
    /* Fallback: triple fault */
    __asm__ volatile(
        "cli\n"
        "xor %%eax,%%eax\n"
        "mov %%eax,(%%eax)\n"
        ::: "eax"
    );
    while(1) __asm__ volatile("hlt");
}

/* ── Restart screen ──────────────────────────────────────────────────────── */
static void draw_restart_screen(void) {
    vclear(0x00);

    vfill(0,0,SCREEN_W,' ',0x2F);
    vstr_center(0,"SOS - Shalu Operating System  |  Restarting",0x2F);

    vstr(20,6,  "+----------------------------------------+",0x0A);
    vstr(20,7,  "|                                        |",0x0A);
    vstr(20,8,  "|       Restarting SOS v1.0              |",0x0F);
    vstr(20,9,  "|                                        |",0x0A);
    vstr(20,10, "|   Saving system state...     [DONE]    |",0x0A);
    vstr(20,11, "|   Flushing buffers...        [DONE]    |",0x0A);
    vstr(20,12, "|   Resetting hardware...      [DONE]    |",0x0A);
    vstr(20,13, "|   Rebooting now...                     |",0x0E);
    vstr(20,14, "|                                        |",0x0A);
    vstr(20,15, "+----------------------------------------+",0x0A);

    vstr_center(17,"System will restart and return to login screen.",0x07);
    vstr_center(18,"Please wait...",0x08);

    vfill(0,24,SCREEN_W,' ',0x2F);
    vstr_center(24,"SOS - Shalu Operating System  |  2025",0x2F);

    /* Animate the rebooting line */
    for(volatile int w=0;w<60000000;w++);
}

/* ── Final shutdown screen ───────────────────────────────────────────────── */
static void draw_shutdown_screen(void) {
    vclear(0x00);

    vfill(0,0,SCREEN_W,' ',0x1F);
    vstr_center(0,"SOS - Shalu Operating System  |  Shutdown",0x1F);

    vstr(20,6,  "+----------------------------------------+",0x17);
    vstr(20,7,  "|                                        |",0x17);
    vstr(20,8,  "|       SOS has been shut down           |",0x1F);
    vstr(20,9,  "|                                        |",0x17);
    vstr(20,10, "|   Saving system state...     [DONE]    |",0x1A);
    vstr(20,11, "|   Flushing buffers...        [DONE]    |",0x1A);
    vstr(20,12, "|   Stopping services...       [DONE]    |",0x1A);
    vstr(20,13, "|   Halting CPU...             [DONE]    |",0x1A);
    vstr(20,14, "|                                        |",0x17);
    vstr(20,15, "+----------------------------------------+",0x17);

    vstr_center(17,"It is now safe to turn off your computer.",0x17);
    vstr_center(18,"Thank you for using SOS - Shalu Operating System.",0x1E);

    vfill(0,24,SCREEN_W,' ',0x1F);
    vstr_center(24,"SOS - Shalu Operating System  |  2025",0x1F);
}

/* ══════════════════════════════════════════════════════════════════════════
   CLI CONFIRMATION PROMPT
   Called from the shell when user types "shutdown" or "restart"
   Shows a text-mode menu and waits for S / R / C
   ══════════════════════════════════════════════════════════════════════════ */
int shutdown_confirm_cli(void) {
    terminal_putchar('\n');
    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_writeline("  +------------------------------------------+");
    terminal_writeline("  |        SOS Power Management               |");
    terminal_writeline("  +------------------------------------------+");
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
    terminal_writeline("  |  S  =  Shutdown the system               |");
    terminal_writeline("  |  R  =  Restart the system                |");
    terminal_writeline("  |  C  =  Cancel and return to shell        |");
    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_writeline("  +------------------------------------------+");
    terminal_putchar('\n');
    terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_write("  Your choice: ");
    terminal_setcolor(VGA_WHITE, VGA_BLACK);

    /* Read choice */
    uint8_t sc = wait_scan();
    /* S=0x1F  R=0x13  C=0x2E */
    if(sc == 0x1F) return POWER_SHUTDOWN;
    if(sc == 0x13) return POWER_RESTART;
    return POWER_CANCEL;
}

/* ══════════════════════════════════════════════════════════════════════════
   GUI CONFIRMATION PROMPT
   Draws a centered VGA confirmation box directly to VGA memory
   Returns POWER_SHUTDOWN / POWER_RESTART / POWER_CANCEL
   ══════════════════════════════════════════════════════════════════════════ */
int shutdown_confirm_gui(void) {
    /* Draw semi-transparent overlay box */
    int bx=15, by=7, bw=50, bh=11;

    /* Box border */
    vput(bx,by,'+',0x4F); vput(bx+bw-1,by,'+',0x4F);
    vput(bx,by+bh-1,'+',0x4F); vput(bx+bw-1,by+bh-1,'+',0x4F);
    for(int x=bx+1;x<bx+bw-1;x++) {
        vput(x,by,'-',0x4F);
        vput(x,by+bh-1,'-',0x4F);
    }
    for(int y=by+1;y<by+bh-1;y++) {
        vput(bx,y,'|',0x4F);
        vput(bx+bw-1,y,'|',0x4F);
        for(int x=bx+1;x<bx+bw-1;x++)
            vput(x,y,' ',0x40);
    }

    /* Title */
    vstr(bx+14, by,   "[ Power Options ]",  0x4F);

    /* Content */
    vstr(bx+4, by+2,  "Are you sure you want to proceed?", 0x4E);
    vstr(bx+4, by+4,  "[ S ]  Shutdown the system",        0x4F);
    vstr(bx+4, by+5,  "[ R ]  Restart  the system",        0x4F);
    vstr(bx+4, by+6,  "[ C ]  Cancel - return to desk",    0x47);
    vstr(bx+4, by+8,  "Press S, R or C",                   0x4E);

    /* Wait for choice */
    uint8_t sc = wait_scan();
    if(sc == 0x1F) return POWER_SHUTDOWN;
    if(sc == 0x13) return POWER_RESTART;
    return POWER_CANCEL;
}

/* ══════════════════════════════════════════════════════════════════════════
   PUBLIC: shutdown_do()
   Direct shutdown with no confirmation — used by GUI after its own confirm
   ══════════════════════════════════════════════════════════════════════════ */
void shutdown_do(void) {
    __asm__ volatile("cli");
    draw_shutdown_screen();
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);
    while(1) __asm__ volatile("hlt");
}

/* ══════════════════════════════════════════════════════════════════════════
   PUBLIC: shutdown()
   Shows confirmation, then either halts or restarts
   ══════════════════════════════════════════════════════════════════════════ */
void shutdown(void) {
    int choice = shutdown_confirm_cli();

    if(choice == POWER_CANCEL) {
        terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
        terminal_writeline("  Shutdown cancelled. Returning to shell.");
        terminal_putchar('\n');
        terminal_setcolor(VGA_WHITE, VGA_BLACK);
        return;
    }

    if(choice == POWER_RESTART) {
        restart();
        return;
    }

    /* POWER_SHUTDOWN */
    __asm__ volatile("cli");
    draw_shutdown_screen();

    /* Try ACPI ports */
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);

    while(1) __asm__ volatile("hlt");
}

/* ══════════════════════════════════════════════════════════════════════════
   PUBLIC: restart()
   Shows restart screen then resets CPU
   ══════════════════════════════════════════════════════════════════════════ */
void restart(void) {
    __asm__ volatile("cli");
    draw_restart_screen();
    do_cpu_reset();
}
