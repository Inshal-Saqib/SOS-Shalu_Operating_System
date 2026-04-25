#include "shutdown.h"
#include "../drivers/vga.h"
#define SCREEN_W 80
#define SCREEN_H 25

#define VGA_BUF ((volatile uint16_t*)0xB8000)

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0,%1"::"a"(val),"Nd"(port));
}
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0,%1"::"a"(val),"Nd"(port));
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

static void draw_shutdown_screen(void) {
    vclear(0x00);

    vfill(0,0,SCREEN_W,' ',0x1F);
    vstr_center(0,"SOS - Shalu Operating System  |  Shutdown",0x1F);

    vstr_center(6, " +--------------------------+ ",0x17);
    vstr_center(7, " |                          | ",0x17);
    vstr_center(8, " |    Shutting down SOS    | ",0x1F);
    vstr_center(9, " |                          | ",0x17);
    vstr_center(10," |  Saving system state...  | ",0x17);
    vstr_center(11," |  Flushing buffers...     | ",0x17);
    vstr_center(12," |  Halting CPU...          | ",0x17);
    vstr_center(13," |                          | ",0x17);
    vstr_center(14," +--------------------------+ ",0x17);

    vstr_center(17,"It is now safe to turn off your computer.",0x17);
    vstr_center(18,"Thank you for using MyOS.",0x1E);

    vfill(0,24,SCREEN_W,' ',0x1F);
    vstr_center(24,"SOS - Shalu Operating System  |  2025",0x1F);
}

static void draw_restart_screen(void) {
    vclear(0x00);
    vfill(0,0,SCREEN_W,' ',0x2F);
    vstr_center(0,"SOS - Shalu Operating System  |  Restart",0x2F);
    vstr_center(11,"Restarting system...",0x0A);
    vstr_center(12,"Please wait.",0x07);
    vfill(0,24,SCREEN_W,' ',0x2F);
}

void shutdown(void) {
    __asm__ volatile("cli");
    draw_shutdown_screen();

    /* Flush keyboard buffer */
    while(1) {
        unsigned char s=0;
        __asm__ volatile("inb $0x64,%0":"=a"(s));
        if(!(s&0x01)) break;
        __asm__ volatile("inb $0x60,%0":"=a"(s));
    }

    /* Try ACPI shutdown (port 0x604 for QEMU/Bochs) */
    outw(0x604, 0x2000);

    /* Try APM shutdown */
    outw(0xB004, 0x2000);

    /* Try VMware/VirtualBox shutdown port */
    outw(0x4004, 0x3400);

    /* Final fallback - halt CPU forever */
    while(1) __asm__ volatile("hlt");
}

void restart(void) {
    __asm__ volatile("cli");
    draw_restart_screen();
    for(volatile int w=0;w<20000000;w++);

    /* Pulse keyboard controller reset line */
    uint8_t good = 0x02;
    while(good & 0x02) {
        __asm__ volatile("inb $0x64,%0":"=a"(good));
    }
    outb(0x64, 0xFE);

    /* If that fails, triple fault */
    while(1) __asm__ volatile("hlt");
}
