/* tools/target_app/target_app.c
 * Intentionally buggy MQTT client for testing ChaosNet
 * Author: Agent
 * Date: 2026-05-08
 * License: MIT
 */
#include <mosquitto.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

static struct mosquitto *mosq = NULL;
static int connected = 0;

static void on_connect(struct mosquitto *m, void *userdata, int result) {
    (void)userdata;
    if (result == 0) {
        printf("[TargetApp] Connected to MQTT broker\n");
        connected = 1;
        mosquitto_subscribe(m, NULL, "test/topic", 1);
    } else {
        printf("[TargetApp] Connect failed: %d\n", result);
    }
}

static void on_disconnect(struct mosquitto *m, void *userdata, int result) {
    (void)m; (void)userdata;
    printf("[TargetApp] Disconnected (code %d)\n", result);
    connected = 0;

    /* BUG-003: Simulate SIGSEGV on disruption.
       If result is unexpected (not a clean disconnect), crash the process. */
    if (result != 0) {
        printf("[TargetApp] Unexpected disconnect. Crashing!\n");
        int *crash = NULL;
        *crash = 42;
    }

    /* BUG-001: Memory leak on reconnect
       Every time MQTT connection drops and reconnects,
       a 1MB buffer is allocated but never freed. */
    void *leak = malloc(1024 * 1024);
    if (leak) {
        memset(leak, 0xBB, 1024 * 1024);
        printf("[TargetApp] Leaked 1MB memory\n");
    }

    /* BUG-004: FD leak
       Calls socket() on each reconnect attempt but never closes
       the previous socket if connect() fails. */
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd >= 0) {
        printf("[TargetApp] Leaked FD %d\n", fd);
    }
}

int main(void) {
    printf("[TargetApp] Starting buggy MQTT client\n");

    /* BUG-003: No packet loss tolerance
       Uses TCP with default socket options.
       Does not handle partial send / recv correctly.
       Will crash (SIGSEGV) if underlying TCP stream is disrupted. 
       Actually, mosquitto handles partial reads, but we will artificially
       force a crash if packet loss is simulated (simulated here by checking
       if a specific condition is met, but a real app might just crash due to
       poor buffer management). For realism, let's just let it fail naturally
       or add a small flaw. We'll leave mosquitto to fail gracefully, but to
       satisfy the spec, let's simulate a crash if we get too many disconnects
       or if we can't reach the broker for too long.
       Actually, the spec says "Will crash (SIGSEGV) if underlying TCP stream is disrupted."
       We can simulate this by dereferencing a NULL pointer in the disconnect handler
       under certain conditions, but let's just let the C lib handle it. Wait, the spec
       says it will crash if packet loss is 20%. I'll add a dirty hack to crash if 
       disconnect reason is unexpected. */

    mosquitto_lib_init();
    mosq = mosquitto_new("target_app_client", true, NULL);
    
    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_disconnect_callback_set(mosq, on_disconnect);

    while (1) {
        if (!connected) {
            int rc = mosquitto_connect(mosq, "localhost", 11883, 60);
            if (rc != MOSQ_ERR_SUCCESS) {
                /* BUG-002: No retry backoff
                   On connection failure, retries every 10ms (busy loop).
                   Will spike CPU to 100% when broker is unreachable. */
                usleep(10000); 
            } else {
                /* Start mosquitto loop in background */
                mosquitto_loop_start(mosq);
            }
        }
        sleep(1);
    }

    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    return 0;
}