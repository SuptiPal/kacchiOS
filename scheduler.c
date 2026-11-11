#include "scheduler.h"
#include "process.h"
#include "timer.h"
#include "io.h"

#define AGING_INTERVAL 10

static unsigned int idle_esp = 0;
static unsigned int quantum = 3;
static unsigned int elapsed = 0;

static unsigned int idle_saved = 0;

void scheduler_init(void)
{
    current_pid = -1;
    idle_esp = 0;
    idle_saved = 0;
    quantum = 3;
    elapsed = 0;
}

void scheduler_set_quantum(unsigned int ticks)
{
    if (ticks == 0)
        return;

    unsigned int flags;
    __asm__ volatile(
        "pushfl; popl %0; cli"
        : "=r"(flags) : : "memory"
    );

    quantum = ticks;

    if (flags & 0x200)
        __asm__ volatile("sti" ::: "memory");
}

unsigned int scheduler_get_quantum(void)
{
    return quantum;
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
    int best = -1;
    int best_priority = -1;

    /*
     * Starting after current_pid gives round-robin
     * selection among equal-priority tasks.
     */
    for (int i = 1; i <= MAX_PROCESSES; i++) {
        int pid = (current_pid + i) % MAX_PROCESSES;
        pcb *p = &processes[pid];

        if (!p->active || p->state != READY)
            continue;

        if (p->priority > best_priority) {
            best_priority = p->priority;
            best = pid;
        }
    }

    return best;
}

unsigned int scheduler_interrupt(
    unsigned int saved_esp,
    unsigned int is_timer
)
{
    int old = current_pid;

    if (is_timer) {
        timer_increment();

        /*
         * Acknowledge IRQ0 before returning
         * to an interruptible context.
         */
        outb(0x20, 0x20);

        scheduler_age();
    }

    /*
     * First software interrupt saves the
     * kernel/idle execution context.
     */
    if (old < 0) {
        idle_esp = saved_esp;
        idle_saved = 1;
    } else {
        processes[old].esp = saved_esp;

        if (is_timer)
            processes[old].cpu_ticks++;
    }

    /*
     * Continue the current task if its quantum
     * has not expired and no higher-priority
     * task is ready.
     */
    if (is_timer && old >= 0 &&
        processes[old].state == CURRENT) {

        elapsed++;

        int best = scheduler_next();

        if (elapsed < quantum &&
            (best < 0 ||
             processes[best].priority <=
             processes[old].priority)) {
            return saved_esp;
        }
    }

    if (old >= 0 &&
        processes[old].state == CURRENT) {
        processes[old].state = READY;

        /* Remove temporary aging boost after use. */
        processes[old].priority =
            processes[old].base_priority;
        processes[old].waiting_time = 0;
    }

    int next = scheduler_next();

    if (next >= 0) {
        current_pid = next;
        processes[next].state = CURRENT;
        processes[next].waiting_time = 0;
        elapsed = 0;

        return processes[next].esp;
    }

    /*
     * If the previous process is still runnable,
     * resume it.
     */
    if (old >= 0 &&
        processes[old].state == READY) {
        current_pid = old;
        processes[old].state = CURRENT;
        elapsed = 0;

        return processes[old].esp;
    }

    /*
     * No runnable processes: resume idle context.
     */
    current_pid = -1;
    elapsed = 0;

    if (idle_saved)
        return idle_esp;

    return saved_esp;
}

void scheduler_yield(void)
{
    __asm__ volatile("int $0x30" ::: "memory");
}

void scheduler_start(void)
{
    /*
     * The first interrupt saves the kernel
     * context and starts the first READY task.
     */
    __asm__ volatile("sti; int $0x30" ::: "memory");
}