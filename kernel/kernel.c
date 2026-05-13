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
#include "session.h"
#include "../drivers/net.h"

#define MAX_CMD  80
#define CLIP_MAX 80

/* ── Global logout flag (set by logout_sos in shutdown.c) ── */
volatile int g_logout_requested = 0;

static char clipboard[CLIP_MAX];
static int  clipboard_len = 0;

/* Command counter for session saving */
static unsigned int cmd_count = 0;
static char last_cmd[MAX_CMD]  = {0};
static char current_user[32]   = "admin";

static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(port));
    return v;
}
static int strcmp_fn(const char* a, const char* b) {
    while(*a && *b && *a==*b){a++;b++;}
    return *a-*b;
}
static void scopy(char* d, const char* s, int max) {
    int i=0; while(s[i]&&i<max-1){d[i]=s[i];i++;} d[i]=0;
}
static void print_num(size_t n) {
    if(n==0){terminal_putchar('0');return;}
    char buf[20]; int i=0;
    while(n>0){buf[i++]='0'+(n%10);n/=10;}
    while(i--) terminal_putchar(buf[i]);
}
static void print_kb(size_t bytes) {
    print_num(bytes/1024); terminal_write(" KB (");
    print_num(bytes); terminal_write(" bytes)");
}

static void print_banner(void) {
    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_writeline("=====================================================");
    terminal_writeline("   SOS - Shalu Operating System  v1.0               ");
    terminal_writeline("   Type 'sos-help' for commands  |  'desk' for GUI  ");
    terminal_writeline("=====================================================");
    terminal_putchar('\n');
}

/* ── Session resume banner ───────────────────────────────────────────────── */
static void print_resume_banner(session_t* s) {
    terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_writeline("  +------------------------------------------+");
    terminal_writeline("  |       Session Restored Successfully       |");
    terminal_writeline("  +------------------------------------------+");
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
    terminal_write("  Welcome back, ");
    terminal_write(s->username);
    terminal_putchar('\n');
    terminal_write("  Commands run this session: ");
    print_num(s->cmd_count);
    terminal_putchar('\n');
    if(s->last_cmd[0]) {
        terminal_write("  Last command: ");
        terminal_writeline(s->last_cmd);
    }
    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_writeline("  +------------------------------------------+");
    terminal_putchar('\n');
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
}

static void readline(char* buf, int max);

static void do_about(void) {
    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_writeline("+--------------------------------------------------+");
    terminal_writeline("|        SOS - Shalu Operating System v1.0         |");
    terminal_writeline("+--------------------------------------------------+");
    terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_writeline("  Build Info:");
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
    terminal_writeline("    Language     : C (GNU C99) + x86 NASM Assembly");
    terminal_writeline("    Architecture : x86 32-bit Protected Mode");
    terminal_writeline("    Bootloader   : GRUB Multiboot Specification");
    terminal_writeline("    Build Tools  : GCC -m32, NASM, GNU LD, grub-mkrescue");
    terminal_writeline("    Platform     : VMware Workstation / QEMU");
    terminal_putchar('\n');
    terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_writeline("  Features:");
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
    terminal_writeline("    [*] Animated boot splash screen");
    terminal_writeline("    [*] Secure login with 3-attempt lockout");
    terminal_writeline("    [*] VGA text driver with 200-line scroll buffer");
    terminal_writeline("    [*] PS/2 keyboard polling driver");
    terminal_writeline("    [*] Heap memory manager (kmalloc / kfree)");
    terminal_writeline("    [*] CMOS real-time clock and calendar");
    terminal_writeline("    [*] Interactive CLI shell with history");
    terminal_writeline("    [*] Ctrl+C copy / Ctrl+V paste clipboard");
    terminal_writeline("    [*] GUI desk mode with arrow-key navigation");
    terminal_writeline("    [*] Session save and restore on logout");
    terminal_writeline("    [*] S/R/L/C power management menu");
    terminal_writeline("    [*] ACPI proper shutdown");
    terminal_writeline("    [*] Letter-operator calculator");
    terminal_writeline("    [*] ASCII art text generator");
    terminal_writeline("    [*] UDP network messaging (E1000 NIC driver)");
    terminal_writeline("    [*] VM-to-VM communication over Host-Only network");
    terminal_putchar('\n');
    terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    terminal_writeline("  Network:");
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
    terminal_writeline("    Protocol     : Ethernet II + ARP + IPv4 + UDP");
    terminal_writeline("    NIC          : Intel E1000 (VMware default)");
    terminal_writeline("    Port         : 5000 (UDP)");
    terminal_writeline("    VM1 IP       : 192.168.100.10");
    terminal_writeline("    VM2 IP       : 192.168.100.20");
    terminal_putchar('\n');
    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_writeline("+--------------------------------------------------+");
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
}

