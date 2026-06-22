#define _GNU_SOURCE
#include "malloc.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

static pthread_mutex_t global_malloc_lock = PTHREAD_MUTEX_INITIALIZER;
static header_t *head = NULL;
static header_t *tail = NULL;

static header_t *get_free_block(size_t size) {
    header_t *curr = head;
    while (curr) {
        if (curr->is_free && curr->size >= size) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

void *my_malloc(size_t size) {
    if (!size) return NULL;

    pthread_mutex_lock(&global_malloc_lock);
    header_t *header = get_free_block(size);
    if (header) {
        header->is_free = 0;
        pthread_mutex_unlock(&global_malloc_lock);
        return (void*)(header + 1);
    }

    size_t total_size = sizeof(header_t) + size;
    void *block = sbrk(total_size);
    if (block == (void*)-1) {
        pthread_mutex_unlock(&global_malloc_lock);
        return NULL;
    }

    header = (header_t*)block;
    header->size = size;
    header->is_free = 0;
    header->next = NULL;

    if (!head) {
        head = header;
    }
    if (tail) {
        tail->next = header;
    }
    tail = header;

    pthread_mutex_unlock(&global_malloc_lock);
    return (void*)(header + 1);
}

void my_free(void *ptr) {
    if (!ptr) return;

    pthread_mutex_lock(&global_malloc_lock);
    header_t *header = (header_t*)ptr - 1;

    header->is_free = 1;

    // Coalesce adjacent free blocks
    header_t *curr = head;
    while (curr && curr->next) {
        if (curr->is_free && curr->next->is_free) {
            curr->size += sizeof(header_t) + curr->next->size;
            curr->next = curr->next->next;
            if (curr->next == NULL) {
                tail = curr;
            }
        }
        curr = curr->next;
    }

    pthread_mutex_unlock(&global_malloc_lock);
}

void *my_calloc(size_t count, size_t size) {
    size_t total = count * size;
    void *ptr = my_malloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void *my_realloc(void *ptr, size_t new_size) {
    if (!ptr) return my_malloc(new_size);
    if (new_size == 0) {
        my_free(ptr);
        return NULL;
    }

    header_t *header = (header_t*)ptr - 1;
    size_t old_size = header->size;

    if (old_size >= new_size) return ptr;

    void *new_ptr = my_malloc(new_size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, old_size < new_size ? old_size : new_size);
        my_free(ptr);
    }
    return new_ptr;
}

void print_heap(void) {
    pthread_mutex_lock(&global_malloc_lock);
    header_t *curr = head;
    printf("\n=== HEAP DUMP ===\n");
    while (curr) {
        printf("[%s] size=%zu bytes | addr=%p\n",
               curr->is_free ? "FREE" : "USED",
               curr->size, (void*)curr);
        curr = curr->next;
    }
    printf("=================\n\n");
    pthread_mutex_unlock(&global_malloc_lock);
}
