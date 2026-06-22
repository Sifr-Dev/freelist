CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I./src -O2 -g
LDFLAGS = -lpthread

SRC_DIR = src
SRCS = $(SRC_DIR)/malloc.c $(SRC_DIR)/test_malloc.c
STRESS_SRC = $(SRC_DIR)/malloc.c $(SRC_DIR)/stress_test.c
TEST_BIN = test_malloc
STRESS_BIN = stress_test

all: $(TEST_BIN) $(STRESS_BIN)

$(TEST_BIN): $(SRCS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(STRESS_BIN): $(STRESS_SRC)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

test: $(TEST_BIN)
	./$(TEST_BIN)

stress: $(STRESS_BIN)
	./$(STRESS_BIN)

clean:
	rm -f $(TEST_BIN) $(STRESS_BIN)

.PHONY: all test stress clean
