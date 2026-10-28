#include "serial.h"
#include "memory.h"
#include "process.h"
#include "scheduler.h"
#include "ipc.h"

#define MAX_INPUT 128

static int message_queue = -1;
static int ipc_test_passed = 1;
static int task_a_done = 0;
static int task_b_done = 0;

/* Process tasks */

static void task_a(void)
{
    for (int i = 1; i <= 3; i++) {
        serial_puts("[TASK 1] Sending message\n");

        if (ipc_send(message_queue, i * 100) != 0) {
            ipc_test_passed = 0;
            serial_puts("[FAIL] IPC send\n");
        }

        scheduler_yield();
    }

    task_a_done = 1;
}

static void task_b(void)
{
    for (int i = 1; i <= 3; i++) {
        int message = 0;

        if (ipc_receive(message_queue, &message) == 0 &&
            message == i * 100) {
            serial_puts("[TASK 2] Message received\n");
        } else {
            ipc_test_passed = 0;
            serial_puts("[FAIL] IPC receive\n");
        }

        scheduler_yield();
    }

    task_b_done = 1;
}

/* Memory management tests */

static void test_memory(void)
{
    serial_puts("\nMemory Management\n");
    serial_puts("-----------------\n");

    unsigned int before = memory_free_blocks();

    void *p = kmalloc(300);

    if (p && memory_free_blocks() == before - 2)
        serial_puts("[PASS] Heap allocation\n");
    else
        serial_puts("[FAIL] Heap allocation\n");

    kfree(p);

    if (memory_free_blocks() == before)
        serial_puts("[PASS] Heap deallocation\n");
    else
        serial_puts("[FAIL] Heap deallocation\n");

    void *a = kmalloc(256);
    void *b = kmalloc(512);
    void *c = kmalloc(256);

    if (!a || !b || !c) {
        serial_puts("[FAIL] Memory test setup\n");

        kfree(a);
        kfree(b);
        kfree(c);
        return;
    }

    kfree(b);

    void *reused = kmalloc(512);

    if (reused == b)
        serial_puts("[PASS] First-fit memory reuse\n");
    else
        serial_puts("[FAIL] First-fit memory reuse\n");

    kfree(reused);
    kfree(a);
    kfree(c);

    if (memory_free_blocks() == before)
        serial_puts("[PASS] Memory cleanup\n");
    else
        serial_puts("[FAIL] Memory cleanup\n");
}

/* Process management tests */

static void test_process(void)
{
    serial_puts("\nProcess Management\n");
    serial_puts("------------------\n");

    unsigned int before = memory_free_blocks();

    int pid = process_create(task_a);

    if (pid < 0) {
        serial_puts("[FAIL] Process creation\n");
        return;
    }

    serial_puts("[PASS] Process creation\n");

    if (process_count() == 1)
        serial_puts("[PASS] Process table\n");
    else
        serial_puts("[FAIL] Process table\n");

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
        serial_puts("[PASS] Process priority utility\n");
    else
        serial_puts("[FAIL] Process priority utility\n");

    if (process_terminate(pid) == 0)
        serial_puts("[PASS] Process termination\n");
    else
        serial_puts("[FAIL] Process termination\n");

    int reused = process_create(task_a);

    if (reused == pid)
        serial_puts("[PASS] Process ID reuse\n");
    else
        serial_puts("[FAIL] Process ID reuse\n");

    if (reused >= 0)
        process_terminate(reused);

    if (memory_free_blocks() == before)
        serial_puts("[PASS] Process stack reclamation\n");
    else
        serial_puts("[FAIL] Process stack reclamation\n");
}

/* Scheduler tests */

