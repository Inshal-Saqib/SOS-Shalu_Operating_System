#include "gui.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../drivers/rtc.h"

#define VGA_BUF ((volatile uint16_t*)0xB8000)
#define VGA_W 80
#define VGA_H 25

static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(port));
    return v;
}
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0,%1"::"a"(val),"Nd"(port));
}

/* ── VGA helpers ─────────────────────────────────────────────────────────── */
static void vput(int x, int y, char c, uint8_t col) {
    if(x<0||x>=VGA_W||y<0||y>=VGA_H) return;
    VGA_BUF[y*VGA_W+x]=(uint16_t)c|((uint16_t)col<<8);
}
static void vstr(int x, int y, const char* s, uint8_t col) {
    while(*s) vput(x++,y,*s++,col);
}
static void vfill(int x, int y, int w, char c, uint8_t col) {
    for(int i=0;i<w;i++) vput(x+i,y,c,col);
}
static int vstrlen(const char* s) {
    int i=0; while(s[i]) i++; return i;
}
static void vstr_center(int y, const char* s, uint8_t col) {
    int x=(VGA_W-vstrlen(s))/2;
    vstr(x,y,s,col);
}

/* ── Number printer ──────────────────────────────────────────────────────── */
static void vnum2(int x, int y, uint8_t n, uint8_t col) {
    vput(x,   y, '0'+n/10, col);
    vput(x+1, y, '0'+n%10, col);
}

/* ── Color constants (fg | bg<<4) ────────────────────────────────────────── */
/* bg: BLACK=0 BLUE=1 GREEN=2 CYAN=3 RED=4 MAGENTA=5 BROWN=6 LGREY=7        */
/*     DGREY=8 LBLUE=9 LGREEN=A LCYAN=B LRED=C LMAG=D YELLOW=E WHITE=F      */
#define COL_TITLEBAR   0x1F  /* white on blue        */
#define COL_TITLEDIM   0x1E  /* yellow on blue       */
#define COL_BG         0x17  /* light grey on blue   */
#define COL_BORDER     0x1B  /* cyan on blue         */
#define COL_HDR        0x1E  /* yellow on blue       */
#define COL_BTN_NORM   0x19  /* lblue on blue        */
#define COL_BTN_SEL    0x7F  /* white on lgrey(hi)   */
#define COL_BTN_BORDER 0x1F  /* white on blue        */
#define COL_BTN_SEL_BD 0x0F  /* white on black       */
#define COL_DESC_BOX   0x30  /* black on cyan        */
#define COL_DESC_TXT   0x3F  /* white on cyan        */
#define COL_HALT_BG    0x4E  /* yellow on red        */
#define COL_HINT       0x1A  /* green on blue        */
#define COL_CLOCK      0x1E  /* yellow on blue       */
#define COL_SEL_LABEL  0x70  /* black on lgrey       */
#define COL_NORM_LABEL 0x1F  /* white on blue        */

/* ── Button layout ───────────────────────────────────────────────────────── */
#define BTN_COUNT  8
#define BTN_W      36
#define BTN_H      3

typedef struct {
    int          col, row;   /* grid position (0-1 col, 0-3 row) */
    const char*  icon;
    const char*  label;
    const char*  desc;
    gui_action_t action;
} Button;

static const Button btns[BTN_COUNT] = {
    {0,0, "[1]", " Clock       ","Display current time and date",    GUI_CLOCK      },
    {0,1, "[2]", " Calendar    ","Show this month's full calendar",  GUI_CALENDAR   },
    {0,2, "[3]", " Memory Info ","View heap allocation statistics",  GUI_MEMINFO    },
    {0,3, "[4]", " Memory Test ","Run live memory allocator test",   GUI_MEMTEST    },
    {1,0, "[5]", " About MyOS  ","About this OS and its features",   GUI_ABOUT      },
    {1,1, "[6]", " History     ","View recent command history",      GUI_HISTORY    },
    {1,2, "[7]", " Clear Screen","Clear terminal output",            GUI_CLEAR      },
    {1,3, "[8]", " Back to CLI ","Switch back to shell mode",        GUI_SWITCH_CLI },
};

/* Button top-left positions */
#define COL0_X  2
#define COL1_X  42
#define ROW0_Y  6
#define ROW_GAP 4

static int btn_x(int i) { return btns[i].col==0 ? COL0_X : COL1_X; }
static int btn_y(int i) { return ROW0_Y + btns[i].row * ROW_GAP; }

/* ── Draw one button ─────────────────────────────────────────────────────── */
static void draw_btn(int i, int sel) {
    int x=btn_x(i), y=btn_y(i), w=BTN_W;
    uint8_t bd = sel ? COL_BTN_SEL_BD : COL_BTN_BORDER;
    uint8_t bg = sel ? COL_SEL_LABEL  : COL_BTN_NORM;
    uint8_t ib = sel ? 0x0E           : COL_TITLEDIM;  /* icon color */

    /* Top border */
    vput(x,   y, '+', bd);
    vfill(x+1, y, w-2, '-', bd);
    vput(x+w-1, y, '+', bd);

    /* Content row */
    vput(x,     y+1, '|', bd);
    vfill(x+1,  y+1, w-2, ' ', bg);
    /* icon */
    int ix = x+2;
    for(const char* s=btns[i].icon; *s; s++)
        vput(ix++, y+1, *s, ib);
    vput(ix++, y+1, ' ', bg);
    /* label */
    for(const char* s=btns[i].label; *s; s++)
        vput(ix++, y+1, *s, bg);
    vput(x+w-1, y+1, '|', bd);

    /* Bottom border */
    vput(x,     y+2, '+', bd);
    vfill(x+1,  y+2, w-2, '-', bd);
    vput(x+w-1, y+2, '+', bd);

    /* Selection arrow indicator */
    if(sel) {
        vput(x-2, y+1, '>', 0x0E);
        vput(x-1, y+1, '>', 0x0E);
    } else {
        vput(x-2, y+1, ' ', COL_BG);
        vput(x-1, y+1, ' ', COL_BG);
    }
}

