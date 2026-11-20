<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset=".github/banner-dark.svg">
    <img alt="freelist — built from zero" src=".github/banner-light.svg" width="100%">
  </picture>
</p>

# freelist

An educational, explicitly linked `malloc`-family allocator in C11, using POSIX
`sbrk(2)` and a single address-ordered free list. The public names are
`my_malloc`, `my_free`, `my_calloc`, and `my_realloc`.

**This is a study project, not a production allocator or a drop-in replacement
for the C library allocator.** See [Design and security limits](#design-and-security-limits).

## Build and verify

Requirements: a C11 compiler, POSIX threads, `fork`/`waitpid` for corruption
tests, and GNU Make. Sanitizer targets additionally require compiler/runtime
support for those sanitizers.

```sh
make clean
make CC=gcc all
make CC=gcc test
make CC=gcc asan
make CC=gcc ubsan
make CC=clang all
make CC=clang test
make CC=clang sanitize

# Optional where ThreadSanitizer is supported:
make CC=gcc tsan
```

The compiler flags treat warnings as errors and include conversion, sign,
shadowing, alignment, format, and prototype checks. The default `make test`
runs unit tests, raw/synchronized invariant checks, a deterministic randomized
differential model, a concurrency stress test, explicit corruption subprocesses,
and a deterministic 1,000-block stress test.

- Model seeds: `0x1`, `0xc0ffee11`, and `0xdeadbeef`; 12,000 operations each.
- Concurrency: eight threads, 5,000 operations per thread, plus a thread that
  repeatedly checks global invariants.
- Corruption tests require `SIGABRT` in a forked child for double free, realloc
  after free, interior/foreign pointers, bad magic, size, link, and requested
  size, and corruption encountered during allocation.

`make asan` runs all six test executables with AddressSanitizer and leak
checking. `make ubsan` runs all six with non-recovering undefined-behavior
checks. `make tsan` runs the five non-corruption executables: intentional death
subprocesses add noise without improving ThreadSanitizer coverage. The suite
uses only this allocator's `my_*` symbols, never `LD_PRELOAD`, symbol
interposition, or replacement of glibc allocation, so sanitizer and C library
allocators do not collide.

## API and deliberate semantics

- `void *my_malloc(size_t size)` returns a payload aligned to
  `_Alignof(max_align_t)`. A zero request returns `NULL`.
- `void my_free(void *pointer)` accepts `NULL`; other invalid pointers and a
  detected double free are fatal (see below).
- `void *my_calloc(size_t count, size_t size)` checks multiplication overflow,
  then zeroes the entire requested payload. If either argument is zero it
  returns `NULL` through `my_malloc(0)`.
- `void *my_realloc(void *pointer, size_t size)` handles `NULL` like
  `my_malloc(size)`, and a zero size frees `pointer` and returns `NULL`.
  Moving growth preserves `min(old_requested_size, size)` bytes. A failed
  allocation leaves the original allocation intact.
- Size-rounding overflow and impossible `sbrk` growth requests fail with
  `errno = ENOMEM`. An `sbrk` failure retains the error set by `sbrk`.
- If another user has moved the program break and made the managed heap
  discontinuous, growth fails with `errno = ENOTSUP` rather than creating
  corrupt list links.

These zero-size choices are deliberate and need not match every libc.
`header_t`, `print_heap`, and the `debug_*` helpers are diagnostic/educational,
not a stable binary interface. `debug_heap_head` and `debug_heap_base` are raw
views for quiescent single-threaded checks; use synchronized
`debug_validate_heap()` in concurrent tests.

## Design and security limits

### Data structure and invariants

Each block has a header containing total block size, exact requested payload
size, next-block pointer, and a state cookie. Block size includes the header;
all sizes and addresses preserve `_Alignof(max_align_t)` alignment.

Under one global mutex, allocation uses first fit and splits a sufficiently
large block. Free and realloc coalesce both physical neighbors, including a
free successor after shrink/growth. For every valid heap:

1. The list is contiguous and address ordered over exactly
   `[heap_base, heap_end)`.
2. Every block is in range, aligned, has an aligned size of at least
   `sizeof(header_t) + _Alignof(max_align_t)`, and has a valid state cookie.
3. Every used block's requested size fits its payload; a free block's requested
   size is zero.
4. `next` is either the exact physical successor or `NULL`; the last block is
   `tail` and ends at `heap_end`.
5. No two adjacent blocks are both free.

`realloc` grows into a free successor when possible and otherwise allocates,
copies only the exact old requested payload, and frees the old block. It does
not copy rounded capacity as though it were initialized user data.

### Threading, process, and corruption boundaries

- One mutex protects all list metadata and the `sbrk` growth operation.
  Independent allocations can be used concurrently. As with standard
  allocation APIs, callers must not race operations on the same allocation,
  use an object after freeing it, or read/write outside its requested size.
- `sbrk` changes process-global state. Do not call `sbrk` directly or combine
  this allocator with another allocator/runtime that moves the program break
  while it is active. A discontinuity is rejected at the next growth, but this
  cannot make arbitrary mixed allocator use safe. The heap is never returned
  to the OS and allocation/free are O(number of blocks).
- Metadata checks are best-effort debugging aids, not a security boundary.
  Aligned interior pointers, a header overwritten with plausible values,
  undetected payload overruns, and malicious corruption can still evade
  checks. Detected corruption aborts immediately; it is not recoverable.
- The allocator is not async-signal-safe, fork-safe in a multithreaded process,
  or cancellation-safe. No hardened freelist encoding, guard pages, quarantine,
  or protection against use-after-free is claimed.

## Continuous integration and releases

`.github/workflows/ci.yml` runs strict GCC and Clang builds/tests plus ASan and
UBSan on Ubuntu. ThreadSanitizer remains a separate optional target because its
availability and behavior vary by toolchain and host environment.

Releases use **0.x** semantic versioning while the educational API and layout
may change. Increment the patch version for compatible fixes, the minor version
for new features or documented behavior/API changes, and use `0.x.y` git tags
matching the release. Every release must document tested compilers/platforms,
link the successful CI run, and restate that this is not production/drop-in
allocator software. No 1.0 or production-safety claim is implied by these
correctness tests.

## License

MIT; see [LICENSE](LICENSE).
