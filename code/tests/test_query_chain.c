#include "ipc.h"
#include "errors.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <socket_path>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int fd;
    int rc = ipc_connect(argv[1], &fd);
    if (rc != PROJECT_OK) {
        fprintf(stderr, "ipc_connect failed: %s\n", project_error_string(rc));
        return EXIT_FAILURE;
    }

    rc = ipc_send(fd, MSG_QUERY_CHAIN, 1, NULL, 0);
    if (rc != PROJECT_OK) {
        fprintf(stderr, "ipc_send failed: %s\n", project_error_string(rc));
        ipc_close(fd);
        return EXIT_FAILURE;
    }

    message_header_t header;
    void *payload = NULL;
    rc = ipc_recv(fd, &header, &payload);
    if (rc != PROJECT_OK) {
        fprintf(stderr, "ipc_recv failed: %s\n", project_error_string(rc));
        ipc_close(fd);
        return EXIT_FAILURE;
    }

    printf("client: reply type=%u payload_len=%u\n", header.type, header.payload_len);
    if (payload != NULL) {
        printf("--- chain data ---\n%.*s------------------\n",
               header.payload_len, (char *)payload);
        free(payload);
    }

    ipc_close(fd);
    return EXIT_SUCCESS;
}
