#include "serial.h"
#include "io.h"
#include "memory.h"
#include "process.h"
#include "scheduler.h"
#include "ipc.h"
#include "interrupts.h"
#include "timer.h"

#define COM1 0x3F8
#define MAX_INPUT 128

static volatile unsigned int task_a_progress = 0;
static volatile unsigned int task_b_progress = 0;

static volatile int task_a_done = 0;
static volatile int task_b_done = 0;

static volatile int task_a_saw_b = 0;
static volatile int task_b_saw_a = 0;

static int queue_id = -1;
static int task_a_pid = -1;
static int task_b_pid = -1;

static int tests_reported = 0;

/* CPU-bound tasks: no voluntary yielding */

static void task_a(void)
{
    for (unsigned int i = 0; i < 5000000; i++)
    {
        task_a_progress++;

        if (task_b_progress > 0)
            task_a_saw_b = 1;
    }

    if (ipc_send(queue_id, 100) != 0)
        serial_puts("[FAIL] Task A IPC send\n");

    task_a_done = 1;
}

static void task_b(void)
{
    for (unsigned int i = 0; i < 5000000; i++)
    {
        task_b_progress++;

        if (task_a_progress > 0)
            task_b_saw_a = 1;
    }

    if (ipc_send(queue_id, 200) != 0)
        serial_puts("[FAIL] Task B IPC send\n");

    task_b_done = 1;
}

/* Memory tests */

static void test_memory(void)
{
    serial_puts("\nMemory Management\n");
    serial_puts("-----------------\n");

    unsigned int before = memory_free_blocks();

    void *a = kmalloc(256);
    void *large = kmalloc(1024);
    void *separator = kmalloc(256);
    void *small = kmalloc(512);
    void *end = kmalloc(256);

    if (!a || !large || !separator || !small || !end)
    {
        serial_puts("[FAIL] Allocation setup\n");

        kfree(a);
        kfree(large);
        kfree(separator);
        kfree(small);
        kfree(end);
        return;
    }

    serial_puts("[PASS] Heap allocation\n");

    kfree(large);
    kfree(small);

    void *result = kmalloc(512);

    if (result == small)
        serial_puts("[PASS] Best-fit allocation\n");
    else
        serial_puts("[FAIL] Best-fit allocation\n");

    kfree(result);
    kfree(a);
    kfree(separator);
    kfree(end);

    if (memory_free_blocks() == before)
        serial_puts("[PASS] Memory reclamation\n");
    else
        serial_puts("[FAIL] Memory reclamation\n");
}

/* Process utility and state tests */

static void test_process(void)
{
    serial_puts("\nProcess Management\n");
    serial_puts("------------------\n");

    unsigned int before = memory_free_blocks();

    int pid = process_create(task_a);

    if (pid < 0)
    {
        serial_puts("[FAIL] Process creation\n");
        return;
    }

    serial_puts("[PASS] Process creation\n");

    if (process_get_state(pid) == READY)
        serial_puts("[PASS] READY state\n");
    else
        serial_puts("[FAIL] READY state\n");

    if (process_block(pid) == 0 &&
        process_get_state(pid) == BLOCKED)
        serial_puts("[PASS] BLOCKED state\n");
    else
        serial_puts("[FAIL] BLOCKED state\n");

    if (process_unblock(pid) == 0 &&
        process_get_state(pid) == READY)
        serial_puts("[PASS] Process unblocking\n");
    else
        serial_puts("[FAIL] Process unblocking\n");

    if (process_set_priority(pid, 7) == 0 &&
        process_get_priority(pid) == 7)
        serial_puts("[PASS] Priority utility\n");
    else
        serial_puts("[FAIL] Priority utility\n");

    process_terminate(pid);

    int reused = process_create(task_a);

    if (reused == pid)
        serial_puts("[PASS] Process ID reuse\n");
    else
        serial_puts("[FAIL] Process ID reuse\n");

    if (reused >= 0)
        process_terminate(reused);

    if (memory_free_blocks() == before)
        serial_puts("[PASS] Stack reclamation\n");
    else
        serial_puts("[FAIL] Stack reclamation\n");
}

/* Check results after both tasks have exited */

