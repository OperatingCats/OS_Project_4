#ifndef PROCESS_INFO_H
#define PROCESS_INFO_H

#include <stddef.h>
#include <sys/types.h>

/* Information about child processes maintained by Bootstrap. */

typedef enum {
    PROCESS_TYPE_NODE,
    PROCESS_TYPE_MINER,
    PROCESS_TYPE_CLIENT
} process_type_t;

typedef enum {
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_PAUSED,
    PROCESS_STATE_TERMINATED
} process_state_t;

typedef struct {
    process_type_t type;
    int id;
    pid_t pid;
    process_state_t state;
} process_entry_t;

#define MAX_TRACKED_PROCESSES 256

typedef struct {
    process_entry_t entries[MAX_TRACKED_PROCESSES];
    size_t count;
} process_table_t;

int process_table_init(process_table_t *table);

int process_table_add(
    process_table_t *table,
    process_type_t type,
    int id,
    pid_t pid
);

process_entry_t *process_table_find(
    process_table_t *table,
    process_type_t type,
    int id
);

process_entry_t *process_table_find_by_pid(
    process_table_t *table,
    pid_t pid
);

void process_table_print(const process_table_t *table);

const char *process_type_string(process_type_t type);
const char *process_state_string(process_state_t state);

#endif /* PROCESS_INFO_H */
