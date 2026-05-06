#include "shutdown.h"
#include "../drivers/vga.h"

#define SCREEN_W 80
#define SCREEN_H 25
#define VGA_BUF  ((volatile uint16_t*)0xB8000)

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
static void wait_ms(int ms) {
    for(volatile int i=0;i<ms*1000;i++);
}

/* ── Wait for scancode, ignore releases and modifier keys ── */
static uint8_t wait_scan(void) {
    uint8_t sc;
    for(;;) {
        while(!(inb(0x64)&1));
        sc = inb(0x60);
        /* Ignore releases (bit 7 set) */
        if(sc & 0x80) continue;
        /* Ignore shift/ctrl/alt/caps */
        if(sc==0x2A||sc==0x36) continue; /* shift */
        if(sc==0x1D)           continue; /* ctrl  */
        if(sc==0x38)           continue; /* alt   */
        if(sc==0x3A)           continue; /* caps  */
        if(sc==0xE0)           continue; /* extended prefix */
        return sc;
    }
}

/* ── Show what key was detected (debug helper on screen) ── */
static void show_choice(uint8_t sc, uint8_t col) {
    /* Print the detected scancode as a letter on screen */
    /* S=0x1F R=0x13 L=0x26 C=0x2E */
    const char* label =
        (sc==0x1F) ? "S - Shutdown" :
        (sc==0x13) ? "R - Restart"  :
        (sc==0x26) ? "L - Logout"   :
        (sc==0x2E) ? "C - Cancel"   : "? - Unknown";
    vstr(25, 20, "                              ", col);
    vstr(25, 20, label, col);
    wait_ms(500);
}

/* ── CPU reset ── */
static void do_cpu_reset(void) {
    uint8_t v;
    do { v=inb(0x64); } while(v & 0x02);
    outb(0x64, 0xFE);
    wait_ms(1000);
    /* Triple fault fallback */
    __asm__ volatile("cli");
    __asm__ volatile("xor %%eax,%%eax\n mov %%eax,(%%eax)\n":::"eax");
    while(1) __asm__ volatile("hlt");
}

/* ── ACPI power off ── */
static void do_power_off(void) {
    __asm__ volatile("cli");

    /* QEMU/KVM ACPI shutdown - most reliable */
    outw(0x604, 0x2000);
    wait_ms(300);

    /* Bochs/old QEMU */
    outw(0xB004, 0x2000);
    wait_ms(300);

    /* VirtualBox ACPI */
    outw(0x4004, 0x3400);
    wait_ms(300);

    /* SeaBIOS */
    outb(0x8900, 0);
    wait_ms(300);

    /* VMware: use backdoor port */
    /* VMware magic: IN from port 0x5658 with EAX=0x564D5868 */
    __asm__ volatile(
        "movl $0x564D5868, %%eax\n"
        "movl $0x07, %%ecx\n"
        "movl $0x5658, %%edx\n"
        "inl %%dx, %%eax\n"
        :::"eax","ecx","edx"
    );
    wait_ms(300);

    /* Final: halt - VM shows as powered off in VMware */
    while(1) __asm__ volatile("hlt");
}

/* ── Screen: shutdown ── */
static void draw_shutdown_screen(void) {
    vclear(0x00);
    vfill(0,0,SCREEN_W,' ',0x1F);
    vstr_center(0,"SOS - Shalu Operating System  |  Shutdown",0x1F);
    vstr(18,5, "+--------------------------------------------+",0x17);
    vstr(18,6, "|                                            |",0x17);
    vstr(18,7, "|       SOS has been shut down              |",0x1F);
    vstr(18,8, "|                                            |",0x17);
    vstr(18,9, "|  [OK] Saving system state                 |",0x1A);
    vstr(18,10,"|  [OK] Flushing write buffers              |",0x1A);
    vstr(18,11,"|  [OK] Stopping all services               |",0x1A);
    vstr(18,12,"|  [OK] Powering off hardware               |",0x1A);
    vstr(18,13,"|                                            |",0x17);
    vstr(18,14,"+--------------------------------------------+",0x17);
    vstr_center(16,"It is now safe to power off your machine.",0x17);
    vstr_center(17,"Thank you for using SOS.",0x1E);
    vfill(0,24,SCREEN_W,' ',0x1F);
    vstr_center(24,"SOS v1.0  |  Shalu Operating System  |  2025",0x1F);
    wait_ms(1500);
}

/* ── Screen: restart ── */
static void draw_restart_screen(void) {
    vclear(0x00);
    vfill(0,0,SCREEN_W,' ',0x2F);
    vstr_center(0,"SOS - Shalu Operating System  |  Restarting",0x2F);
    vstr(18,5, "+--------------------------------------------+",0x0A);
    vstr(18,6, "|                                            |",0x0A);
    vstr(18,7, "|       Restarting SOS v1.0                 |",0x0F);
    vstr(18,8, "|                                            |",0x0A);
    vstr(18,9, "|  [OK] Saving session state                |",0x0A);
    vstr(18,10,"|  [OK] Flushing buffers                    |",0x0A);
    vstr(18,11,"|  [OK] Resetting CPU                       |",0x0A);
    vstr(18,12,"|  [>>] Rebooting now...                    |",0x0E);
    vstr(18,13,"|                                            |",0x0A);
    vstr(18,14,"+--------------------------------------------+",0x0A);
    vstr_center(16,"Returning to login screen. Please wait...",0x07);
    vfill(0,24,SCREEN_W,' ',0x2F);
    vstr_center(24,"SOS v1.0  |  2025",0x2F);
    wait_ms(2000);
}