static void do_memstat(void) {
    size_t used,free_mem,total;
    memory_stats(&used,&free_mem,&total);
    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_writeline("  Memory Status:");
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
    terminal_write("  Total  : "); print_kb(total);    terminal_putchar('\n');
    terminal_write("  Used   : "); print_kb(used);     terminal_putchar('\n');
    terminal_write("  Free   : "); print_kb(free_mem); terminal_putchar('\n');
}

static void do_memcheck(void) {
    void *a,*b,*cc,*d;
    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_writeline("  Running SOS memory check...");
    a=kmalloc(256); b=kmalloc(512); cc=kmalloc(1024);
    terminal_setcolor((a&&b&&cc)?VGA_LIGHT_GREEN:VGA_LIGHT_RED,VGA_BLACK);
    terminal_writeline((a&&b&&cc)?"  [PASS] Allocated 256+512+1024 bytes"
                                 :"  [FAIL] Allocation failed!");
    kfree(b);
    terminal_setcolor(VGA_LIGHT_GREEN,VGA_BLACK);
    terminal_writeline("  [PASS] Freed 512 byte block");
    d=kmalloc(512);
    if(d) terminal_writeline("  [PASS] Re-allocated 512 bytes");
    kfree(a); kfree(cc); kfree(d);
    terminal_writeline("  [PASS] All blocks freed - memory healthy!");
    terminal_setcolor(VGA_WHITE,VGA_BLACK);
}

static void do_logbook(void) {
    int count=history_count_get(), show, i;
    if(count==0){terminal_writeline("  Logbook is empty.");return;}
    terminal_setcolor(VGA_LIGHT_CYAN,VGA_BLACK);
    terminal_writeline("  SOS Command Logbook (last 10):");
    terminal_setcolor(VGA_WHITE,VGA_BLACK);
    show=count<10?count:10;
    for(i=show;i>=1;i--){
        char hbuf[MAX_CMD];
        if(history_get(i,hbuf)){
            terminal_write("    ");
            print_num((size_t)(count-i+1));
            terminal_write(": ");
            terminal_writeline(hbuf);
        }
    }
}

static void do_desk(void);

