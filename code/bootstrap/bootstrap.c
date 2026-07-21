/* Main project controller: starting, managing, and shutting down processes. */

/* -std=c11 alone leaves the compiler in strict ISO mode, where glibc hides
 * fork()/execv()/kill()/waitpid()/mkdir() and friends. This has to be
 * defined before any system header is pulled in. */
#define _POSIX_C_SOURCE 200809L

#include "block.h"
#include "errors.h"
#include "ipc.h"
#include "process_info.h"
#include "protocol.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_DIFFICULTY 4
#define DEFAULT_TRANSACTION_FREQUENCY 0
#define SHUTDOWN_TIMEOUT_SECONDS 5
#define INITIAL_STATE_FILENAME "initial_state.csv"
#define RUNTIME_DIR_BUFFER_SIZE 256
#define COMMAND_LINE_BUFFER_SIZE 512

/* Every child gets argv[1] = id, argv[2] = runtime_dir; miner and client
 * append one more argument for their role-specific config. */
static const char *const NODE_BINARY = "bin/node";
static const char *const MINER_BINARY = "bin/miner";
static const char *const CLIENT_BINARY = "bin/client";

static volatile sig_atomic_t g_shutdown_requested = 0;

typedef struct {
    int num_nodes;
    int num_miners;
    int num_clients;
    int transaction_frequency;
    int difficulty;
    const char *initial_state_path; /* NULL if not provided */
} bootstrap_config_t;

static void handle_sigchld(int signum)
{
    (void)signum;
    /* Empty on purpose: without SA_RESTART, its only job is to interrupt
     * the blocking fgets() on stdin so the main loop can reap promptly. */
}

static void handle_termination_signal(int signum)
{
    (void)signum;
    g_shutdown_requested = 1;
}

static int parse_non_negative_int(const char *text, int *out)
{
    char *end;
    long value;

    if (text == NULL || text[0] == '\0') {
        return ERR_INVALID_ARGUMENT;
    }

    errno = 0;
    value = strtol(text, &end, 10);

    if (errno != 0 || *end != '\0' || value < 0 || value > INT_MAX) {
        return ERR_INVALID_ARGUMENT;
    }

    *out = (int)value;

    return PROJECT_OK;
}

static int parse_arguments(
    int argc,
    char *argv[],
    bootstrap_config_t *config
)
{
    if (argc < 4 || argc > 7) {
        return ERR_INVALID_ARGUMENT;
    }

    if (parse_non_negative_int(argv[1], &config->num_nodes) != PROJECT_OK ||
        parse_non_negative_int(argv[2], &config->num_miners) != PROJECT_OK ||
        parse_non_negative_int(argv[3], &config->num_clients) != PROJECT_OK) {
        return ERR_INVALID_ARGUMENT;
    }

    if (config->num_nodes < 1) {
        /* Node 0 is the consensus coordinator; the system is meaningless
         * without it. */
        return ERR_INVALID_ARGUMENT;
    }

    config->transaction_frequency = DEFAULT_TRANSACTION_FREQUENCY;
    config->difficulty = DEFAULT_DIFFICULTY;
    config->initial_state_path = NULL;

    if (argc >= 5 &&
        parse_non_negative_int(
            argv[4],
            &config->transaction_frequency
        ) != PROJECT_OK) {
        return ERR_INVALID_ARGUMENT;
    }

    if (argc >= 6 &&
        parse_non_negative_int(argv[5], &config->difficulty) != PROJECT_OK) {
        return ERR_INVALID_ARGUMENT;
    }

    if (argc >= 7) {
        config->initial_state_path = argv[6];
    }

    return PROJECT_OK;
}

static int create_runtime_directory(char *out_dir, size_t out_size)
{
    int written = snprintf(
        out_dir,
        out_size,
        RUNTIME_DIR_FORMAT,
        (int)getpid()
    );

    if (written < 0 || (size_t)written >= out_size) {
        return ERR_INVALID_ARGUMENT;
    }

    if (mkdir(out_dir, 0700) != 0) {
        return ERR_FILE_WRITE;
    }

    char logs_dir[RUNTIME_DIR_BUFFER_SIZE + sizeof(LOG_SUBDIR_NAME) + 1];
    written = snprintf(
        logs_dir,
        sizeof(logs_dir),
        "%s/%s",
        out_dir,
        LOG_SUBDIR_NAME
    );

    if (written < 0 || (size_t)written >= sizeof(logs_dir)) {
        return ERR_INVALID_ARGUMENT;
    }

    if (mkdir(logs_dir, 0700) != 0) {
        return ERR_FILE_WRITE;
    }

    return PROJECT_OK;
}

