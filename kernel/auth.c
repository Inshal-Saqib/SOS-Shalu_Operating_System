#include "auth.h"
#include "../drivers/vga.h"
#define SCREEN_W 80
#define SCREEN_H 25

#define VGA_BUF ((volatile uint16_t*)0xB8000)

/* ── Credentials To Login ─────────────────────────────── */
#define VALID_USER "admin"
#define VALID_PASS "123"
#define MAX_ATTEMPTS 3
#define MAX_LEN      32

/* ── I/O helpers ─────────────────────────────────────────────────────────── */
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
    int x=(SCREEN_W-len)/2;
    vstr(x,y,s,col);
}
static void vclear(uint8_t col) {
    for(int y=0;y<SCREEN_H;y++) vfill(0,y,SCREEN_W,' ',col);
}

/* ── Scancode to ASCII ───────────────────────────────────────────────────── */
static const char sc_map[128] = {
    0,  0, '1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n',  0,
    'a','s','d','f','g','h','j','k','l',';','\'','`',  0, '\\',
    'z','x','c','v','b','n','m',',','.','/',  0, '*',  0,  ' ',
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0
};

/* ── String compare ──────────────────────────────────────────────────────── */
static int str_eq(const char* a, const char* b) {
    while(*a && *b && *a==*b){a++;b++;}
    return *a=='\0' && *b=='\0';
}

/* ── Read input, mask with * if hidden ───────────────────────────────────── */
static void read_input(int x, int y, char* buf, int max, int hide) {
    int i=0;
    buf[0]='\0';

    /* Clear input area */
    vfill(x,y,max,' ',0x07);
    vput(x,y,'_',0x07);

    while(1) {
        while(!(inb(0x64)&1));
        uint8_t sc=inb(0x60);
        if(sc&0x80) continue;

        if(sc==0x1C) { /* Enter */
            buf[i]='\0';
            break;
        }
        if(sc==0x0E) { /* Backspace */
            if(i>0) {
                i--;
                buf[i]='\0';
                vfill(x,y,max,' ',0x07);
                for(int j=0;j<i;j++)
                    vput(x+j,y,hide?'*':buf[j],0x0F);
                vput(x+i,y,'_',0x07);
            }
            continue;
        }
        char c=(sc<128)?sc_map[sc]:0;
        if(c && c!='\t' && i<max-1) {
            buf[i++]=c;
            buf[i]='\0';
            vfill(x,y,max,' ',0x07);
            for(int j=0;j<i;j++)
                vput(x+j,y,hide?'*':buf[j],0x0F);
            vput(x+i,y,'_',0x07);
        }
    }
}

/* ── Draw the login screen ───────────────────────────────────────────────── */
static void draw_login(int attempt, const char* msg, uint8_t msg_col) {
    vclear(0x17);

    /* Top bar */
    vfill(0,0,SCREEN_W,' ',0x1F);
    vstr_center(0,"SOS - Shalu Operating System  |  Secure Login",0x1F);

    /* ASCII art logo */
    vstr_center(2, " __  __       ___  ____  ",0x1B);
    vstr_center(3, "|  \\/  |_   _/ _ \\/ ___|",0x1B);
    vstr_center(4, "| |\\/| | | | | | |\\___ \\",0x1B);
    vstr_center(5, "| |  | | |_| | |_| |___) |",0x1B);
    vstr_center(6, "|_|  |_|\\__, |\\___/|____/ ",0x1B);
    vstr_center(7, "        |___/              ",0x1B);

    vstr_center(8,"Shalu Operating System v1.0",0x17);
    vstr_center(9,"Secure  |  Custom  |  Bare Metal",0x18);

    /* Login box */
    vfill(0,11,SCREEN_W,'=',0x1B);
    vput(0,11,'+',0x1B); vput(79,11,'+',0x1B);

    vfill(0,12,SCREEN_W,' ',0x17);
    vstr_center(12,"Please login to continue",0x1E);

    vfill(0,13,SCREEN_W,'-',0x1B);

    /* Username field */
    vfill(0,14,SCREEN_W,' ',0x17);
    vstr(20,14,"Username : ",0x1F);

    /* Password field */
    vfill(0,16,SCREEN_W,' ',0x17);
    vstr(20,16,"Password : ",0x1F);

    /* Hint */
    vfill(0,18,SCREEN_W,' ',0x17);
    vstr_center(18,"Default credentials: admin / sos123",0x18);

    /* Message area */
    vfill(0,19,SCREEN_W,' ',0x17);
    if(msg) vstr_center(19,msg,msg_col);

    /* Attempts */
    vfill(0,20,SCREEN_W,' ',0x17);
    if(attempt>0) {
        vstr(20,20,"Attempts remaining: ",0x1C);
        vput(40,20,'0'+(MAX_ATTEMPTS-attempt),0x0C);
        vstr(41,20," / ",0x1C);
        vput(44,20,'0'+MAX_ATTEMPTS,0x1C);
    }

    vfill(0,21,SCREEN_W,'=',0x1B);
    vput(0,21,'+',0x1B); vput(79,21,'+',0x1B);

    /* Bottom bar */
    vfill(0,24,SCREEN_W,' ',0x1F);
    vstr_center(24,"[ Press Enter to confirm each field ]",0x1F);
}