static void run_command(const char* base, const char* args) {

    //sos-help command with no args shows general help, with args shows specific help for that command

    if(strcmp_fn(base,"sos-help")==0 && args[0]==0) {
        terminal_setcolor(VGA_LIGHT_CYAN,VGA_BLACK);
        terminal_writeline("  SOS Command Reference");
        terminal_writeline("  +----------------------------------------------------+");
        terminal_setcolor(VGA_LIGHT_GREEN,VGA_BLACK);
        terminal_writeline("  System:");
        terminal_setcolor(VGA_WHITE,VGA_BLACK);
        terminal_writeline("    sos-help   Show this help");
        terminal_writeline("    wipe       Clear the screen");
        terminal_writeline("    whoami     About SOS");
        terminal_writeline("    shutdown   Shutdown / Restart / Logout / Cancel");
        terminal_setcolor(VGA_LIGHT_GREEN,VGA_BLACK);
        terminal_writeline("  Information:");
        terminal_setcolor(VGA_WHITE,VGA_BLACK);
        terminal_writeline("    status     Full system dashboard");
        terminal_writeline("    runtime    System uptime");
        terminal_writeline("    memstat    Memory usage");
        terminal_writeline("    memcheck   Memory diagnostic");
        terminal_writeline("    time       Current time and date");
        terminal_writeline("    date       Monthly calendar");
        terminal_setcolor(VGA_LIGHT_GREEN,VGA_BLACK);
        terminal_writeline("  Tools:");
        terminal_setcolor(VGA_WHITE,VGA_BLACK);
        terminal_writeline("    compute    Calculator (5a3 9s4 6m7 8d2 9r4)");
        terminal_writeline("    splash     ASCII art  (splash SOS)");
        terminal_writeline("    say        Print text (say Hello)");
        terminal_writeline("    logbook    Command history");
        terminal_setcolor(VGA_LIGHT_GREEN,VGA_BLACK);
        terminal_writeline("  Interface:");
        terminal_setcolor(VGA_WHITE,VGA_BLACK);
        terminal_writeline("    desk       Open GUI desk mode");
        terminal_setcolor(VGA_LIGHT_GREEN,VGA_BLACK);
        terminal_writeline("  Network (CCN):");
        terminal_setcolor(VGA_WHITE,VGA_BLACK);
        terminal_writeline("    netstat    Show network status");
        terminal_writeline("    netstart   Start network interface (netstart 1 or netstart 2)");
        terminal_writeline("    send       Send message (send 192.168.100.20 Hi!)");
        terminal_writeline("    recv       Wait for incoming message");
        terminal_setcolor(VGA_LIGHT_GREEN,VGA_BLACK);
        terminal_writeline("  Keys:");
        terminal_setcolor(VGA_WHITE,VGA_BLACK);
        terminal_writeline("    PgUp/PgDn  Scroll terminal");
        terminal_writeline("    Tab        4 spaces");
        terminal_writeline("    Ctrl+C     Copy  |  Ctrl+V  Paste");
        terminal_writeline("    Home Scrolls to top | End scrolls to bottom");                

        //Argumented help

    } else if(strcmp_fn(base,"sos-help")==0 && args[0]!=0) {
        if     (strcmp_fn(args,"compute")==0) terminal_writeline("  compute: a=add s=sub m=mul d=div r=mod. e.g: compute 5a3");
        else if(strcmp_fn(args,"splash") ==0) terminal_writeline("  splash: Big ASCII art. e.g: splash SOS");
        else if(strcmp_fn(args,"say")    ==0) terminal_writeline("  say: Print text. e.g: say Hello World");
        else if(strcmp_fn(args,"status") ==0) terminal_writeline("  status: Full OS dashboard.");
        else if(strcmp_fn(args,"shutdown")==0)terminal_writeline("  shutdown: Shows S=Shutdown R=Restart L=Logout C=Cancel menu.");
        else if(strcmp_fn(args,"netstart")==0)terminal_writeline("  netstart: Start network interface.");
        else if(strcmp_fn(args,"send")==0)terminal_writeline("  send: Send message to the assigned IP Address. e.g: send Hi!");
        else if(strcmp_fn(args,"recv")==0)terminal_writeline("  recv: Wait for incoming message.");
        else if(strcmp_fn(args,"netstat")==0)terminal_writeline("  netstat: Show network status.");
        else if(strcmp_fn(args,"wipe")==0)terminal_writeline("  wipe: Clear the terminal screen.");
        else if(strcmp_fn(args,"memcheck")==0)terminal_writeline("  memcheck: Check memory diagnostics.");
        else if(strcmp_fn(args,"memstat")==0)terminal_writeline("  memstat: Show memory status.");
        else if(strcmp_fn(args,"whoami")==0)terminal_writeline("  whoami: Display SOS build information and features.");
        else if(strcmp_fn(args,"runtime")==0)terminal_writeline("  runtime: Show system uptime.");
        else if(strcmp_fn(args,"logbook")==0)terminal_writeline("  logbook: shows Command history (upto 10 commands).");
        else if(strcmp_fn(args,"time")==0)terminal_writeline("  time: Show current time.");
        else if(strcmp_fn(args,"date")==0)terminal_writeline("  date: Show current calendar date.");
        else if(strcmp_fn(args,"desk")==0)terminal_writeline("  desk: Open GUI desk mode.");
        
        else { terminal_write("  No help for: "); terminal_writeline(args); }

    } else if(strcmp_fn(base,"wipe")==0) {
        terminal_clear(); print_banner();

    } else if(strcmp_fn(base,"whoami")==0) {
        do_about();

    } else if(strcmp_fn(base,"time")==0) {
        terminal_setcolor(VGA_LIGHT_CYAN,VGA_BLACK);
        terminal_writeline("  ----------------------------------------");
        rtc_print_time(); rtc_print_date();
        terminal_setcolor(VGA_LIGHT_CYAN,VGA_BLACK);
        terminal_writeline("  ----------------------------------------");
        terminal_setcolor(VGA_WHITE,VGA_BLACK);

    } else if(strcmp_fn(base,"date")==0) {
        terminal_setcolor(VGA_LIGHT_CYAN,VGA_BLACK);
        terminal_writeline("  ----------------------------------------");
        rtc_print_calendar();
        terminal_setcolor(VGA_LIGHT_CYAN,VGA_BLACK);
        terminal_writeline("  ----------------------------------------");
        terminal_setcolor(VGA_WHITE,VGA_BLACK);

    } else if(strcmp_fn(base,"memstat")==0)  { do_memstat();  }
    else if(strcmp_fn(base,"memcheck")==0) { do_memcheck(); }
    else if(strcmp_fn(base,"status")==0)   { sysinfo_print(); }
    else if(strcmp_fn(base,"runtime")==0)  { uptime_print();  }
    else if(strcmp_fn(base,"logbook")==0)  { do_logbook();    }
    else if(strcmp_fn(base,"desk")==0)     { do_desk();       }

    else if(strcmp_fn(base,"say")==0) {
        terminal_writeline(args[0]?args:"");

    } else if(strcmp_fn(base,"compute")==0) {
        calc_run(args);

    } else if(strcmp_fn(base,"splash")==0) {
        banner_print(args);

    } else if(strcmp_fn(base,"netstart")==0) {
        /* netstart 1  or  netstart 2 */
        int _vmnum = (args[0]=='2') ? 2 : 1;
        terminal_setcolor(VGA_LIGHT_BROWN,VGA_BLACK);
        terminal_write("  [NET] Starting as VM");
        terminal_putchar('0'+_vmnum);
        terminal_writeline("...");
        terminal_setcolor(VGA_WHITE,VGA_BLACK);

        if(net_start(_vmnum)) {
            terminal_setcolor(VGA_LIGHT_GREEN,VGA_BLACK);
            terminal_write("  [NET] Ready!  My  IP : ");
            net_print_ip(g_net.my_ip);
            terminal_putchar('\n');
            terminal_write("  [NET] Ready!  Peer IP: ");
            net_print_ip(g_net.peer_ip);
            terminal_putchar('\n');
            terminal_write("  [NET] My MAC : ");
            int _mi;
            for(_mi=0;_mi<6;_mi++){
                uint8_t _b=g_net.my_mac[_mi];
                terminal_putchar("0123456789ABCDEF"[_b>>4]);
                terminal_putchar("0123456789ABCDEF"[_b&0xF]);
                if(_mi<5) terminal_putchar(':');
            }
            terminal_putchar('\n');
            terminal_setcolor(VGA_LIGHT_CYAN,VGA_BLACK);
            terminal_writeline("  Now use: send <message>   recv");
            terminal_setcolor(VGA_WHITE,VGA_BLACK);
        } else {
            terminal_setcolor(VGA_LIGHT_RED,VGA_BLACK);
            terminal_writeline("  [NET] NIC not found!");
            terminal_writeline("  Check VMware Network Adapter is set to Host-Only.");
            terminal_setcolor(VGA_WHITE,VGA_BLACK);
        }

    } else if(strcmp_fn(base,"send")==0) {
        /* send <message>  — sends to peer VM automatically */
        if(!g_net.ready) {
            terminal_setcolor(VGA_LIGHT_RED,VGA_BLACK);
            terminal_writeline("  [NET] Not started. Run: netstart 1  or  netstart 2");
            terminal_setcolor(VGA_WHITE,VGA_BLACK);
        } else if(!args[0]) {
            terminal_setcolor(VGA_LIGHT_RED,VGA_BLACK);
            terminal_writeline("  Usage: send <message>");
            terminal_writeline("  Example: send Hello from SOS!");
            terminal_setcolor(VGA_WHITE,VGA_BLACK);
        } else {
            terminal_setcolor(VGA_LIGHT_BROWN,VGA_BLACK);
            terminal_write("  [NET] Sending to peer (");
            net_print_ip(g_net.peer_ip);
            terminal_write("): ");
            terminal_writeline(args);
            terminal_setcolor(VGA_WHITE,VGA_BLACK);
            if(net_send(g_net.peer_ip, args)) {
                terminal_setcolor(VGA_LIGHT_GREEN,VGA_BLACK);
                terminal_writeline("  [NET] Sent!");
            } else {
                terminal_setcolor(VGA_LIGHT_RED,VGA_BLACK);
                terminal_writeline("  [NET] Send failed.");
                terminal_writeline("  Is other VM running with netstart?");
            }
            terminal_setcolor(VGA_WHITE,VGA_BLACK);
        }

    } else if(strcmp_fn(base,"recv")==0) {
        /* recv — poll for incoming messages */
        if(!g_net.ready) {
            terminal_setcolor(VGA_LIGHT_RED,VGA_BLACK);
            terminal_writeline("  [NET] Network not ready. Use: netstart to initiate Network first.");
            terminal_setcolor(VGA_WHITE,VGA_BLACK);
        } else {
            char _mbuf[256];
            terminal_setcolor(VGA_LIGHT_BROWN,VGA_BLACK);
            terminal_writeline("  [NET] Waiting for message...");
            terminal_writeline("  [NET] You have 60 seconds. Switch VM and type send.");
            terminal_setcolor(VGA_WHITE,VGA_BLACK);

            /* Count down visually so user knows time remaining */
            int _got = 0;
            int _secs;
            for(_secs = 60; _secs > 0 && !_got; _secs--) {
                /* Print countdown on same line */
                terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
                terminal_write("  Waiting: ");
                /* Print seconds remaining */
                if(_secs >= 10) {
                    terminal_putchar('0' + _secs/10);
                }
                terminal_putchar('0' + _secs%10);
                terminal_write(" seconds remaining...   \r");
                terminal_setcolor(VGA_WHITE, VGA_BLACK);

                /* Poll for ~1 second worth of iterations */
                volatile int _w;
                for(_w = 0; _w < 80000000; _w++) {
                    if(net_poll(_mbuf, 255)) { _got = 1; break; }
                }
            }
            /* Clear the countdown line */
            terminal_write("                                    \r");
            if(_got) {
                terminal_setcolor(VGA_LIGHT_GREEN,VGA_BLACK);
                terminal_write("  [NET] Message received: ");
                terminal_setcolor(VGA_WHITE,VGA_BLACK);
                terminal_writeline(_mbuf);
            } else {
                terminal_setcolor(VGA_LIGHT_GREY,VGA_BLACK);
                terminal_writeline("  [NET] No message received (timeout).");
                terminal_setcolor(VGA_WHITE,VGA_BLACK);
            }
        }

    } else if(strcmp_fn(base,"netstat")==0) {
        terminal_setcolor(VGA_LIGHT_CYAN,VGA_BLACK);
        terminal_writeline("  SOS Network Status:");
        terminal_setcolor(VGA_WHITE,VGA_BLACK);
        terminal_write("  Ready   : "); terminal_writeline(g_net.ready?"Yes":"No - run netstart 1 or netstart 2");
        terminal_write("  My IP   : "); if(g_net.my_ip) net_print_ip(g_net.my_ip); else terminal_write("Not set"); terminal_putchar('\n');
        terminal_write("  Peer IP : "); if(g_net.peer_ip) net_print_ip(g_net.peer_ip); else terminal_write("Not set"); terminal_putchar('\n');
        terminal_write("  Port    : 5000 (UDP)\n");
        terminal_write("  Network : Host-Only\n");
        terminal_setcolor(VGA_LIGHT_GREY,VGA_BLACK);
        terminal_writeline("  Commands: netstart 1/2   send <msg>   recv   netstat");
        terminal_setcolor(VGA_WHITE,VGA_BLACK);

    } else if(strcmp_fn(base,"shutdown")==0) {
        shutdown();
        /* If we get here, user chose Cancel or Logout */
        /* Logout is handled via g_logout_requested flag */

    } else if(base[0]==0) {
        /* empty */
    } else {
        terminal_setcolor(VGA_LIGHT_RED,VGA_BLACK);
        terminal_write("  Unknown command: "); terminal_writeline(base);
        terminal_setcolor(VGA_WHITE,VGA_BLACK);
        terminal_writeline("  Type 'sos-help' for commands.");
    }
}

