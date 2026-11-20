#include "malloc.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#define MAGIC_USED ((size_t)UINT64_C(0x534946522d5501))
#define MAGIC_FREE ((size_t)UINT64_C(0x534946522d4601))

static void check_raw_heap(void)
{
    unsigned char *base = debug_heap_base();
    unsigned char *current = base;
    const unsigned char *end = sbrk(0);
    size_t block_count = 0U;

    assert(debug_validate_heap() == 0);
    assert((void *)current == debug_heap_head());
    while (current != end) {
        header_t *block;
        uintptr_t end_address;

        assert(current != NULL);
        assert((uintptr_t)current % (size_t)_Alignof(max_align_t) == 0U);
        assert((size_t)(end - current) >= sizeof(header_t));
        block = (void *)current;
        end_address = (uintptr_t)current + block->size;
        assert(end_address >= (uintptr_t)current);
        assert(end_address <= (uintptr_t)end);
        assert(block->magic == MAGIC_USED || block->magic == MAGIC_FREE);
        assert(block->size >= sizeof(header_t) + (size_t)_Alignof(max_align_t));
        assert(block->size % (size_t)_Alignof(max_align_t) == 0U);
        assert(block->requested <= block->size - sizeof(header_t));
        if (block->magic == MAGIC_FREE) {
            assert(block->requested == 0U);
        }
        assert(block->next == NULL ||
               (void *)block->next == (void *)(current + block->size));
        if (block->magic == MAGIC_FREE && block->next != NULL) {
            assert(block->next->magic != MAGIC_FREE);
        }
        current += block->size;
        ++block_count;
        assert(block_count < 100000U);
    }
}

int main(void)
{
    void *pointers[64];
    void *grow;

    setvbuf(stdout, NULL, _IONBF, 0);
    for (size_t index = 0U; index < 64U; ++index) {
        pointers[index] = my_malloc(index + 1U);
        assert(pointers[index] != NULL);
        check_raw_heap();
    }
    for (size_t index = 0U; index < 64U; index += 2U) {
        my_free(pointers[index]);
        pointers[index] = NULL;
        check_raw_heap();
    }
    for (size_t index = 1U; index < 64U; index += 2U) {
        my_free(pointers[index]);
        pointers[index] = NULL;
        check_raw_heap();
    }

    grow = my_malloc(4096U);
    assert(grow != NULL);
    grow = my_realloc(grow, 128U);
    assert(grow != NULL);
    check_raw_heap();
    grow = my_realloc(grow, 8192U);
    assert(grow != NULL);
    check_raw_heap();
    my_free(grow);

    grow = my_calloc(100U, 4U);
    assert(grow != NULL);
    check_raw_heap();
    my_free(grow);
    check_raw_heap();
    printf("raw and synchronized invariants after each transition: PASS\n");
    return 0;
}
