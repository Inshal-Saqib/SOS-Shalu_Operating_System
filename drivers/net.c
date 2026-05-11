#include "net.h"
#include "../drivers/vga.h"
#include <stdint.h>

net_state_t g_net = { 0, 0, {0}, {0}, 0 };

/* VM1 MAC from your vmx: 00:0c:29:19:37:98 */
static uint8_t VM1_MAC[6] = {0x00,0x0C,0x29,0x19,0x37,0x98};
/* VM2 MAC: broadcast until we detect it via ARP reply */
static uint8_t VM2_MAC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

#define VM1_IP 0x0A64A8C0UL
#define VM2_IP 0x1464A8C0UL

static inline uint8_t  inb(uint16_t p){ uint8_t  v; __asm__ volatile("inb %1,%0":"=a"(v):"Nd"(p)); return v; }
static inline uint32_t inl(uint16_t p){ uint32_t v; __asm__ volatile("inl %1,%0":"=a"(v):"Nd"(p)); return v; }
static inline void outl(uint16_t p,uint32_t v){ __asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p)); }
static inline void iowait(void){ for(volatile int i=0;i<1000;i++); }

#define PCI_ADDR 0xCF8
#define PCI_DATA 0xCFC

static uint32_t pci_r(uint8_t bus,uint8_t dev,uint8_t fn,uint8_t off){
    outl(PCI_ADDR,0x80000000|((uint32_t)bus<<16)|((uint32_t)dev<<11)|((uint32_t)fn<<8)|(off&0xFC));
    return inl(PCI_DATA);
}
static void pci_w(uint8_t bus,uint8_t dev,uint8_t fn,uint8_t off,uint32_t v){
    outl(PCI_ADDR,0x80000000|((uint32_t)bus<<16)|((uint32_t)dev<<11)|((uint32_t)fn<<8)|(off&0xFC));
    outl(PCI_DATA,v);
}

static uint32_t ebase=0;
static uint8_t  ebus=0,edev=0;

#define E1000_CTRL  0x0000
#define E1000_IMC   0x00D8
#define E1000_RCTL  0x0100
#define E1000_TCTL  0x0400
#define E1000_TIPG  0x0410
#define E1000_RDBAL 0x2800
#define E1000_RDBAH 0x2804
#define E1000_RDLEN 0x2808
#define E1000_RDH   0x2810
#define E1000_RDT   0x2818
#define E1000_TDBAL 0x3800
#define E1000_TDBAH 0x3804
#define E1000_TDLEN 0x3808
#define E1000_TDH   0x3810
#define E1000_TDT   0x3818
#define E1000_MTA   0x5200
#define E1000_RAL   0x5400
#define E1000_RAH   0x5404

static uint32_t er(uint32_t o){ return *((volatile uint32_t*)(ebase+o)); }
static void     ew(uint32_t o,uint32_t v){ *((volatile uint32_t*)(ebase+o))=v; }

#define NUM_RX 8
#define NUM_TX 8
#define BUF_SZ 2048

typedef struct __attribute__((packed)){
    uint64_t addr;
    uint16_t len;
    uint16_t csum;
    uint8_t  status;
    uint8_t  err;
    uint16_t special;
} rx_desc_t;

typedef struct __attribute__((packed)){
    uint64_t addr;
    uint16_t len;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} tx_desc_t;

static rx_desc_t rxd[NUM_RX] __attribute__((aligned(16)));
static tx_desc_t txd[NUM_TX] __attribute__((aligned(16)));
static uint8_t   rxb[NUM_RX][BUF_SZ] __attribute__((aligned(16)));
static uint8_t   txb[NUM_TX][BUF_SZ] __attribute__((aligned(16)));
static int rxc=0,txc=0;

static int find_e1000(void){
    uint8_t bus,dev;
    for(bus=0;bus<16;bus++){
        for(dev=0;dev<32;dev++){
            uint32_t id=pci_r(bus,dev,0,0);
            if(id==0xFFFFFFFF) continue;
            uint16_t vendor=id&0xFFFF, device=id>>16;
            if(vendor==0x8086){
                if(device==0x100E||device==0x100F||device==0x10D3||
                   device==0x10EA||device==0x1502||device==0x1503||
                   device==0x107C||device==0x10A7||device==0x10BC||
                   device==0x10F5||device==0x10C9||device==0x10E6){
                    uint32_t bar0=pci_r(bus,dev,0,0x10);
                    ebase=(bar0&0xFFFFFFF0);
                    ebus=bus; edev=dev;
                    return 1;
                }
            }
        }
    }
    return 0;
}