static void do_desk(void) {
    while(1) {
        gui_action_t action = gui_run();
        char tmp[4];
        if(action==GUI_SWITCH_CLI) {
            terminal_init(); print_banner();
            terminal_setcolor(VGA_LIGHT_GREEN,VGA_BLACK);
            terminal_writeline("[SOS] Returned to terminal.");
            terminal_putchar('\n');
            terminal_setcolor(VGA_WHITE,VGA_BLACK);
            break;
        }
        if      (action==GUI_CLOCK)    { terminal_init();rtc_print_time();rtc_print_date();terminal_writeline("\nPress Enter to return...");readline(tmp,2); }
        else if (action==GUI_CALENDAR) { terminal_init();rtc_print_calendar();terminal_writeline("\nPress Enter to return...");readline(tmp,2); }
        else if (action==GUI_MEMINFO)  { terminal_init();do_memstat();terminal_writeline("\nPress Enter to return...");readline(tmp,2); }
        else if (action==GUI_MEMTEST)  { terminal_init();do_memcheck();terminal_writeline("\nPress Enter to return...");readline(tmp,2); }
        else if (action==GUI_ABOUT)    { terminal_init();do_about();terminal_writeline("\nPress Enter to return...");readline(tmp,2); }
        else if (action==GUI_HISTORY)  { terminal_init();do_logbook();terminal_writeline("\nPress Enter to return...");readline(tmp,2); }
        else if (action==GUI_CLEAR)    { terminal_clear();terminal_writeline("Press Enter...");readline(tmp,2); }
        else if (action==GUI_HALT) {
            /* GUI power menu */
            session_save(last_cmd, cmd_count, current_user);
            int choice = shutdown_confirm_gui();
            if(choice==POWER_SHUTDOWN) { shutdown_do(); }
            else if(choice==POWER_RESTART) { restart(); }
            else if(choice==POWER_LOGOUT)  { logout_sos(); break; }
            /* else cancel — stay in GUI */
        }
    }
}

