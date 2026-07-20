#ifndef IPC_H
#define IPC_H

#include <stddef.h>
#include <stdint.h>
#include "protocol.h"

/* Unix domain socket helpers used by bootstrap, node, miner, and client.
 * Return values follow project_error_t (PROJECT_OK == 0 on success).
 *
 * A listener calls ipc_server_create() once at startup, then loops on
 * ipc_server_accept() + ipc_recv(). A sender calls ipc_connect(),
 * ipc_send(), optionally ipc_recv() for a reply, then ipc_close().
 * Connections are short-lived: one request (and optional reply) each.
 */

/* Binds and listens on a Unix domain stream socket at sock_path. Returns
 * ERR_ALREADY_EXISTS if sock_path already exists (stale socket files must
 * be unlinked first), ERR_IPC on any socket/bind/listen failure. */
int ipc_server_create(const char *sock_path, int *out_fd);

/* Blocks until a connection arrives on server_fd, returns the
 * per-connection fd in *out_client_fd. */
int ipc_server_accept(int server_fd, int *out_client_fd);

/* Connects to the listener at sock_path. */
int ipc_connect(const char *sock_path, int *out_fd);

/* Writes one message (header + payload) to fd. Returns
 * ERR_INVALID_ARGUMENT if payload_len exceeds MAX_PAYLOAD_SIZE. */
int ipc_send(int fd, message_type_t type, uint32_t sender_id,
             const void *payload, uint32_t payload_len);

/* Reads one message from fd. *out_payload is a heap buffer the caller
 * must free(), or NULL if payload_len is 0. Returns ERR_SERIALIZATION if
 * magic/version don't match. */
int ipc_recv(int fd, message_header_t *out_header, void **out_payload);

/* Closes a connection fd. */
int ipc_close(int fd);

/* Formats "<runtime_dir>/<name_format with id substituted>" into
 * out_path. Pass id < 0 for a fixed name with no substitution (e.g.
 * BOOTSTRAP_SOCK_NAME). */
int ipc_build_socket_path(const char *runtime_dir, const char *name_format,
                           int id, char *out_path, size_t out_size);

/* Removes a socket file; not an error if it's already gone. */
int ipc_unlink_socket(const char *sock_path);

#endif /* IPC_H */
