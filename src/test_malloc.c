#include "malloc.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <limits.h>

static void fill(void *p, size_t n, unsigned char v) {
    memset(p, v, n);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Running malloc allocator tests...\n\n");

    /* Test 1: Basic allocation */
    printf("Test 1: Basic allocation\n");
    int *arr = (int *)my_malloc(10 * sizeof(int));
    assert(arr != NULL);
    printf("  PASS: allocated 10 ints\n");

    /* Test 2: Write and read */
    printf("Test 2: Write and read\n");
    for (int i = 0; i < 10; i++) arr[i] = i * 2;
    for (int i = 0; i < 10; i++) assert(arr[i] == i * 2);
    printf("  PASS: wrote and read values correctly\n");

    /* Test 3: Every allocation is 16-byte aligned */
    printf("Test 3: 16-byte alignment of payloads\n");
    for (size_t size = 1; size < 257; size += 7) {
        void *p = my_malloc(size);
        assert(p != NULL);
        assert(((uintptr_t)p % 16) == 0);
        my_free(p);
    }
    printf("  PASS: all tested sizes returned 16-byte aligned pointers\n");

    /* Test 4: calloc zero-initialized */
    printf("Test 4: calloc zero-initialized\n");
    int *zero = (int *)my_calloc(100, sizeof(int));
    assert(zero != NULL);
    for (int i = 0; i < 100; i++) assert(zero[i] == 0);
    printf("  PASS: calloc returns zeroed memory\n");

    /* Test 5: calloc overflow is rejected */
    printf("Test 5: calloc overflow rejected\n");
    void *too_big = my_calloc(SIZE_MAX / sizeof(int) + 1, sizeof(int));
    assert(too_big == NULL);
    printf("  PASS: wrapped multiplication returns NULL\n");

    /* Test 6: realloc grows and preserves data */
    printf("Test 6: realloc preserves data on grow\n");
    int *small = (int *)my_malloc(5 * sizeof(int));
    for (int i = 0; i < 5; i++) small[i] = 42 + i;
    int *bigger = (int *)my_realloc(small, 64 * sizeof(int));
    assert(bigger != NULL);
    for (int i = 0; i < 5; i++) assert(bigger[i] == 42 + i);
    printf("  PASS: realloc preserved data\n");

    /* Test 7: realloc shrink keeps pointer and data */
    printf("Test 7: realloc shrink keeps pointer\n");
    int *shrinkable = (int *)my_malloc(1024 * sizeof(int));
    for (int i = 0; i < 1024; i++) shrinkable[i] = i;
    int *shrunk = (int *)my_realloc(shrinkable, 16 * sizeof(int));
    assert(shrunk == shrinkable);
    for (int i = 0; i < 16; i++) assert(shrunk[i] == i);
    printf("  PASS: in-place shrink preserved pointer and data\n");

    /* Test 8: realloc grows in place into the adjacent free block */
    printf("Test 8: realloc grows in place into free neighbor\n");
    void *left = my_malloc(128);
    void *right = my_malloc(4096);
    assert(left && right);
    my_free(right); /* free the neighbor */
    void *left_grown = my_realloc(left, 2048);
    assert(left_grown == left); /* same pointer: neighbor absorbed in place */
    printf("  PASS: grew in place into the freed neighbor\n");

    /* Test 9: free(NULL) and malloc(0) */
    printf("Test 9: free(NULL) and malloc(0)\n");
    my_free(NULL);
    assert(my_malloc(0) == NULL);
    printf("  PASS\n");

    /* Test 10: deallocate everything, big alloc must still work (coalescing) */
    printf("Test 10: coalescing across many frees\n");
    void *a = my_malloc(256);
    void *b = my_malloc(256);
    void *c = my_malloc(256);
    fill(a, 256, 0xA5);
    my_free(a);
    my_free(b);
    my_free(c);
    void *big = my_malloc(4096);
    assert(big != NULL);
    {
        extern void print_heap(void);
        print_heap();
        fflush(stdout);
    }
    fill(big, 4096, 0x5A);
    my_free(big);
    my_free(left_grown);
    my_free(shrunk);
    my_free(bigger);
    my_free(arr);
    my_free(zero);
    printf("  PASS: large allocations succeed after full deallocations\n");

    printf("\nAll tests passed.\n");
    return 0;
}
