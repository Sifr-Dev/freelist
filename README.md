# Lattice

**A study of a free-list allocator in C11 — explicitly not production software.**

[![C](https://img.shields.io/badge/C-C11-00599c?style=for-the-badge&logo=c&logoColor=white&labelColor=1b1f27)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![CI](https://github.com/Sifr-Dev/freelist/actions/workflows/ci.yml/badge.svg)](https://github.com/Sifr-Dev/freelist/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/LICENSE-MIT-2ea043?style=for-the-badge&labelColor=1b1f27)](LICENSE)

> The GitHub repository remains `Sifr-Dev/freelist`; Lattice is the project
> name. The public allocation symbols (`my_malloc`, `my_free`, `my_calloc`,
> `my_realloc`) and the installed header (`freelist.h`) are unchanged.

## What it is

Lattice is an educational `malloc`-family allocator built on POSIX `sbrk(2)`
and an address-ordered free list. It exists to make allocator invariants,
coalescing, alignment, corruption detection and sanitizer behaviour explicit
and testable. It is a study project, not a drop-in replacement for the C
library allocator.

## Current capabilities

- **First-fit allocation with splitting, bidirectional coalescing and
  realloc growth into a free successor.**
  Proof: `src/malloc.c`; `make CC=gcc test` runs the unit suite.
- **Aligned blocks and exact requested-size tracking**, with overflow checks
  and `ENOMEM`/`ENOTSUP` failure paths.
  Proof: `src/malloc.c`, `src/malloc.h`,
  `build/test_malloc` (`splitting and realloc NULL/zero semantics`).
- **Raw and synchronized invariant checks** over the whole heap.
  Proof: `src/test_invariants.c`,
  `build/test_invariants` (`raw and synchronized invariants after each
  transition: PASS`).
- **Deterministic randomized differential model** against a reference model
  across three seeds and 12,000 operations each.
  Proof: `src/test_model.c`, `build/test_model`.
- **Concurrency stress test**: eight threads × 5,000 deterministic operations,
  with a thread continuously checking global invariants.
  Proof: `src/test_threads.c`, `build/test_threads`.
- **Corruption subprocesses** that require `SIGABRT` for double free, realloc
  after free, interior/foreign pointers, bad magic, bad size, bad link, bad
  requested size, and corruption during allocation.
  Proof: `src/test_corruption.c`, `build/test_corruption`.
- **Static library and install target**, with an installed-consumer check.
  Proof: `make` builds `build/libfreelist.a`; `tests/install-check.sh`.
- **Sanitizer targets** for AddressSanitizer, UndefinedBehaviorSanitizer and
  optional ThreadSanitizer.
  Proof: `make asan`, `make ubsan`, `make tsan` in the `Makefile`.

## Quick start

Requirements: a C11 compiler, POSIX threads, `fork`/`waitpid` for the
corruption tests, and GNU Make.

```sh
git clone https://github.com/Sifr-Dev/freelist.git
cd freelist
make clean
make CC=gcc all
make CC=gcc test
make CC=gcc asan
make CC=gcc ubsan
make CC=clang all
make CC=clang test
make CC=clang sanitize
```

Optional where ThreadSanitizer is supported:

```sh
make CC=gcc tsan
```

Link the static archive into a consumer:

```sh
make DESTDIR=/tmp/lattice-package PREFIX=/usr install
sh tests/install-check.sh
```

The installed header is `freelist.h`; the allocation symbols remain `my_*`.
`make uninstall` removes the installed archive, header and licence.

## Verification

Executed on 2026-11-28 on Linux x86-64 with GCC 16.2.1:

```sh
make clean
make CC=gcc test
```

Observed: all six test executables passed — unit tests, raw/synchronized
invariants, three deterministic model seeds (12,000 operations each),
eight-thread concurrency stress, nine corruption cases requiring `SIGABRT`,
and the deterministic 1,000-block stress test. The sanitizer targets are
available but were not re-run in this verification pass; CI runs
`make CC=<gcc|clang> sanitize`.

## Architecture

```text
src/malloc.c        allocator implementation and public my_* API
src/malloc.h        installed public header
src/test_malloc.c   unit and API-semantics tests
src/test_invariants.c  raw and synchronized heap invariants
src/test_model.c    randomized differential model
src/test_threads.c  concurrency stress and invariant observer
src/test_corruption.c  forked corruption cases
src/stress_test.c   deterministic 1,000-block stress
tests/install-check.sh  installed-header and archive consumer check
Makefile            build, test, sanitizer, install and uninstall targets
```

## Benchmarks

No benchmark is published. Allocation and free are O(number of blocks); the
project makes no throughput claim and is not intended for performance work.

## Limitations

- One global mutex; allocation and free scale poorly and are O(number of
  blocks).
- The heap is never returned to the OS.
- `sbrk` changes process-global state. Do not mix this allocator with another
  allocator or runtime that moves the program break.
- Metadata checks are best-effort debugging aids, not a security boundary.
  Aligned interior pointers and plausible header corruption can evade them.
- Detected corruption aborts immediately; it is not recoverable.
- Not async-signal-safe, fork-safe in a multithreaded process, or
  cancellation-safe.
- No hardened freelist encoding, guard pages, quarantine or use-after-free
  protection.
- The zero-size and invalid-pointer semantics are deliberate and need not match
  every libc.
- Development API and layout are `0.x`; binary stability is not promised.

## Roadmap (NOT IMPLEMENTED)

- Per-thread caches or size-class bins for a comparative study.
- Guard pages and quarantine as an optional hardening experiment.
- Heap return to the OS.
- A reproducible benchmark against the system allocator with documented
  hardware and workload.
- A stable ABI beyond the educational one.

## License

MIT; see [LICENSE](LICENSE).
