#include "idt.h"
#include <stdint.h>

#define IDT_ENTRIES 256

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t   idt_ptr;

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low  = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].selector  = sel;
    idt[num].zero      = 0;
    idt[num].flags     = flags;
}

/* Default handler for all unhandled interrupts */
void default_handler(void) {}

__asm__(
    ".global default_irq_handler\n"
    "default_irq_handler:\n"
    "  pusha\n"
    "  call default_handler\n"
    "  popa\n"
    "  iret\n"
);
extern void default_irq_handler(void);

void idt_init(void) {
    idt_ptr.limit = sizeof(idt_entry_t) * IDT_ENTRIES - 1;
    idt_ptr.base  = (uint32_t)&idt;

    /* Fill ALL entries with a safe default handler first */
    for (int i = 0; i < IDT_ENTRIES; i++)
        idt_set_gate(i, (uint32_t)default_irq_handler, 0x08, 0x8E);

    __asm__ volatile("lidt %0" : : "m"(idt_ptr));
}