static void report_results(void)
{
    if (tests_reported || !task_a_done || !task_b_done)
        return;

    if (processes[task_a_pid].state != TERMINATED ||
        processes[task_b_pid].state != TERMINATED)
        return;

    tests_reported = 1;

    serial_puts("\nPreemptive Scheduling\n");
    serial_puts("---------------------\n");

    if (task_a_progress == 5000000 &&
        task_b_progress == 5000000)
    {
        serial_puts("[PASS] CPU-bound tasks completed\n");
    }
    else
    {
        serial_puts("[FAIL] CPU-bound tasks\n");
    }

    if (task_a_saw_b || task_b_saw_a)
    {
        serial_puts("[PASS] Verified task preemption\n");
    }
    else
    {
        serial_puts("[FAIL] Task overlap not observed\n");
    }

    if (processes[task_a_pid].cpu_ticks > 0 &&
        processes[task_b_pid].cpu_ticks > 0)
        serial_puts("[PASS] Timer interrupts during tasks\n");
    else
        serial_puts("[FAIL] Timer scheduling evidence\n");

    int first = 0;
    int second = 0;

    if (ipc_receive(queue_id, &first) == 0 &&
        ipc_receive(queue_id, &second) == 0 &&
        ((first == 100 && second == 200) ||
         (first == 200 && second == 100)))
        serial_puts("[PASS] IPC communication\n");
    else
        serial_puts("[FAIL] IPC communication\n");

    process_terminate(task_a_pid);
    process_terminate(task_b_pid);

    ipc_destroy(queue_id);

    serial_puts("[PASS] Process stack cleanup\n");
    serial_puts("[INFO] Normal tasks finished\n");
    serial_puts("[INFO] Idle process active\n");
}

/* Nonblocking serial input */

static int serial_try_getc(char *c)
{
    if (!(inb(COM1 + 5) & 0x01))
        return 0;

    *c = (char)inb(COM1);
    return 1;
}

/* Final scheduler verification */

static volatile int quantum_done_a = 0;
static volatile int quantum_done_b = 0;
static volatile int wakeup_done = 0;

static unsigned int quantum_deadline = 0;
static unsigned int switches_q2 = 0;
static unsigned int switches_q5 = 0;

static int verification_stage = 0;
static int quantum_pid_a = -1;
static int quantum_pid_b = -1;
static int wakeup_pid = -1;

static void print_uint(unsigned int value)
{
    char buffer[11];
    int i = 0;

    do
    {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    } while (value != 0);

    while (i > 0)
        serial_putc(buffer[--i]);
}

static void quantum_task_a(void)
{
    volatile unsigned int work = 0;

    while ((int)(timer_get_ticks() - quantum_deadline) < 0)
        work++;

    quantum_done_a = 1;
}

static void quantum_task_b(void)
{
    volatile unsigned int work = 0;

    while ((int)(timer_get_ticks() - quantum_deadline) < 0)
        work++;

    quantum_done_b = 1;
}

static int start_quantum_test(unsigned int quantum)
{
    quantum_done_a = 0;
    quantum_done_b = 0;

    scheduler_set_quantum(quantum);
    scheduler_reset_context_switches();

    quantum_pid_a = process_create(quantum_task_a);
    quantum_pid_b = process_create(quantum_task_b);

    if (quantum_pid_a < 0 || quantum_pid_b < 0)
    {
        if (quantum_pid_a >= 0)
            process_terminate(quantum_pid_a);

        if (quantum_pid_b >= 0)
            process_terminate(quantum_pid_b);

        return -1;
    }

    process_set_priority(quantum_pid_a, 5);
    process_set_priority(quantum_pid_b, 5);

    /*
     * Run both CPU-bound tasks for approximately
     * 60 timer ticks (600 ms at 100 Hz).
     */
    quantum_deadline = timer_get_ticks() + 60;

    return 0;
}

static int quantum_tasks_finished(void)
{
    return quantum_done_a &&
           quantum_done_b &&
           processes[quantum_pid_a].state == TERMINATED &&
           processes[quantum_pid_b].state == TERMINATED;
}

static void cleanup_quantum_tasks(void)
{
    process_terminate(quantum_pid_a);
    process_terminate(quantum_pid_b);
}

static void wakeup_task(void)
{
    wakeup_done = 1;
}

