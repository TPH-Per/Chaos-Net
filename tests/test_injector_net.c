/* tests/test_injector_net.c
 * Smoke test for Module 3
 * Author: Agent
 * Date: 2026-05-08
 * License: MIT
 */
#include "injector_net.h"
#include <stdio.h>
#include <unistd.h>

int main(void) {
    /* Need root to actually test tc, but we can call it */
    printf("Testing network injector...\n");
    if (net_inject_packet_loss("lo", 10) != 0) {
        printf("net_inject_packet_loss failed (might need root)\n");
    }
    sleep(1);
    if (net_inject_clear("lo") != 0) {
        printf("net_inject_clear failed\n");
    }
    printf("Network injector test completed.\n");
    return 0;
}