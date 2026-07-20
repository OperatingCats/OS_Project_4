CC := gcc

CPPFLAGS := -Iinclude -Ithird_party/sha256
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror
LDLIBS :=

BUILD_DIR := build
BIN_DIR := bin

CRYPTO_TEST := $(BIN_DIR)/test_crypto

CRYPTO_TEST_SOURCES := \
	common/errors.c \
	common/crypto.c \
	third_party/sha256/sha256.c \
	tests/test_crypto.c

.PHONY: build test clean

build: $(CRYPTO_TEST)

$(CRYPTO_TEST): $(CRYPTO_TEST_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CRYPTO_TEST_SOURCES) \
		-o $(CRYPTO_TEST) $(LDLIBS)

test: build
	./$(CRYPTO_TEST)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)