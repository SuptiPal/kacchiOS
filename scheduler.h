#ifndef SCHEDULER_H
#define SCHEDULER_H

void scheduler_init(void);
int scheduler_next(void);

void scheduler_yield(void);
void scheduler_start(void);

void scheduler_set_quantum(unsigned int quantum);
unsigned int scheduler_get_quantum(void);

void scheduler_tick(void);
void scheduler_age(void);

#endif