#ifndef NET_H
#define NET_H

#include <stdint.h>

#define VM1_IP_STR  "192.168.100.10"
#define VM2_IP_STR  "192.168.100.20"
#define SOS_PORT    5000

typedef struct {
    int      ready;
    uint32_t my_ip;
    uint8_t  my_mac[6];
    uint8_t  peer_mac[6];
    uint32_t peer_ip;
} net_state_t;

extern net_state_t g_net;

int      net_start(int vm_num);   /* 1 = VM1, 2 = VM2 */
int      net_init(void);
int      net_send(uint32_t dst_ip, const char* msg);
int      net_poll(char* buf, int max);
uint32_t net_parse_ip(const char* s);
void     net_print_ip(uint32_t ip);

#endif
