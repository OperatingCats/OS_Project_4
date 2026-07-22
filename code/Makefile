CC := gcc

CPPFLAGS := -Iinclude -Ithird_party/sha256
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror
PTHREAD_FLAGS := -pthread

BUILD_DIR := build
BIN_DIR := bin

CRYPTO_TEST := $(BIN_DIR)/test_crypto
BLOCK_TEST := $(BIN_DIR)/test_block
MERKLE_TEST := $(BIN_DIR)/test_merkle
VALIDATION_TEST := $(BIN_DIR)/test_validation
BLOCK_HASH_TEST := $(BIN_DIR)/test_block_hash
BLOCK_VALIDATION_TEST := $(BIN_DIR)/test_block_validation
IPC_TEST := $(BIN_DIR)/test_ipc
BLOCK_LINK_TEST := $(BIN_DIR)/test_block_link
BLOCKCHAIN_TEST := $(BIN_DIR)/test_blockchain
MEMPOOL_TEST := $(BIN_DIR)/test_mempool

PROPOSE_BLOCK_TEST := $(BIN_DIR)/test_propose_block
QUERY_TEST := $(BIN_DIR)/test_query
PROPOSE_VALID_BLOCK_TEST := $(BIN_DIR)/test_propose_valid_block
PROPOSE_NEXT_BLOCK_TEST := $(BIN_DIR)/test_propose_next_block
RELAY_COMMIT_TEST := $(BIN_DIR)/test_relay_commit

BOOTSTRAP_BIN := $(BIN_DIR)/blockchain
MINER_BIN := $(BIN_DIR)/miner
CLIENT_BIN := $(BIN_DIR)/client
NODE_BIN := $(BIN_DIR)/node

CRYPTO_TEST_SOURCES := \
	common/errors.c \
	common/crypto.c \
	common/block.c \
	third_party/sha256/sha256.c \
	tests/test_crypto.c

BLOCK_TEST_SOURCES := \
	common/errors.c \
	common/block.c \
	tests/test_block.c

MERKLE_TEST_SOURCES := \
	common/errors.c \
	common/crypto.c \
	common/block.c \
	third_party/sha256/sha256.c \
	tests/test_merkle.c

VALIDATION_TEST_SOURCES := \
	common/errors.c \
	common/block.c \
	common/crypto.c \
	common/validation.c \
	third_party/sha256/sha256.c \
	tests/test_validation.c

BLOCK_HASH_TEST_SOURCES := \
	common/errors.c \
	common/block.c \
	common/crypto.c \
	third_party/sha256/sha256.c \
	tests/test_block_hash.c

BLOCK_VALIDATION_TEST_SOURCES := \
	common/errors.c \
	common/block.c \
	common/crypto.c \
	common/validation.c \
	third_party/sha256/sha256.c \
	tests/test_block_validation.c

IPC_TEST_SOURCES := \
	common/errors.c \
	common/ipc.c \
	tests/test_ipc.c

BLOCK_LINK_TEST_SOURCES := \
	common/errors.c \
	common/block.c \
	common/crypto.c \
	common/validation.c \
	third_party/sha256/sha256.c \
	tests/test_block_link.c

BLOCKCHAIN_TEST_SOURCES := \
	common/errors.c \
	common/block.c \
	common/blockchain.c \
	common/crypto.c \
	common/validation.c \
	third_party/sha256/sha256.c \
	tests/test_blockchain.c

MEMPOOL_TEST_SOURCES := \
	common/errors.c \
	miner/mempool.c \
	tests/test_mempool.c

PROPOSE_BLOCK_TEST_SOURCES := \
	common/errors.c \
	common/ipc.c \
	tests/test_propose_block.c

QUERY_TEST_SOURCES := \
	common/errors.c \
	common/ipc.c \
	tests/test_query.c

PROPOSE_VALID_BLOCK_TEST_SOURCES := \
	common/errors.c \
	common/block.c \
	common/crypto.c \
	common/ipc.c \
	third_party/sha256/sha256.c \
	tests/test_propose_valid_block.c

PROPOSE_NEXT_BLOCK_TEST_SOURCES := \
	common/errors.c \
	common/block.c \
	common/crypto.c \
	common/ipc.c \
	third_party/sha256/sha256.c \
	tests/test_propose_next_block.c

RELAY_COMMIT_TEST_SOURCES := \
	common/errors.c \
	common/ipc.c \
	tests/test_relay_commit.c

BOOTSTRAP_SOURCES := \
	common/errors.c \
	common/ipc.c \
	common/process_info.c \
	bootstrap/bootstrap.c

MINER_SOURCES := \
	common/errors.c \
	common/block.c \
	common/crypto.c \
	common/validation.c \
	common/ipc.c \
	third_party/sha256/sha256.c \
	miner/mempool.c \
	miner/miner.c

CLIENT_SOURCES := \
	common/errors.c \
	common/ipc.c \
	client/client.c

