#include "memory.h"

static unsigned char pool[MEMORY_SIZE]
    __attribute__((aligned(16)));

static unsigned char used[TOTAL_BLOCKS];
static unsigned short allocation[TOTAL_BLOCKS];

void memory_init(void)
{
    for (unsigned int i = 0; i < TOTAL_BLOCKS; i++) {
        used[i] = 0;
        allocation[i] = 0;
    }
}

void *kmalloc(unsigned int size)
{
    if (size == 0 || size > MEMORY_SIZE)
        return 0;

    unsigned int needed =
        (size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    for (unsigned int i = 0; i + needed <= TOTAL_BLOCKS; i++) {

        unsigned int j = 0;

        while (j < needed && !used[i + j])
            j++;

        if (j == needed) {
            for (j = 0; j < needed; j++)
                used[i + j] = 1;

            allocation[i] = needed;

            return &pool[i * BLOCK_SIZE];
        }
    }

    return 0;
}

void kfree(void *ptr)
{
    if (!ptr)
        return;

    unsigned long base = (unsigned long)pool;
    unsigned long address = (unsigned long)ptr;

    if (address < base ||
        address >= base + MEMORY_SIZE)
        return;

    unsigned long offset = address - base;

    if (offset % BLOCK_SIZE != 0)
        return;

    unsigned int index = offset / BLOCK_SIZE;
    unsigned int count = allocation[index];

    if (count == 0)
        return;

    for (unsigned int i = 0; i < count; i++)
        used[index + i] = 0;

    allocation[index] = 0;
}

unsigned int memory_free_blocks(void)
{
    unsigned int count = 0;

    for (unsigned int i = 0; i < TOTAL_BLOCKS; i++) {
        if (!used[i])
            count++;
    }

    return count;
}

unsigned int memory_used_blocks(void)
{
    return TOTAL_BLOCKS - memory_free_blocks();
}

unsigned int memory_largest_free_region(void)
{
    unsigned int largest = 0;
    unsigned int current = 0;

    for (unsigned int i = 0; i < TOTAL_BLOCKS; i++) {
        if (!used[i]) {
            current++;

            if (current > largest)
                largest = current;
        } else {
            current = 0;
        }
    }

    return largest;
}