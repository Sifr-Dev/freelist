#include "malloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
    srand(time(NULL));
    printf("Stress test: allocating and freeing 1000 blocks...\n");

    void *ptrs[1000];

    for (int i = 0; i < 1000; i++) {
        size_t size = (rand() % 256) + 1;
        ptrs[i] = my_malloc(size);
        if (!ptrs[i]) {
            printf("FAIL: allocation %d failed\n", i);
            return 1;
        }
    }

    // Free half randomly
    for (int i = 0; i < 500; i++) {
        int idx = rand() % 1000;
        if (ptrs[idx]) {
            my_free(ptrs[idx]);
            ptrs[idx] = NULL;
        }
    }

    // Allocate again
    for (int i = 0; i < 500; i++) {
        size_t size = (rand() % 128) + 1;
        void *p = my_malloc(size);
        if (!p) {
            printf("FAIL: re-allocation %d failed\n", i);
            return 1;
        }
    }

    printf("PASS: stress test completed!\n");
    return 0;
}
