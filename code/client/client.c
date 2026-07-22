/*
 * Client process.
 *
 * Usage:
 *   bin/client <client_id> <runtime_dir> <transaction_frequency>
 *
 * The client periodically generates valid random transactions and sends
 * them to one of the miners discovered in the runtime directory.
 */

#define _POSIX_C_SOURCE 200809L

#include "block.h"
#include "errors.h"
#include "ipc.h"
#include "protocol.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define CLIENT_NAME_COUNT 12
#define LOG_PATH_SIZE 512
#define MAX_DISCOVERED_MINERS 256
#define RETRY_DELAY_SECONDS 1

static volatile sig_atomic_t g_shutdown_requested = 0;

static const char *const CLIENT_NAMES[CLIENT_NAME_COUNT] = {
    "Alice",
    "Bob",
    "Charlie",
    "Dave",
    "Eve",
    "Frank",
    "Grace",
    "Heidi",
    "Ivan",
    "Judy",
    "Mallory",
    "Oscar"
};

typedef struct {
    int client_id;
    int transaction_frequency;
    char runtime_dir[256];
    FILE *log_file;
    size_t next_miner;
} client_state_t;

static void handle_termination_signal(int signum)
{
    (void)signum;
    g_shutdown_requested = 1;
}

static int parse_non_negative_int(const char *text, int *out)
{
    char *end;
    long value;

    if (text == NULL || out == NULL || text[0] == '\0') {
        return ERR_INVALID_ARGUMENT;
    }

    errno = 0;
    value = strtol(text, &end, 10);

    if (errno != 0 ||
        end == text ||
        *end != '\0' ||
        value < 0 ||
        value > INT_MAX) {
        return ERR_INVALID_ARGUMENT;
    }

    *out = (int)value;

    return PROJECT_OK;
}

static void client_log(client_state_t *state, const char *format, ...)
{
    va_list args;
    struct timespec now;
    struct tm local_time;
    char date_part[32];
    char timestamp[40];

    if (state == NULL || format == NULL) {
        return;
    }

    if (clock_gettime(CLOCK_REALTIME, &now) == 0 &&
        localtime_r(&now.tv_sec, &local_time) != NULL) {
        unsigned int milliseconds =
            (unsigned int)(now.tv_nsec / 1000000L);

        strftime(
            date_part,
            sizeof(date_part),
            "%Y-%m-%d %H:%M:%S",
            &local_time
        );

        snprintf(
            timestamp,
            sizeof(timestamp),
            "%s.%03u",
            date_part,
            milliseconds
        );
    } else {
        snprintf(timestamp, sizeof(timestamp), "unknown-time");
    }

    va_start(args, format);

    if (state->log_file != NULL) {
        fprintf(
            state->log_file,
            "[%s] client %d: ",
            timestamp,
            state->client_id
        );

        vfprintf(state->log_file, format, args);
        fputc('\n', state->log_file);
        fflush(state->log_file);
    }

    va_end(args);
}

