#include "node_queue.h"
#include "errors.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Stress test: many producer threads push more items than the queue's
 * fixed capacity while many consumer threads pop concurrently. Verifies
 * every item pushed is received by exactly one consumer: nothing lost,
 * nothing duplicated, even when the queue fills up and blocks. */

#define NUM_PRODUCERS 4
#define ITEMS_PER_PRODUCER 50
#define NUM_CONSUMERS 4
#define TOTAL_ITEMS (NUM_PRODUCERS * ITEMS_PER_PRODUCER)

static node_queue_t g_queue;
static int g_seen[TOTAL_ITEMS];
static pthread_mutex_t g_seen_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_total_popped = 0;
static int g_duplicates = 0;

typedef struct {
    int producer_id;
} producer_arg_t;

static void *producer_main(void *arg) {
    producer_arg_t *pa = (producer_arg_t *)arg;
    for (int i = 0; i < ITEMS_PER_PRODUCER; i++) {
        queue_message_t msg;
        memset(&msg, 0, sizeof(msg));
        int unique_id = pa->producer_id * ITEMS_PER_PRODUCER + i;
        msg.header.sender_id = (uint32_t)unique_id;
        msg.header.type = 1;
        msg.payload = NULL;
        msg.client_fd = -1;

        int rc = node_queue_push(&g_queue, &msg);
        if (rc != PROJECT_OK) {
            fprintf(stderr, "producer %d: push failed for item %d (rc=%d)\n",
                    pa->producer_id, unique_id, rc);
        }
    }
    return NULL;
}

static void *consumer_main(void *arg) {
    (void)arg;
    for (;;) {
        queue_message_t msg;
        int rc = node_queue_pop(&g_queue, &msg);
        if (rc == ERR_NOT_FOUND) {
            break; /* queue shut down and drained */
        }
        if (rc != PROJECT_OK) {
            continue;
        }

        int unique_id = (int)msg.header.sender_id;

        pthread_mutex_lock(&g_seen_lock);
        if (unique_id >= 0 && unique_id < TOTAL_ITEMS) {
            if (g_seen[unique_id]) {
                g_duplicates++;
            }
            g_seen[unique_id] = 1;
        }
        g_total_popped++;
        pthread_mutex_unlock(&g_seen_lock);
    }
    return NULL;
}

int main(void) {
    pthread_t producers[NUM_PRODUCERS];
    producer_arg_t producer_args[NUM_PRODUCERS];
    pthread_t consumers[NUM_CONSUMERS];

    if (node_queue_init(&g_queue) != PROJECT_OK) {
        fprintf(stderr, "FAIL: node_queue_init failed\n");
        return 1;
    }

    for (int i = 0; i < NUM_CONSUMERS; i++) {
        pthread_create(&consumers[i], NULL, consumer_main, NULL);
    }

    for (int i = 0; i < NUM_PRODUCERS; i++) {
        producer_args[i].producer_id = i;
        pthread_create(&producers[i], NULL, producer_main, &producer_args[i]);
    }

    for (int i = 0; i < NUM_PRODUCERS; i++) {
        pthread_join(producers[i], NULL);
    }

    /* All items pushed; signal shutdown so consumers can exit once the
     * queue is fully drained, rather than blocking forever. */
    node_queue_shutdown(&g_queue);

    for (int i = 0; i < NUM_CONSUMERS; i++) {
        pthread_join(consumers[i], NULL);
    }

    node_queue_destroy(&g_queue);

    int missing = 0;
    for (int i = 0; i < TOTAL_ITEMS; i++) {
        if (!g_seen[i]) {
            missing++;
        }
    }

    printf("pushed=%d popped=%d missing=%d duplicates=%d\n",
           TOTAL_ITEMS, g_total_popped, missing, g_duplicates);

    if (g_total_popped != TOTAL_ITEMS || missing != 0 || g_duplicates != 0) {
        printf("FAIL: queue lost or duplicated items under concurrent load\n");
        return 1;
    }

    printf("PASS: all %d items received exactly once under concurrent load\n", TOTAL_ITEMS);
    return 0;
}
