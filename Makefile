CC ?= gcc
CPPFLAGS ?= -Isrc -D_DEFAULT_SOURCE
CFLAGS ?= -O2 -g
WARNINGS := -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion \
	-Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wcast-align \
	-Wpointer-arith -Wformat=2 -Wundef -Wvla
COMMON_FLAGS := $(CPPFLAGS) $(CFLAGS) -std=c11 $(WARNINGS) -pthread
ASAN_FLAGS := -fsanitize=address -fno-omit-frame-pointer
UBSAN_FLAGS := -fsanitize=undefined -fno-sanitize-recover=all \
	-fno-omit-frame-pointer
TSAN_FLAGS := -fsanitize=thread -fno-omit-frame-pointer

TESTS := test_malloc test_invariants test_model test_threads test_corruption stress_test
TSAN_TESTS := test_malloc test_invariants test_model test_threads stress_test
LIB := build/libfreelist.a
PREFIX ?= /usr/local
DESTDIR ?=
AR ?= ar
RANLIB ?= ranlib
INSTALL ?= install
ARFLAGS = rcs
BINS := $(addprefix build/,$(TESTS))
ASAN_BINS := $(addprefix build/asan/,$(TESTS))
UBSAN_BINS := $(addprefix build/ubsan/,$(TESTS))
TSAN_BINS := $(addprefix build/tsan/,$(TSAN_TESTS))

all: $(BINS) $(LIB)

build/malloc.o: src/malloc.c src/malloc.h
	@mkdir -p $(@D)
	$(CC) $(COMMON_FLAGS) -c src/malloc.c -o $@

$(LIB): build/malloc.o
	$(AR) $(ARFLAGS) $@ $<
	$(RANLIB) $@

build/%: src/malloc.c src/%.c src/malloc.h
	@mkdir -p $(@D)
	$(CC) $(COMMON_FLAGS) src/malloc.c src/$*.c $(LDFLAGS) -o $@

build/asan/%: src/malloc.c src/%.c src/malloc.h
	@mkdir -p $(@D)
	$(CC) $(COMMON_FLAGS) $(ASAN_FLAGS) src/malloc.c src/$*.c $(LDFLAGS) $(ASAN_FLAGS) -o $@

build/ubsan/%: src/malloc.c src/%.c src/malloc.h
	@mkdir -p $(@D)
	$(CC) $(COMMON_FLAGS) $(UBSAN_FLAGS) src/malloc.c src/$*.c $(LDFLAGS) $(UBSAN_FLAGS) -o $@

build/tsan/%: src/malloc.c src/%.c src/malloc.h
	@mkdir -p $(@D)
	$(CC) $(COMMON_FLAGS) $(TSAN_FLAGS) src/malloc.c src/$*.c $(LDFLAGS) $(TSAN_FLAGS) -o $@

test: $(BINS)
	@set -eu; for test_bin in $(BINS); do \
		echo "==> $$test_bin"; ./$$test_bin; \
	done

asan: $(ASAN_BINS)
	@set -eu; for test_bin in $(ASAN_BINS); do \
		echo "==> $$test_bin"; \
		ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./$$test_bin; \
	done

ubsan: $(UBSAN_BINS)
	@set -eu; for test_bin in $(UBSAN_BINS); do \
		echo "==> $$test_bin"; \
		UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ./$$test_bin; \
	done

sanitize: asan ubsan

tsan: $(TSAN_BINS)
	@set -eu; for test_bin in $(TSAN_BINS); do \
		echo "==> $$test_bin"; TSAN_OPTIONS=halt_on_error=1 ./$$test_bin; \
	done

stress: build/stress_test
	./build/stress_test

invariant: build/test_invariants
	./build/test_invariants

install: $(LIB)
	$(INSTALL) -d $(DESTDIR)$(PREFIX)/lib $(DESTDIR)$(PREFIX)/include $(DESTDIR)$(PREFIX)/share/licenses/freelist
	$(INSTALL) -m644 $(LIB) $(DESTDIR)$(PREFIX)/lib/libfreelist.a
	$(INSTALL) -m644 src/malloc.h $(DESTDIR)$(PREFIX)/include/freelist.h
	$(INSTALL) -m644 LICENSE $(DESTDIR)$(PREFIX)/share/licenses/freelist/LICENSE

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/lib/libfreelist.a $(DESTDIR)$(PREFIX)/include/freelist.h $(DESTDIR)$(PREFIX)/share/licenses/freelist/LICENSE

clean:
	rm -rf build

.PHONY: all test asan ubsan sanitize tsan stress invariant clean install uninstall