static int open_log_file(client_state_t *state)
{
    char path[LOG_PATH_SIZE];
    int written;

    if (state == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    written = snprintf(
        path,
        sizeof(path),
        "%s/%s/client-%d.log",
        state->runtime_dir,
        LOG_SUBDIR_NAME,
        (int)getpid()
    );

    if (written < 0 || (size_t)written >= sizeof(path)) {
        return ERR_INVALID_ARGUMENT;
    }

    state->log_file = fopen(path, "a");

    if (state->log_file == NULL) {
        return ERR_FILE_OPEN;
    }

    return PROJECT_OK;
}

static int generate_transaction(char *out, size_t out_size)
{
    size_t sender_index;
    size_t receiver_index;
    unsigned int amount;
    int written;

    if (out == NULL || out_size == 0) {
        return ERR_INVALID_ARGUMENT;
    }

    sender_index = (size_t)(rand() % CLIENT_NAME_COUNT);

    do {
        receiver_index = (size_t)(rand() % CLIENT_NAME_COUNT);
    } while (receiver_index == sender_index);

    amount = 1u + (unsigned int)(rand() % 1000);

    written = snprintf(
        out,
        out_size,
        "%s pays %s %u coins",
        CLIENT_NAMES[sender_index],
        CLIENT_NAMES[receiver_index],
        amount
    );

    if (written < 0 || (size_t)written >= out_size) {
        return ERR_INVALID_ARGUMENT;
    }

    return PROJECT_OK;
}

static int compare_ints(const void *left, const void *right)
{
    const int first = *(const int *)left;
    const int second = *(const int *)right;

    if (first < second) {
        return -1;
    }

    if (first > second) {
        return 1;
    }

    return 0;
}

/*
 * Finds filenames having exactly the form miner_<id>.sock.
 *
 * Socket IDs are sorted so round-robin selection remains deterministic
 * even though readdir() does not guarantee directory ordering.
 */
static int discover_miners(
    const char *runtime_dir,
    int miner_ids[MAX_DISCOVERED_MINERS],
    size_t *out_count
)
{
    DIR *directory;
    struct dirent *entry;
    size_t count = 0;

    if (runtime_dir == NULL || miner_ids == NULL || out_count == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    directory = opendir(runtime_dir);

    if (directory == NULL) {
        return ERR_FILE_OPEN;
    }

    while ((entry = readdir(directory)) != NULL) {
        int miner_id;
        int consumed = 0;

        if (sscanf(
                entry->d_name,
                "miner_%d.sock%n",
                &miner_id,
                &consumed
            ) == 1 &&
            entry->d_name[consumed] == '\0' &&
            miner_id >= 0) {
            if (count < MAX_DISCOVERED_MINERS) {
                miner_ids[count++] = miner_id;
            }
        }
    }

    if (closedir(directory) != 0) {
        return ERR_FILE_OPEN;
    }

    qsort(miner_ids, count, sizeof(miner_ids[0]), compare_ints);

    *out_count = count;

    return PROJECT_OK;
}

static int send_transaction_to_miner(
    client_state_t *state,
    int miner_id,
    const char *transaction
)
{
    char socket_path[MAX_SOCKET_PATH_LEN];
    int fd;
    int result;
    message_header_t response_header;
    void *response_payload = NULL;

    result = ipc_build_socket_path(
        state->runtime_dir,
        MINER_SOCK_FORMAT,
        miner_id,
        socket_path,
        sizeof(socket_path)
    );

    if (result != PROJECT_OK) {
        return result;
    }

    result = ipc_connect(socket_path, &fd);

    if (result != PROJECT_OK) {
        return result;
    }

    result = ipc_send(
        fd,
        MSG_TX_SUBMIT,
        (uint32_t)state->client_id,
        transaction,
        (uint32_t)strlen(transaction)
    );

    if (result != PROJECT_OK) {
        ipc_close(fd);
        return result;
    }

    result = ipc_recv(
        fd,
        &response_header,
        &response_payload
    );

    ipc_close(fd);

    if (result != PROJECT_OK) {
        free(response_payload);
        return result;
    }

    if ((message_type_t)response_header.type != MSG_TX_ACK) {
        client_log(
            state,
            "miner %d returned unexpected message type %u",
            miner_id,
            (unsigned int)response_header.type
        );

        free(response_payload);
        return ERR_SERIALIZATION;
    }

    if (response_header.payload_len == 2 &&
        response_payload != NULL &&
        memcmp(response_payload, "OK", 2) == 0) {
        client_log(
            state,
            "transaction accepted by miner %d: %s",
            miner_id,
            transaction
        );

        free(response_payload);
        return PROJECT_OK;
    }

    if (response_header.payload_len > 0 && response_payload != NULL) {
        client_log(
            state,
            "transaction rejected by miner %d: %.*s",
            miner_id,
            (int)response_header.payload_len,
            (const char *)response_payload
        );
    } else {
        client_log(
            state,
            "transaction rejected by miner %d without an explanation",
            miner_id
        );
    }

    free(response_payload);

    return ERR_INVALID_TRANSACTION;
}

/*
 * Tries every currently discovered miner, starting from the round-robin
 * position. This allows the remaining clients to keep working if a miner
 * crashes or its socket disappears.
 */
static int submit_transaction(
    client_state_t *state,
    const char *transaction
)
{
    int miner_ids[MAX_DISCOVERED_MINERS];
    size_t miner_count;
    size_t start;
    int result;

    result = discover_miners(
        state->runtime_dir,
        miner_ids,
        &miner_count
    );

    if (result != PROJECT_OK) {
        return result;
    }

    if (miner_count == 0) {
        return ERR_IPC;
    }

    start = state->next_miner % miner_count;

    for (size_t attempt = 0; attempt < miner_count; attempt++) {
        size_t position = (start + attempt) % miner_count;
        int miner_id = miner_ids[position];

        result = send_transaction_to_miner(
            state,
            miner_id,
            transaction
        );

        if (result == PROJECT_OK ||
            result == ERR_INVALID_TRANSACTION) {
            state->next_miner = position + 1;
            return result;
        }

        client_log(
            state,
            "miner %d unavailable: %s",
            miner_id,
            project_error_string(result)
        );
    }

    state->next_miner = start + 1;

    return ERR_IPC;
}

static void interruptible_sleep(unsigned int seconds)
{
    struct timespec remaining;

    remaining.tv_sec = (time_t)seconds;
    remaining.tv_nsec = 0;

    while (!g_shutdown_requested &&
           nanosleep(&remaining, &remaining) != 0) {
        if (errno != EINTR) {
            break;
        }
    }
}

int main(int argc, char *argv[])
{
    client_state_t state;
    struct sigaction action;

    if (argc != 4) {
        fprintf(
            stderr,
            "Usage: %s <client_id> <runtime_dir> "
            "<transaction_frequency>\n",
            argv[0]
        );

        return EXIT_FAILURE;
    }

    memset(&state, 0, sizeof(state));

    if (parse_non_negative_int(argv[1], &state.client_id) != PROJECT_OK ||
        parse_non_negative_int(
            argv[3],
            &state.transaction_frequency
        ) != PROJECT_OK) {
        fprintf(stderr, "client: invalid numeric argument\n");
        return EXIT_FAILURE;
    }

    if (snprintf(
            state.runtime_dir,
            sizeof(state.runtime_dir),
            "%s",
            argv[2]
        ) >= (int)sizeof(state.runtime_dir)) {
        fprintf(stderr, "client: runtime directory path is too long\n");
        return EXIT_FAILURE;
    }

    if (open_log_file(&state) != PROJECT_OK) {
        fprintf(
            stderr,
            "client %d: could not open log file\n",
            state.client_id
        );

        return EXIT_FAILURE;
    }

    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_termination_signal;

    sigemptyset(&action.sa_mask);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    srand(
        (unsigned int)time(NULL) ^
        (unsigned int)getpid() ^
        (unsigned int)state.client_id
    );

    client_log(
        &state,
        "started with transaction frequency %d",
        state.transaction_frequency
    );

    /*
     * Frequency 0 disables automatic generation. The process stays alive
     * so pause/resume/stop and failure handling still work correctly.
     */
    while (!g_shutdown_requested) {
        char transaction[MAX_TRANSACTION_LENGTH];
        int result;

        if (state.transaction_frequency == 0) {
            interruptible_sleep(RETRY_DELAY_SECONDS);
            continue;
        }

        result = generate_transaction(
            transaction,
            sizeof(transaction)
        );

        if (result != PROJECT_OK) {
            client_log(
                &state,
                "could not generate transaction: %s",
                project_error_string(result)
            );

            interruptible_sleep(RETRY_DELAY_SECONDS);
            continue;
        }

        client_log(
            &state,
            "generated transaction: %s",
            transaction
        );

        result = submit_transaction(&state, transaction);

        if (result != PROJECT_OK) {
            client_log(
                &state,
                "transaction submission failed: %s",
                project_error_string(result)
            );
        }

        interruptible_sleep(
            (unsigned int)state.transaction_frequency
        );
    }

    client_log(&state, "shutting down");

    fclose(state.log_file);

    return EXIT_SUCCESS;
}