static int e1000_setup(void){
    int i;
    /* Enable bus master + memory */
    uint32_t cmd=pci_r(ebus,edev,0,0x04);
    pci_w(ebus,edev,0,0x04,cmd|0x06);

    /* Save MAC before reset */
    uint32_t ral=er(E1000_RAL);
    uint32_t rah=er(E1000_RAH);

    /* Reset */
    ew(E1000_CTRL, er(E1000_CTRL)|0x04000000);
    for(volatile int w=0;w<1000000;w++);

    /* Disable interrupts */
    ew(E1000_IMC,0xFFFFFFFF);

    /* Link up */
    uint32_t ctrl=er(E1000_CTRL);
    ctrl|=0x40; ctrl|=0x20; ctrl&=~0x08; ctrl&=~0x80;
    ew(E1000_CTRL,ctrl);

    /* Clear multicast table */
    for(i=0;i<128;i++) ew(E1000_MTA+i*4,0);

    /* Restore MAC */
    ew(E1000_RAL,ral);
    ew(E1000_RAH,rah|0x80000000);

    /* Extract MAC bytes */
    int mac_ok=0;
    g_net.my_mac[0]= ral     &0xFF; g_net.my_mac[1]=(ral>>8) &0xFF;
    g_net.my_mac[2]=(ral>>16)&0xFF; g_net.my_mac[3]=(ral>>24)&0xFF;
    g_net.my_mac[4]= rah     &0xFF; g_net.my_mac[5]=(rah>>8) &0xFF;
    for(i=0;i<6;i++) if(g_net.my_mac[i]){mac_ok=1;break;}

    /* If MAC still zero use hardcoded one */
    if(!mac_ok){
        uint8_t *src=(g_net.my_ip==VM1_IP)?VM1_MAC:VM2_MAC;
        for(i=0;i<6;i++) g_net.my_mac[i]=src[i];
    }

    /* RX ring */
    for(i=0;i<NUM_RX;i++){ rxd[i].addr=(uint32_t)rxb[i]; rxd[i].status=0; }
    ew(E1000_RDBAL,(uint32_t)rxd); ew(E1000_RDBAH,0);
    ew(E1000_RDLEN,NUM_RX*sizeof(rx_desc_t));
    ew(E1000_RDH,0); ew(E1000_RDT,NUM_RX-1);
    ew(E1000_RCTL,0x04008002|(1<<15)|(1<<26));

    /* TX ring */
    for(i=0;i<NUM_TX;i++){ txd[i].addr=(uint32_t)txb[i]; txd[i].status=0x01; }
    ew(E1000_TDBAL,(uint32_t)txd); ew(E1000_TDBAH,0);
    ew(E1000_TDLEN,NUM_TX*sizeof(tx_desc_t));
    ew(E1000_TDH,0); ew(E1000_TDT,0);
    ew(E1000_TCTL,0x0003F0FA);
    ew(E1000_TIPG,0x0060200A);

    return 1;
}

static uint16_t ip_cksum(uint16_t *d,int l){
    uint32_t s=0;
    while(l>1){s+=*d++;l-=2;}
    if(l) s+=*(uint8_t*)d;
    while(s>>16) s=(s&0xFFFF)+(s>>16);
    return (uint16_t)(~s);
}

static int build_frame(uint8_t *f,uint8_t *dm,uint32_t dip,const char *msg,int ml){
    uint16_t udp=(uint16_t)(8+ml);
    uint16_t ip=(uint16_t)(20+udp);
    uint16_t tot=(uint16_t)(14+ip);
    int p=0,i;
    for(i=0;i<6;i++) f[p++]=dm[i];
    for(i=0;i<6;i++) f[p++]=g_net.my_mac[i];
    f[p++]=0x08; f[p++]=0x00;
    f[p++]=0x45; f[p++]=0x00;
    f[p++]=(ip>>8)&0xFF; f[p++]=ip&0xFF;
    f[p++]=0x00; f[p++]=0x01;
    f[p++]=0x40; f[p++]=0x00;
    f[p++]=0x40; f[p++]=0x11;
    f[p++]=0x00; f[p++]=0x00;
    f[p++]=(g_net.my_ip)   &0xFF; f[p++]=(g_net.my_ip>>8) &0xFF;
    f[p++]=(g_net.my_ip>>16)&0xFF; f[p++]=(g_net.my_ip>>24)&0xFF;
    f[p++]=(dip)   &0xFF; f[p++]=(dip>>8) &0xFF;
    f[p++]=(dip>>16)&0xFF; f[p++]=(dip>>24)&0xFF;
    uint16_t ck=ip_cksum((uint16_t*)(f+14),20);
    f[24]=(ck>>8)&0xFF; f[25]=ck&0xFF;
    f[p++]=0x13; f[p++]=0x88;
    f[p++]=0x13; f[p++]=0x88;
    f[p++]=(udp>>8)&0xFF; f[p++]=udp&0xFF;
    f[p++]=0x00; f[p++]=0x00;
    for(i=0;i<ml;i++) f[p++]=(uint8_t)msg[i];
    while(tot<60){f[p++]=0;tot++;}
    return p;
}