/* ── Screen: logout ── */
static void draw_logout_screen(void) {
    vclear(0x00);
    vfill(0,0,SCREEN_W,' ',0x3F);
    vstr_center(0,"SOS - Shalu Operating System  |  Logging Out",0x3F);
    vstr(18,6, "+--------------------------------------------+",0x0B);
    vstr(18,7, "|                                            |",0x0B);
    vstr(18,8, "|       Logging out of SOS v1.0             |",0x0F);
    vstr(18,9, "|                                            |",0x0B);
    vstr(18,10,"|  [OK] Saving session context              |",0x0B);
    vstr(18,11,"|  [OK] Clearing screen buffers             |",0x0B);
    vstr(18,12,"|  [OK] Returning to login screen           |",0x0B);
    vstr(18,13,"|                                            |",0x0B);
    vstr(18,14,"+--------------------------------------------+",0x0B);
    vstr_center(16,"Your session has been saved.",0x0B);
    vstr_center(17,"Please log in again to continue.",0x0F);
    vfill(0,24,SCREEN_W,' ',0x3F);
    vstr_center(24,"SOS v1.0  |  2025",0x3F);
    wait_ms(2000);
}

/* ══════════════════════════════════════════════════════════════════════
   CLI CONFIRMATION
   Draws in terminal mode, reads ONE scancode, maps to action
   ══════════════════════════════════════════════════════════════════════ */
int shutdown_confirm_cli(void) {
    uint8_t sc;

    terminal_putchar('\n');
    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_writeline("  +----------------------------------------------+");
    terminal_writeline("  |          SOS  Power  Management              |");
    terminal_writeline("  +----------------------------------------------+");
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
    terminal_writeline("  |  S  =  Shutdown   (power off completely)    |");
    terminal_writeline("  |  R  =  Restart    (reboot to login)         |");
    terminal_writeline("  |  L  =  Log out    (save and return to login)|");
    terminal_writeline("  |  C  =  Cancel     (return to shell)         |");
    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_writeline("  +----------------------------------------------+");
    terminal_putchar('\n');
    terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_write("  Press S / R / L / C : ");
    terminal_setcolor(VGA_WHITE, VGA_BLACK);

    /* Read scancode directly - bypass keyboard driver */
    sc = wait_scan();

    /* Echo the choice */
    if(sc==0x1F){ terminal_writeline("S"); return POWER_SHUTDOWN; }
    if(sc==0x13){ terminal_writeline("R"); return POWER_RESTART;  }
    if(sc==0x26){ terminal_writeline("L"); return POWER_LOGOUT;   }
    /* Any other key = cancel */
    terminal_writeline("C");
    return POWER_CANCEL;
}

/* ══════════════════════════════════════════════════════════════════════
   GUI CONFIRMATION
   Draws VGA box overlay directly, reads ONE scancode
   ══════════════════════════════════════════════════════════════════════ */
int shutdown_confirm_gui(void) {
    uint8_t sc;
    int bx=13, by=6, bw=54, bh=13;

    /* Draw box */
    vput(bx,by,'+',0x4F); vput(bx+bw-1,by,'+',0x4F);
    vput(bx,by+bh-1,'+',0x4F); vput(bx+bw-1,by+bh-1,'+',0x4F);
    for(int x=bx+1;x<bx+bw-1;x++){
        vput(x,by,'-',0x4F);
        vput(x,by+bh-1,'-',0x4F);
    }
    for(int y=by+1;y<by+bh-1;y++){
        vput(bx,y,'|',0x4F);
        vput(bx+bw-1,y,'|',0x4F);
        for(int x=bx+1;x<bx+bw-1;x++) vput(x,y,' ',0x40);
    }
    vstr(bx+15, by,    "[ SOS Power Management ]",   0x4F);
    vstr(bx+4, by+2,   "What would you like to do?",  0x4E);
    vstr(bx+4, by+4,   "[ S ]  Shutdown  - Power off the system",  0x4F);
    vstr(bx+4, by+5,   "[ R ]  Restart   - Reboot to login screen",0x4F);
    vstr(bx+4, by+6,   "[ L ]  Log out   - Save & return to login",0x4F);
    vstr(bx+4, by+7,   "[ C ]  Cancel    - Return to desk",        0x47);
    vstr(bx+4, by+9,   "Press S, R, L or C:",                      0x4E);

    sc = wait_scan();
    show_choice(sc, 0x4E);

    if(sc==0x1F) return POWER_SHUTDOWN;
    if(sc==0x13) return POWER_RESTART;
    if(sc==0x26) return POWER_LOGOUT;
    return POWER_CANCEL;
}

/* ══════════════════════════════════════════════════════════════════════
   PUBLIC API
   ══════════════════════════════════════════════════════════════════════ */

/* Called from shell — runs confirm then acts */
void shutdown(void) {
    int choice = shutdown_confirm_cli();
    switch(choice) {
        case POWER_SHUTDOWN:
            draw_shutdown_screen();
            do_power_off();
            break;
        case POWER_RESTART:
            draw_restart_screen();
            do_cpu_reset();
            break;
        case POWER_LOGOUT:
            draw_logout_screen();
            g_logout_requested = 1;
            break;
        case POWER_CANCEL:
        default:
            terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
            terminal_writeline("  Cancelled. Returning to shell.");
            terminal_putchar('\n');
            terminal_setcolor(VGA_WHITE, VGA_BLACK);
            break;
    }
}

/* Called from GUI after confirm_gui — direct shutdown no re-confirm */
void shutdown_do(void) {
    draw_shutdown_screen();
    do_power_off();
}

void restart(void) {
    draw_restart_screen();
    do_cpu_reset();
}

void logout_sos(void) {
    draw_logout_screen();
    g_logout_requested = 1;
}
