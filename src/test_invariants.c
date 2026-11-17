/* Invariant checker for the rewritten SIFR allocator:
 *  - valid magic cookie on every block
 *  - blocks are address-ordered and physically contiguous
 *  - every block lies inside [heap_base, program break)
 */
#define _GNU_SOURCE
#include <unistd.h>
#include "malloc.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

extern void *debug_heap_head(void);
extern void *debug_heap_base(void);

typedef struct blk { size_t size; size_t magic; struct blk *next; size_t _pad; } blk;
#define MAGIC_USED_V 0x534946522D5501ULL
#define MAGIC_FREE_V 0x534946522D4601ULL

static void dump(void) {
    blk *c = (blk *)debug_heap_head();
    size_t i = 0;
    printf("--- heap dump ---\n");
    while (c) {
        printf("  [%zu] addr=%p size=%zu next=%p magic=%s\n", i, (void *)c, c->size, (void *)c->next,
               c->magic == MAGIC_USED_V ? "USED" : c->magic == MAGIC_FREE_V ? "FREE" : "????");
        c = c->next;
        i++;
    }
    printf("--- end ---\n");
}

static int check_heap(const char *where) {
    void *base = debug_heap_base();
    void * brk_  = sbrk(0);
    blk *c = (blk *)debug_heap_head();
    size_t idx = 0;
    while (c) {
        blk *nxt = c->next;
        if (c->magic != MAGIC_USED_V && c->magic != MAGIC_FREE_V) {
            printf("INVARIANT: bad magic @ %s block[%zu] addr=%p\n", where, idx, (void *)c);
            return -1;
        }
        if (c->size < 48 || (c->size & (16 - 1)) != 0) {
            printf("INVARIANT: bad size @ %s block[%zu] addr=%p size=%zu\n", where, idx, (void *)c, c->size);
            return -1;
        }
        if ((char *)c < (char *)base || (char *)c + c->size > (char *)brk_) {
            printf("INVARIANT: block out of bounds @ %s block[%zu] addr=%p end=%p base=%p brk=%p\n",
                   where, idx, (void *)c, (char *)c + c->size, base, brk_);
            return -1;
        }
        if (nxt) {
            if ((char *)nxt != (char *)c + c->size) {
                printf("INVARIANT: non-contiguity @ %s block[%zu] addr=%p size=%zu next=%p expected=%p\n",
                       where, idx, (void *)c, c->size, (void *)nxt, (char *)c + c->size);
                return -1;
            }
        }
        c = nxt;
        idx++;
    }
    return 0;
}

static void print_brk(void) {
    printf("program break = %p\n", sbrk(0));
}

int main(void) {
    print_brk();
    void *a = my_malloc(40); if (check_heap("malloc(40)") != 0) return 1;
    my_free(a);              if (check_heap("free(a)") != 0)    return 1;

    printf("phase 2: interleaved alloc 1..64\n");
    void *p[64];
    for (size_t i = 0; i < 64; i++) {
        char tag[64]; snprintf(tag, sizeof tag, "malloc(%zu)", i + 1);
        (void)sbrk(0);
        p[i] = my_malloc(i + 1);
        if (check_heap(tag) != 0) { dump(); return 1; }
    }
    for (size_t i = 0; i < 64; i++) {
        char tag[64]; snprintf(tag, sizeof tag, "free[%zu]", i);
        my_free(p[i]);
        if (check_heap(tag) != 0) { dump(); return 1; }
    }

    printf("phase 3: realloc shrink + grow\n");
    void *q = my_malloc(4096); if (check_heap("malloc(4096)") != 0) return 1;
    void *q2 = my_realloc(q, 128); if (check_heap("realloc(4096->128)") != 0) return 1;
    void *q3 = my_realloc(q2, 8192); if (check_heap("realloc(128->8192)") != 0) return 1;
    my_free(q3);                 if (check_heap("free(q3)") != 0) return 1;

    printf("phase 4: calloc\n");
    void *z = my_calloc(100, 4); if (check_heap("calloc(100,4)") != 0) return 1;
    my_free(z);

    printf("phase 5: realloc in-place growth chain\n");
    void *L = my_malloc(128); if (check_heap("malloc(L)") != 0) return 1;
    void *R = my_malloc(4096); if (check_heap("malloc(R)") != 0) return 1;
    my_free(R);                if (check_heap("free(R)") != 0) return 1;
    void *L2 = my_realloc(L, 2048); if (check_heap("realloc(L,2048)") != 0) return 1;
    printf("L==L2: %d\n", L == L2);
    my_free(L2);

    printf("DONE — all invariants hold\n");
    return 0;
}
