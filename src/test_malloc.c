#include "malloc.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main() {
    printf("Running malloc allocator tests...\n\n");

    // Test 1: Basic allocation
    printf("Test 1: Basic allocation\n");
    int *arr = (int*)my_malloc(10 * sizeof(int));
    assert(arr != NULL);
    printf("  PASS: allocated 10 ints\n");

    // Test 2: Write and read
    printf("Test 2: Write and read\n");
    for (int i = 0; i < 10; i++) {
        arr[i] = i * 2;
    }
    for (int i = 0; i < 10; i++) {
        assert(arr[i] == i * 2);
    }
    printf("  PASS: wrote and read values correctly\n");

    // Test 3: Free and reallocate
    printf("Test 3: Free and reallocate\n");
    my_free(arr);
    int *arr2 = (int*)my_malloc(10 * sizeof(int));
    assert(arr2 != NULL);
    printf("  PASS: freed and reallocated\n");

    // Test 4: calloc (zero-initialized)
    printf("Test 4: calloc zero-initialized\n");
    int *zero = (int*)my_calloc(100, sizeof(int));
    assert(zero != NULL);
    for (int i = 0; i < 100; i++) {
        assert(zero[i] == 0);
    }
    printf("  PASS: calloc returns zeroed memory\n");

    // Test 5: realloc
    printf("Test 5: realloc\n");
    int *small = (int*)my_malloc(5 * sizeof(int));
    for (int i = 0; i < 5; i++) small[i] = 42;
    int *bigger = (int*)my_realloc(small, 20 * sizeof(int));
    assert(bigger != NULL);
    for (int i = 0; i < 5; i++) {
        assert(bigger[i] == 42);
    }
    printf("  PASS: realloc preserved data\n");

    // Test 6: NULL pointer free
    printf("Test 6: NULL pointer free (should not crash)\n");
    my_free(NULL);
    printf("  PASS: free(NULL) handled gracefully\n");

    // Test 7: Zero size allocation
    printf("Test 7: Zero size allocation\n");
    void *zero_size = my_malloc(0);
    assert(zero_size == NULL);
    printf("  PASS: malloc(0) returns NULL\n");

    print_heap();

    printf("\nAll tests passed!\n");
    return 0;
}
