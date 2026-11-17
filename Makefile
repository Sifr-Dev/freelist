CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I./src -O2 -g
LDFLAGS = -lpthread

SRC_DIR = src
SRCS = $(SRC_DIR)/malloc.c $(SRC_DIR)/test_malloc.c
STRESS_SRC = $(SRC_DIR)/malloc.c $(SRC_DIR)/stress_test.c
INV_SRC = $(SRC_DIR)/malloc.c $(SRC_DIR)/test_invariants.c
TEST_BIN = test_malloc
STRESS_BIN = stress_test
INV_BIN = test_invariants

all: $(TEST_BIN) $(STRESS_BIN) $(INV_BIN)

$(TEST_BIN): $(SRCS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(STRESS_BIN): $(STRESS_SRC)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(INV_BIN): $(INV_SRC)
	$(CC) $(CFLAGS) $^ -o $@

test: $(TEST_BIN) $(STRESS_BIN) $(INV_BIN)
	./$(TEST_BIN)
	./$(STRESS_BIN)
	./$(INV_BIN)

stress: $(STRESS_BIN)
	./$(STRESS_BIN)

invariant: $(INV_BIN)
	./$(INV_BIN)

# Sanitizer build: undefined-behavior checks run unconditionally.
asan: $(SRC_DIR)/malloc.c $(SRC_DIR)/test_malloc.c
	$(CC) $(CFLAGS) -fsanitize=undefined -fno-omit-frame-pointer $(SRCS) -o test_malloc_ubsan $(LDFLAGS)
	./test_malloc_ubsan

clean:
	rm -f $(TEST_BIN) $(STRESS_BIN) $(INV_BIN) test_malloc_asan test_malloc_ubsan

.PHONY: all test stress invariant asan clean
