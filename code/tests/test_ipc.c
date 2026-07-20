#include "ipc.h"
#include "errors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static const char *SOCK_PATH = "/tmp/test_ipc.sock";

static int test_build_socket_path(void)
{
    char path[256];

    int result = ipc_build_socket_path(
        "/tmp/blockchain_1234",
        MINER_SOCK_FORMAT,
        2,
        path,
        sizeof(path)
    );

    if (result != PROJECT_OK) {
        fprintf(
            stderr,
            "ipc_build_socket_path failed: %s\n",
            project_error_string(result)
        );

        return 1;
    }

    if (strcmp(path, "/tmp/blockchain_1234/miner_2.sock") != 0) {
        fprintf(stderr, "Unexpected socket path: %s\n", path);

        return 1;
    }

    printf("PASS: ipc_build_socket_path formats miner socket path\n");

    return 0;
}

static int test_build_socket_path_invalid_args(void)
{
    char path[256];

    int result = ipc_build_socket_path(
        NULL,
        MINER_SOCK_FORMAT,
        0,
        path,
        sizeof(path)
    );

    if (result != ERR_INVALID_ARGUMENT) {
        fprintf(
            stderr,
            "Invalid-args test failed. Returned: %d\n",
            result
        );

        return 1;
    }

    printf("PASS: ipc_build_socket_path rejects NULL runtime_dir\n");

    return 0;
}

static int test_round_trip(void)
{
    ipc_unlink_socket(SOCK_PATH);

    int server_fd;
    int result = ipc_server_create(SOCK_PATH, &server_fd);

    if (result != PROJECT_OK) {
        fprintf(
            stderr,
            "ipc_server_create failed: %s\n",
            project_error_string(result)
        );

        return 1;
    }

    pid_t pid = fork();

    if (pid == 0) {
        int fd;

        if (ipc_connect(SOCK_PATH, &fd) != PROJECT_OK) {
            _exit(1);
        }

        const char *payload = "Alice pays Bob 10 coins";

        if (ipc_send(fd, MSG_TX_SUBMIT, 7, payload, (uint32_t)strlen(payload)) != PROJECT_OK) {
            _exit(1);
        }

        ipc_close(fd);
        _exit(0);
    }

    int client_fd;

    if (ipc_server_accept(server_fd, &client_fd) != PROJECT_OK) {
        fprintf(stderr, "ipc_server_accept failed\n");

        return 1;
    }

    message_header_t header;
    void *payload = NULL;

    result = ipc_recv(client_fd, &header, &payload);

    if (result != PROJECT_OK) {
        fprintf(
            stderr,
            "ipc_recv failed: %s\n",
            project_error_string(result)
        );

        return 1;
    }

    int failed = 0;

    if (header.type != MSG_TX_SUBMIT) {
        fprintf(stderr, "Round-trip test failed: wrong message type\n");
        failed = 1;
    }

    if (header.sender_id != 7) {
        fprintf(stderr, "Round-trip test failed: wrong sender_id\n");
        failed = 1;
    }

    if (payload == NULL ||
        strncmp(payload, "Alice pays Bob 10 coins", header.payload_len) != 0) {
        fprintf(stderr, "Round-trip test failed: wrong payload\n");
        failed = 1;
    }

    free(payload);
    ipc_close(client_fd);
    ipc_close(server_fd);
    ipc_unlink_socket(SOCK_PATH);

    int status;
    waitpid(pid, &status, 0);

    if (failed) {
        return 1;
    }

    printf("PASS: round-trip message delivered correctly\n");

    return 0;
}

static int test_send_rejects_oversized_payload(void)
{
    ipc_unlink_socket(SOCK_PATH);

    int server_fd;

    if (ipc_server_create(SOCK_PATH, &server_fd) != PROJECT_OK) {
        fprintf(stderr, "ipc_server_create failed\n");

        return 1;
    }

    int fd;

    if (ipc_connect(SOCK_PATH, &fd) != PROJECT_OK) {
        fprintf(stderr, "ipc_connect failed\n");

        return 1;
    }

    char dummy = 0;
    int result = ipc_send(fd, MSG_TX_SUBMIT, 0, &dummy, MAX_PAYLOAD_SIZE + 1);

    ipc_close(fd);
    ipc_close(server_fd);
    ipc_unlink_socket(SOCK_PATH);

    if (result != ERR_INVALID_ARGUMENT) {
        fprintf(
            stderr,
            "Oversized-payload test failed. Returned: %d\n",
            result
        );

        return 1;
    }

    printf("PASS: ipc_send rejects payload larger than MAX_PAYLOAD_SIZE\n");

    return 0;
}

static int test_server_create_rejects_existing_path(void)
{
    ipc_unlink_socket(SOCK_PATH);

    int first_fd;

    if (ipc_server_create(SOCK_PATH, &first_fd) != PROJECT_OK) {
        fprintf(stderr, "ipc_server_create failed\n");

        return 1;
    }

    int second_fd;
    int result = ipc_server_create(SOCK_PATH, &second_fd);

    ipc_close(first_fd);
    ipc_unlink_socket(SOCK_PATH);

    if (result != ERR_ALREADY_EXISTS) {
        fprintf(
            stderr,
            "Already-exists test failed. Returned: %d\n",
            result
        );

        return 1;
    }

    printf("PASS: ipc_server_create rejects an already-existing socket path\n");

    return 0;
}

int main(void)
{
    int failed_tests = 0;

    failed_tests += test_build_socket_path();
    failed_tests += test_build_socket_path_invalid_args();
    failed_tests += test_round_trip();
    failed_tests += test_send_rejects_oversized_payload();
    failed_tests += test_server_create_rejects_existing_path();

    if (failed_tests != 0) {
        fprintf(
            stderr,
            "%d IPC test(s) failed.\n",
            failed_tests
        );

        return 1;
    }

    printf("All IPC tests passed.\n");

    return 0;
}
