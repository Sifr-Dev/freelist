#ifndef FREELIST_MALLOC_H
#define FREELIST_MALLOC_H

#include <stddef.h>

/* Exposed for education and corruption tests, not as a stable public ABI. */
typedef struct header_t {
    size_t size;          /* Total block size, including this header. */
    size_t requested;     /* Requested payload size; zero for a free block. */
    struct header_t *next;
    size_t magic;
} header_t;

void *my_malloc(size_t size);
void my_free(void *pointer);
void *my_calloc(size_t count, size_t size);
void *my_realloc(void *pointer, size_t new_size);

void print_heap(void);

/* Synchronized invariant check: 0 means valid, -1 means invalid. */
int debug_validate_heap(void);

/* Unsynchronized raw views: use only in quiescent single-threaded diagnostics. */
void *debug_heap_head(void);
void *debug_heap_base(void);

#endif
