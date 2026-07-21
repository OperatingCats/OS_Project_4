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

    const char *payload = "hello from test client";
    rc = ipc_send(fd, MSG_TX_SUBMIT, 99, payload, (uint32_t)strlen(payload));
    if (rc != PROJECT_OK) {
        fprintf(stderr, "ipc_send failed: %s\n", project_error_string(rc));
        ipc_close(fd);
        return EXIT_FAILURE;
    }

    printf("client: message sent successfully\n");
    ipc_close(fd);
    return EXIT_SUCCESS;
}
