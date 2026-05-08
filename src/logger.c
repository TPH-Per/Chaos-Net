/* src/logger.c
 * Thread-safe structured logger implementation
 * Author: Agent
 * Date: 2026-05-08
 * License: MIT
 */
#include "logger.h"
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

static FILE *log_file = NULL;
static int current_min_level = LOG_LEVEL_INFO;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

static const char *level_to_string(int level) {
    switch(level) {
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_INFO:  return "INFO ";
        case LOG_LEVEL_WARN:  return "WARN ";
        case LOG_LEVEL_ERROR: return "ERROR";
        default:              return "UNKN ";
    }
}

int log_init(const char *filepath, int min_level) {
    pthread_mutex_lock(&log_mutex);
    current_min_level = min_level;
    if (filepath != NULL) {
        log_file = fopen(filepath, "a");
        if (!log_file) {
            pthread_mutex_unlock(&log_mutex);
            return -1;
        }
    } else {
        log_file = NULL;
    }
    pthread_mutex_unlock(&log_mutex);
    return 0;
}

void log_write(int level, const char *file, int line, const char *fmt, ...) {
    if (level < current_min_level) {
        return;
    }

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm *tm_info = localtime(&ts.tv_sec);

    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

    /* Extract filename from path */
    const char *filename = strrchr(file, '/');
    if (filename) filename++;
    else filename = file;

    pthread_mutex_lock(&log_mutex);

    char log_buf[1024];
    int prefix_len = snprintf(log_buf, sizeof(log_buf), "[%s.%03ld] [%s] [%s:%d] ",
                              time_buf, ts.tv_nsec / 1000000,
                              level_to_string(level), filename, line);

    if (prefix_len > 0 && prefix_len < (int)sizeof(log_buf)) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(log_buf + prefix_len, sizeof(log_buf) - prefix_len, fmt, args);
        va_end(args);

        /* Remove trailing newline if it exists to add our own */
        size_t len = strlen(log_buf);
        if (len > 0 && log_buf[len-1] == '\n') {
            log_buf[len-1] = '\0';
        }

        fprintf(stderr, "%s\n", log_buf);
        if (log_file) {
            fprintf(log_file, "%s\n", log_buf);
            fflush(log_file);
        }
    }

    pthread_mutex_unlock(&log_mutex);
}

void log_close(void) {
    pthread_mutex_lock(&log_mutex);
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
    pthread_mutex_unlock(&log_mutex);
}