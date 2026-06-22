# Custom Low-Level Memory Allocator (Malloc)

A custom dynamic memory allocator implementation in C, replacing standard library malloc/free.

## Tech Stack
- C (C99)
- POSIX system calls (sbrk)
- POSIX threads (pthread_mutex)

## Architecture
- Free-list based allocation with block splitting and coalescing
- Thread-safe via mutex locking
- Uses sbrk() for heap expansion
- Minimal memory overhead per allocation

## Build & Test
```bash
make          # Build test and stress binaries
make test     # Run unit tests
make stress   # Run stress test
```

## API
- `void *my_malloc(size_t size)` - Allocate memory
- `void my_free(void *ptr)` - Free allocated memory
- `void *my_calloc(size_t count, size_t size)` - Zero-initialized allocation
- `void *my_realloc(void *ptr, size_t new_size)` - Resize allocation
