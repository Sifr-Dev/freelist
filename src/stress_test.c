#include "malloc.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ALLOCATION_COUNT 1000U
#define RETAINED_COUNT 100U

static uint64_t next_random(uint64_t *state)
{
    uint64_t value = *state;

    value ^= value >> 12;
    value ^= value << 25;
    value ^= value >> 27;
    *state = value;
    return value * UINT64_C(2685821657736338717);
}

int main(void)
{
    uint64_t random_state = UINT64_C(0x5349465253545245);
    void *pointers[ALLOCATION_COUNT] = {NULL};
    size_t sizes[ALLOCATION_COUNT] = {0U};

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Deterministic stress seed=0x5349465253545245\n");

    for (size_t index = 0U; index < ALLOCATION_COUNT; ++index) {
        size_t size = (size_t)(next_random(&random_state) % UINT64_C(256)) + 1U;

        pointers[index] = my_malloc(size);
        assert(pointers[index] != NULL);
        sizes[index] = size;
        memset(pointers[index], (int)(index % 256U), size);
    }
    for (size_t count = 0U; count < ALLOCATION_COUNT - RETAINED_COUNT; ++count) {
        size_t index = (size_t)(next_random(&random_state) %
                                (uint64_t)ALLOCATION_COUNT);

        if (pointers[index] != NULL) {
            for (size_t byte = 0U; byte < sizes[index]; ++byte) {
                assert(((unsigned char *)pointers[index])[byte] ==
                       (unsigned char)(index % 256U));
            }
            my_free(pointers[index]);
            pointers[index] = NULL;
        }
    }
    for (size_t index = 0U; index < ALLOCATION_COUNT; ++index) {
        if (pointers[index] == NULL) {
            size_t size = (size_t)(next_random(&random_state) % UINT64_C(128)) + 1U;

            pointers[index] = my_calloc(size, 1U);
            assert(pointers[index] != NULL);
            sizes[index] = size;
            for (size_t byte = 0U; byte < size; ++byte) {
                assert(((unsigned char *)pointers[index])[byte] == 0U);
            }
            memset(pointers[index], (int)(index % 256U), size);
        }
    }
    for (size_t index = 0U; index < ALLOCATION_COUNT; ++index) {
        for (size_t byte = 0U; byte < sizes[index]; ++byte) {
            assert(((unsigned char *)pointers[index])[byte] ==
                   (unsigned char)(index % 256U));
        }
        my_free(pointers[index]);
        if (index % 31U == 0U) {
            assert(debug_validate_heap() == 0);
        }
    }
    assert(debug_validate_heap() == 0);
    printf("1000 blocks, deterministic reuse and payload verification: PASS\n");
    return 0;
}
