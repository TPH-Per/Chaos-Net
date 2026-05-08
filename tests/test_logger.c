/* tests/test_logger.c
 * Smoke test for Module 1
 * Author: Agent
 * Date: 2026-05-08
 * License: MIT
 */
#include "logger.h"
#include <stdio.h>

int main(void) {
    log_init("/tmp/chaosnet_test.log", LOG_LEVEL_INFO);
    LOG_DEBUG("this should NOT appear");
    LOG_INFO("info message %d", 42);
    LOG_WARN("warning here");
    LOG_ERROR("error here");
    log_close();

    /* verify /tmp/chaosnet_test.log has exactly 3 lines */
    FILE *f = fopen("/tmp/chaosnet_test.log", "r");
    if (!f) {
        fprintf(stderr, "Failed to open log file\n");
        return 1;
    }

    int lines = 0;
    char buf[1024];
    while (fgets(buf, sizeof(buf), f)) {
        lines++;
    }
    fclose(f);

    if (lines == 3) {
        printf("Logger test passed.\n");
        return 0;
    } else {
        printf("Logger test failed. Expected 3 lines, got %d.\n", lines);
        return 1;
    }
}