static int copy_initial_state(
    const char *source_path,
    const char *runtime_dir
)
{
    char dest_path[RUNTIME_DIR_BUFFER_SIZE + sizeof(INITIAL_STATE_FILENAME) + 1];
    FILE *source;
    FILE *dest;
    char buffer[4096];
    size_t read_count;
    int had_error;
    int written;

    written = snprintf(
        dest_path,
        sizeof(dest_path),
        "%s/%s",
        runtime_dir,
        INITIAL_STATE_FILENAME
    );

    if (written < 0 || (size_t)written >= sizeof(dest_path)) {
        return ERR_INVALID_ARGUMENT;
    }

    source = fopen(source_path, "rb");

    if (source == NULL) {
        return ERR_FILE_OPEN;
    }

    dest = fopen(dest_path, "wb");

    if (dest == NULL) {
        fclose(source);
        return ERR_FILE_OPEN;
    }

    while ((read_count = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        if (fwrite(buffer, 1, read_count, dest) != read_count) {
            fclose(source);
            fclose(dest);
            return ERR_FILE_WRITE;
        }
    }

    had_error = ferror(source);
    fclose(source);

    if (fclose(dest) != 0 || had_error) {
        return ERR_FILE_WRITE;
    }

    return PROJECT_OK;
}

static int recursive_remove(const char *path)
{
    DIR *dir;
    struct dirent *entry;
    int had_error = 0;

    dir = opendir(path);

    if (dir == NULL) {
        return (errno == ENOENT) ? PROJECT_OK : ERR_IPC;
    }

    while ((entry = readdir(dir)) != NULL) {
        char child_path[600];
        struct stat st;
        int written;

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        written = snprintf(
            child_path,
            sizeof(child_path),
            "%s/%s",
            path,
            entry->d_name
        );

        if (written < 0 || (size_t)written >= sizeof(child_path)) {
            had_error = 1;
            continue;
        }

        if (lstat(child_path, &st) != 0) {
            had_error = 1;
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            if (recursive_remove(child_path) != PROJECT_OK) {
                had_error = 1;
            }
        } else if (unlink(child_path) != 0) {
            had_error = 1;
        }
    }

    closedir(dir);

    if (rmdir(path) != 0) {
        had_error = 1;
    }

    return had_error ? ERR_IPC : PROJECT_OK;
}

static int spawn_process(
    const char *binary_path,
    char *const argv_child[],
    pid_t *out_pid
)
{
    pid_t pid = fork();

    if (pid < 0) {
        return ERR_PROCESS;
    }

    if (pid == 0) {
        execv(binary_path, argv_child);
        fprintf(
            stderr,
            "bootstrap: failed to exec %s: %s\n",
            binary_path,
            strerror(errno)
        );
        _exit(127);
    }

    *out_pid = pid;

    return PROJECT_OK;
}

static int spawn_nodes(
    int count,
    const char *runtime_dir,
    process_table_t *table
)
{
    for (int id = 0; id < count; id++) {
        char id_text[16];
        pid_t pid;
        int result;

        snprintf(id_text, sizeof(id_text), "%d", id);

        char *argv_child[] = {
            (char *)"node",
            id_text,
            (char *)runtime_dir,
            NULL
        };

        result = spawn_process(NODE_BINARY, argv_child, &pid);

        if (result != PROJECT_OK) {
            return result;
        }

        result = process_table_add(table, PROCESS_TYPE_NODE, id, pid);

        if (result != PROJECT_OK) {
            return result;
        }
    }

    return PROJECT_OK;
}

static int spawn_miners(
    int count,
    const char *runtime_dir,
    int difficulty,
    process_table_t *table
)
{
    for (int id = 0; id < count; id++) {
        char id_text[16];
        char difficulty_text[16];
        pid_t pid;
        int result;

        snprintf(id_text, sizeof(id_text), "%d", id);
        snprintf(difficulty_text, sizeof(difficulty_text), "%d", difficulty);

        char *argv_child[] = {
            (char *)"miner",
            id_text,
            (char *)runtime_dir,
            difficulty_text,
            NULL
        };

        result = spawn_process(MINER_BINARY, argv_child, &pid);

        if (result != PROJECT_OK) {
            return result;
        }

        result = process_table_add(table, PROCESS_TYPE_MINER, id, pid);

        if (result != PROJECT_OK) {
            return result;
        }
    }

    return PROJECT_OK;
}

static int spawn_clients(
    int count,
    const char *runtime_dir,
    int transaction_frequency,
    process_table_t *table
)
{
    for (int id = 0; id < count; id++) {
        char id_text[16];
        char frequency_text[16];
        pid_t pid;
        int result;

        snprintf(id_text, sizeof(id_text), "%d", id);
        snprintf(
            frequency_text,
            sizeof(frequency_text),
            "%d",
            transaction_frequency
        );

        char *argv_child[] = {
            (char *)"client",
            id_text,
            (char *)runtime_dir,
            frequency_text,
            NULL
        };

        result = spawn_process(CLIENT_BINARY, argv_child, &pid);

        if (result != PROJECT_OK) {
            return result;
        }

        result = process_table_add(table, PROCESS_TYPE_CLIENT, id, pid);

        if (result != PROJECT_OK) {
            return result;
        }
    }

    return PROJECT_OK;
}

static void reap_terminated_children(process_table_t *table)
{
    pid_t pid;
    int status;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        process_entry_t *entry = process_table_find_by_pid(table, pid);

        if (entry != NULL) {
            entry->state = PROCESS_STATE_TERMINATED;
            printf(
                "%s %d (pid %d) terminated\n",
                process_type_string(entry->type),
                entry->id,
                (int)pid
            );
        }
    }
}

