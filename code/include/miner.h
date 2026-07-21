#ifndef MINER_H
#define MINER_H

#include <pthread.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>

#include "block.h"

/* Miner-specific structures: state, mining configuration,
 * restart/shutdown flags, current candidate block. */

#define MEMPOOL_CAPACITY 256

typedef struct {
    char transactions[MEMPOOL_CAPACITY][MAX_TRANSACTION_LENGTH];
    size_t count;
    pthread_mutex_t lock;
} mempool_t;

int mempool_init(mempool_t *pool);
void mempool_destroy(mempool_t *pool);

/* Adds a copy of `transaction` to the pool. Returns ERR_INVALID_BLOCK if
 * the pool is already at MEMPOOL_CAPACITY. */
int mempool_add(mempool_t *pool, const char *transaction);

/* Copies up to `max_count` pending transactions into `out` without
 * removing them, and returns how many were copied. */
size_t mempool_snapshot(
    mempool_t *pool,
    char out[][MAX_TRANSACTION_LENGTH],
    size_t max_count
);

/* Removes every transaction in `included` from the pool -- called once
 * a block containing them has committed. */
void mempool_remove_included(
    mempool_t *pool,
    char *const included[],
    size_t included_count
);

size_t mempool_size(mempool_t *pool);

typedef struct {
    int miner_id;
    char runtime_dir[256];
    int difficulty;

    mempool_t mempool;

    /* This miner's view of the chain tip, updated from BLOCK_COMMIT
     * broadcasts and BLOCK_REJECT-triggered resyncs with Node 0. */
    uint64_t next_index;
    char previous_hash[SHA256_HEX_STRING_SIZE];

    pthread_mutex_t coordination_lock;
    pthread_cond_t coordination_cond;

    /* Signal-writable, so plain int is not enough. */
    volatile sig_atomic_t restart_mining;
    volatile sig_atomic_t shutting_down;
} miner_state_t;

#endif /* MINER_H */
