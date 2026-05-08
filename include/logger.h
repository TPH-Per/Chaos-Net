/* include/logger.h
 * Thread-safe structured logger
 * Author: Agent
 * Date: 2026-05-08
 * License: MIT
 */
#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include "config.h"

/* Initialize logger. If filepath is NULL, logs to stderr only. */
int  log_init(const char *filepath, int min_level);

/* Core log function — use macros below instead */
void log_write(int level, const char *file, int line, const char *fmt, ...);

/* Convenience macros */
#define LOG_DEBUG(fmt, ...) log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  log_write(LOG_LEVEL_INFO,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  log_write(LOG_LEVEL_WARN,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) log_write(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

void log_close(void);

#endif /* LOGGER_H */