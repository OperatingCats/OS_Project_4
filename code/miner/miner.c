/* Miner executable: communication thread and mining thread. */

#define _POSIX_C_SOURCE 200809L

#include "block.h"
#include "crypto.h"
#include "errors.h"
#include "ipc.h"
#include "miner.h"
#include "protocol.h"
#include "validation.h"

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

/* Lets the signal handler reach the one miner_state_t this process has. */
static miner_state_t *g_state = NULL;

static void handle_termination_signal(int signum)
{
    (void)signum;

    if (g_state != NULL) {
        g_state->shutting_down = 1;
    }
}

static int open_log_file(miner_state_t *state)
{
    char log_path[600];
    int written;

    written = snprintf(
        log_path,
        sizeof(log_path),
        "%s/%s/" LOG_FILE_FORMAT,
        state->runtime_dir,
        LOG_SUBDIR_NAME,
        "miner",
        (int)getpid()
    );

    if (written < 0 || (size_t)written >= sizeof(log_path)) {
        return ERR_INVALID_ARGUMENT;
    }

    state->log_file = fopen(log_path, "w");

    return (state->log_file != NULL) ? PROJECT_OK : ERR_FILE_OPEN;
}

static void miner_log(const miner_state_t *state, const char *format, ...)
{
    va_list args;
    struct timespec now;
    struct tm local_time;
    char timestamp[32];

    if (state->log_file == NULL) {
        return;
    }

    clock_gettime(CLOCK_REALTIME, &now);

    if (localtime_r(&now.tv_sec, &local_time) != NULL) {
        strftime(
            timestamp,
            sizeof(timestamp),
            "%Y-%m-%d %H:%M:%S",
            &local_time
        );
    } else {
        snprintf(timestamp, sizeof(timestamp), "unknown-time");
    }

    fprintf(
        state->log_file,
        "[%s.%03ld] miner %d: ",
        timestamp,
        now.tv_nsec / 1000000,
        state->miner_id
    );

    va_start(args, format);
    vfprintf(state->log_file, format, args);
    va_end(args);

    fprintf(state->log_file, "\n");
    fflush(state->log_file);
}

static int query_node(
    const char *runtime_dir,
    message_type_t type,
    const char *payload,
    message_header_t *out_header,
    void **out_response
)
{
    char sock_path[MAX_SOCKET_PATH_LEN];
    int fd;
    int result;
    uint32_t payload_len;

    if (ipc_build_socket_path(
            runtime_dir,
            NODE_SOCK_FORMAT,
            0,
            sock_path,
            sizeof(sock_path)
        ) != PROJECT_OK) {
        return ERR_INVALID_ARGUMENT;
    }

    result = ipc_connect(sock_path, &fd);

    if (result != PROJECT_OK) {
        return result;
    }

    payload_len = (payload != NULL) ? (uint32_t)strlen(payload) : 0;
    result = ipc_send(fd, type, 0, payload, payload_len);

    if (result != PROJECT_OK) {
        ipc_close(fd);
        return result;
    }

    result = ipc_recv(fd, out_header, out_response);
    ipc_close(fd);

    return result;
}

/* Learns the real chain tip from Node 0, so a late-joining miner doesn't
 * assume index 0 forever. Best effort: on failure, leaves state alone. */
