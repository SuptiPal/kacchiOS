#ifndef IPC_H
#define IPC_H

#define MAX_QUEUES 4
#define QUEUE_SIZE 8

typedef struct {
    int data[QUEUE_SIZE];
    int front;
    int rear;
    int count;
    int active;
} MessageQueue;

void ipc_init(void);
int ipc_create(void);
int ipc_send(int qid, int message);
int ipc_receive(int qid, int *message);
int ipc_destroy(int qid);
int ipc_count(int qid);

#endif