#include "malloc.h"

#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define THREAD_COUNT 8U
#define SLOTS_PER_THREAD 12U
#define MAX_PAYLOAD 2049U
#define OPERATIONS 5000U

typedef struct {
    void *pointer;
    size_t size;
    size_t generation;
    unsigned char model[MAX_PAYLOAD];
} slot_t;

static slot_t slots[THREAD_COUNT][SLOTS_PER_THREAD];
static pthread_barrier_t start_barrier;
static atomic_size_t finished;

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

static unsigned char pattern(size_t thread, size_t generation, size_t index)
{
    return (unsigned char)((thread * 43U + generation * 19U + index * 31U) &
                           (size_t)0xffU);
}

static void fill_slot(slot_t *slot, size_t thread)
{
    for (size_t index = 0U; index < slot->size; ++index) {
        unsigned char value = pattern(thread, slot->generation, index);

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

static void *worker(void *argument)
{
    size_t thread = (size_t)(uintptr_t)argument;
    uint64_t random_state = UINT64_C(0x9e3779b97f4a7c15) ^
                            (uint64_t)(thread + 1U);
    int barrier_result = pthread_barrier_wait(&start_barrier);

    assert(barrier_result == 0 || barrier_result == PTHREAD_BARRIER_SERIAL_THREAD);

    for (size_t operation = 0U; operation < OPERATIONS; ++operation) {
        size_t slot_index = random_size(&random_state, SLOTS_PER_THREAD);
        size_t choice = random_size(&random_state, 100U);
        slot_t *slot = &slots[thread][slot_index];

        if (slot->pointer != NULL) {
            check_slot(slot);
        }
        if (choice < 40U) {
            if (slot->pointer == NULL) {
                slot->size = random_size(&random_state, MAX_PAYLOAD) + 1U;
                slot->generation = operation + 1U;
                slot->pointer = my_malloc(slot->size);
                assert(slot->pointer != NULL);
                fill_slot(slot, thread);
            }
        } else if (choice < 60U) {
            if (slot->pointer != NULL) {
                my_free(slot->pointer);
                slot->pointer = NULL;
            }
        } else if (choice < 72U) {
            if (slot->pointer == NULL) {
                size_t count = random_size(&random_state, 33U) + 1U;
                size_t element_size = random_size(&random_state, 31U) + 1U;

                slot->size = count * element_size;
                slot->pointer = my_calloc(count, element_size);
                assert(slot->pointer != NULL);
                for (size_t index = 0U; index < slot->size; ++index) {
                    assert(((unsigned char *)slot->pointer)[index] == 0U);
                }
                slot->generation = operation + 1U;
                fill_slot(slot, thread);
            }
        } else if (slot->pointer != NULL) {
            size_t old_size = slot->size;
            size_t new_size = random_size(&random_state, MAX_PAYLOAD) + 1U;

            slot->pointer = my_realloc(slot->pointer, new_size);
            assert(slot->pointer != NULL);
            slot->size = new_size;
            assert(memcmp(slot->pointer, slot->model,
                          old_size < new_size ? old_size : new_size) == 0);
            fill_slot(slot, thread);
        }
    }

    for (size_t index = 0U; index < SLOTS_PER_THREAD; ++index) {
        slot_t *slot = &slots[thread][index];

        if (slot->pointer != NULL) {
            check_slot(slot);
            my_free(slot->pointer);
            slot->pointer = NULL;
        }
    }
    atomic_fetch_add_explicit(&finished, 1U, memory_order_release);
    return NULL;
}

int main(void)
{
    pthread_t threads[THREAD_COUNT];

    setvbuf(stdout, NULL, _IONBF, 0);
    atomic_init(&finished, 0U);
    assert(pthread_barrier_init(&start_barrier, NULL, THREAD_COUNT + 1U) == 0);

    /* Create runtimes and stacks before this test starts using sbrk(). */
    for (size_t index = 0U; index < THREAD_COUNT; ++index) {
        assert(pthread_create(&threads[index], NULL, worker,
                              (void *)(uintptr_t)index) == 0);
    }
    int barrier_result = pthread_barrier_wait(&start_barrier);
    assert(barrier_result == 0 || barrier_result == PTHREAD_BARRIER_SERIAL_THREAD);

    while (atomic_load_explicit(&finished, memory_order_acquire) < THREAD_COUNT) {
        assert(debug_validate_heap() == 0);
        sched_yield();
    }
    for (size_t index = 0U; index < THREAD_COUNT; ++index) {
        assert(pthread_join(threads[index], NULL) == 0);
    }
    assert(pthread_barrier_destroy(&start_barrier) == 0);
    assert(debug_validate_heap() == 0);
    printf("concurrency: %u threads x %u deterministic operations: PASS\n",
           (unsigned int)THREAD_COUNT, (unsigned int)OPERATIONS);
    return 0;
}
