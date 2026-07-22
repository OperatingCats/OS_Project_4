#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

/* Shared logging interface. Format: [timestamp] <role> <id>: <event>
 * Each process calls logger_init() once at startup, logger_log() for each
 * event, and logger_close() before exiting. */

FILE *logger_init(const char *runtime_dir, const char *role, int id);
void logger_log(FILE *log_file, const char *role, int id, const char *format, ...);
void logger_close(FILE *log_file);

#endif /* LOGGER_H */
