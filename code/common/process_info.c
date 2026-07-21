/* Utility functions for the process table maintained by Bootstrap. */

#include "process_info.h"
#include "errors.h"

#include <stdio.h>
#include <string.h>

int process_table_init(process_table_t *table)
{
    if (table == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    memset(table, 0, sizeof(*table));

    return PROJECT_OK;
}

int process_table_add(
    process_table_t *table,
    process_type_t type,
    int id,
    pid_t pid
)
{
    process_entry_t *entry;

    if (table == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    if (table->count >= MAX_TRACKED_PROCESSES) {
        return ERR_MEMORY_ALLOCATION;
    }

    entry = &table->entries[table->count];
    entry->type = type;
    entry->id = id;
    entry->pid = pid;
    entry->state = PROCESS_STATE_RUNNING;

    table->count++;

    return PROJECT_OK;
}

process_entry_t *process_table_find(
    process_table_t *table,
    process_type_t type,
    int id
)
{
    if (table == NULL) {
        return NULL;
    }

    for (size_t index = 0; index < table->count; index++) {
        if (table->entries[index].type == type &&
            table->entries[index].id == id) {
            return &table->entries[index];
        }
    }

    return NULL;
}

process_entry_t *process_table_find_by_pid(
    process_table_t *table,
    pid_t pid
)
{
    if (table == NULL) {
        return NULL;
    }

    for (size_t index = 0; index < table->count; index++) {
        if (table->entries[index].pid == pid) {
            return &table->entries[index];
        }
    }

    return NULL;
}

const char *process_type_string(process_type_t type)
{
    switch (type) {
        case PROCESS_TYPE_NODE:
            return "NODE";
        case PROCESS_TYPE_MINER:
            return "MINER";
        case PROCESS_TYPE_CLIENT:
            return "CLIENT";
        default:
            return "UNKNOWN";
    }
}

const char *process_state_string(process_state_t state)
{
    switch (state) {
        case PROCESS_STATE_RUNNING:
            return "RUNNING";
        case PROCESS_STATE_PAUSED:
            return "PAUSED";
        case PROCESS_STATE_TERMINATED:
            return "TERMINATED";
        default:
            return "UNKNOWN";
    }
}

void process_table_print(const process_table_t *table)
{
    if (table == NULL) {
        return;
    }

    printf("%-8s %-4s %-8s %-10s\n", "TYPE", "ID", "PID", "STATE");

    for (size_t index = 0; index < table->count; index++) {
        const process_entry_t *entry = &table->entries[index];

        printf(
            "%-8s %-4d %-8d %-10s\n",
            process_type_string(entry->type),
            entry->id,
            (int)entry->pid,
            process_state_string(entry->state)
        );
    }
}
