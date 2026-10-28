#include "ipc.h"

static MessageQueue queues[MAX_QUEUES];

void ipc_init(void)
{
    for (int i = 0; i < MAX_QUEUES; i++) {
        queues[i].front = 0;
        queues[i].rear = 0;
        queues[i].count = 0;
        queues[i].active = 0;
    }
}

int ipc_create(void)
{
    for (int i = 0; i < MAX_QUEUES; i++) {
        if (!queues[i].active) {
            queues[i].front = 0;
            queues[i].rear = 0;
            queues[i].count = 0;
            queues[i].active = 1;

            return i;
        }
    }

    return -1;
}

int ipc_send(int qid, int message)
{
    if (qid < 0 || qid >= MAX_QUEUES)
        return -1;

    MessageQueue *q = &queues[qid];

    if (!q->active || q->count == QUEUE_SIZE)
        return -1;

    q->data[q->rear] = message;
    q->rear = (q->rear + 1) % QUEUE_SIZE;
    q->count++;

    return 0;
}

int ipc_receive(int qid, int *message)
{
    if (qid < 0 || qid >= MAX_QUEUES || !message)
        return -1;

    MessageQueue *q = &queues[qid];

    if (!q->active || q->count == 0)
        return -1;

    *message = q->data[q->front];
    q->front = (q->front + 1) % QUEUE_SIZE;
    q->count--;

    return 0;
}

int ipc_destroy(int qid)
{
    if (qid < 0 || qid >= MAX_QUEUES)
        return -1;

    if (!queues[qid].active)
        return -1;

    queues[qid].active = 0;
    queues[qid].front = 0;
    queues[qid].rear = 0;
    queues[qid].count = 0;

    return 0;
}

int ipc_count(int qid)
{
    if (qid < 0 || qid >= MAX_QUEUES)
        return -1;

    if (!queues[qid].active)
        return -1;

    return queues[qid].count;
}