#include "interrupts.h"
#include "io.h"

#define IDT_SIZE 256

struct idt_entry {
    unsigned short offset_low;
    unsigned short selector;
    unsigned char zero;
    unsigned char flags;
    unsigned short offset_high;
} __attribute__((packed));

struct idt_pointer {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

static struct idt_entry idt[IDT_SIZE];
static struct idt_pointer idtp;

extern void irq0_stub(void);
extern void yield_stub(void);
extern void exception_stub(void);

static void set_gate(int vector, unsigned int handler)
{
    idt[vector].offset_low = handler & 0xFFFF;
    idt[vector].selector = 0x08;
    idt[vector].zero = 0;
    idt[vector].flags = 0x8E;
    idt[vector].offset_high = handler >> 16;
}

static void pic_remap(void)
{
    outb(0x20, 0x11);
    outb(0xA0, 0x11);

    outb(0x21, 0x20);
    outb(0xA1, 0x28);

    outb(0x21, 0x04);
    outb(0xA1, 0x02);

    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    /* Unmask timer IRQ0 only. */
    outb(0x21, 0xFE);
    outb(0xA1, 0xFF);
}

void interrupts_init(void)
{
    interrupts_disable();

    for (int i = 0; i < IDT_SIZE; i++)
        set_gate(i, (unsigned int)exception_stub);

    set_gate(32, (unsigned int)irq0_stub);
    set_gate(48, (unsigned int)yield_stub);

    idtp.limit = sizeof(idt) - 1;
    idtp.base = (unsigned int)idt;

    pic_remap();

    __asm__ volatile("lidt %0" : : "m"(idtp));
}

void interrupts_enable(void)
{
    __asm__ volatile("sti" ::: "memory");
}

void interrupts_disable(void)
{
    __asm__ volatile("cli" ::: "memory");
}