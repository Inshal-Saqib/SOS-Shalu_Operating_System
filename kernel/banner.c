#include "banner.h"
#include "../drivers/vga.h"

/* 5-row tall ASCII font for A-Z, 0-9, space */
/* Each char is 6 columns wide */
static const char* font[37][5] = {
    /* A */
    {" *** ","*   *","*****","*   *","*   *"},
    /* B */
    {"**** ","*   *","**** ","*   *","**** "},
    /* C */
    {" *** ","*    ","*    ","*    "," *** "},
    /* D */
    {"***  ","*  * ","*   *","*  * ","***  "},
    /* E */
    {"*****","*    ","***  ","*    ","*****"},
    /* F */
    {"*****","*    ","***  ","*    ","*    "},
    /* G */
    {" *** ","*    ","*  **","*   *"," *** "},
    /* H */
    {"*   *","*   *","*****","*   *","*   *"},
    /* I */
    {" *** ","  *  ","  *  ","  *  "," *** "},
    /* J */
    {"  ***","   * ","   * ","*  * "," **  "},
    /* K */
    {"*   *","*  * ","***  ","*  * ","*   *"},
    /* L */
    {"*    ","*    ","*    ","*    ","*****"},
    /* M */
    {"*   *","** **","* * *","*   *","*   *"},
    /* N */
    {"*   *","**  *","* * *","*  **","*   *"},
    /* O */
    {" *** ","*   *","*   *","*   *"," *** "},
    /* P */
    {"**** ","*   *","**** ","*    ","*    "},
    /* Q */
    {" *** ","*   *","* * *","*  **"," ****"},
    /* R */
    {"**** ","*   *","**** ","*  * ","*   *"},
    /* S */
    {" ****","*    "," *** ","    *","**** "},
    /* T */
    {"*****","  *  ","  *  ","  *  ","  *  "},
    /* U */
    {"*   *","*   *","*   *","*   *"," *** "},
    /* V */
    {"*   *","*   *","*   *"," * * ","  *  "},
    /* W */
    {"*   *","*   *","* * *","** **","*   *"},
    /* X */
    {"*   *"," * * ","  *  "," * * ","*   *"},
    /* Y */
    {"*   *"," * * ","  *  ","  *  ","  *  "},
    /* Z */
    {"*****","   * ","  *  "," *   ","*****"},
    /* 0 */
    {" *** ","*  **","* * *","**  *"," *** "},
    /* 1 */
    {"  *  "," **  ","  *  ","  *  "," *** "},
    /* 2 */
    {" *** ","*   *","  ** ","  *  ","*****"},
    /* 3 */
    {"**** ","    *"," *** ","    *","**** "},
    /* 4 */
    {"*   *","*   *","*****","    *","    *"},
    /* 5 */
    {"*****","*    ","**** ","    *","**** "},
    /* 6 */
    {" *** ","*    ","**** ","*   *"," *** "},
    /* 7 */
    {"*****","    *","   * ","  *  ","  *  "},
    /* 8 */
    {" *** ","*   *"," *** ","*   *"," *** "},
    /* 9 */
    {" *** ","*   *"," ****","    *"," *** "},
    /* SPACE (index 36) */
    {"     ","     ","     ","     ","     "},
};

static int char_index(char c) {
    if(c>='A'&&c<='Z') return c-'A';
    if(c>='a'&&c<='z') return c-'a';
    if(c>='0'&&c<='9') return 26+(c-'0');
    return 36; /* space */
}

static uint8_t banner_colors[] = {
    VGA_LIGHT_CYAN, VGA_LIGHT_GREEN, VGA_LIGHT_RED,
    VGA_LIGHT_MAGENTA, VGA_LIGHT_BROWN, VGA_LIGHT_BLUE
};

void banner_print(const char* text) {
    if(!text||!*text) {
        terminal_setcolor(VGA_LIGHT_RED,VGA_BLACK);
        terminal_writeline("  Usage: banner <text>");
        terminal_writeline("  Example: banner MYOS");
        terminal_setcolor(VGA_WHITE,VGA_BLACK);
        return;
    }

    /* Count length (max 10 chars to fit screen) */
    int len=0;
    while(text[len]&&len<10) len++;

    terminal_putchar('\n');

    /* Print each row of the banner */
    for(int row=0;row<5;row++) {
        terminal_setcolor(banner_colors[row%6], VGA_BLACK);
        terminal_write("  ");
        for(int ci=0;ci<len;ci++) {
            int idx=char_index(text[ci]);
            terminal_write(font[idx][row]);
            terminal_putchar(' ');
        }
        terminal_putchar('\n');
    }
    terminal_putchar('\n');
    terminal_setcolor(VGA_WHITE,VGA_BLACK);
}