static void pause_all(process_table_t *table)
{
    for (size_t index = 0; index < table->count; index++) {
        process_entry_t *entry = &table->entries[index];

        if (entry->state == PROCESS_STATE_RUNNING &&
            kill(entry->pid, SIGSTOP) == 0) {
            entry->state = PROCESS_STATE_PAUSED;
        }
    }
}

static void resume_all(process_table_t *table)
{
    for (size_t index = 0; index < table->count; index++) {
        process_entry_t *entry = &table->entries[index];

        if (entry->state == PROCESS_STATE_PAUSED &&
            kill(entry->pid, SIGCONT) == 0) {
            entry->state = PROCESS_STATE_RUNNING;
        }
    }
}

static void shutdown_all(process_table_t *table)
{
    time_t deadline;
    int remaining;

    for (size_t index = 0; index < table->count; index++) {
        process_entry_t *entry = &table->entries[index];

        if (entry->state == PROCESS_STATE_TERMINATED) {
            continue;
        }

        if (entry->state == PROCESS_STATE_PAUSED) {
            /* A stopped process can't act on SIGTERM until continued. */
            kill(entry->pid, SIGCONT);
        }

        kill(entry->pid, SIGTERM);
    }

    deadline = time(NULL) + SHUTDOWN_TIMEOUT_SECONDS;

    do {
        reap_terminated_children(table);

        remaining = 0;
        for (size_t index = 0; index < table->count; index++) {
            if (table->entries[index].state != PROCESS_STATE_TERMINATED) {
                remaining++;
            }
        }

        if (remaining > 0 && time(NULL) < deadline) {
            struct timespec delay = { .tv_sec = 0, .tv_nsec = 100000000L };
            nanosleep(&delay, NULL);
        }
    } while (remaining > 0 && time(NULL) < deadline);

    if (remaining > 0) {
        for (size_t index = 0; index < table->count; index++) {
            if (table->entries[index].state != PROCESS_STATE_TERMINATED) {
                kill(table->entries[index].pid, SIGKILL);
            }
        }

        while (waitpid(-1, NULL, 0) > 0) {
            /* drain */
        }

        for (size_t index = 0; index < table->count; index++) {
            table->entries[index].state = PROCESS_STATE_TERMINATED;
        }
    }
}

static int send_request(
    const char *sock_path,
    message_type_t type,
    const char *payload,
    message_header_t *out_header,
    void **out_response
)
{
    int fd;
    int result = ipc_connect(sock_path, &fd);
    uint32_t payload_len;

    if (result != PROJECT_OK) {
        fprintf(
            stderr,
            "bootstrap: could not reach %s: %s\n",
            sock_path,
            project_error_string(result)
        );
        return result;
    }

    payload_len = (payload != NULL) ? (uint32_t)strlen(payload) : 0;

    result = ipc_send(fd, type, 0, payload, payload_len);

    if (result != PROJECT_OK) {
        fprintf(
            stderr,
            "bootstrap: send failed: %s\n",
            project_error_string(result)
        );
        ipc_close(fd);
        return result;
    }

    result = ipc_recv(fd, out_header, out_response);
    ipc_close(fd);

    if (result != PROJECT_OK) {
        fprintf(
            stderr,
            "bootstrap: no response: %s\n",
            project_error_string(result)
        );
        return result;
    }

    return PROJECT_OK;
}