NODE_SOURCES := \
	common/errors.c \
	common/block.c \
	common/blockchain.c \
	common/crypto.c \
	common/validation.c \
	common/ipc.c \
	common/node_queue.c \
	common/logger.c \
	third_party/sha256/sha256.c \
	node/node.c

ALL_TEST_BINARIES := \
	$(CRYPTO_TEST) \
	$(BLOCK_TEST) \
	$(MERKLE_TEST) \
	$(VALIDATION_TEST) \
	$(BLOCK_HASH_TEST) \
	$(BLOCK_VALIDATION_TEST) \
	$(IPC_TEST) \
	$(BLOCK_LINK_TEST) \
	$(BLOCKCHAIN_TEST) \
	$(MEMPOOL_TEST) \
	$(PROPOSE_BLOCK_TEST) \
	$(QUERY_TEST) \
	$(PROPOSE_VALID_BLOCK_TEST) \
	$(PROPOSE_NEXT_BLOCK_TEST) \
	$(RELAY_COMMIT_TEST)

ALL_PROGRAMS := \
	$(BOOTSTRAP_BIN) \
	$(MINER_BIN) \
	$(CLIENT_BIN) \
	$(NODE_BIN)

.PHONY: build test test-c test-shell clean run

build: $(ALL_TEST_BINARIES) $(ALL_PROGRAMS)

$(CRYPTO_TEST): $(CRYPTO_TEST_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CRYPTO_TEST_SOURCES) -o $@

$(BLOCK_TEST): $(BLOCK_TEST_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(BLOCK_TEST_SOURCES) -o $@

$(MERKLE_TEST): $(MERKLE_TEST_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(MERKLE_TEST_SOURCES) -o $@

$(VALIDATION_TEST): $(VALIDATION_TEST_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(VALIDATION_TEST_SOURCES) -o $@

$(BLOCK_HASH_TEST): $(BLOCK_HASH_TEST_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(BLOCK_HASH_TEST_SOURCES) -o $@

$(BLOCK_VALIDATION_TEST): $(BLOCK_VALIDATION_TEST_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(BLOCK_VALIDATION_TEST_SOURCES) -o $@

$(IPC_TEST): $(IPC_TEST_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(IPC_TEST_SOURCES) -o $@

$(BLOCK_LINK_TEST): $(BLOCK_LINK_TEST_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(BLOCK_LINK_TEST_SOURCES) -o $@

$(BLOCKCHAIN_TEST): $(BLOCKCHAIN_TEST_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(BLOCKCHAIN_TEST_SOURCES) -o $@

$(MEMPOOL_TEST): $(MEMPOOL_TEST_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(PTHREAD_FLAGS) $(MEMPOOL_TEST_SOURCES) -o $@ $(PTHREAD_FLAGS)

$(PROPOSE_BLOCK_TEST): $(PROPOSE_BLOCK_TEST_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(PROPOSE_BLOCK_TEST_SOURCES) -o $@

$(QUERY_TEST): $(QUERY_TEST_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(QUERY_TEST_SOURCES) -o $@

$(PROPOSE_VALID_BLOCK_TEST): $(PROPOSE_VALID_BLOCK_TEST_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(PROPOSE_VALID_BLOCK_TEST_SOURCES) -o $@

$(PROPOSE_NEXT_BLOCK_TEST): $(PROPOSE_NEXT_BLOCK_TEST_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(PROPOSE_NEXT_BLOCK_TEST_SOURCES) -o $@

$(RELAY_COMMIT_TEST): $(RELAY_COMMIT_TEST_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RELAY_COMMIT_TEST_SOURCES) -o $@

$(BOOTSTRAP_BIN): $(BOOTSTRAP_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(BOOTSTRAP_SOURCES) -o $@

$(MINER_BIN): $(MINER_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(PTHREAD_FLAGS) $(MINER_SOURCES) -o $@ $(PTHREAD_FLAGS)

$(CLIENT_BIN): $(CLIENT_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CLIENT_SOURCES) -o $@

$(NODE_BIN): $(NODE_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(PTHREAD_FLAGS) $(NODE_SOURCES) -o $@ $(PTHREAD_FLAGS)

test: test-c test-shell

test-c: build
	./$(CRYPTO_TEST)
	./$(BLOCK_TEST)
	./$(MERKLE_TEST)
	./$(VALIDATION_TEST)
	./$(BLOCK_HASH_TEST)
	./$(BLOCK_VALIDATION_TEST)
	./$(IPC_TEST)
	./$(BLOCK_LINK_TEST)
	./$(BLOCKCHAIN_TEST)
	./$(MEMPOOL_TEST)

test-shell: build
	bash tests/run_all_tests.sh

run: build
	./$(BOOTSTRAP_BIN) $(ARGS)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
	@if [ -f scripts/cleanup.sh ]; then \
		bash scripts/cleanup.sh; \
	fi
