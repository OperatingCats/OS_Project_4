#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

/* Message types and wire format exchanged between bootstrap, node, miner,
 * and client processes over Unix domain sockets.
 *
 * Block-carrying messages reuse the CSV-line encoding from
 * block_to_csv()/block_from_csv() (include/block.h): index,timestamp,
 * prev_hash,merkle_root,nonce,transactions, with hashes as big-endian hex
 * and transactions "::"-separated -- the same format used for the on-disk
 * blockchain state file.
 *
 * Every message is [message_header_t][payload_len raw bytes], sent over a
 * short-lived connection: connect, write one message, optionally read one
 * reply, then close.
 */

#define PROTOCOL_MAGIC   0x424C4B43u /* ASCII "BLKC" */
#define PROTOCOL_VERSION 1

#define MAX_PAYLOAD_SIZE 65536u

typedef enum {
    MSG_TX_SUBMIT       = 1,  /* client -> miner.  payload: raw transaction string */
    MSG_TX_ACK          = 2,  /* miner  -> client. payload: "OK" or "ERR <project_error_t> <text>" */

    MSG_BLOCK_PROPOSAL  = 3,  /* miner  -> node 0. payload: candidate block, CSV line */
    MSG_BLOCK_COMMIT    = 4,  /* node0  -> nodes+miners (broadcast). payload: accepted block, CSV line */
    MSG_BLOCK_REJECT    = 5,  /* node0  -> miner.  payload: "<index> <reason text>" */

    MSG_SYNC_REQUEST    = 6,  /* node   -> node 0. payload: "<from_index>" (decimal) */
    MSG_SYNC_RESPONSE   = 7,  /* node0  -> node.   payload: 0..N block CSV lines, one per '\n' */

    MSG_QUERY_CHAIN     = 8,  /* client/bootstrap -> node. payload: "" | "--index <n>" | "--hash <h>" */
    MSG_QUERY_BLOCK     = 9,  /* client/bootstrap -> node. payload: "--index <n>" | "--hash <h>" */
    MSG_QUERY_HEIGHT    = 10, /* client/bootstrap -> node. payload: "" */

    MSG_RESPONSE_OK     = 11, /* generic success reply. payload: requested data (text / CSV lines) */
    MSG_RESPONSE_ERR    = 12, /* generic error reply.   payload: "<project_error_t> <text>" */

    MSG_SHUTDOWN        = 13  /* bootstrap -> any process. payload: "" */
} message_type_t;

/* Host-order fields: all sockets are local (Unix domain), so there is no
 * cross-endianness concern here (unlike the block hash input, which IS
 * hashed and does need the fixed big-endian encoding from block.h). */
typedef struct {
    uint32_t magic;       /* must equal PROTOCOL_MAGIC */
    uint16_t version;     /* must equal PROTOCOL_VERSION */
    uint16_t type;        /* a message_type_t value */
    uint32_t sender_id;   /* 0-based id of the sender within its role */
    uint32_t payload_len; /* bytes immediately following this header */
} message_header_t;

#define RUNTIME_DIR_FORMAT  "/tmp/blockchain_%d"   /* %d = bootstrap PID */

#define BOOTSTRAP_SOCK_NAME "bootstrap.sock"
#define NODE_SOCK_FORMAT    "node_%d.sock"   /* node 0 is the coordinator */
#define MINER_SOCK_FORMAT   "miner_%d.sock"
#define CLIENT_SOCK_FORMAT  "client_%d.sock"

#define LOG_SUBDIR_NAME     "logs"
#define LOG_FILE_FORMAT     "%s-%d.log"      /* e.g. miner-12847.log */

#define MAX_SOCKET_PATH_LEN 108u /* sizeof(sockaddr_un.sun_path) on Linux */

#endif /* PROTOCOL_H */
