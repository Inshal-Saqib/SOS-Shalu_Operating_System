#include "keyboard.h"
#include <stdint.h>

static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static const char sc_map[128] = {
    0,  0, '1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n',  0,
    'a','s','d','f','g','h','j','k','l',';','\'','`',  0, '\\',
    'z','x','c','v','b','n','m',',','.','/',  0, '*',  0,  ' ',
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0
};

void keyboard_init(void) {}

char keyboard_getchar(void) {
    while (1) {
        while (!(inb(0x64) & 0x01));
        uint8_t sc = inb(0x60);
        if (sc & 0x80) continue;
        char c = (sc < 128) ? sc_map[sc] : 0;
        if (c) return c;
    }
}

char keyboard_getchar_sc(uint8_t sc) {
    if (sc & 0x80) return 0;
    return (sc < 128) ? sc_map[sc] : 0;
}
