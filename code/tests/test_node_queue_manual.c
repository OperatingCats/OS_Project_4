
#include "node_queue.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    node_queue_t q;
    node_queue_init(&q);

    queue_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.header.type = 1;
    msg.header.sender_id = 42;
    msg.payload = NULL;
    msg.client_fd = -1;

    int rc = node_queue_push(&q, &msg);
    printf("push returned: %d\n", rc);

    queue_message_t out;
    rc = node_queue_pop(&q, &out);
    printf("pop returned: %d, sender_id=%u, type=%u\n",
           rc, out.header.sender_id, out.header.type);

    node_queue_destroy(&q);
    return 0;
}