static void test_scheduler(void)
{
    serial_puts("\nScheduler\n");
    serial_puts("---------\n");

    int a = process_create(task_a);
    int b = process_create(task_b);

    if (a < 0 || b < 0) {
        serial_puts("[FAIL] Scheduler test setup\n");

        if (a >= 0)
            process_terminate(a);

        if (b >= 0)
            process_terminate(b);

        return;
    }

    process_set_priority(a, 2);
    process_set_priority(b, 5);

    if (scheduler_next() == b)
        serial_puts("[PASS] Priority scheduling\n");
    else
        serial_puts("[FAIL] Priority scheduling\n");

    int previous = process_get_priority(a);

    scheduler_age();
    scheduler_age();
    scheduler_age();

    if (process_get_priority(a) > previous)
        serial_puts("[PASS] Priority aging\n");
    else
        serial_puts("[FAIL] Priority aging\n");

    scheduler_set_quantum(3);

    if (scheduler_get_quantum() == 3)
        serial_puts("[PASS] Quantum configuration\n");
    else
        serial_puts("[FAIL] Quantum configuration\n");

    process_terminate(a);
    process_terminate(b);
}

/* Interactive console */

static void null_process(void)
{
    char input[MAX_INPUT];

    serial_puts("\nInteractive Console\n");
    serial_puts("-------------------\n");
    serial_puts("[INFO] Console ready\n");
    serial_puts("[INFO] Type a message and press Enter\n\n");

    while (1) {
        int pos = 0;

        serial_puts("kacchiOS> ");

        while (1) {
            char c = serial_getc();

            if (c == '\r' || c == '\n') {
                input[pos] = '\0';
                serial_puts("\n");
                break;
            }

            if ((c == '\b' || c == 127) && pos > 0) {
                pos--;
                serial_puts("\b \b");
            } else if (c >= 32 && c < 127 &&
                       pos < MAX_INPUT - 1) {
                input[pos++] = c;
                serial_putc(c);
            }
        }

        if (pos > 0) {
            serial_puts("You typed: ");
            serial_puts(input);
            serial_puts("\n");
        }
    }
}

/* Kernel entry point */

void kmain(void)
{
    serial_init();

    serial_puts("\nkacchiOS v1.0\n");
    serial_puts("Minimal x86 Operating System\n\n");

    serial_puts("[BOOT] Initializing kernel services...\n");

    memory_init();
    process_init();
    scheduler_init();
    ipc_init();

    serial_puts("[ OK ] Memory manager initialized\n");
    serial_puts("[ OK ] Process manager initialized\n");
    serial_puts("[ OK ] Scheduler initialized\n");
    serial_puts("[ OK ] IPC initialized\n");

    test_memory();
    test_process();
    test_scheduler();

    serial_puts("\nIPC and Scheduling\n");
    serial_puts("------------------\n");

    message_queue = ipc_create();

    if (message_queue < 0) {
        serial_puts("[FAIL] IPC queue creation\n");

        for (;;)
            __asm__ volatile("cli; hlt");
    }

    int a = process_create(task_a);
    int b = process_create(task_b);

    if (a < 0 || b < 0) {
        serial_puts("[FAIL] Task creation\n");

        if (a >= 0)
            process_terminate(a);

        if (b >= 0)
            process_terminate(b);

        ipc_destroy(message_queue);

        for (;;)
            __asm__ volatile("cli; hlt");
    }

    process_set_priority(a, 5);
    process_set_priority(b, 5);

    serial_puts("[INFO] Starting cooperative scheduler\n");

    scheduler_start();

    if (task_a_done && task_b_done &&
        processes[a].state == TERMINATED &&
        processes[b].state == TERMINATED)
        serial_puts("[PASS] Process execution and termination\n");
    else
        serial_puts("[FAIL] Process execution or termination\n");

    process_terminate(a);
    process_terminate(b);

    if (ipc_test_passed &&
        ipc_count(message_queue) == 0)
        serial_puts("[PASS] Inter-process communication\n");
    else
        serial_puts("[FAIL] Inter-process communication\n");

    if (ipc_destroy(message_queue) == 0)
        serial_puts("[PASS] IPC queue destruction\n");
    else
        serial_puts("[FAIL] IPC queue destruction\n");

    serial_puts("\n[ OK ] Kernel tests completed.\n");

    null_process();
}