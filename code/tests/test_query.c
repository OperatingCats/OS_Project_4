#include "ipc.h"
#include "errors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <socket_path> <height|chain|block> [payload]\n", argv[0]);
        return EXIT_FAILURE;
    }

    message_type_t type;
    if (strcmp(argv[2], "height") == 0) {
        type = MSG_QUERY_HEIGHT;
    } else if (strcmp(argv[2], "chain") == 0) {
        type = MSG_QUERY_CHAIN;
    } else if (strcmp(argv[2], "block") == 0) {
        type = MSG_QUERY_BLOCK;
    } else if (strcmp(argv[2], "sync") == 0) {
        type = MSG_SYNC_REQUEST;
    } else {
        fprintf(stderr, "Unknown query type: %s\n", argv[2]);
        return EXIT_FAILURE;
    }

    const char *payload = (argc >= 4) ? argv[3] : "";

    int fd;
    int rc = ipc_connect(argv[1], &fd);
    if (rc != PROJECT_OK) {
        fprintf(stderr, "ipc_connect failed: %s\n", project_error_string(rc));
        return EXIT_FAILURE;
    }

    rc = ipc_send(fd, type, 1, payload, (uint32_t)strlen(payload));
    if (rc != PROJECT_OK) {
        fprintf(stderr, "ipc_send failed: %s\n", project_error_string(rc));
        ipc_close(fd);
        return EXIT_FAILURE;
    }

    message_header_t header;
    void *reply = NULL;
    rc = ipc_recv(fd, &header, &reply);
    if (rc != PROJECT_OK) {
        fprintf(stderr, "ipc_recv failed: %s\n", project_error_string(rc));
        ipc_close(fd);
        return EXIT_FAILURE;
    }

    printf("reply type=%u payload_len=%u\n", header.type, header.payload_len);
    if (reply != NULL) {
        printf("--- reply ---\n%.*s\n-------------\n", header.payload_len, (char *)reply);
        free(reply);
    }

    ipc_close(fd);
    return EXIT_SUCCESS;
}