/* ── Lockout screen ──────────────────────────────────────────────────────── */
static void draw_lockout(void) {
    vclear(0x40);
    vfill(0,0,SCREEN_W,' ',0x4F);
    vstr_center(0,"!!! SYSTEM LOCKED !!!",0x4F);

    vstr_center(8, "  +--------------------------+  ",0x4E);
    vstr_center(9, "  |   ACCESS DENIED          |  ",0x4E);
    vstr_center(10,"  |   Too many failed attempts|  ",0x4E);
    vstr_center(11,"  |   System is halted.       |  ",0x4E);
    vstr_center(12,"  +--------------------------+  ",0x4E);

    vstr_center(15,"Maximum login attempts exceeded.",0x4F);
    vstr_center(16,"Please restart the machine.",0x4F);

    vfill(0,24,SCREEN_W,' ',0x4F);
    vstr_center(24,"SOS Security System",0x4F);

    /* Halt forever */
    __asm__ volatile("cli");
    while(1) __asm__ volatile("hlt");
}

/* ── Main login function ─────────────────────────────────────────────────── */
int auth_login(void) {
    char username[MAX_LEN];
    char password[MAX_LEN];

    for(int attempt=0; attempt<MAX_ATTEMPTS; attempt++) {
        const char* msg = (attempt==0) ? NULL : "Invalid username or password. Try again.";
        uint8_t msg_col = 0x0C;

        draw_login(attempt, msg, msg_col);

        /* Read username */
        read_input(31, 14, username, MAX_LEN, 0);

        /* Read password */
        read_input(31, 16, password, MAX_LEN, 1);

        /* Validate */
        if(str_eq(username, VALID_USER) && str_eq(password, VALID_PASS)) {
            /* Success screen - all black background */
            vclear(0x00);

            /* Top bar */
            vfill(0,0,SCREEN_W,' ',0x20);
            vstr_center(0,"SOS - Shalu Operating System  |  Secure Login",0x2F);

            /* Box - exactly 40 chars wide, centered at col 20 */
            vstr(20, 7,  "+--------------------------------------+", 0x0A);
            vstr(20, 8,  "|                                      |", 0x0A);
            vstr(20, 9,  "|        ACCESS GRANTED                |", 0x0A);
            vstr(20, 10, "|                                      |", 0x0A);
            vstr(20, 11, "|  Welcome back,                       |", 0x0A);
            vstr(20, 12, "|                                      |", 0x0A);
            vstr(20, 13, "|  Loading SOS...                     |", 0x0A);
            vstr(20, 14, "|                                      |", 0x0A);
            vstr(20, 15, "+--------------------------------------+", 0x0A);

            /* Print username inside the box */
            int ui=0;
            int ux=36;
            while(username[ui] && ui<16) {
                vput(ux++, 11, username[ui++], 0x0F);
            }

            /* Loading bar */
            vstr(20, 17, "  Progress: [                    ]", 0x07);
            for(int p=0;p<28;p++) {
                vput(33+p, 17, '#', 0x0A);
                for(volatile int w=0;w<3000000;w++);
            }

            vstr_center(19, "Login successful!", 0x0A);
            vstr_center(20, "Initializing SOS environment...", 0x07);

            /* Bottom bar */
            vfill(0,24,SCREEN_W,' ',0x20);
            vstr_center(24,"SOS Security System  |  2025",0x2F);

            for(volatile int w=0;w<30000000;w++);
            return 1;
        }
    }

    /* All attempts failed */
    draw_lockout();
    return 0;
}
