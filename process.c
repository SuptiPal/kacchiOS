#include "process.h"
#include "memory.h"
#include "scheduler.h"

pcb processes[MAX_PROCESSES];

int current_pid = -1;

void process_init(void)
{
    for (int i = 0; i < MAX_PROCESSES; i++) {
        pcb *p = &processes[i];

        p->pid = i;
        p->state = TERMINATED;
        p->entry = 0;
        p->stack = 0;
        p->esp = 0;
        p->active = 0;

        p->priority = DEFAULT_PRIORITY;
        p->base_priority = DEFAULT_PRIORITY;
        p->waiting_time = 0;
        p->cpu_ticks = 0;
    }
}

pcb *process_get(int pid)
{
    if (pid < 0 || pid >= MAX_PROCESSES)
        return 0;

    if (!processes[pid].active)
        return 0;

    return &processes[pid];
}

static void process_trampoline(void)
{
    processes[current_pid].entry();
    process_exit();

    for (;;)
        __asm__ volatile("cli; hlt");
}

int process_create(void (*entry)(void))
{
    if (!entry)
        return -1;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        pcb *p = &processes[i];

        if (p->active)
            continue;

        void *stack = kmalloc(STACK_SIZE);

        if (!stack)
            return -1;

        unsigned int *top = (unsigned int *)
            ((unsigned char *)stack + STACK_SIZE);

        /*
         * Initial frame expected by switch.S:
         * return address, EBP, EBX, ESI, EDI.
         */
        *--top = (unsigned int)process_trampoline;
        *--top = 0;
        *--top = 0;
        *--top = 0;
        *--top = 0;

        p->pid = i;
        p->state = READY;
        p->entry = entry;
        p->stack = stack;
        p->esp = (unsigned int)top;
        p->active = 1;

        p->priority = DEFAULT_PRIORITY;
        p->base_priority = DEFAULT_PRIORITY;
        p->waiting_time = 0;
        p->cpu_ticks = 0;

        return i;
    }

    return -1;
}

int process_terminate(int pid)
{
    pcb *p = process_get(pid);

    if (!p || pid == current_pid)
        return -1;

    p->state = TERMINATED;

    if (p->stack)
        kfree(p->stack);

    p->stack = 0;
    p->esp = 0;
    p->entry = 0;
    p->active = 0;

    return 0;
}

void process_exit(void)
{
    if (current_pid < 0)
        return;

    processes[current_pid].state = TERMINATED;

    /*
     * Switch away before reclaiming this stack.
     * The kernel will reclaim it later.
     */
    scheduler_yield();

    for (;;)
        __asm__ volatile("cli; hlt");
}

int process_set_priority(int pid, int priority)
{
    pcb *p = process_get(pid);

    if (!p)
        return -1;

    if (priority < MIN_PRIORITY ||
        priority > MAX_PRIORITY)
        return -1;

    p->priority = priority;
    p->base_priority = priority;
    p->waiting_time = 0;

    return 0;
}

int process_get_priority(int pid)
{
    pcb *p = process_get(pid);

    if (!p)
        return -1;

    return p->priority;
}

int process_block(int pid)
{
    pcb *p = process_get(pid);

    if (!p || p->state == TERMINATED)
        return -1;

    if (p->state == BLOCKED)
        return -1;

    if (pid == current_pid) {
        p->state = BLOCKED;
        scheduler_yield();
        return 0;
    }

    if (p->state != READY)
        return -1;

    p->state = BLOCKED;
    p->waiting_time = 0;

    return 0;
}

int process_unblock(int pid)
{
    pcb *p = process_get(pid);

    if (!p || p->state != BLOCKED)
        return -1;

    p->state = READY;
    p->waiting_time = 0;

    return 0;
}

int process_count(void)
{
    int count = 0;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].active)
            count++;
    }

    return count;
}

process_state process_get_state(int pid)
{
    pcb *p = process_get(pid);

    if (!p)
        return TERMINATED;

    return p->state;
}