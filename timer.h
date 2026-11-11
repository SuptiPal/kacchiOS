#ifndef TIMER_H
#define TIMER_H

#define TIMER_FREQUENCY 100

void timer_init(unsigned int frequency);
unsigned int timer_get_ticks(void);
void timer_increment(void);

#endif