#include "calc.h"
#include "../drivers/vga.h"

/* ── SOS Calculator ──────────────────────────────────────────────────────
   Operators:
     a = addition        (e.g. 5a3  = 8)
     s = subtraction     (e.g. 9s4  = 5)
     m = multiplication  (e.g. 6m7  = 42)
     d = division        (e.g. 8d2  = 4)
     r = remainder/mod   (e.g. 9r4  = 1)
   Chaining supported:  5a3m2 = 16
   ─────────────────────────────────────────────────────────────────────── */

static void print_int(int n) {
    if (n < 0) { terminal_putchar('-'); n = -n; }
    if (n == 0) { terminal_putchar('0'); return; }
    char buf[20]; int i = 0;
    while (n > 0) { buf[i++] = '0' + n % 10; n /= 10; }
    while (i--) terminal_putchar(buf[i]);
}

static int is_digit(char c) { return c >= '0' && c <= '9'; }
static int is_op(char c) {
    return c=='a' || c=='s' || c=='m' || c=='d' || c=='r';
}

static int parse_num(const char* p, int* out, int* len) {
    int neg = 0, val = 0, count = 0;
    if (*p == '-') { neg = 1; p++; count++; }
    if (!is_digit(*p)) return 0;
    while (is_digit(*p)) {
        val = val * 10 + (*p - '0');
        p++; count++;
    }
    *out = neg ? -val : val;
    *len = count;
    return 1;
}

void calc_run(const char* expr) {
    if (!expr || !*expr) {
        terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
        terminal_writeline("  SOS Compute - Usage:");
        terminal_setcolor(VGA_WHITE, VGA_BLACK);
        terminal_writeline("    compute 5a3   -> 5 + 3 = 8");
        terminal_writeline("    compute 9s4   -> 9 - 4 = 5");
        terminal_writeline("    compute 6m7   -> 6 x 7 = 42");
        terminal_writeline("    compute 8d2   -> 8 / 2 = 4");
        terminal_writeline("    compute 9r4   -> 9 mod 4 = 1");
        terminal_writeline("    compute 2a3m4 -> chained: (2+3)*4");
        terminal_setcolor(VGA_LIGHT_GREY, VGA_BLACK);
        terminal_writeline("  Operators: a=add s=sub m=mul d=div r=mod");
        terminal_setcolor(VGA_WHITE, VGA_BLACK);
        return;
    }

    const char* p = expr;
    int result = 0, num = 0, nlen = 0;

    /* Parse first number */
    if (!parse_num(p, &result, &nlen)) {
        terminal_setcolor(VGA_LIGHT_RED, VGA_BLACK);
        terminal_writeline("  Error: Expression must start with a number");
        terminal_writeline("  Example: compute 5a3");
        terminal_setcolor(VGA_WHITE, VGA_BLACK);
        return;
    }
    p += nlen;

    /* Process operator + number pairs */
    while (*p && is_op(*p)) {
        char op = *p++;
        if (!parse_num(p, &num, &nlen)) {
            terminal_setcolor(VGA_LIGHT_RED, VGA_BLACK);
            terminal_writeline("  Error: Expected number after operator");
            terminal_setcolor(VGA_WHITE, VGA_BLACK);
            return;
        }
        p += nlen;

        if (op == 'a') result = result + num;
        else if (op == 's') result = result - num;
        else if (op == 'm') result = result * num;
        else if (op == 'd') {
            if (num == 0) {
                terminal_setcolor(VGA_LIGHT_RED, VGA_BLACK);
                terminal_writeline("  Error: Cannot divide by zero");
                terminal_setcolor(VGA_WHITE, VGA_BLACK);
                return;
            }
            result = result / num;
        }
        else if (op == 'r') {
            if (num == 0) {
                terminal_setcolor(VGA_LIGHT_RED, VGA_BLACK);
                terminal_writeline("  Error: Cannot mod by zero");
                terminal_setcolor(VGA_WHITE, VGA_BLACK);
                return;
            }
            result = result % num;
        }
    }

    if (*p != '\0') {
        terminal_setcolor(VGA_LIGHT_RED, VGA_BLACK);
        terminal_write("  Error: Unexpected character '");
        terminal_putchar(*p);
        terminal_writeline("'");
        terminal_writeline("  Use: a s m d r as operators");
        terminal_setcolor(VGA_WHITE, VGA_BLACK);
        return;
    }

    /* Show result */
    terminal_setcolor(VGA_LIGHT_CYAN, VGA_BLACK);
    terminal_write("  ");
    terminal_write(expr);
    terminal_setcolor(VGA_LIGHT_GREY, VGA_BLACK);
    terminal_write("  ==>  ");
    terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    print_int(result);
    terminal_putchar('\n');
    terminal_setcolor(VGA_WHITE, VGA_BLACK);
}
