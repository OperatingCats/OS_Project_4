CC := gcc

CPPFLAGS := -Iinclude -Ithird_party/sha256
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror

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
	third_party/sha256/sha256.c \
	node/node.c

.PHONY: build test clean run

build: $(CRYPTO_TEST) \
	$(BLOCK_TEST) \
	$(MERKLE_TEST) \
	$(VALIDATION_TEST) \
	$(BLOCK_HASH_TEST) \
	$(BLOCK_VALIDATION_TEST) \
	$(IPC_TEST) \
	$(BLOCK_LINK_TEST) \
	$(BLOCKCHAIN_TEST) \
	$(BOOTSTRAP_BIN) \
    $(MINER_BIN) \
    $(CLIENT_BIN) \
	$(NODE_BIN)

$(CRYPTO_TEST): $(CRYPTO_TEST_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CRYPTO_TEST_SOURCES) \
		-o $(CRYPTO_TEST)

$(BLOCK_TEST): $(BLOCK_TEST_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(BLOCK_TEST_SOURCES) \
		-o $(BLOCK_TEST)

$(MERKLE_TEST): $(MERKLE_TEST_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(MERKLE_TEST_SOURCES) \
		-o $(MERKLE_TEST)

$(VALIDATION_TEST): $(VALIDATION_TEST_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(VALIDATION_TEST_SOURCES) \
		-o $(VALIDATION_TEST)

$(BLOCK_HASH_TEST): $(BLOCK_HASH_TEST_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(BLOCK_HASH_TEST_SOURCES) \
		-o $(BLOCK_HASH_TEST)

$(BLOCK_VALIDATION_TEST): $(BLOCK_VALIDATION_TEST_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(BLOCK_VALIDATION_TEST_SOURCES) \
		-o $(BLOCK_VALIDATION_TEST)

$(IPC_TEST): $(IPC_TEST_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(IPC_TEST_SOURCES) \
		-o $(IPC_TEST)
$(BLOCK_LINK_TEST): $(BLOCK_LINK_TEST_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(BLOCK_LINK_TEST_SOURCES) \
		-o $(BLOCK_LINK_TEST)

$(BLOCKCHAIN_TEST): $(BLOCKCHAIN_TEST_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(BLOCKCHAIN_TEST_SOURCES) \
		-o $(BLOCKCHAIN_TEST)

$(BOOTSTRAP_BIN): $(BOOTSTRAP_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(BOOTSTRAP_SOURCES) \
		-o $(BOOTSTRAP_BIN)

$(MINER_BIN): $(MINER_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -pthread $(MINER_SOURCES) \
		-o $(MINER_BIN) -pthread

$(CLIENT_BIN): $(CLIENT_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CLIENT_SOURCES) \
	        -o $(CLIENT_BIN)
	
$(NODE_BIN): $(NODE_SOURCES)
	mkdir -p $(BUILD_DIR) $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -pthread $(NODE_SOURCES) \
		-o $(NODE_BIN) -pthread

test: build
	./$(CRYPTO_TEST)
	./$(BLOCK_TEST)
	./$(MERKLE_TEST)
	./$(VALIDATION_TEST)
	./$(BLOCK_HASH_TEST)
	./$(BLOCK_VALIDATION_TEST)
	./$(IPC_TEST)
	./$(BLOCK_LINK_TEST)
	./$(BLOCKCHAIN_TEST)

run: $(BOOTSTRAP_BIN)
	./$(BOOTSTRAP_BIN) $(ARGS)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

