#include "keyboard.h"
#include <stdint.h>

static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static const char scancode_map[128] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n',0,
    'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\',
    'z','x','c','v','b','n','m',',','.','/',0,'*',0,' ',
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0
};

void keyboard_init(void) {
    /* No interrupts needed - we use polling */
}

char keyboard_getchar(void) {
    uint8_t scancode;
    char c = 0;

    while (!c) {
        /* Wait until keyboard buffer has data (bit 0 of port 0x64) */
        while (!(inb(0x64) & 0x01));

        scancode = inb(0x60);

        /* Only handle key DOWN events (bit 7 = 0) */
        if (!(scancode & 0x80)) {
            c = scancode_map[scancode & 0x7F];
        }
    }
    return c;
}