static void readline(char* buf, int max) {
    int     i=0, ctrl_dn=0, extended=0;
    uint8_t sc=0;
    char    c=0;
    buf[0]=0;
    while(1) {
        while(!(inb(0x64)&0x01));
        sc=inb(0x60);
        if(sc==0xE0){extended=1;continue;}
        if(sc==0x1D){ctrl_dn=1;continue;}
        if(sc==0x9D){ctrl_dn=0;continue;}
        if(extended){
            extended=0;
            if(sc&0x80) continue;
            if(sc==0x49){terminal_scroll_up(5);continue;}
            if(sc==0x51){terminal_scroll_down(5);continue;}
            if(sc==0x48){terminal_scroll_up(1);continue;}
            if(sc==0x50){terminal_scroll_down(1);continue;}
            if(sc==0x47){terminal_scroll_up(999);continue;}
            if(sc==0x4F){terminal_scroll_bottom();continue;}
            continue;
        }
        if(sc&0x80) continue;
        terminal_scroll_bottom();
        if(sc==0x1C){buf[i]=0;terminal_putchar('\n');break;}
        if(sc==0x0E){if(i>0){i--;buf[i]=0;terminal_erase_char();}continue;}
        if(sc==0x0F){int s;for(s=0;s<4&&i<max-1;s++){buf[i++]=' ';terminal_putchar(' ');}buf[i]=0;continue;}
        if(ctrl_dn&&sc==0x2E){int j;clipboard_len=i;for(j=0;j<i;j++)clipboard[j]=buf[j];clipboard[i]=0;terminal_setcolor(VGA_LIGHT_GREY,VGA_BLACK);terminal_write(" [copied]");terminal_setcolor(VGA_WHITE,VGA_BLACK);ctrl_dn=0;continue;}
        if(ctrl_dn&&sc==0x2F){int j;for(j=0;j<clipboard_len&&i<max-1;j++){buf[i++]=clipboard[j];terminal_putchar(clipboard[j]);}buf[i]=0;ctrl_dn=0;continue;}
        ctrl_dn=0;
        c=keyboard_getchar_sc(sc);
        if(c&&i<max-1){buf[i++]=c;buf[i]=0;terminal_putchar(c);}
    }
}