static int build_node0_path(
    const char *runtime_dir,
    char *out_path,
    size_t out_size
)
{
    return ipc_build_socket_path(
        runtime_dir,
        NODE_SOCK_FORMAT,
        0,
        out_path,
        out_size
    );
}

static void print_response(const message_header_t *header, const void *response)
{
    if (header->payload_len > 0) {
        printf("%.*s\n", (int)header->payload_len, (const char *)response);
    } else {
        printf("(empty response)\n");
    }
}

static void command_submit(
    const char *args,
    const bootstrap_config_t *config,
    const char *runtime_dir
)
{
    static int next_miner = 0;
    const char *first_quote;
    const char *last_quote;
    size_t length;
    char transaction[MAX_TRANSACTION_LENGTH];
    char sock_path[MAX_SOCKET_PATH_LEN];
    message_header_t header;
    void *response = NULL;

    if (config->num_miners <= 0) {
        fprintf(stderr, "bootstrap: no miners running\n");
        return;
    }

    first_quote = strchr(args, '"');
    last_quote = (first_quote != NULL)
        ? strrchr(first_quote + 1, '"')
        : NULL;

    if (first_quote == NULL || last_quote == NULL || last_quote <= first_quote) {
        fprintf(stderr, "usage: submit \"<transaction>\"\n");
        return;
    }

    length = (size_t)(last_quote - first_quote - 1);

    if (length >= sizeof(transaction)) {
        fprintf(stderr, "bootstrap: transaction too long\n");
        return;
    }

    memcpy(transaction, first_quote + 1, length);
    transaction[length] = '\0';

    if (ipc_build_socket_path(
            runtime_dir,
            MINER_SOCK_FORMAT,
            next_miner,
            sock_path,
            sizeof(sock_path)
        ) != PROJECT_OK) {
        fprintf(stderr, "bootstrap: could not build miner socket path\n");
        return;
    }

    next_miner = (next_miner + 1) % config->num_miners;

    if (send_request(
            sock_path,
            MSG_TX_SUBMIT,
            transaction,
            &header,
            &response
        ) == PROJECT_OK) {
        print_response(&header, response);
        free(response);
    }
}

static void command_request_blockchain(
    const char *args,
    const char *runtime_dir
)
{
    char sock_path[MAX_SOCKET_PATH_LEN];
    message_header_t header;
    void *response = NULL;

    while (*args == ' ') {
        args++;
    }

    if (build_node0_path(runtime_dir, sock_path, sizeof(sock_path)) != PROJECT_OK) {
        fprintf(stderr, "bootstrap: could not build node socket path\n");
        return;
    }

    if (send_request(
            sock_path,
            MSG_QUERY_CHAIN,
            args,
            &header,
            &response
        ) == PROJECT_OK) {
        print_response(&header, response);
        free(response);
    }
}

static void command_request_block(
    const char *args,
    const char *runtime_dir
)
{
    char sock_path[MAX_SOCKET_PATH_LEN];
    message_header_t header;
    void *response = NULL;

    if (build_node0_path(runtime_dir, sock_path, sizeof(sock_path)) != PROJECT_OK) {
        fprintf(stderr, "bootstrap: could not build node socket path\n");
        return;
    }

    if (send_request(
            sock_path,
            MSG_QUERY_BLOCK,
            args,
            &header,
            &response
        ) == PROJECT_OK) {
        print_response(&header, response);
        free(response);
    }
}

static void command_save_blockchain(
    const char *filename,
    const char *runtime_dir
)
{
    char sock_path[MAX_SOCKET_PATH_LEN];
    message_header_t header;
    void *response = NULL;
    FILE *file;

    if (build_node0_path(runtime_dir, sock_path, sizeof(sock_path)) != PROJECT_OK) {
        fprintf(stderr, "bootstrap: could not build node socket path\n");
        return;
    }

    if (send_request(
            sock_path,
            MSG_QUERY_CHAIN,
            "",
            &header,
            &response
        ) != PROJECT_OK) {
        return;
    }

    file = fopen(filename, "wb");

    if (file == NULL) {
        fprintf(stderr, "bootstrap: could not open %s for writing\n", filename);
        free(response);
        return;
    }

    if (header.payload_len > 0) {
        fwrite(response, 1, header.payload_len, file);
    }

    fclose(file);
    free(response);

    printf("blockchain saved to %s\n", filename);
}

