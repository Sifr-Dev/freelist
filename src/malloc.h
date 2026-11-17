#ifndef MALLOC_H
#define MALLOC_H

#include <stddef.h>
#include <stdint.h>

typedef struct header_t {
    size_t size;          /* total bytes occupied by this block, header included */
    size_t magic;         /* MAGIC_USED / MAGIC_FREE */
    struct header_t *next;
    size_t _pad;
} header_t;

void *my_malloc(size_t size);
void my_free(void *ptr);
void *my_calloc(size_t count, size_t size);
void *my_realloc(void *ptr, size_t new_size);

void print_heap(void);

/* Debug accessors used by the invariant checker. Not API. */
void *debug_heap_head(void);
void *debug_heap_base(void);

#endif
