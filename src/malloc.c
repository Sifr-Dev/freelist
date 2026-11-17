#define _GNU_SOURCE
#include "malloc.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <pthread.h>

#define ALIGN 16
#define HEADER_SIZE 32
#define MIN_BLOCK (HEADER_SIZE + 16)
#define MAGIC_USED ((size_t)0x534946522D5501ULL)   /* "SIFR-U" */
#define MAGIC_FREE ((size_t)0x534946522D4601ULL)   /* "SIFR-F" */

/*
 * Address-ordered, physically contiguous free-list allocator.
 * Invariants (checked by tests/invariant_check.c):
 *  - every block header carries a valid magic cookie (USED or FREE)
 *  - blocks are sorted by address
 *  - for every consecutive pair (curr, next):
 *        (char*)next == (char*)curr + curr->size
 *  - the sum of all block sizes equals the heap span [heap_base, break)
 *  - every block lies inside [heap_base, program break)
 * All sizes INCLUDE the 32-byte header.
 */
static pthread_mutex_t global_malloc_lock = PTHREAD_MUTEX_INITIALIZER;
static header_t *head = NULL;
static header_t *tail = NULL;
static void *heap_base = NULL;

static size_t round_up(size_t n) {
    return (n + ALIGN - 1) & ~((size_t)ALIGN - 1);
}

static header_t *find_free_block(size_t need) {
    header_t *curr = head;
    while (curr) {
        if (curr->magic == MAGIC_FREE && curr->size >= need) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

/* Split `block` to exactly `need` bytes; second half becomes a FREE remainder. */
static void split_block(header_t *block, size_t need) {
    header_t *remainder = (header_t *)((char *)block + need);
    remainder->size = block->size - need;
    remainder->magic = MAGIC_FREE;
    remainder->next = block->next;
    remainder->_pad = 0;

    block->size = need;
    block->next = remainder;
    if (block == tail) {
        tail = remainder;
    }
}

void *my_malloc(size_t size) {
    if (!size) return NULL;

    size_t need = round_up(size) + HEADER_SIZE;
    if (need < MIN_BLOCK) need = MIN_BLOCK;

    pthread_mutex_lock(&global_malloc_lock);

    header_t *header = find_free_block(need);
    if (header) {
        if (header->size - need >= MIN_BLOCK) {
            split_block(header, need);
        }
        header->magic = MAGIC_USED;
        pthread_mutex_unlock(&global_malloc_lock);
        return (void *)((char *)header + HEADER_SIZE);
    }

    /* Grow the heap. Ask sbrk for `need` rounded up to a page. */
    size_t page = ((need + 4095) / 4096) * 4096;
    void *block = sbrk((intptr_t)page);
    if (block == (void *)-1) {
        pthread_mutex_unlock(&global_malloc_lock);
        return NULL;
    }
    if (!heap_base) heap_base = block;

    header = (header_t *)block;
    header->size = page;
    header->magic = MAGIC_USED;
    header->next = NULL;
    header->_pad = 0;

    if (page - need >= MIN_BLOCK) {
        header->size = need;
        header_t *remainder = (header_t *)((char *)block + need);
        remainder->size = page - need;
        remainder->magic = MAGIC_FREE;
        remainder->next = NULL;
        remainder->_pad = 0;
        header->next = remainder;
        if (!tail) head = header;
        else tail->next = header;
        tail = remainder;
    } else {
        if (!tail) head = header;
        else tail->next = header;
        tail = header;
    }

    pthread_mutex_unlock(&global_malloc_lock);
    return (void *)((char *)header + HEADER_SIZE);
}

static int validate_header(header_t *header) {
    if (header == NULL) return 0;
    if (header->magic != MAGIC_USED && header->magic != MAGIC_FREE) return 0;
    return 1;
}

void my_free(void *ptr) {
    if (!ptr) return;

    header_t *header = (header_t *)((char *)ptr - HEADER_SIZE);

    pthread_mutex_lock(&global_malloc_lock);

    if (!validate_header(header)) {
        fprintf(stderr, "my_free: invalid pointer %p\n", ptr);
        abort();
    }
    if (header->magic == MAGIC_FREE) {
        fprintf(stderr, "my_free: double free of %p\n", ptr);
        abort();
    }
    header->magic = MAGIC_FREE;

    /* Coalesce with the previous FREE block if adjacent (free blocks are address-ordered). */
    header_t *curr = head;
    while (curr) {
        if (curr->magic == MAGIC_FREE && curr->next
            && (char *)curr + curr->size == (char *)curr->next
            && curr->next->magic == MAGIC_FREE) {
            curr->size += curr->next->size;
            curr->next = curr->next->next;
            curr->_pad = 0;
            if (curr->next == NULL) tail = curr;
            /* re-check the same block: may merge again */
        } else {
            curr = curr->next;
        }
    }

    pthread_mutex_unlock(&global_malloc_lock);
}

void *my_calloc(size_t count, size_t size) {
    if (count != 0 && size > SIZE_MAX / count) {
        errno = ENOMEM;
        return NULL;
    }
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

    header_t *header = (header_t *)((char *)ptr - HEADER_SIZE);

    pthread_mutex_lock(&global_malloc_lock);
    if (!validate_header(header)) {
        fprintf(stderr, "my_realloc: invalid pointer %p\n", ptr);
        abort();
    }
    if (header->magic == MAGIC_FREE) {
        fprintf(stderr, "my_realloc: pointer %p is on the free list\n", ptr);
        abort();
    }
    size_t old_size = header->size;
    size_t need = round_up(new_size) + HEADER_SIZE;
    if (need < MIN_BLOCK) need = MIN_BLOCK;

    /* Shrink: split the excess off the tail if large enough. */
    if (old_size >= need) {
        if (old_size - need >= MIN_BLOCK) {
            header_t *remainder = (header_t *)((char *)header + need);
            remainder->size = old_size - need;
            remainder->magic = MAGIC_FREE;
            remainder->next = header->next;
            remainder->_pad = 0;
            header->size = need;
            header->next = remainder;
            if (remainder->next == NULL) tail = remainder;
        }
        pthread_mutex_unlock(&global_malloc_lock);
        return ptr;
    }

    /* Grow in place if the physically adjacent next block is free and big enough. */
    if (header->next && header->next->magic == MAGIC_FREE
        && old_size + header->next->size >= need) {
        header->size += header->next->size;
        header->next = header->next->next;
        if (header->next == NULL) tail = header;
        pthread_mutex_unlock(&global_malloc_lock);
        return ptr;
    }

    pthread_mutex_unlock(&global_malloc_lock);

    /* Otherwise allocate, copy, free. */
    void *new_ptr = my_malloc(new_size);
    if (!new_ptr) return NULL;
    memcpy(new_ptr, ptr, old_size - HEADER_SIZE);
    my_free(ptr);
    return new_ptr;
}

void print_heap(void) {
    pthread_mutex_lock(&global_malloc_lock);
    header_t *curr = head;
    printf("\n=== HEAP DUMP ===\n");
    while (curr) {
        printf("[%s] size=%zu bytes | addr=%p\n",
               curr->magic == MAGIC_USED ? "USED" : "FREE",
               curr->size, (void *)curr);
        curr = curr->next;
    }
    printf("=================\n\n");
    pthread_mutex_unlock(&global_malloc_lock);
}

/* Debug accessor for invariant checking in tests. Not part of the API contract. */
void *debug_heap_head(void) {
    return head;
}
void *debug_heap_base(void) {
    return heap_base;
}
