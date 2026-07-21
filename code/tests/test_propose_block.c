#include "ipc.h"
#include "errors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <socket_path>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *sock_path = argv[1];

    int fd;
    int rc = ipc_connect(sock_path, &fd);
    if (rc != PROJECT_OK) {
        fprintf(stderr, "ipc_connect failed: %s\n", project_error_string(rc));
        return EXIT_FAILURE;
    }

    const char *block_csv =
        "0000000000000000,0000000067890001,"
        "0000000000000000000000000000000000000000000000000000000000000000,"
        "b815a93dd7f59058a27e63558ba5aa6445d851f316070ec13db673d5ab38e0cc,"
        "0000000000000000,\"Genesis block\"";

    rc = ipc_send(fd, MSG_BLOCK_PROPOSAL, 1, block_csv, (uint32_t)strlen(block_csv));
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

    printf("client: got reply type=%u payload_len=%u\n", header.type, header.payload_len);
    if (payload != NULL) {
        printf("client: reply payload: %.*s\n", header.payload_len, (char *)payload);
        free(payload);
    }

    ipc_close(fd);
    return EXIT_SUCCESS;
}
