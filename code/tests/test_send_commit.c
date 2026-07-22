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
    int fd;
    if (ipc_connect(argv[1], &fd) != PROJECT_OK) {
        fprintf(stderr, "connect failed\n");
        return EXIT_FAILURE;
    }
    const char *block_csv =
        "0000000000000000,0000000067890001,"
        "0000000000000000000000000000000000000000000000000000000000000000,"
        "b815a93dd7f59058a27e63558ba5aa6445d851f316070ec13db673d5ab38e0cc,"
        "0000000000000000,\"Genesis block\"";
    ipc_send(fd, MSG_BLOCK_COMMIT, 1, block_csv, (uint32_t)strlen(block_csv));
    printf("client: commit sent\n");
    ipc_close(fd);
    return EXIT_SUCCESS;
}
