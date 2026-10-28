#ifndef MEMORY_H
#define MEMORY_H

#define BLOCK_SIZE 256
#define TOTAL_BLOCKS 256
#define MEMORY_SIZE (BLOCK_SIZE * TOTAL_BLOCKS)

void memory_init(void);
void *kmalloc(unsigned int size);
void kfree(void *ptr);

unsigned int memory_free_blocks(void);
unsigned int memory_used_blocks(void);
unsigned int memory_largest_free_region(void);

#endif