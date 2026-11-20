#include "malloc.h"

#include <assert.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SLOT_COUNT 48U
#define MAX_PAYLOAD 4097U
#define OPERATIONS 12000U

typedef struct {
    void *pointer;
    size_t size;
    size_t generation;
    unsigned char model[MAX_PAYLOAD];
} slot_t;

static slot_t slots[SLOT_COUNT];

static uint64_t next_random(uint64_t *state)
{
    uint64_t value = *state;

    value ^= value >> 12;
    value ^= value << 25;
    value ^= value >> 27;
    *state = value;
    return value * UINT64_C(2685821657736338717);
}

static size_t random_size(uint64_t *state, size_t upper)
{
    return (size_t)(next_random(state) % (uint64_t)upper);
}

static unsigned char pattern(size_t slot, size_t generation, size_t index)
{
    return (unsigned char)((slot * 17U + generation * 53U + index * 29U) &
                           (size_t)0xffU);
}

static void fill_slot(slot_t *slot, size_t slot_index)
{
    size_t index;

    for (index = 0U; index < slot->size; ++index) {
        unsigned char value = pattern(slot_index, slot->generation, index);

        slot->model[index] = value;
        ((unsigned char *)slot->pointer)[index] = value;
    }
}

static void check_slot(const slot_t *slot)
{
    assert(slot->pointer != NULL);
    assert((uintptr_t)slot->pointer % (size_t)_Alignof(max_align_t) == 0U);
    assert(memcmp(slot->pointer, slot->model, slot->size) == 0);
}

static void check_all_slots(void)
{
    size_t index;

    for (index = 0U; index < SLOT_COUNT; ++index) {
        if (slots[index].pointer != NULL) {
            check_slot(&slots[index]);
        }
    }
}

static void run_seed(uint64_t seed)
{
    uint64_t random_state = seed;
    size_t operation;

    assert(random_state != 0U);
    memset(slots, 0, sizeof(slots));

    for (operation = 0U; operation < OPERATIONS; ++operation) {
        size_t slot_index = random_size(&random_state, SLOT_COUNT);
        size_t choice = random_size(&random_state, 100U);
        slot_t *slot = &slots[slot_index];

        if (slot->pointer != NULL) {
            check_slot(slot);
        }

        if (choice < 35U) {
            if (slot->pointer == NULL) {
                slot->size = random_size(&random_state, MAX_PAYLOAD) + 1U;
                slot->generation = operation + 1U;
                slot->pointer = my_malloc(slot->size);
                assert(slot->pointer != NULL);
                fill_slot(slot, slot_index);
            }
        } else if (choice < 55U) {
            if (slot->pointer != NULL) {
                my_free(slot->pointer);
                slot->pointer = NULL;
                slot->size = 0U;
            }
        } else if (choice < 70U) {
            if (slot->pointer == NULL) {
                size_t count = random_size(&random_state, 65U) + 1U;
                size_t element_size = random_size(&random_state, 31U) + 1U;

                slot->size = count * element_size;
                slot->pointer = my_calloc(count, element_size);
                assert(slot->pointer != NULL);
                for (size_t index = 0U; index < slot->size; ++index) {
                    assert(((unsigned char *)slot->pointer)[index] == 0U);
                }
                slot->generation = operation + 1U;
                fill_slot(slot, slot_index);
            }
        } else if (choice < 95U) {
            if (slot->pointer != NULL) {
                size_t old_size = slot->size;
                size_t new_size = random_size(&random_state, MAX_PAYLOAD) + 1U;
                void *new_pointer = my_realloc(slot->pointer, new_size);

                assert(new_pointer != NULL);
                slot->pointer = new_pointer;
                slot->size = new_size;
                assert(memcmp(slot->pointer, slot->model,
                              old_size < new_size ? old_size : new_size) == 0);
                fill_slot(slot, slot_index);
            }
        } else if (slot->pointer != NULL) {
            check_slot(slot);
        }

        if (operation % 61U == 0U) {
            assert(debug_validate_heap() == 0);
            check_all_slots();
        }
    }

    check_all_slots();
    for (size_t index = 0U; index < SLOT_COUNT; ++index) {
        if (slots[index].pointer != NULL) {
            check_slot(&slots[index]);
            my_free(slots[index].pointer);
            slots[index].pointer = NULL;
        }
    }
    assert(debug_validate_heap() == 0);
    printf("model seed=0x%08" PRIx64 " operations=%u: PASS\n", seed,
           (unsigned int)OPERATIONS);
}

int main(void)
{
    static const uint64_t seeds[] = {
        UINT64_C(1), UINT64_C(0xc0ffee11), UINT64_C(0xdeadbeef)
    };

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Deterministic randomized differential model tests\n");
    for (size_t index = 0U; index < sizeof(seeds) / sizeof(seeds[0]); ++index) {
        run_seed(seeds[index]);
    }
    return 0;
}