/* ── Shell loop — extracted so we can re-enter after logout ─────────────── */
static void run_shell(void) {
    char cmd[MAX_CMD], base[MAX_CMD], args[MAX_CMD];
    int bi,ai,idx;

    print_banner();

    terminal_setcolor(VGA_LIGHT_GREEN,VGA_BLACK);
    terminal_writeline("[SOS] Kernel loaded");
    terminal_writeline("[SOS] VGA driver ready");
    terminal_writeline("[SOS] Keyboard ready");
    terminal_writeline("[SOS] Memory manager ready");
    terminal_writeline("[SOS] RTC clock ready");
    terminal_writeline("[SOS] Security module active");
    terminal_writeline("[SOS] All systems nominal");
    terminal_putchar('\n');

    rtc_print_time();
    terminal_putchar('\n');

    terminal_setcolor(VGA_WHITE,VGA_BLACK);
    terminal_writeline("Type 'sos-help' for commands. Type 'desk' for GUI.");
    terminal_putchar('\n');

    g_logout_requested = 0;

    while(!g_logout_requested) {
        terminal_setcolor(VGA_LIGHT_GREEN,VGA_BLACK);
        terminal_write("sos> ");
        terminal_setcolor(VGA_WHITE,VGA_BLACK);

        readline(cmd, MAX_CMD);
        if(cmd[0]==0) continue;

        /* Update session tracking */
        cmd_count++;
        scopy(last_cmd, cmd, MAX_CMD);
        history_add(cmd);

        /* Split base + args */
        bi=0; ai=0; idx=0;
        while(cmd[idx]&&cmd[idx]!=' ') base[bi++]=cmd[idx++];
        base[bi]=0;
        while(cmd[idx]==' ') idx++;
        while(cmd[idx]) args[ai++]=cmd[idx++];
        args[ai]=0;

        run_command(base, args);

        /* Save session after every command so logout always has latest */
        if(!g_logout_requested)
            session_save(last_cmd, cmd_count, current_user);
    }
}

