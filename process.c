#include "process.h"
#include "memory.h"
#include "scheduler.h"
#include "interrupts.h"

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
        __asm__ volatile("hlt");
}

int process_create(void (*entry)(void))
{
    if (!entry)
        return -1;

    unsigned int flags;
    __asm__ volatile(
        "pushfl; popl %0; cli"
        : "=r"(flags) : : "memory"
    );

    for (int i = 0; i < MAX_PROCESSES; i++) {
        pcb *p = &processes[i];

        if (p->active)
            continue;

        void *stack = kmalloc(STACK_SIZE);

        if (!stack) {
            if (flags & 0x200)
                interrupts_enable();
            return -1;
        }

        unsigned int *sp = (unsigned int *)
            ((unsigned char *)stack + STACK_SIZE);

        /*
         * iret frame:
         * EFLAGS, CS, EIP (pushed in reverse order)
         */
        *--sp = 0x202;  /* EFLAGS: IF enabled */
        *--sp = 0x08;   /* Kernel code selector */
        *--sp = (unsigned int)process_trampoline;

        /*
         * pushal frame, read by popal:
         * EDI ESI EBP old-ESP EBX EDX ECX EAX
         */
        for (int j = 0; j < 8; j++)
            *--sp = 0;

        p->pid = i;
        p->state = READY;
        p->entry = entry;
        p->stack = stack;
        p->esp = (unsigned int)sp;
        p->active = 1;

        p->priority = DEFAULT_PRIORITY;
        p->base_priority = DEFAULT_PRIORITY;
        p->waiting_time = 0;
        p->cpu_ticks = 0;

        if (flags & 0x200)
            interrupts_enable();

        return i;
    }

    if (flags & 0x200)
        interrupts_enable();

    return -1;
}

int process_terminate(int pid)
{
    unsigned int flags;
    __asm__ volatile(
        "pushfl; popl %0; cli"
        : "=r"(flags) : : "memory"
    );

    pcb *p = process_get(pid);

    if (!p || pid == current_pid) {
        if (flags & 0x200)
            interrupts_enable();
        return -1;
    }

    p->state = TERMINATED;

    if (p->stack)
        kfree(p->stack);

    p->stack = 0;
    p->esp = 0;
    p->entry = 0;
    p->active = 0;

    if (flags & 0x200)
        interrupts_enable();

    return 0;
}

void process_exit(void)
{
    if (current_pid < 0)
        return;

    interrupts_disable();

    processes[current_pid].state = TERMINATED;

    scheduler_yield();

    for (;;)
        __asm__ volatile("cli; hlt");
}

int process_set_priority(int pid, int priority)
{
    if (priority < MIN_PRIORITY ||
        priority > MAX_PRIORITY)
        return -1;

    unsigned int flags;
    __asm__ volatile(
        "pushfl; popl %0; cli"
        : "=r"(flags) : : "memory"
    );

    pcb *p = process_get(pid);

    if (!p) {
        if (flags & 0x200)
            interrupts_enable();
        return -1;
    }

    p->priority = priority;
    p->base_priority = priority;
    p->waiting_time = 0;

    if (flags & 0x200)
        interrupts_enable();

    return 0;
}

int process_get_priority(int pid)
{
    pcb *p = process_get(pid);
    return p ? p->priority : -1;
}

int process_block(int pid)
{
    unsigned int flags;
    __asm__ volatile(
        "pushfl; popl %0; cli"
        : "=r"(flags) : : "memory"
    );

    pcb *p = process_get(pid);

    if (!p || p->state == TERMINATED ||
        p->state == BLOCKED) {
        if (flags & 0x200)
            interrupts_enable();
        return -1;
    }

    if (pid == current_pid) {
        p->state = BLOCKED;
        scheduler_yield();

        if (flags & 0x200)
            interrupts_enable();

        return 0;
    }

    if (p->state != READY) {
        if (flags & 0x200)
            interrupts_enable();
        return -1;
    }

    p->state = BLOCKED;
    p->waiting_time = 0;

    if (flags & 0x200)
        interrupts_enable();

    return 0;
}

int process_unblock(int pid)
{
    unsigned int flags;
    __asm__ volatile(
        "pushfl; popl %0; cli"
        : "=r"(flags) : : "memory"
    );

    pcb *p = process_get(pid);

    if (!p || p->state != BLOCKED) {
        if (flags & 0x200)
            interrupts_enable();
        return -1;
    }

    p->state = READY;
    p->waiting_time = 0;

    if (flags & 0x200)
        interrupts_enable();

    return 0;
}

int process_count(void)
{
    int count = 0;

    for (int i = 0; i < MAX_PROCESSES; i++)
        if (processes[i].active)
            count++;

    return count;
}

process_state process_get_state(int pid)
{
    pcb *p = process_get(pid);
    return p ? p->state : TERMINATED;
}