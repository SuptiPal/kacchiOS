#ifndef PROCESS_H
#define PROCESS_H

#define MAX_PROCESSES 8
#define STACK_SIZE 4096

#define MIN_PRIORITY 0
#define MAX_PRIORITY 10
#define DEFAULT_PRIORITY 5

typedef enum {
    CURRENT,
    READY,
    BLOCKED,
    TERMINATED
} process_state;

typedef struct {
    int pid;
    process_state state;

    void (*entry)(void);

    void *stack;
    unsigned int esp;

    int active;

    int priority;
    int base_priority;

    unsigned int waiting_time;
    unsigned int cpu_ticks;
} pcb;

extern pcb processes[MAX_PROCESSES];
extern int current_pid;

void process_init(void);

int process_create(void (*entry)(void));
int process_terminate(int pid);
void process_exit(void);

pcb *process_get(int pid);

int process_set_priority(int pid, int priority);
int process_get_priority(int pid);

int process_block(int pid);
int process_unblock(int pid);

int process_count(void);
process_state process_get_state(int pid);

#endif