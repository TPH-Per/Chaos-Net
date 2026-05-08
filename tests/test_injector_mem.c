/* tests/test_injector_mem.c
 * Smoke test for Module 4
 * Author: Agent
 * Date: 2026-05-08
 * License: MIT
 */
#include "injector_mem.h"
#include "logger.h"
#include <stdio.h>
#include <unistd.h>

int main(void) {
    log_init(NULL, LOG_LEVEL_DEBUG);
    printf("Testing memory injector...\n");

    if (mem_inject_pressure(50, 3) != 0) {
        printf("Failed to start memory injection\n");
        return 1;
    }

    printf("Waiting for 1 second...\n");
    sleep(1);
    
    mem_inject_clear();
    printf("Clear signaled. Waiting for 1 second...\n");
    sleep(1);

    printf("Memory injector test completed.\n");
    return 0;
}