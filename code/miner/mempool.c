/* Management of the pending transaction pool (mutex-protected). */

#include "miner.h"
#include "errors.h"

#include <string.h>

int mempool_init(mempool_t *pool)
{
    if (pool == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    pool->count = 0;

    if (pthread_mutex_init(&pool->lock, NULL) != 0) {
        return ERR_PROCESS;
    }

    return PROJECT_OK;
}

void mempool_destroy(mempool_t *pool)
{
    if (pool == NULL) {
        return;
    }

    pthread_mutex_destroy(&pool->lock);
    pool->count = 0;
}

int mempool_add(mempool_t *pool, const char *transaction)
{
    size_t length;
    int result = PROJECT_OK;

    if (pool == NULL || transaction == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    length = strlen(transaction);

    if (length == 0 || length >= MAX_TRANSACTION_LENGTH) {
        return ERR_INVALID_TRANSACTION;
    }

    pthread_mutex_lock(&pool->lock);

    if (pool->count >= MEMPOOL_CAPACITY) {
        result = ERR_INVALID_BLOCK;
    } else {
        strcpy(pool->transactions[pool->count], transaction);
        pool->count++;
    }

    pthread_mutex_unlock(&pool->lock);

    return result;
}

size_t mempool_snapshot(
    mempool_t *pool,
    char out[][MAX_TRANSACTION_LENGTH],
    size_t max_count
)
{
    size_t copied;

    if (pool == NULL || out == NULL) {
        return 0;
    }

    pthread_mutex_lock(&pool->lock);

    copied = (pool->count < max_count) ? pool->count : max_count;

    for (size_t index = 0; index < copied; index++) {
        strcpy(out[index], pool->transactions[index]);
    }

    pthread_mutex_unlock(&pool->lock);

    return copied;
}

void mempool_remove_included(
    mempool_t *pool,
    char *const included[],
    size_t included_count
)
{
    if (pool == NULL || included == NULL) {
        return;
    }

    pthread_mutex_lock(&pool->lock);

    for (size_t index = 0; index < pool->count; ) {
        int matched = 0;

        for (size_t included_index = 0;
             included_index < included_count;
             included_index++) {
            if (strcmp(
                    pool->transactions[index],
                    included[included_index]
                ) == 0) {
                matched = 1;
                break;
            }
        }

        if (matched) {
            pool->count--;

            if (index != pool->count) {
                strcpy(
                    pool->transactions[index],
                    pool->transactions[pool->count]
                );
            }
            /* Re-check this slot: it now holds the element that used to
             * be last. */
        } else {
            index++;
        }
    }

    pthread_mutex_unlock(&pool->lock);
}

size_t mempool_size(mempool_t *pool)
{
    size_t size;

    if (pool == NULL) {
        return 0;
    }

    pthread_mutex_lock(&pool->lock);
    size = pool->count;
    pthread_mutex_unlock(&pool->lock);

    return size;
}
