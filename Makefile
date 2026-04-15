CC := gcc
AR := ar

CFLAGS := -Wall -Wextra -pedantic -std=c99 -D_POSIX_C_SOURCE=200809L -g -Icommon/include -Iserver -Iclient
LDFLAGS :=

BUILD_DIR := build
BIN_DIR := bin

COMMON_SRCS := $(wildcard common/src/*.c)
SERVER_SRCS := $(wildcard server/*.c)
CLIENT_SRCS := $(wildcard client/*.c)
TEST_SRCS := $(wildcard tests/*.c)

COMMON_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(COMMON_SRCS))
SERVER_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SERVER_SRCS))
CLIENT_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(CLIENT_SRCS))
TEST_OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(TEST_SRCS))

COMMON_LIB := $(BUILD_DIR)/libbomberman.a
SERVER_BIN := $(BIN_DIR)/bomberman_server
CLIENT_BIN := $(BIN_DIR)/bomberman_client
TEST_BINS := $(patsubst tests/%.c,$(BIN_DIR)/%,$(TEST_SRCS))

all: $(SERVER_BIN) $(CLIENT_BIN)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(COMMON_LIB): $(COMMON_OBJS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(SERVER_BIN): $(SERVER_OBJS) $(COMMON_LIB)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(SERVER_OBJS) $(COMMON_LIB) -o $@ $(LDFLAGS)

$(CLIENT_BIN): $(CLIENT_OBJS) $(COMMON_LIB)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(CLIENT_OBJS) $(COMMON_LIB) -o $@ $(LDFLAGS)

$(BIN_DIR)/%: $(BUILD_DIR)/tests/%.o $(COMMON_LIB)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $< $(COMMON_LIB) -o $@ $(LDFLAGS)

tests: $(TEST_BINS)
	@set -e; \
	for t in $(TEST_BINS); do \
		echo "Running $$t"; \
		$$t; \
	done

run-server: $(SERVER_BIN)
	$(SERVER_BIN) 4000 server/config/level_1.txt

run-client: $(CLIENT_BIN)
	$(CLIENT_BIN) 127.0.0.1 4000 player

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.PHONY: all clean tests run-server run-client
