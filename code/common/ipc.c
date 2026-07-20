#include "ipc.h"
#include "errors.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#define IPC_BACKLOG 16

/* read()/write() on a socket are not guaranteed to move every byte in one
 * call. These loop until exactly `len` bytes have moved, or the
 * connection fails/closes. Returns 0 on success, -1 otherwise. */
static int read_full(int fd, void *buf, size_t len) {
    unsigned char *p = buf;
    size_t done = 0;
    while (done < len) {
        ssize_t n = read(fd, p + done, len - done);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            /* peer closed before sending everything it promised */
            return -1;
        }
        done += (size_t)n;
    }
    return 0;
}

static int write_full(int fd, const void *buf, size_t len) {
    const unsigned char *p = buf;
    size_t done = 0;
    while (done < len) {
        ssize_t n = write(fd, p + done, len - done);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        done += (size_t)n;
    }
    return 0;
}

int ipc_build_socket_path(const char *runtime_dir, const char *name_format,
                           int id, char *out_path, size_t out_size) {
    if (runtime_dir == NULL || name_format == NULL || out_path == NULL || out_size == 0) {
        return ERR_INVALID_ARGUMENT;
    }

    char name[MAX_SOCKET_PATH_LEN];
    int name_len;
    if (id < 0) {
        name_len = snprintf(name, sizeof(name), "%s", name_format);
    } else {
        name_len = snprintf(name, sizeof(name), name_format, id);
    }
    if (name_len < 0 || (size_t)name_len >= sizeof(name)) {
        return ERR_INVALID_ARGUMENT;
    }

    int written = snprintf(out_path, out_size, "%s/%s", runtime_dir, name);
    if (written < 0 || (size_t)written >= out_size || (size_t)written >= MAX_SOCKET_PATH_LEN) {
        return ERR_INVALID_ARGUMENT;
    }

    return PROJECT_OK;
}

int ipc_unlink_socket(const char *sock_path) {
    if (sock_path == NULL) {
        return ERR_INVALID_ARGUMENT;
    }
    if (unlink(sock_path) != 0 && errno != ENOENT) {
        return ERR_IPC;
    }
    return PROJECT_OK;
}

int ipc_server_create(const char *sock_path, int *out_fd) {
    if (sock_path == NULL || out_fd == NULL) {
        return ERR_INVALID_ARGUMENT;
    }
    if (strlen(sock_path) >= MAX_SOCKET_PATH_LEN) {
        return ERR_INVALID_ARGUMENT;
    }

    struct stat st;
    if (stat(sock_path, &st) == 0) {
        return ERR_ALREADY_EXISTS;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return ERR_IPC;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return (errno == EADDRINUSE) ? ERR_ALREADY_EXISTS : ERR_IPC;
    }

    if (listen(fd, IPC_BACKLOG) != 0) {
        close(fd);
        unlink(sock_path);
        return ERR_IPC;
    }

    *out_fd = fd;
    return PROJECT_OK;
}

int ipc_server_accept(int server_fd, int *out_client_fd) {
    if (out_client_fd == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    int client_fd;
    do {
        client_fd = accept(server_fd, NULL, NULL);
    } while (client_fd < 0 && errno == EINTR);

    if (client_fd < 0) {
        return ERR_IPC;
    }

    *out_client_fd = client_fd;
    return PROJECT_OK;
}

int ipc_connect(const char *sock_path, int *out_fd) {
    if (sock_path == NULL || out_fd == NULL) {
        return ERR_INVALID_ARGUMENT;
    }
    if (strlen(sock_path) >= MAX_SOCKET_PATH_LEN) {
        return ERR_INVALID_ARGUMENT;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return ERR_IPC;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return ERR_IPC;
    }

    *out_fd = fd;
    return PROJECT_OK;
}

int ipc_send(int fd, message_type_t type, uint32_t sender_id,
             const void *payload, uint32_t payload_len) {
    if (payload_len > MAX_PAYLOAD_SIZE || (payload_len > 0 && payload == NULL)) {
        return ERR_INVALID_ARGUMENT;
    }

    message_header_t header;
    header.magic = PROTOCOL_MAGIC;
    header.version = PROTOCOL_VERSION;
    header.type = (uint16_t)type;
    header.sender_id = sender_id;
    header.payload_len = payload_len;

    if (write_full(fd, &header, sizeof(header)) != 0) {
        return ERR_IPC;
    }
    if (payload_len > 0 && write_full(fd, payload, payload_len) != 0) {
        return ERR_IPC;
    }

    return PROJECT_OK;
}

int ipc_recv(int fd, message_header_t *out_header, void **out_payload) {
    if (out_header == NULL || out_payload == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    message_header_t header;
    if (read_full(fd, &header, sizeof(header)) != 0) {
        return ERR_IPC;
    }

    if (header.magic != PROTOCOL_MAGIC || header.version != PROTOCOL_VERSION) {
        return ERR_SERIALIZATION;
    }
    if (header.payload_len > MAX_PAYLOAD_SIZE) {
        return ERR_INVALID_ARGUMENT;
    }

    void *payload = NULL;
    if (header.payload_len > 0) {
        payload = malloc(header.payload_len);
        if (payload == NULL) {
            return ERR_MEMORY_ALLOCATION;
        }
        if (read_full(fd, payload, header.payload_len) != 0) {
            free(payload);
            return ERR_IPC;
        }
    }

    *out_header = header;
    *out_payload = payload;
    return PROJECT_OK;
}

int ipc_close(int fd) {
    if (close(fd) != 0) {
        return ERR_IPC;
    }
    return PROJECT_OK;
}
