#include "timer.h"
#include "io.h"

#define PIT_FREQUENCY 1193182

static volatile unsigned int ticks = 0;

void timer_init(unsigned int frequency)
{
    if (frequency < 19)
        frequency = 19;

    if (frequency > PIT_FREQUENCY)
        frequency = PIT_FREQUENCY;

    unsigned int divisor = PIT_FREQUENCY / frequency;

    if (divisor < 1)
        divisor = 1;

    if (divisor > 65535)
        divisor = 65535;

    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
}

unsigned int timer_get_ticks(void)
{
    return ticks;
}

void timer_increment(void)
{
    ticks++;
}