static void handle_line(
    const char *line,
    const bootstrap_config_t *config,
    const char *runtime_dir,
    process_table_t *table,
    int *running
)
{
    static const char CMD_SUBMIT[] = "submit ";
    static const char CMD_REQUEST_BLOCKCHAIN[] = "request blockchain";
    static const char CMD_REQUEST_BLOCK[] = "request block ";
    static const char CMD_SAVE_BLOCKCHAIN[] = "save blockchain ";

    if (strncmp(line, CMD_SUBMIT, sizeof(CMD_SUBMIT) - 1) == 0) {
        command_submit(line + sizeof(CMD_SUBMIT) - 1, config, runtime_dir);
        return;
    }

    if (strncmp(
            line,
            CMD_REQUEST_BLOCKCHAIN,
            sizeof(CMD_REQUEST_BLOCKCHAIN) - 1
        ) == 0) {
        command_request_blockchain(
            line + sizeof(CMD_REQUEST_BLOCKCHAIN) - 1,
            runtime_dir
        );
        return;
    }

    if (strncmp(line, CMD_REQUEST_BLOCK, sizeof(CMD_REQUEST_BLOCK) - 1) == 0) {
        command_request_block(
            line + sizeof(CMD_REQUEST_BLOCK) - 1,
            runtime_dir
        );
        return;
    }

    if (strncmp(
            line,
            CMD_SAVE_BLOCKCHAIN,
            sizeof(CMD_SAVE_BLOCKCHAIN) - 1
        ) == 0) {
        command_save_blockchain(
            line + sizeof(CMD_SAVE_BLOCKCHAIN) - 1,
            runtime_dir
        );
        return;
    }

    if (strcmp(line, "pause") == 0) {
        pause_all(table);
        return;
    }

    if (strcmp(line, "resume") == 0) {
        resume_all(table);
        return;
    }

    if (strcmp(line, "status") == 0) {
        process_table_print(table);
        return;
    }

    if (strcmp(line, "stop") == 0) {
        *running = 0;
        return;
    }

    fprintf(stderr, "bootstrap: unknown command: %s\n", line);
}

int main(int argc, char *argv[])
{
    bootstrap_config_t config;
    char runtime_dir[RUNTIME_DIR_BUFFER_SIZE];
    process_table_t table;
    struct sigaction action;
    int running = 1;
    char line[COMMAND_LINE_BUFFER_SIZE];

    if (parse_arguments(argc, argv, &config) != PROJECT_OK) {
        fprintf(
            stderr,
            "Usage: %s <num_nodes> <num_miners> <num_clients> "
            "[transaction_frequency] [difficulty] [initial_state.csv]\n",
            argv[0]
        );
        return EXIT_FAILURE;
    }

    if (create_runtime_directory(runtime_dir, sizeof(runtime_dir)) != PROJECT_OK) {
        fprintf(stderr, "bootstrap: failed to create runtime directory\n");
        return EXIT_FAILURE;
    }

    if (config.initial_state_path != NULL &&
        copy_initial_state(config.initial_state_path, runtime_dir) != PROJECT_OK) {
        fprintf(
            stderr,
            "bootstrap: failed to copy initial state file %s\n",
            config.initial_state_path
        );
        recursive_remove(runtime_dir);
        return EXIT_FAILURE;
    }

    process_table_init(&table);

    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_sigchld;
    sigaction(SIGCHLD, &action, NULL);

    action.sa_handler = handle_termination_signal;
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);

    if (spawn_nodes(config.num_nodes, runtime_dir, &table) != PROJECT_OK ||
        spawn_miners(
            config.num_miners,
            runtime_dir,
            config.difficulty,
            &table
        ) != PROJECT_OK ||
        spawn_clients(
            config.num_clients,
            runtime_dir,
            config.transaction_frequency,
            &table
        ) != PROJECT_OK) {
        fprintf(stderr, "bootstrap: failed to start child processes\n");
        shutdown_all(&table);
        recursive_remove(runtime_dir);
        return EXIT_FAILURE;
    }

    printf("blockchain system running (runtime dir: %s)\n", runtime_dir);
    process_table_print(&table);

    while (running) {
        size_t length;

        reap_terminated_children(&table);

        if (g_shutdown_requested) {
            break;
        }

        printf("> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            if (ferror(stdin) && errno == EINTR) {
                clearerr(stdin);
                continue;
            }
            break;
        }

        length = strlen(line);

        while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
            line[--length] = '\0';
        }

        if (length == 0) {
            continue;
        }

        handle_line(line, &config, runtime_dir, &table, &running);
    }

    printf("bootstrap: shutting down...\n");
    shutdown_all(&table);
    recursive_remove(runtime_dir);
    printf("bootstrap: shutdown complete\n");

    return EXIT_SUCCESS;
}