static void resync_from_node(miner_state_t *state)
{
    message_header_t header;
    void *response = NULL;
    char height_text[32];
    size_t copy_len;
    long height;
    char *end;

    if (query_node(
            state->runtime_dir,
            MSG_QUERY_HEIGHT,
            "",
            &header,
            &response
        ) != PROJECT_OK) {
        free(response);
        return;
    }

    if (header.payload_len == 0) {
        free(response);
        return;
    }

    copy_len = (header.payload_len < sizeof(height_text) - 1)
        ? header.payload_len
        : sizeof(height_text) - 1;
    memcpy(height_text, response, copy_len);
    height_text[copy_len] = '\0';
    free(response);

    errno = 0;
    height = strtol(height_text, &end, 10);

    if (end == height_text || errno != 0 || height < 0) {
        return;
    }

    if (height == 0) {
        pthread_mutex_lock(&state->coordination_lock);
        state->next_index = 0;
        memset(state->previous_hash, '0', SHA256_HEX_LENGTH);
        state->previous_hash[SHA256_HEX_LENGTH] = '\0';
        state->restart_mining = 1;
        pthread_cond_broadcast(&state->coordination_cond);
        pthread_mutex_unlock(&state->coordination_lock);

        miner_log(state, "resynced with node 0: chain is empty, mining index 0");
        return;
    }

    {
        char query_args[32];
        char *line;
        block_t last_block;
        char last_hash[SHA256_HEX_STRING_SIZE];

        block_init(&last_block);

        snprintf(query_args, sizeof(query_args), "--index %ld", height - 1);

        response = NULL;

        if (query_node(
                state->runtime_dir,
                MSG_QUERY_BLOCK,
                query_args,
                &header,
                &response
            ) != PROJECT_OK) {
            free(response);
            return;
        }

        if (header.payload_len == 0) {
            free(response);
            return;
        }

        line = malloc(header.payload_len + 1);

        if (line == NULL) {
            free(response);
            return;
        }

        memcpy(line, response, header.payload_len);
        line[header.payload_len] = '\0';
        free(response);

        if (block_from_csv(line, &last_block) != PROJECT_OK) {
            free(line);
            return;
        }

        free(line);

        if (calculate_block_hash(&last_block, last_hash) != PROJECT_OK) {
            block_destroy(&last_block);
            return;
        }

        pthread_mutex_lock(&state->coordination_lock);
        state->next_index = last_block.index + 1;
        memcpy(state->previous_hash, last_hash, sizeof(state->previous_hash));
        state->restart_mining = 1;
        pthread_cond_broadcast(&state->coordination_cond);
        pthread_mutex_unlock(&state->coordination_lock);

        miner_log(
            state,
            "resynced with node 0: next_index=%llu",
            (unsigned long long)(last_block.index + 1)
        );

        block_destroy(&last_block);
    }
}

static void handle_tx_submit(
    miner_state_t *state,
    const void *payload,
    uint32_t payload_len,
    int client_fd
)
{
    char transaction[MAX_TRANSACTION_LENGTH];
    size_t copy_len;
    int result;
    char reply[128];

    copy_len = (payload_len < sizeof(transaction) - 1)
        ? payload_len
        : sizeof(transaction) - 1;

    if (payload != NULL && copy_len > 0) {
        memcpy(transaction, payload, copy_len);
    }

    transaction[copy_len] = '\0';

    result = validate_transaction(transaction);

    if (result != PROJECT_OK) {
        miner_log(state, "rejected transaction (invalid format): %s", transaction);
        snprintf(
            reply,
            sizeof(reply),
            "ERR %d %s",
            result,
            project_error_string(result)
        );
        ipc_send(
            client_fd,
            MSG_TX_ACK,
            (uint32_t)state->miner_id,
            reply,
            (uint32_t)strlen(reply)
        );
        return;
    }

    result = mempool_add(&state->mempool, transaction);

    if (result != PROJECT_OK) {
        miner_log(state, "rejected transaction (mempool full): %s", transaction);
        snprintf(
            reply,
            sizeof(reply),
            "ERR %d %s",
            result,
            project_error_string(result)
        );
        ipc_send(
            client_fd,
            MSG_TX_ACK,
            (uint32_t)state->miner_id,
            reply,
            (uint32_t)strlen(reply)
        );
        return;
    }

    miner_log(state, "accepted transaction: %s", transaction);
    ipc_send(client_fd, MSG_TX_ACK, (uint32_t)state->miner_id, "OK", 2);

    pthread_mutex_lock(&state->coordination_lock);
    pthread_cond_broadcast(&state->coordination_cond);
    pthread_mutex_unlock(&state->coordination_lock);
}

