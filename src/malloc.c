#include "malloc.h"

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ALIGNMENT ((size_t)_Alignof(max_align_t))
#define MIN_BLOCK_SIZE (sizeof(header_t) + ALIGNMENT)
#define MAGIC_USED ((size_t)UINT64_C(0x534946522d5501))
#define MAGIC_FREE ((size_t)UINT64_C(0x534946522d4601))

_Static_assert(sizeof(header_t) % ALIGNMENT == 0,
               "header size must preserve max_align_t alignment");

/*
 * Educational, address-ordered free-list allocator.
 *
 * All list metadata is protected by global_malloc_lock. Block sizes include
 * their headers and are multiples of ALIGNMENT. Adjacent free blocks are
 * always coalesced. The list covers exactly [heap_base, heap_end).
 *
 * sbrk() is process-global. No other code may move the program break while
 * this allocator is in use; see README.md for the consequences.
 */
static pthread_mutex_t global_malloc_lock = PTHREAD_MUTEX_INITIALIZER;
static header_t *head;
static header_t *tail;
static unsigned char *heap_base;
static unsigned char *heap_end;

static void corruption_abort(const char *message)
{
    size_t length = strlen(message);
    size_t written = 0U;

    /* Diagnostics are best effort; no allocation or stdio while corrupt. */
    while (written < length) {
        ssize_t result = write(STDERR_FILENO, message + written,
                               length - written);

        if (result <= 0) {
            break;
        }
        written += (size_t)result;
    }
    if (write(STDERR_FILENO, "\n", 1U) < 0) {
        /* A closed stderr must not suppress the required abort. */
    }
    abort();
}

/* Integer addresses let us reject foreign pointers without dereferencing them. */
static int range_contains(const void *pointer, size_t length)
{
    uintptr_t address = (uintptr_t)pointer;
    uintptr_t base = (uintptr_t)heap_base;
    uintptr_t end = (uintptr_t)heap_end;

    return heap_base != NULL && address >= base && address <= end &&
           length <= (size_t)(end - address);
}

static int block_is_valid(const header_t *block)
{
    uintptr_t address;
    size_t block_size;

    if (block == NULL || !range_contains(block, sizeof(*block)) ||
        (uintptr_t)block % ALIGNMENT != 0U) {
        return 0;
    }

    address = (uintptr_t)block;
    block_size = block->size;
    if ((block->magic != MAGIC_USED && block->magic != MAGIC_FREE) ||
        block_size < MIN_BLOCK_SIZE || block_size % ALIGNMENT != 0U ||
        block_size > (size_t)((uintptr_t)heap_end - address) ||
        block->requested > block_size - sizeof(*block) ||
        (block->magic == MAGIC_FREE && block->requested != 0U)) {
        return 0;
    }

    if (block->next == NULL) {
        return block == tail;
    }

    return block != tail && range_contains(block->next, sizeof(*block->next)) &&
           (uintptr_t)block->next == address + block_size;
}

static void require_valid_block(const header_t *block)
{
    if (!block_is_valid(block)) {
        corruption_abort("freelist: corrupt or inconsistent block metadata");
    }
}

static int block_size_for(size_t payload_size, size_t *result)
{
    size_t payload;
    size_t total;

    if (payload_size > SIZE_MAX - (ALIGNMENT - 1U)) {
        return 0;
    }
    payload = ((payload_size + ALIGNMENT - 1U) / ALIGNMENT) * ALIGNMENT;
    if (payload > SIZE_MAX - sizeof(header_t)) {
        return 0;
    }
    total = payload + sizeof(header_t);
    if (total < MIN_BLOCK_SIZE) {
        total = MIN_BLOCK_SIZE;
    }
    if (total > (size_t)INTPTR_MAX) {
        return 0;
    }
    *result = total;
    return 1;
}

