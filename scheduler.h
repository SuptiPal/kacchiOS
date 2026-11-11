#ifndef SCHEDULER_H
#define SCHEDULER_H

void scheduler_init(void);
int scheduler_next(void);

void scheduler_yield(void);
void scheduler_start(void);

void scheduler_set_quantum(unsigned int ticks);
unsigned int scheduler_get_quantum(void);

void scheduler_age(void);

unsigned int scheduler_get_context_switches(void);
void scheduler_reset_context_switches(void);

/* Called from interrupt_stubs.S */
unsigned int scheduler_interrupt(
    unsigned int saved_esp,
    unsigned int is_timer
);

#endif