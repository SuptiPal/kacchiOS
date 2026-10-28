#include "scheduler.h"
#include "process.h"

#define AGING_INTERVAL 3

static unsigned int kernel_esp;
static unsigned int time_quantum = 3;
static unsigned int elapsed_ticks = 0;

extern void context_switch(
    unsigned int *old_esp,
    unsigned int new_esp
);

void scheduler_init(void)
{
    current_pid = -1;
    kernel_esp = 0;
    time_quantum = 3;
    elapsed_ticks = 0;
}

void scheduler_set_quantum(unsigned int quantum)
{
    if (quantum > 0)
        time_quantum = quantum;
}

unsigned int scheduler_get_quantum(void)
{
    return time_quantum;
}

void scheduler_age(void)
{
    for (int i = 0; i < MAX_PROCESSES; i++) {
        pcb *p = &processes[i];

        if (!p->active || p->state != READY)
            continue;

        p->waiting_time++;

        if (p->waiting_time >= AGING_INTERVAL) {
            if (p->priority < MAX_PRIORITY)
                p->priority++;

            p->waiting_time = 0;
        }
    }
}

int scheduler_next(void)
{
    int best_pid = -1;
    int best_priority = -1;

    for (int i = 1; i <= MAX_PROCESSES; i++) {
        int pid = (current_pid + i) % MAX_PROCESSES;

        pcb *p = &processes[pid];

        if (!p->active || p->state != READY)
            continue;

        if (p->priority > best_priority) {
            best_priority = p->priority;
            best_pid = pid;
        }
    }

    return best_pid;
}

void scheduler_yield(void)
{
    int old = current_pid;

    if (old >= 0 && processes[old].state == CURRENT) {
        processes[old].state = READY;
        processes[old].priority =
            processes[old].base_priority;
        processes[old].waiting_time = 0;
    }

    scheduler_age();

    int next = scheduler_next();

    if (next < 0) {
        if (old >= 0 &&
            processes[old].state == READY) {
            processes[old].state = CURRENT;
            return;
        }

        if (old >= 0) {
            current_pid = -1;

            context_switch(
                &processes[old].esp,
                kernel_esp
            );
        }

        return;
    }

    if (next == old) {
        processes[next].state = CURRENT;
        return;
    }

    current_pid = next;
    processes[next].state = CURRENT;
    processes[next].waiting_time = 0;

    elapsed_ticks = 0;

    if (old < 0) {
        context_switch(
            &kernel_esp,
            processes[next].esp
        );
    } else {
        context_switch(
            &processes[old].esp,
            processes[next].esp
        );
    }
}

void scheduler_tick(void)
{
    if (current_pid < 0)
        return;

    elapsed_ticks++;
    processes[current_pid].cpu_ticks++;

    if (elapsed_ticks >= time_quantum) {
        elapsed_ticks = 0;
        scheduler_yield();
    }
}

void scheduler_start(void)
{
    scheduler_yield();
}