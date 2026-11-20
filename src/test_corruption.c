#include "malloc.h"

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

typedef void (*corruption_fn)(void);

static max_align_t foreign_object;

static header_t *header_for(void *pointer)
{
    return (void *)((unsigned char *)pointer - sizeof(header_t));
}

static void allocate_pair(void **left, void **right)
{
    *left = my_malloc(128U);
    *right = my_malloc(256U);
    assert(*left != NULL && *right != NULL);
}

static void double_free(void)
{
    void *pointer = my_malloc(64U);

    assert(pointer != NULL);
    my_free(pointer);
    my_free(pointer);
}

static void realloc_freed(void)
{
    void *pointer = my_malloc(64U);

    assert(pointer != NULL);
    my_free(pointer);
    (void)my_realloc(pointer, 128U);
}

static void interior_free(void)
{
    void *pointer = my_malloc(64U);

    assert(pointer != NULL);
    my_free((unsigned char *)pointer + 1U);
}

static void foreign_free(void)
{
    my_free(&foreign_object);
}

static void bad_magic(void)
{
    void *pointer = my_malloc(64U);

    assert(pointer != NULL);
    header_for(pointer)->magic ^= (size_t)1U;
    my_free(pointer);
}

static void bad_size(void)
{
    void *left;
    void *right;

    allocate_pair(&left, &right);
    header_for(left)->size += (size_t)_Alignof(max_align_t);
    my_free(left);
}

static void bad_next(void)
{
    void *left;
    void *right;

    allocate_pair(&left, &right);
    header_for(left)->next = (header_t *)(uintptr_t)1U;
    my_free(left);
}

static void bad_requested_size(void)
{
    void *pointer = my_malloc(64U);

    assert(pointer != NULL);
    header_for(pointer)->requested = SIZE_MAX;
    (void)my_realloc(pointer, 128U);
}

static void corrupt_before_malloc(void)
{
    void *pointer = my_malloc(64U);

    assert(pointer != NULL);
    header_for(pointer)->magic ^= (size_t)1U;
    (void)my_malloc(32U);
}

static void run_death_test(const char *name, corruption_fn function)
{
    pid_t child = fork();
    int status;

    assert(child >= 0);
    if (child == 0) {
        function();
        _exit(0);
    }
    assert(waitpid(child, &status, 0) == child);
    if (!WIFSIGNALED(status) || WTERMSIG(status) != SIGABRT) {
        fprintf(stderr, "corruption case did not abort: %s (status=%d)\n",
                name, status);
        exit(EXIT_FAILURE);
    }
    printf("corruption case %-24s: detected\n", name);
}

int main(void)
{
    static const struct {
        const char *name;
        corruption_fn function;
    } cases[] = {
        {"double free", double_free},
        {"realloc after free", realloc_freed},
        {"interior pointer", interior_free},
        {"foreign pointer", foreign_free},
        {"bad magic", bad_magic},
        {"bad block size", bad_size},
        {"bad next link", bad_next},
        {"bad requested size", bad_requested_size},
        {"corruption during malloc", corrupt_before_malloc}
    };

    setvbuf(stdout, NULL, _IONBF, 0);
    for (size_t index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        run_death_test(cases[index].name, cases[index].function);
    }
    printf("all explicit corruption tests passed\n");
    return 0;
}
