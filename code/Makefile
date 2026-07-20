CC := gcc

CPPFLAGS := -Iinclude -Ithird_party/sha256
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror

BUILD_DIR := build
BIN_DIR := bin

CRYPTO_TEST := $(BIN_DIR)/test_crypto
BLOCK_TEST := $(BIN_DIR)/test_block

COMMON_SOURCES := \
	common/errors.c \
	common/crypto.c \
	common/block.c \
	third_party/sha256/sha256.c

CRYPTO_TEST_SOURCES := \
	$(COMMON_SOURCES) \
	tests/test_crypto.c

BLOCK_TEST_SOURCES := \
	common/errors.c \
	common/block.c \
	tests/test_block.c

.PHONY: build test clean

build: $(CRYPTO_TEST) $(BLOCK_TEST)

$(CRYPTO_TEST): $(CRYPTO_TEST_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CRYPTO_TEST_SOURCES) \
		-o $(CRYPTO_TEST)

$(BLOCK_TEST): $(BLOCK_TEST_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(BLOCK_TEST_SOURCES) \
		-o $(BLOCK_TEST)

test: build
	./$(CRYPTO_TEST)
	./$(BLOCK_TEST)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)