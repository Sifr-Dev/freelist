#include "malloc.h"

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static header_t *header_for(void *pointer)
{
    return (void *)((unsigned char *)pointer - sizeof(header_t));
}

static void test_alignment_and_coalescing(void)
{
    void *pointers[257];

    for (size_t size = 1U; size <= 257U; ++size) {
        pointers[size - 1U] = my_malloc(size);
        assert(pointers[size - 1U] != NULL);
        assert((uintptr_t)pointers[size - 1U] %
               (size_t)_Alignof(max_align_t) == 0U);
        memset(pointers[size - 1U], (int)(size % 256U), size);
        assert(debug_validate_heap() == 0);
    }
    for (size_t index = 0U; index < 257U; index += 2U) {
        my_free(pointers[index]);
        pointers[index] = NULL;
    }
    for (size_t index = 1U; index < 257U; index += 2U) {
        my_free(pointers[index]);
        pointers[index] = NULL;
    }
    assert(debug_validate_heap() == 0);
    printf("alignment and bidirectional coalescing: PASS\n");
}

static void test_zero_and_overflow(void)
{
    void *pointer;

    my_free(NULL);
    errno = 0;
    assert(my_malloc(0U) == NULL);
    assert(errno == 0);
    assert(my_calloc(0U, 64U) == NULL);
    assert(my_calloc(64U, 0U) == NULL);
    assert(my_realloc(NULL, 0U) == NULL);

    errno = 0;
    assert(my_malloc(SIZE_MAX) == NULL);
    assert(errno == ENOMEM);
    errno = 0;
    assert(my_malloc(SIZE_MAX - 7U) == NULL);
    assert(errno == ENOMEM);
    errno = 0;
    assert(my_calloc(SIZE_MAX / 4U + 1U, 4U) == NULL);
    assert(errno == ENOMEM);
    errno = 0;
    assert(my_calloc(SIZE_MAX, 2U) == NULL);
    assert(errno == ENOMEM);

    pointer = my_malloc(64U);
    assert(pointer != NULL);
    memset(pointer, 0x5a, 64U);
    errno = 0;
    assert(my_realloc(pointer, SIZE_MAX) == NULL);
    assert(errno == ENOMEM);
    for (size_t index = 0U; index < 64U; ++index) {
        assert(((unsigned char *)pointer)[index] == 0x5aU);
    }
    my_free(pointer);
    printf("zero sizes and checked size arithmetic: PASS\n");
}

static void test_calloc_and_reuse(void)
{
    void *first = my_malloc(257U);
    unsigned char *zeroed;

    assert(first != NULL);
    memset(first, 0xff, 257U);
    my_free(first);
    zeroed = my_calloc(31U, 7U);
    assert(zeroed == first);
    for (size_t index = 0U; index < 31U * 7U; ++index) {
        assert(zeroed[index] == 0U);
    }
    my_free(zeroed);
    printf("calloc zeroes reused payload and rejects overflow: PASS\n");
}

static void test_realloc_paths(void)
{
    unsigned char *left = my_malloc(128U);
    void *right = my_malloc(128U);
    unsigned char *grown;
    header_t *left_header;
    size_t combined_size;

    assert(left != NULL && right != NULL);
    for (size_t index = 0U; index < 128U; ++index) {
        left[index] = (unsigned char)(index ^ 0xa5U);
    }
    my_free(right);

    /* Growth into a free successor used to copy the whole old block (160
     * bytes for a 128-byte request), reading past the initialized payload. */
    grown = my_realloc(left, 160U);
    assert(grown == left);
    assert(header_for(grown)->requested == 160U);
    for (size_t index = 0U; index < 128U; ++index) {
        assert(grown[index] == (unsigned char)(index ^ 0xa5U));
    }
    for (size_t index = 128U; index < 160U; ++index) {
        grown[index] = (unsigned char)index;
    }

    right = my_malloc(128U);
    assert(right != NULL);
    my_free(right);
    left_header = header_for(grown);
    combined_size = left_header->size + left_header->next->size;
    grown = my_realloc(grown, 64U);
    assert(grown == left);
    assert(header_for(grown)->requested == 64U);
    assert(header_for(grown)->next->requested == 0U);
    assert(header_for(grown)->size + header_for(grown)->next->size ==
           combined_size);
    for (size_t index = 0U; index < 64U; ++index) {
        assert(grown[index] == (unsigned char)(index ^ 0xa5U));
    }
    assert(debug_validate_heap() == 0);
    my_free(grown);
    printf("realloc exact payload copy, in-place growth, shrink/coalesce: PASS\n");

    left = my_malloc(96U);
    assert(left != NULL);
    for (size_t index = 0U; index < 96U; ++index) {
        left[index] = (unsigned char)(index + 1U);
    }
    grown = my_realloc(left, 16384U);
    assert(grown != NULL);
    for (size_t index = 0U; index < 96U; ++index) {
        assert(grown[index] == (unsigned char)(index + 1U));
    }
    my_free(grown);
    printf("realloc moving growth preserves exactly the old request: PASS\n");
}

static void test_sbrk_discontinuity(void)
{
    void *first = my_malloc(64U);
    void *gap;
    size_t managed_span;
    long page_size = sysconf(_SC_PAGESIZE);

    assert(first != NULL);
    assert(page_size > 0);
    assert(debug_heap_head() != NULL);
    gap = sbrk(0);
    assert(gap != (void *)-1);
    managed_span = (size_t)((unsigned char *)gap -
                            (unsigned char *)debug_heap_base());
    gap = sbrk((intptr_t)page_size);
    assert(gap != (void *)-1);
    memset(gap, 0, (size_t)page_size);
    errno = 0;
    /* Exceed every existing free block, forcing the guarded growth path. */
    assert(my_malloc(managed_span * 2U) == NULL);
    assert(errno == ENOTSUP);
    my_free(first);
    printf("external sbrk discontinuity rejected without false links: PASS\n");
}

static void test_split_and_zero_semantics(void)
{
    void *first = my_malloc(512U);
    void *second = my_malloc(64U);
    void *split;
    void *null_allocation;

    assert(first != NULL && second != NULL);
    my_free(first);
    split = my_malloc(64U);
    assert(split == first);
    assert(header_for(split)->size < 512U + sizeof(header_t));
    assert(debug_validate_heap() == 0);
    my_free(split);
    my_free(second);

    null_allocation = my_realloc(NULL, 33U);
    assert(null_allocation != NULL);
    assert(my_realloc(null_allocation, 0U) == NULL);
    assert(debug_validate_heap() == 0);
    printf("splitting and realloc NULL/zero semantics: PASS\n");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Allocator unit tests\n");
    test_alignment_and_coalescing();
    test_zero_and_overflow();
    test_calloc_and_reuse();
    test_realloc_paths();
    test_sbrk_discontinuity();
    test_split_and_zero_semantics();
    printf("all unit tests passed\n");
    return 0;
}
