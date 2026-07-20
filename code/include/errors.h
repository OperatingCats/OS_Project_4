#ifndef ERRORS_H
#define ERRORS_H

typedef enum {
    PROJECT_OK = 0,

    ERR_INVALID_ARGUMENT,
    ERR_INVALID_TRANSACTION,
    ERR_INVALID_BLOCK,
    ERR_INVALID_BLOCKCHAIN,

    ERR_MEMORY_ALLOCATION,

    ERR_FILE_OPEN,
    ERR_FILE_READ,
    ERR_FILE_WRITE,
    ERR_FILE_FORMAT,

    ERR_CRYPTO,
    ERR_SERIALIZATION,
    ERR_NOT_FOUND,

    ERR_IPC,
    ERR_TIMEOUT,
    ERR_PROCESS,
    ERR_ALREADY_EXISTS
} project_error_t;

const char *project_error_string(project_error_t error);

#endif