static void handle_block_commit(
    miner_state_t *state,
    const void *payload,
    uint32_t payload_len
)
{
    char *line;
    block_t block;
    char hash[SHA256_HEX_STRING_SIZE];

    if (payload == NULL || payload_len == 0) {
        return;
    }

    line = malloc(payload_len + 1);

    if (line == NULL) {
        return;
    }

    memcpy(line, payload, payload_len);
    line[payload_len] = '\0';

    block_init(&block);

    if (block_from_csv(line, &block) != PROJECT_OK) {
        free(line);
        return;
    }

    free(line);

    if (calculate_block_hash(&block, hash) != PROJECT_OK) {
        block_destroy(&block);
        return;
    }

    mempool_remove_included(
        &state->mempool,
        (char *const *)block.transactions,
        block.transaction_count
    );

    pthread_mutex_lock(&state->coordination_lock);

    if (block.index >= state->next_index) {
        state->next_index = block.index + 1;
        memcpy(state->previous_hash, hash, sizeof(state->previous_hash));
        state->restart_mining = 1;
        pthread_cond_broadcast(&state->coordination_cond);
        pthread_mutex_unlock(&state->coordination_lock);

        miner_log(
            state,
            "block committed: index=%llu, chain advanced",
            (unsigned long long)block.index
        );
    } else {
        pthread_mutex_unlock(&state->coordination_lock);

        miner_log(
            state,
            "ignored old/duplicate block commit: index=%llu",
            (unsigned long long)block.index
        );
    }

    block_destroy(&block);
}

static void handle_message(
    miner_state_t *state,
    const message_header_t *header,
    void *payload,
    int client_fd
)
{
    switch ((message_type_t)header->type) {
        case MSG_TX_SUBMIT:
            handle_tx_submit(state, payload, header->payload_len, client_fd);
            break;

        case MSG_BLOCK_COMMIT:
            handle_block_commit(state, payload, header->payload_len);
            break;

        case MSG_BLOCK_REJECT:
            /* Our worldview might be stale relative to Node 0's; find
             * out where it actually is. */
            miner_log(state, "proposal rejected by node 0; resyncing");
            resync_from_node(state);
            break;

        case MSG_SHUTDOWN:
            miner_log(state, "shutdown requested by bootstrap");
            state->shutting_down = 1;
            break;

        default:
            break;
    }
}

