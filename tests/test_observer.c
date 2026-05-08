/* tests/test_observer.c
 * Smoke test for Module 2
 * Author: Agent
 * Date: 2026-05-08
 * License: MIT
 */
#include "observer.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(void) {
    app_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    
    pthread_mutex_init(&ctx.obs_mutex, NULL);
    pthread_mutex_init(&ctx.findings_mutex, NULL);
    pthread_mutex_init(&ctx.scenario_mutex, NULL);

    ctx.target_pid = getpid(); /* Target ourselves */
    ctx.running = 1;

    if (observer_start(&ctx) != 0) {
        printf("Failed to start observer\n");
        return 1;
    }

    sleep(5); /* collect about 10 snapshots */
    ctx.running = 0;
    pthread_join(ctx.thread_observer, NULL);

    if (ctx.obs_head >= 9) {
        printf("Observer test passed. Collected %u snapshots.\n", ctx.obs_head);
        return 0;
    } else {
        printf("Observer test failed. Only collected %u snapshots.\n", ctx.obs_head);
        return 1;
    }
}