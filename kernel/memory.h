#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>

void  memory_init(void);
void* kmalloc(size_t size);
void  kfree(void* ptr);
void  memory_stats(size_t* used, size_t* free, size_t* total);

#endif
