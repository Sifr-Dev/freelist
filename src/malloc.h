#ifndef MALLOC_H
#define MALLOC_H

#include <stddef.h>

typedef struct header_t {
    size_t size;
    unsigned int is_free;
    struct header_t *next;
} header_t;

void *my_malloc(size_t size);
void my_free(void *ptr);
void *my_calloc(size_t count, size_t size);
void *my_realloc(void *ptr, size_t new_size);

void print_heap(void);

#endif
