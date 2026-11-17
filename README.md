<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset=".github/banner-dark.svg">
    <img alt="malloc_allocator — built from zero" src=".github/banner-light.svg" width="100%">
  </picture>
</p>

# malloc_allocator

custom dynamic memory allocator in C — a free-list `malloc`/`free`/`calloc`/`realloc`
replacing the standard library allocator. zero dependencies beyond POSIX.

## tech stack

- C (C99), POSIX (`sbrk`, `pthread_mutex`)
- free-list allocation with block splitting/coalescing
- thread-safe (mutex); educational but correct — every claim below is verified

## build & verify

```sh
make          # build test + stress binaries
make test     # unit tests
make stress   # 1000-block alloc/free stress
make clean
```

verified on this machine (2026-11-17): build exit 0, `make test` → all 7 tests
pass, `make stress` → PASS (1000 blocks).

## api

- `void *my_malloc(size_t size)` — allocate
- `void my_free(void *ptr)` — free (NULL-safe)
- `void *my_calloc(size_t count, size_t size)` — zero-initialized
- `void *my_realloc(void *ptr, size_t new_size)` — resize, preserves data

## design notes

- `sbrk` for heap growth (educational; a production allocator would `mmap`)
- per-block header: size, free flag, magic canary
- splitting on alloc, coalescing on free

## known limitations

- `sbrk` model: never returns memory to the OS (fine for a from-scratch
  allocator used as a study piece; not a drop-in for long-lived servers)
- no size-class segregation (single free list; O(n) worst case)

## license

`MIT` — see `LICENSE`. built from zero.
