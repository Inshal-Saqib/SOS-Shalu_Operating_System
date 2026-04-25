#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

void keyboard_init(void);
char keyboard_getchar(void);
char keyboard_getchar_sc(uint8_t sc);

#endif