/* ── Draw full GUI ───────────────────────────────────────────────────────── */
static void gui_draw(int sel) {

    /* Fill background */
    for(int y=0;y<VGA_H;y++) vfill(0,y,VGA_W,' ',COL_BG);

    /* ── Title bar ── */
    vfill(0,0,VGA_W,' ',COL_TITLEBAR);
    vstr_center(0,"  MyOS v0.3  |  Graphical Interface  ",COL_TITLEBAR);

    /* Clock top-right */
    rtc_time_t t; rtc_read(&t);
    vnum2(70,0,t.hour,   COL_CLOCK);
    vput (72,0,':',      COL_CLOCK);
    vnum2(73,0,t.minute, COL_CLOCK);
    vput (75,0,':',      COL_CLOCK);
    vnum2(76,0,t.second, COL_CLOCK);

    /* ── Subtitle bar ── */
    vfill(0,1,VGA_W,' ',COL_BG);
    vstr_center(1,"Use [1-8] number keys  |  Arrow keys to navigate  |  Enter to select",COL_HINT);

    /* ── Top double border ── */
    vfill(0,2,VGA_W,'=',COL_BORDER);
    vput(0,2,'+',COL_BORDER); vput(79,2,'+',COL_BORDER);

    /* ── Column headers ── */
    vfill(0,3,VGA_W,' ',COL_BG);
    vstr(8,  3,"System & Information",COL_HDR);
    vstr(49, 3,"Utilities & Settings",COL_HDR);

    /* underline headers */
    vfill(0,4,VGA_W,' ',COL_BG);
    vfill(8, 4,20,'-',COL_BORDER);
    vfill(49,4,20,'-',COL_BORDER);

    /* spacer row */
    vfill(0,5,VGA_W,' ',COL_BG);

    /* ── Buttons ── */
    for(int i=0;i<BTN_COUNT;i++) draw_btn(i, i==sel);

    /* ── Center divider ── */
    for(int y=3;y<22;y++) vput(40,y,'|',COL_BORDER);

    /* ── Description box ── */
    vfill(0,21,VGA_W,'=',COL_BORDER);
    vput(0,21,'+',COL_BORDER); vput(79,21,'+',COL_BORDER);

    vfill(0,22,VGA_W,' ',COL_DESC_BOX);
    vstr(2, 22,"  Info: ",0x3E);
    vstr(10,22,btns[sel].desc,COL_DESC_TXT);

    /* ── Halt bar ── */
    vfill(0,23,VGA_W,'=',COL_BORDER);
    vput(0,23,'+',COL_BORDER); vput(79,23,'+',COL_BORDER);

    vfill(0,24,VGA_W,' ',COL_HALT_BG);
    vstr(2, 24,"  [9] HALT SYSTEM",  COL_HALT_BG);
    vstr(30,24,"Press 9 to shutdown the OS",0x4F);
    vstr(60,24,"MyOS (c) 2025",0x4C);
}

/* ── GUI event loop ──────────────────────────────────────────────────────── */
gui_action_t gui_run(void) {
    int sel=0;
    int extended=0;   /* tracks E0 prefix for arrow keys */
    gui_draw(sel);

    while(1) {
        /* wait for scancode */
        while(!(inb(0x64)&1));
        uint8_t sc=inb(0x60);

        /* ── Handle E0 extended key prefix ── */
        if(sc==0xE0){ extended=1; continue; }

        /* ── Key release - ignore ── */
        if(sc&0x80){ extended=0; continue; }

        /* ── Arrow keys (come after E0 prefix) ── */
        if(extended) {
            extended=0;
            int c=btns[sel].col, r=btns[sel].row;
            if(sc==0x48) { /* UP */
                r=(r-1+4)%4;
            } else if(sc==0x50) { /* DOWN */
                r=(r+1)%4;
            } else if(sc==0x4B) { /* LEFT */
                if(c==1) c=0;
            } else if(sc==0x4D) { /* RIGHT */
                if(c==0) c=1;
            }
            /* Find button matching new col,row */
            for(int i=0;i<BTN_COUNT;i++) {
                if(btns[i].col==c && btns[i].row==r) {
                    sel=i; break;
                }
            }
            gui_draw(sel);
            continue;
        }

        /* ── Number keys 1-8 ── */
        if(sc>=0x02 && sc<=0x09) {
            int n=sc-0x02;
            sel=n;
            gui_draw(sel);
            for(volatile int w=0;w<2000000;w++);
            return btns[n].action;
        }

        /* ── 9 = halt ── */
        if(sc==0x0A) return GUI_HALT;

        /* ── Enter = activate selected ── */
        if(sc==0x1C) {
            gui_draw(sel);
            for(volatile int w=0;w<2000000;w++);
            return btns[sel].action;
        }

        /* ── Tab = next button ── */
        if(sc==0x0F) {
            sel=(sel+1)%BTN_COUNT;
            gui_draw(sel);
        }
    }
}
