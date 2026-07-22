#include "errors.h"

const char *project_error_string(project_error_t error)
{
    switch (error) {
        case PROJECT_OK:
            return "Success";

        case ERR_INVALID_ARGUMENT:
            return "Invalid argument";

        case ERR_INVALID_TRANSACTION:
            return "Invalid transaction";

        case ERR_INVALID_BLOCK:
            return "Invalid block";

        case ERR_INVALID_BLOCKCHAIN:
            return "Invalid blockchain";

        case ERR_MEMORY_ALLOCATION:
            return "Memory allocation failed";

        case ERR_FILE_OPEN:
            return "Unable to open file";

        case ERR_FILE_READ:
            return "Unable to read file";

        case ERR_FILE_WRITE:
            return "Unable to write file";

        case ERR_FILE_FORMAT:
            return "Invalid file format";

        case ERR_CRYPTO:
            return "Cryptographic operation failed";

        case ERR_SERIALIZATION:
            return "Serialization failed";

        case ERR_NOT_FOUND:
            return "Requested element not found";

        case ERR_IPC:
            return "IPC operation failed";

        case ERR_TIMEOUT:
            return "Operation timed out";

        case ERR_PROCESS:
            return "Process operation failed";

        case ERR_ALREADY_EXISTS:
            return "Resource already exists";

        default:
            return "Unknown project error";
    }
}