static header_t *find_free_block(size_t needed)
{
    header_t *current = head;

    while (current != NULL) {
        require_valid_block(current);
        if (current->magic == MAGIC_FREE && current->size >= needed) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

/* The caller holds the lock and has validated the pointer's alignment. */
static header_t *find_owner(const void *pointer, header_t **previous)
{
    header_t *prior = NULL;
    header_t *current = head;

    while (current != NULL) {
        require_valid_block(current);
        if ((unsigned char *)current + sizeof(*current) == pointer) {
            if (previous != NULL) {
                *previous = prior;
            }
            return current;
        }
        prior = current;
        current = current->next;
    }
    corruption_abort("freelist: pointer was not allocated by this allocator");
    return NULL;
}

static header_t *owner_for_operation(void *pointer, header_t **previous,
                                     const char *operation)
{
    if ((uintptr_t)pointer % ALIGNMENT != 0U) {
        corruption_abort(operation);
    }
    return find_owner(pointer, previous);
}

/* Both blocks have already been range-checked; their sum fits in the heap. */
static void merge_with_next(header_t *block)
{
    header_t *next = block->next;

    require_valid_block(block);
    require_valid_block(next);
    block->size += next->size;
    block->next = next->next;
    if (tail == next) {
        tail = block;
    }

    /* A stale pointer to the absorbed header is no longer a list member. */
    next->size = 0U;
    next->requested = 0U;
    next->magic = 0U;
    next->next = NULL;
}

static void split_block(header_t *block, size_t needed)
{
    header_t *remainder = (void *)((unsigned char *)block + needed);

    remainder->size = block->size - needed;
    remainder->requested = 0U;
    remainder->magic = MAGIC_FREE;
    remainder->next = block->next;
    block->size = needed;
    block->next = remainder;
    if (tail == block) {
        tail = remainder;
    }

    if (remainder->next != NULL) {
        require_valid_block(remainder->next);
        if (remainder->next->magic == MAGIC_FREE) {
            merge_with_next(remainder);
        }
    }
}

void *my_malloc(size_t size)
{
    size_t needed;
    header_t *block;

    if (size == 0U) {
        return NULL;
    }
    if (!block_size_for(size, &needed)) {
        errno = ENOMEM;
        return NULL;
    }

    pthread_mutex_lock(&global_malloc_lock);
    block = find_free_block(needed);
    if (block != NULL) {
        if (block->size - needed >= MIN_BLOCK_SIZE) {
            split_block(block, needed);
        }
        block->magic = MAGIC_USED;
        block->requested = size;
        pthread_mutex_unlock(&global_malloc_lock);
        return (unsigned char *)block + sizeof(*block);
    }

    block = sbrk((intptr_t)needed);
    if (block == (void *)-1) {
        pthread_mutex_unlock(&global_malloc_lock);
        return NULL;
    }

    if ((heap_base == NULL && head != NULL) ||
        (heap_base != NULL && (unsigned char *)block != heap_end)) {
        /* Another sbrk() user created a gap. Do not link a false adjacency. */
        errno = ENOTSUP;
        pthread_mutex_unlock(&global_malloc_lock);
        return NULL;
    }

    if (heap_base == NULL) {
        heap_base = (unsigned char *)block;
    }
    heap_end = (unsigned char *)block + needed;
    block->size = needed;
    block->requested = size;
    block->magic = MAGIC_USED;
    block->next = NULL;

    if (tail == NULL) {
        head = block;
    } else {
        tail->next = block;
    }
    tail = block;

    pthread_mutex_unlock(&global_malloc_lock);
    return (unsigned char *)block + sizeof(*block);
}

void my_free(void *pointer)
{
    header_t *previous;
    header_t *block;

    if (pointer == NULL) {
        return;
    }

    pthread_mutex_lock(&global_malloc_lock);
    block = owner_for_operation(pointer, &previous,
                                "freelist: invalid or unaligned free");
    if (block->magic == MAGIC_FREE) {
        corruption_abort("freelist: double free detected");
    }

    block->magic = MAGIC_FREE;
    block->requested = 0U;
    if (block->next != NULL) {
        require_valid_block(block->next);
        if (block->next->magic == MAGIC_FREE) {
            merge_with_next(block);
        }
    }
    if (previous != NULL && previous->magic == MAGIC_FREE) {
        merge_with_next(previous);
    }
    pthread_mutex_unlock(&global_malloc_lock);
}

void *my_calloc(size_t count, size_t size)
{
    size_t total;
    void *pointer;

    if (count != 0U && size > SIZE_MAX / count) {
        errno = ENOMEM;
        return NULL;
    }
    total = count * size;
    pointer = my_malloc(total);
    if (pointer != NULL) {
        memset(pointer, 0, total);
    }
    return pointer;
}

void *my_realloc(void *pointer, size_t new_size)
{
    size_t needed;
    size_t old_requested;
    header_t *block;
    void *replacement;

    if (pointer == NULL) {
        return my_malloc(new_size);
    }
    if (new_size == 0U) {
        my_free(pointer);
        return NULL;
    }
    if (!block_size_for(new_size, &needed)) {
        errno = ENOMEM;
        return NULL;
    }

    pthread_mutex_lock(&global_malloc_lock);
    block = owner_for_operation(pointer, NULL,
                                "freelist: invalid or unaligned realloc");
    if (block->magic == MAGIC_FREE) {
        corruption_abort("freelist: realloc of freed pointer");
    }
    old_requested = block->requested;

    if (block->size >= needed) {
        if (block->size - needed >= MIN_BLOCK_SIZE) {
            split_block(block, needed);
        }
        block->magic = MAGIC_USED;
        block->requested = new_size;
        pthread_mutex_unlock(&global_malloc_lock);
        return pointer;
    }

    if (block->next != NULL) {
        require_valid_block(block->next);
        if (block->next->magic == MAGIC_FREE &&
            block->next->size >= needed - block->size) {
            merge_with_next(block);
            if (block->size - needed >= MIN_BLOCK_SIZE) {
                split_block(block, needed);
            }
            block->magic = MAGIC_USED;
            block->requested = new_size;
            pthread_mutex_unlock(&global_malloc_lock);
            return pointer;
        }
    }
    pthread_mutex_unlock(&global_malloc_lock);

    /* As with realloc(3), the caller must not race another operation on ptr. */
    replacement = my_malloc(new_size);
    if (replacement == NULL) {
        return NULL;
    }
    memcpy(replacement, pointer,
           old_requested < new_size ? old_requested : new_size);
    my_free(pointer);
    return replacement;
}

void print_heap(void)
{
    const header_t *current;

    pthread_mutex_lock(&global_malloc_lock);
    current = head;
    printf("\n=== HEAP DUMP ===\n");
    while (current != NULL) {
        printf("[%s] size=%zu requested=%zu bytes | addr=%p\n",
               current->magic == MAGIC_USED ? "USED" : "FREE", current->size,
               current->requested, (void *)current);
        current = current->next;
    }
    printf("=================\n\n");
    pthread_mutex_unlock(&global_malloc_lock);
}

/* Returns 0 only if the entire list satisfies the documented invariants. */
int debug_validate_heap(void)
{
    const header_t *current;
    int valid = 1;

    pthread_mutex_lock(&global_malloc_lock);
    current = head;
    if (head == NULL) {
        valid = tail == NULL && heap_base == NULL && heap_end == NULL;
    } else if ((unsigned char *)head != heap_base) {
        valid = 0;
    }

    while (valid && current != NULL) {
        if (!block_is_valid(current)) {
            valid = 0;
            break;
        }
        if (current->magic == MAGIC_FREE && current->next != NULL &&
            current->next->magic == MAGIC_FREE) {
            valid = 0;
            break;
        }
        if (current->next == NULL &&
            (current != tail ||
             (unsigned char *)current + current->size != heap_end)) {
            valid = 0;
            break;
        }
        current = current->next;
    }
    if (valid && head != NULL && tail == NULL) {
        valid = 0;
    }
    pthread_mutex_unlock(&global_malloc_lock);
    return valid ? 0 : -1;
}

/* These raw views are for quiescent single-threaded diagnostics only. */
void *debug_heap_head(void)
{
    return head;
}

void *debug_heap_base(void)
{
    return heap_base;
}
