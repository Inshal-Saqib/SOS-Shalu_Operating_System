#include "splash.h"
#include "../drivers/vga.h"
#include "../drivers/rtc.h"

#define VGA_BUF ((volatile uint16_t*)0xB8000)
#define SCREEN_W 80
#define SCREEN_H 25

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
static void vclear(uint8_t col) {
    for(int y=0;y<SCREEN_H;y++) vfill(0,y,SCREEN_W,' ',col);
}
static void vstr_center(int y, const char* s, uint8_t col) {
    int len=0; while(s[len]) len++;
    vstr((SCREEN_W-len)/2,y,s,col);
}
static void wait(int n) {
    for(volatile int i=0;i<n;i++);
}
static void loading_bar(int x, int y, int total, int step, uint8_t col) {
    vput(x,y,'[',0x07);
    vput(x+total+1,y,']',0x07);
    for(int i=0;i<step;i++)    vput(x+1+i,y,'=',col);
    for(int i=step;i<total;i++) vput(x+1+i,y,' ',0x07);
    int pct=(step*100)/total;
    vput(x+total+3,y,'0'+pct/100,    0x0F);
    vput(x+total+4,y,'0'+(pct/10)%10,0x0F);
    vput(x+total+5,y,'0'+pct%10,     0x0F);
    vput(x+total+6,y,'%',            0x0F);
}

void splash_show(void) {
    vclear(0x00);

    /* Top and bottom bars */
    vfill(0,0,SCREEN_W,' ',0x1F);
    vstr_center(0,"SOS - Shalu Operating System v1.0",0x1F);
    vfill(0,24,SCREEN_W,' ',0x1F);
    vstr_center(24,"Shalu Operating System  |  Built with C & x86 Assembly",0x1F);

    /* ASCII logo */
    vstr_center(2, " ____  ___  ____  ",0x0B);
    vstr_center(3, "|  \\/  |_   _/ __ \\/ ___|",0x0B);
    vstr_center(4, "| |\\/| | | | | | |\\___ \\ ",0x09);
    vstr_center(5, "| ___) | |_| |___) |",0x09);
    vstr_center(6, "|_|  |_|\\__, |\\____/|____/ ",0x0B);
    vstr_center(7, "  Shalu Operating System",0x0B);
    vstr_center(8, "  Secure  |  Fast  |  Custom",0x07);

    const char* checks[] = {
        "Initializing CPU registers...",
        "Setting up GDT and memory segments...",
        "Loading kernel into memory...",
        "Initializing VGA text driver...",
        "Setting up keyboard controller...",
        "Initializing heap memory manager...",
        "Reading CMOS real-time clock...",
        "Starting security module...",
        "Preparing shell environment...",
        "System ready!",
    };
    uint8_t check_cols[] = {
        0x07,0x07,0x07,0x0A,0x0A,0x0A,0x0E,0x0E,0x0B,0x0F
    };

    int BAR_X   = 20;
    int BAR_Y   = 21;
    int BAR_LEN = 40;
    int n       = 10;

    for(int i=0;i<n;i++) {
        vfill(0,10,SCREEN_W,' ',0x00);
        vstr_center(10,checks[i],check_cols[i]);
        vfill(0,11,SCREEN_W,' ',0x00);
        if(i<n-1) vstr_center(11,"[ LOADING ]",0x08);
        else      vstr_center(11,"[  DONE!  ]",0x0A);

        int from=(i*BAR_LEN)/n;
        int to=((i+1)*BAR_LEN)/n;
        for(int b=from;b<=to;b++) {
            loading_bar(BAR_X,BAR_Y,BAR_LEN,b,0x0A);
            wait(800000);
        }
    }

    /* ── Hold the completed splash for 2 seconds ── */
    wait(60000000);

    /* ── Fully clear screen before handing off to login ── */
    vclear(0x00);

    /* ── Brief black pause so screens don't bleed together ── */
    wait(10000000);
}