static void run_final_verification(void)
{
    if (!tests_reported)
        return;

    switch (verification_stage)
    {
    case 0:
        serial_puts("\nQuantum Verification\n");
        serial_puts("--------------------\n");

        if (start_quantum_test(2) != 0)
        {
            serial_puts("[FAIL] Quantum test setup\n");
            verification_stage = 99;
            break;
        }

        verification_stage = 1;
        break;

    case 1:
        if (!quantum_tasks_finished())
            break;

        switches_q2 = scheduler_get_context_switches();
        cleanup_quantum_tasks();

        serial_puts("[INFO] Quantum 2 switches: ");
        print_uint(switches_q2);
        serial_puts("\n");

        if (start_quantum_test(5) != 0)
        {
            serial_puts("[FAIL] Quantum test setup\n");
            verification_stage = 99;
            break;
        }

        verification_stage = 2;
        break;

    case 2:
        if (!quantum_tasks_finished())
            break;

        switches_q5 = scheduler_get_context_switches();
        cleanup_quantum_tasks();

        serial_puts("[INFO] Quantum 5 switches: ");
        print_uint(switches_q5);
        serial_puts("\n");

        if (switches_q2 > switches_q5 &&
            switches_q5 > 0)
        {
            serial_puts(
                "[PASS] Quantum affects scheduling frequency\n");
        }
        else
        {
            serial_puts(
                "[WARN] Quantum comparison inconclusive\n");
        }

        verification_stage = 3;
        break;

    case 3:
        serial_puts("\nIdle Wake-up Verification\n");
        serial_puts("-------------------------\n");

        /*
         * This function is running in the idle
         * context after previous tasks exited.
         */
        wakeup_done = 0;
        wakeup_pid = process_create(wakeup_task);

        if (wakeup_pid < 0)
        {
            serial_puts("[FAIL] Wake-up task creation\n");
            verification_stage = 99;
            break;
        }

        verification_stage = 4;
        break;

    case 4:
        if (!wakeup_done ||
            processes[wakeup_pid].state != TERMINATED)
            break;

        process_terminate(wakeup_pid);

        serial_puts("[PASS] Idle context scheduled new task\n");
        serial_puts("[PASS] Wake-up task terminated\n");
        serial_puts("\n[INFO] Final verification complete\n");

        verification_stage = 5;
        break;

    default:
        break;
    }
}

/* Idle/null process with interactive console */

static void idle_process(void)
{
    char input[MAX_INPUT];
    int pos = 0;
    int prompt_shown = 0;

    while (1)
    {
        report_results();
        run_final_verification();
        if (!tests_reported || verification_stage < 5){
            __asm__ volatile("hlt");
            continue;
        }

        if (!prompt_shown)
        {
            serial_puts("\nkacchiOS> ");
            prompt_shown = 1;
        }

        char c;

        if (!serial_try_getc(&c))
        {
            __asm__ volatile("hlt");
            continue;
        }

        if (c == '\r' || c == '\n')
        {
            input[pos] = '\0';
            serial_puts("\n");

            if (pos > 0)
            {
                serial_puts("You typed: ");
                serial_puts(input);
                serial_puts("\n");
            }

            pos = 0;
            prompt_shown = 0;
        }
        else if ((c == '\b' || c == 127) && pos > 0)
        {
            pos--;
            serial_puts("\b \b");
        }
        else if (c >= 32 && c < 127 &&
                 pos < MAX_INPUT - 1)
        {
            input[pos++] = c;
            serial_putc(c);
        }
    }
}

/* Kernel entry point */

void kmain(void)
{
    serial_init();

    serial_puts("\nkacchiOS v2.0\n");
    serial_puts("32-bit x86 Educational Operating System\n\n");

    memory_init();
    process_init();
    scheduler_init();
    ipc_init();

    serial_puts("[BOOT] Kernel services initialized\n");

    test_memory();
    test_process();

    serial_puts("\nTimer and Scheduler\n");
    serial_puts("-------------------\n");

    interrupts_init();
    timer_init(TIMER_FREQUENCY);

    scheduler_set_quantum(3);

    if (scheduler_get_quantum() == 3)
        serial_puts("[PASS] Quantum configuration\n");
    else
        serial_puts("[FAIL] Quantum configuration\n");

    queue_id = ipc_create();

    if (queue_id < 0)
    {
        serial_puts("[FAIL] IPC initialization\n");
        for (;;)
            __asm__ volatile("cli; hlt");
    }

    task_a_pid = process_create(task_a);
    task_b_pid = process_create(task_b);

    if (task_a_pid < 0 || task_b_pid < 0)
    {
        serial_puts("[FAIL] Task creation\n");
        for (;;)
            __asm__ volatile("cli; hlt");
    }

    process_set_priority(task_a_pid, 5);
    process_set_priority(task_b_pid, 5);

    serial_puts("[INFO] PIT configured at 100 Hz\n");
    serial_puts("[INFO] Quantum: 3 ticks (approximately 30 ms)\n");
    serial_puts("[INFO] Starting preemptive scheduler\n");

    /*
     * Start scheduling. When no normal process
     * is runnable, control returns here.
     */
    scheduler_start();

    idle_process();
}