static int e1000_send(uint8_t *frame,int flen){
    int i;
    for(i=0;i<500000;i++){ if(txd[txc].status&0x01) break; iowait(); }
    uint8_t *buf=txb[txc];
    for(i=0;i<flen;i++) buf[i]=frame[i];
    txd[txc].len=(uint16_t)flen;
    txd[txc].cmd=0x0B;
    txd[txc].status=0;
    int tail=(txc+1)%NUM_TX;
    ew(E1000_TDT,(uint32_t)tail);
    for(i=0;i<500000;i++){ if(txd[txc].status&0x01) break; iowait(); }
    txc=tail;
    return 1;
}

static int e1000_recv(uint8_t *out,int max){
    if(!(rxd[rxc].status&0x01)) return 0;
    int len=(int)rxd[rxc].len;
    if(len>max) len=max;
    int i; for(i=0;i<len;i++) out[i]=rxb[rxc][i];
    rxd[rxc].status=0;
    ew(E1000_RDT,(uint32_t)rxc);
    rxc=(rxc+1)%NUM_RX;
    return len;
}

int net_start(int vm_num){
    int i;
    if(!find_e1000()) return 0;
    if(vm_num==2){
        g_net.my_ip  =VM2_IP;
        g_net.peer_ip=VM1_IP;
        for(i=0;i<6;i++) g_net.peer_mac[i]=VM1_MAC[i];
    } else {
        g_net.my_ip  =VM1_IP;
        g_net.peer_ip=VM2_IP;
        for(i=0;i<6;i++) g_net.peer_mac[i]=VM2_MAC[i];
    }
    if(!e1000_setup()) return 0;
    g_net.ready=1;
    return 1;
}

int net_init(void){ return net_start(1); }

int net_send(uint32_t dst_ip,const char *msg){
    if(!g_net.ready) return 0;
    int ml=0; while(msg[ml]&&ml<1400) ml++;
    uint8_t *dm=(dst_ip==VM1_IP)?VM1_MAC:VM2_MAC;
    uint8_t frame[1514];
    int fl=build_frame(frame,dm,dst_ip,msg,ml);
    return e1000_send(frame,fl);
}

int net_poll(char *buf,int max){
    if(!g_net.ready) return 0;
    uint8_t frame[2048];
    int len=e1000_recv(frame,sizeof(frame));
    if(len<42) return 0;
    if(frame[12]!=0x08||frame[13]!=0x00) return 0;
    if(frame[23]!=0x11) return 0;
    int ihl=(frame[14]&0x0F)*4;
    int uo=14+ihl;
    if(uo+8>len) return 0;
    uint16_t dp=((uint16_t)frame[uo+2]<<8)|frame[uo+3];
    if(dp!=5000) return 0;
    int po=uo+8;
    int pl=((int)frame[uo+4]<<8|frame[uo+5])-8;
    if(pl<=0||pl>max-1) return 0;
    int i; for(i=0;i<pl;i++) buf[i]=(char)frame[po+i];
    buf[pl]=0;
    return 1;
}

uint32_t net_parse_ip(const char *s){
    uint32_t ip=0; int shift=0;
    while(*s&&shift<=24){
        uint32_t oct=0;
        while(*s>='0'&&*s<='9'){oct=oct*10+(uint32_t)(*s-'0');s++;}
        if(oct>255) oct=255;
        ip|=(oct<<shift); shift+=8;
        if(*s=='.') s++;
    }
    return ip;
}

void net_print_ip(uint32_t ip){
    int i;
    for(i=0;i<4;i++){
        uint8_t b=(uint8_t)(ip>>(i*8));
        char tmp[4]; int len=0;
        if(!b){tmp[len++]='0';}
        else{uint8_t n=b;while(n){tmp[len++]='0'+n%10;n/=10;}}
        int j; for(j=len-1;j>=0;j--) terminal_putchar(tmp[j]);
        if(i<3) terminal_putchar('.');
    }
}