static void *communication_thread_main(void *arg)
{
    miner_state_t *state = (miner_state_t *)arg;
    char sock_path[MAX_SOCKET_PATH_LEN];
    int server_fd;

    if (ipc_build_socket_path(
            state->runtime_dir,
            MINER_SOCK_FORMAT,
            state->miner_id,
            sock_path,
            sizeof(sock_path)
        ) != PROJECT_OK) {
        fprintf(
            stderr,
            "miner %d: failed to build socket path\n",
            state->miner_id
        );
        return NULL;
    }

    ipc_unlink_socket(sock_path);

    if (ipc_server_create(sock_path, &server_fd) != PROJECT_OK) {
        fprintf(
            stderr,
            "miner %d: failed to create socket at %s\n",
            state->miner_id,
            sock_path
        );
        return NULL;
    }

    for (;;) {
        fd_set read_fds;
        struct timeval timeout;
        int ready;

        if (state->shutting_down) {
            break;
        }

        /* ipc_server_accept() retries internally on EINTR, so a signal
         * can't be relied on to unblock it -- poll with a short timeout
         * instead so the shutdown flag gets rechecked regularly. */
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        ready = select(server_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (ready <= 0) {
            continue;
        }

        {
            int client_fd;
            message_header_t header;
            void *payload = NULL;

            if (ipc_server_accept(server_fd, &client_fd) != PROJECT_OK) {
                continue;
            }

            if (ipc_recv(client_fd, &header, &payload) != PROJECT_OK) {
                ipc_close(client_fd);
                continue;
            }

            handle_message(state, &header, payload, client_fd);

            free(payload);
            ipc_close(client_fd);
        }
    }

    ipc_close(server_fd);
    ipc_unlink_socket(sock_path);

    return NULL;
}

/* Interruptible sleep for the simulated mining attempt: waits up to
 * `seconds`, but returns early (aborted = true) the moment a fresher
 * block commits or shutdown is requested. */
static int wait_or_abort(miner_state_t *state, int seconds)
{
    struct timespec deadline;
    int aborted;

    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += seconds;

    pthread_mutex_lock(&state->coordination_lock);

    while (!state->restart_mining && !state->shutting_down) {
        int wait_result = pthread_cond_timedwait(
            &state->coordination_cond,
            &state->coordination_lock,
            &deadline
        );

        if (wait_result == ETIMEDOUT) {
            break;
        }
    }

    aborted = (state->restart_mining || state->shutting_down);

    pthread_mutex_unlock(&state->coordination_lock);

    return aborted;
}

static void propose_block(miner_state_t *state, const block_t *block)
{
    char *line = NULL;
    char sock_path[MAX_SOCKET_PATH_LEN];
    int fd;
    message_header_t header;
    void *response = NULL;

    if (block_to_csv(block, &line) != PROJECT_OK) {
        return;
    }

    if (ipc_build_socket_path(
            state->runtime_dir,
            NODE_SOCK_FORMAT,
            0,
            sock_path,
            sizeof(sock_path)
        ) != PROJECT_OK) {
        free(line);
        return;
    }

    if (ipc_connect(sock_path, &fd) != PROJECT_OK) {
        miner_log(
            state,
            "block mined (index=%llu) but node 0 is unreachable",
            (unsigned long long)block->index
        );
        free(line);
        return;
    }

    miner_log(
        state,
        "block mined: index=%llu, proposing to node 0",
        (unsigned long long)block->index
    );

    ipc_send(
        fd,
        MSG_BLOCK_PROPOSAL,
        (uint32_t)state->miner_id,
        line,
        (uint32_t)strlen(line)
    );

    if (ipc_recv(fd, &header, &response) == PROJECT_OK) {
        free(response);
    }

    ipc_close(fd);
    free(line);
}

static void *mining_thread_main(void *arg)
{
    miner_state_t *state = (miner_state_t *)arg;

    resync_from_node(state);

    for (;;) {
        uint64_t candidate_index;
        char previous_hash[SHA256_HEX_STRING_SIZE];
        char batch[MAX_TRANSACTIONS_PER_BLOCK][MAX_TRANSACTION_LENGTH];
        size_t batch_count;
        block_t candidate;
        int build_failed = 0;
        int sleep_seconds;
        int aborted;
        int mined;

        pthread_mutex_lock(&state->coordination_lock);

        if (state->shutting_down) {
            pthread_mutex_unlock(&state->coordination_lock);
            break;
        }

        state->restart_mining = 0;
        candidate_index = state->next_index;
        memcpy(
            previous_hash,
            state->previous_hash,
            sizeof(previous_hash)
        );

        pthread_mutex_unlock(&state->coordination_lock);

        batch_count = mempool_snapshot(
            &state->mempool,
            batch,
            MAX_TRANSACTIONS_PER_BLOCK
        );

        if (batch_count == 0) {
            struct timespec deadline;

            clock_gettime(CLOCK_REALTIME, &deadline);
            deadline.tv_sec += 1;

            pthread_mutex_lock(&state->coordination_lock);

            if (!state->shutting_down && !state->restart_mining) {
                pthread_cond_timedwait(
                    &state->coordination_cond,
                    &state->coordination_lock,
                    &deadline
                );
            }

            pthread_mutex_unlock(&state->coordination_lock);
            continue;
        }

        if (block_init(&candidate) != PROJECT_OK) {
            continue;
        }

        miner_log(
            state,
            "mining attempt: index=%llu, %zu transaction(s)",
            (unsigned long long)candidate_index,
            batch_count
        );

        candidate.index = candidate_index;
        candidate.timestamp = (uint64_t)time(NULL);
        candidate.nonce = (uint64_t)rand();
        memcpy(
            candidate.previous_hash,
            previous_hash,
            sizeof(candidate.previous_hash)
        );

        for (size_t index = 0; index < batch_count; index++) {
            if (block_add_transaction(&candidate, batch[index]) != PROJECT_OK) {
                build_failed = 1;
                break;
            }
        }

        if (build_failed) {
            block_destroy(&candidate);
            continue;
        }

        if (calculate_merkle_root(
                (const char *const *)candidate.transactions,
                candidate.transaction_count,
                candidate.merkle_root
            ) != PROJECT_OK) {
            block_destroy(&candidate);
            continue;
        }

        /* Simulated proof-of-work: sleep 1-5s (interruptible by a
         * fresher commit or shutdown), then roll the dice. */
        sleep_seconds = 1 + (rand() % 5);
        aborted = wait_or_abort(state, sleep_seconds);

        if (aborted) {
            miner_log(state, "mining attempt aborted (chain advanced)");
            block_destroy(&candidate);
            continue;
        }

        mined = (state->difficulty > 0) &&
                ((rand() % state->difficulty) == 0);

        if (mined) {
            propose_block(state, &candidate);
        }

        block_destroy(&candidate);
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    miner_state_t state;
    pthread_t communication_thread;
    pthread_t mining_thread;
    struct sigaction action;

    if (argc != 4) {
        fprintf(
            stderr,
            "Usage: %s <miner_id> <runtime_dir> <difficulty>\n",
            argv[0]
        );
        return EXIT_FAILURE;
    }

    memset(&state, 0, sizeof(state));
    state.miner_id = atoi(argv[1]);
    snprintf(state.runtime_dir, sizeof(state.runtime_dir), "%s", argv[2]);
    state.difficulty = atoi(argv[3]);

    if (state.difficulty <= 0) {
        state.difficulty = 1;
    }

    memset(state.previous_hash, '0', SHA256_HEX_LENGTH);
    state.previous_hash[SHA256_HEX_LENGTH] = '\0';
    state.next_index = 0;

    if (open_log_file(&state) != PROJECT_OK) {
        fprintf(
            stderr,
            "miner %d: could not open log file, continuing without one\n",
            state.miner_id
        );
    }

    miner_log(&state, "started (difficulty=%d)", state.difficulty);

    if (mempool_init(&state.mempool) != PROJECT_OK) {
        fprintf(
            stderr,
            "miner %d: failed to initialize mempool\n",
            state.miner_id
        );
        return EXIT_FAILURE;
    }

    pthread_mutex_init(&state.coordination_lock, NULL);
    pthread_cond_init(&state.coordination_cond, NULL);

    srand((unsigned int)time(NULL) ^ (unsigned int)getpid());

    g_state = &state;

    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_termination_signal;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    if (pthread_create(
            &communication_thread,
            NULL,
            communication_thread_main,
            &state
        ) != 0) {
        fprintf(
            stderr,
            "miner %d: failed to start communication thread\n",
            state.miner_id
        );
        mempool_destroy(&state.mempool);
        return EXIT_FAILURE;
    }

    if (pthread_create(
            &mining_thread,
            NULL,
            mining_thread_main,
            &state
        ) != 0) {
        fprintf(
            stderr,
            "miner %d: failed to start mining thread\n",
            state.miner_id
        );
        state.shutting_down = 1;
        pthread_join(communication_thread, NULL);
        mempool_destroy(&state.mempool);
        return EXIT_FAILURE;
    }

    pthread_join(communication_thread, NULL);

    pthread_mutex_lock(&state.coordination_lock);
    state.shutting_down = 1;
    pthread_cond_broadcast(&state.coordination_cond);
    pthread_mutex_unlock(&state.coordination_lock);

    pthread_join(mining_thread, NULL);

    miner_log(&state, "shut down");

    mempool_destroy(&state.mempool);
    pthread_mutex_destroy(&state.coordination_lock);
    pthread_cond_destroy(&state.coordination_cond);

    if (state.log_file != NULL) {
        fclose(state.log_file);
    }

    return EXIT_SUCCESS;
}