/* ══════════════════════════════════════════════════════════════════════════
   KERNEL MAIN
   Outer loop handles logout — re-runs login without full reboot
   ══════════════════════════════════════════════════════════════════════════ */
void kernel_main(void) {
    terminal_init();

    /* One-time boot sequence */
    splash_show();
    uptime_init();
    memory_init();
    keyboard_init();

    /* ── Outer login loop — re-entered on logout ── */
    while(1) {
        terminal_clear();

        /* Login */
        auth_login();

        /* After login — check if session exists to restore */
        terminal_clear();
        for(volatile int w=0;w<20000000;w++);

        if(session_exists()) {
            session_t s;
            session_load(&s);
            /* Restore cmd_count and last_cmd */
            cmd_count = s.cmd_count;
            scopy(last_cmd,   s.last_cmd,  MAX_CMD);
            scopy(current_user, s.username, 32);
            session_clear();

            /* Show terminal with session restore info */
            terminal_init();
            print_banner();
            print_resume_banner(&s);
        } else {
            terminal_init();
            /* Fresh login */
            cmd_count = 0;
            last_cmd[0] = 0;
        }

        /* Run the shell — returns when g_logout_requested = 1 */
        run_shell();

        /* If logout was requested, loop back to login screen */
        /* Session was already saved inside shutdown() before logout_sos() */
    }
}
