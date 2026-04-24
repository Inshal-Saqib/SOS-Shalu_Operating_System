#include "memory.h"

/* ─── Heap configuration ─────────────────────────────────────────────────── */
#define HEAP_SIZE   (1024 * 1024)   /* 1 MB heap */
#define MAGIC_FREE  0xDEADBEEF
#define MAGIC_USED  0xBEEFDEAD

/* ─── Block header ───────────────────────────────────────────────────────── */
typedef struct block {
    uint32_t     magic;     /* MAGIC_FREE or MAGIC_USED          */
    size_t       size;      /* size of data area (not header)    */
    struct block* next;     /* next block in list                */
    struct block* prev;     /* previous block in list            */
} block_t;

#define HEADER_SIZE  sizeof(block_t)
#define MIN_SPLIT    (HEADER_SIZE + 16)   /* min leftover to split */

/* ─── Static heap ────────────────────────────────────────────────────────── */
static uint8_t  heap[HEAP_SIZE];
static block_t* head = (block_t*)0;

/* ─── Init ───────────────────────────────────────────────────────────────── */
void memory_init(void) {
    head        = (block_t*)heap;
    head->magic = MAGIC_FREE;
    head->size  = HEAP_SIZE - HEADER_SIZE;
    head->next  = (block_t*)0;
    head->prev  = (block_t*)0;
}

/* ─── kmalloc ────────────────────────────────────────────────────────────── */
void* kmalloc(size_t size) {
    if (!size) return (void*)0;

    /* Align size to 4 bytes */
    size = (size + 3) & ~3;

    block_t* b = head;
    while (b) {
        if (b->magic == MAGIC_FREE && b->size >= size) {

            /* Split block if there is enough room left over */
            if (b->size >= size + MIN_SPLIT) {
                block_t* newb = (block_t*)((uint8_t*)b + HEADER_SIZE + size);
                newb->magic   = MAGIC_FREE;
                newb->size    = b->size - size - HEADER_SIZE;
                newb->next    = b->next;
                newb->prev    = b;
                if (b->next) b->next->prev = newb;
                b->next       = newb;
                b->size       = size;
            }

            b->magic = MAGIC_USED;
            return (void*)((uint8_t*)b + HEADER_SIZE);
        }
        b = b->next;
    }
    return (void*)0;   /* out of memory */
}

/* ─── kfree ──────────────────────────────────────────────────────────────── */
void kfree(void* ptr) {
    if (!ptr) return;

    block_t* b = (block_t*)((uint8_t*)ptr - HEADER_SIZE);
    if (b->magic != MAGIC_USED) return;   /* bad pointer, ignore */

    b->magic = MAGIC_FREE;

    /* Merge with next block if it is also free */
    if (b->next && b->next->magic == MAGIC_FREE) {
        b->size += HEADER_SIZE + b->next->size;
        b->next  = b->next->next;
        if (b->next) b->next->prev = b;
    }

    /* Merge with previous block if it is also free */
    if (b->prev && b->prev->magic == MAGIC_FREE) {
        b->prev->size += HEADER_SIZE + b->size;
        b->prev->next  = b->next;
        if (b->next) b->next->prev = b->prev;
    }
}

/* ─── Stats ──────────────────────────────────────────────────────────────── */
void memory_stats(size_t* used, size_t* free_mem, size_t* total) {
    *used     = 0;
    *free_mem = 0;
    *total    = HEAP_SIZE;

    block_t* b = head;
    while (b) {
        if (b->magic == MAGIC_USED)
            *used     += b->size;
        else
            *free_mem += b->size;
        b = b->next;
    }
}
