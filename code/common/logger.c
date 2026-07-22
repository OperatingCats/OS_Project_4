#include "logger.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

FILE *logger_init(const char *runtime_dir, const char *role, int id) {
    
    (void)id;
    if (runtime_dir == NULL || role == NULL) {
        return NULL;
    }

    char logs_dir[300];
    snprintf(logs_dir, sizeof(logs_dir), "%s/logs", runtime_dir);
    mkdir(logs_dir, 0755); /* ignore error if it already exists */

    char path[350];
    snprintf(path, sizeof(path), "%s/%s-%d.log", logs_dir, role, getpid());

    FILE *f = fopen(path, "a");
    return f; /* NULL on failure; callers should tolerate that */
}

void logger_log(FILE *log_file, const char *role, int id, const char *format, ...) {
    if (log_file == NULL) {
        return;
    }

    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_now);

    fprintf(log_file, "[%s] %s %d: ", timestamp, role, id);

    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);

    fprintf(log_file, "\n");
    fflush(log_file);
}

void logger_close(FILE *log_file) {
    if (log_file != NULL) {
        fclose(log_file);
    }
}
