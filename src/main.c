/* src/main.c
 * ChaosNet main entry point and watchdog
 * Author: Agent
 * Date: 2026-05-08
 * License: MIT
 */
#include "chaosnet.h"
#include "logger.h"
#include "config.h"
#include "observer.h"
#include "control_http.h"
#include "control_mqtt.h"
#include "scenario.h"
#include "report.h"
#include "injector_net.h"
#include "injector_cpu.h"
#include "injector_mem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

static volatile sig_atomic_t g_signal_received = 0;
static app_context_t *g_ctx_ptr = NULL;

static void signal_handler(int sig) {
    g_signal_received = sig;
    if (g_ctx_ptr) g_ctx_ptr->running = 0;
}

void *thread_watchdog_run(void *arg) {
    app_context_t *ctx = (app_context_t *)arg;
    LOG_INFO("Watchdog thread started");

    while (ctx->running) {
        uint32_t slept = 0;
        while (slept < WATCHDOG_INTERVAL_SEC && ctx->running) {
            sleep(1);
            slept++;
        }
        
        if (!ctx->running) break;

        /* Check if target process exists */
        if (kill(ctx->target_pid, 0) != 0) {
            LOG_WARN("Target process PID %d is gone", ctx->target_pid);
        }

        /* Basic thread alive checks (in a real system, we'd use heartbeats) */
        if (pthread_kill(ctx->thread_observer, 0) != 0) {
            LOG_ERROR("Observer thread died unexpectedly");
            observer_start(ctx);
        }
        if (pthread_kill(ctx->thread_http, 0) != 0) {
            LOG_ERROR("HTTP thread died unexpectedly");
            http_server_start(ctx);
        }
        if (pthread_kill(ctx->thread_mqtt, 0) != 0) {
            LOG_ERROR("MQTT thread died unexpectedly");
            mqtt_client_start(ctx);
        }
    }

    LOG_INFO("Watchdog thread stopped");
    return NULL;
}

static void print_usage(const char *prog_name) {
    printf("Usage: %s --pid <pid> [options]\n", prog_name);
    printf("Options:\n");
    printf("  --pid <pid>           Target process PID (required)\n");
    printf("  --iface <name>        Network interface (default: eth0)\n");
    printf("  --http-port <port>    HTTP port (default: 8080)\n");
    printf("  --mqtt-host <host>    MQTT broker host (default: localhost)\n");
    printf("  --mqtt-port <port>    MQTT broker port (default: 1883)\n");
    printf("  --scenario <file>     Load and run scenario file immediately\n");
    printf("  --log-level <0-3>     Log verbosity (0=DEBUG, 1=INFO, 2=WARN, 3=ERROR)\n");
    printf("  --log-file <path>     Log file path\n");
    printf("  --duration <sec>      Auto-stop after N seconds (0=run forever)\n");
    printf("  --no-inject           Do not start injectors (dry run)\n");
}

int main(int argc, char **argv) {
    app_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    
    int log_level = LOG_LEVEL_INFO;
    char log_file[256] = {0};
    char scenario_file[256] = {0};
    uint32_t duration = 0;
    
    strncpy(ctx.iface, DEFAULT_IFACE, sizeof(ctx.iface)-1);
    ctx.http_port = DEFAULT_HTTP_PORT;
    strncpy(ctx.mqtt_host, DEFAULT_MQTT_HOST, sizeof(ctx.mqtt_host)-1);
    ctx.mqtt_port = DEFAULT_MQTT_PORT;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--pid") == 0 && i + 1 < argc) {
            ctx.target_pid = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--iface") == 0 && i + 1 < argc) {
            strncpy(ctx.iface, argv[++i], sizeof(ctx.iface)-1);
        } else if (strcmp(argv[i], "--http-port") == 0 && i + 1 < argc) {
            ctx.http_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--mqtt-host") == 0 && i + 1 < argc) {
            strncpy(ctx.mqtt_host, argv[++i], sizeof(ctx.mqtt_host)-1);
        } else if (strcmp(argv[i], "--mqtt-port") == 0 && i + 1 < argc) {
            ctx.mqtt_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--scenario") == 0 && i + 1 < argc) {
            strncpy(scenario_file, argv[++i], sizeof(scenario_file)-1);
        } else if (strcmp(argv[i], "--log-level") == 0 && i + 1 < argc) {
            log_level = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--log-file") == 0 && i + 1 < argc) {
            strncpy(log_file, argv[++i], sizeof(log_file)-1);
        } else if (strcmp(argv[i], "--duration") == 0 && i + 1 < argc) {
            duration = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--no-inject") == 0) {
            /* ignore for now in tests */
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }
    
    if (ctx.target_pid == 0) {
        fprintf(stderr, "Error: --pid <pid> is required\n");
        print_usage(argv[0]);
        return 1;
    }
    
    log_init(strlen(log_file) > 0 ? log_file : NULL, log_level);
    LOG_INFO("ChaosNet starting. Target PID: %d", ctx.target_pid);
    
    pthread_mutex_init(&ctx.obs_mutex, NULL);
    pthread_mutex_init(&ctx.findings_mutex, NULL);
    pthread_mutex_init(&ctx.scenario_mutex, NULL);
    
    g_ctx_ptr = &ctx;
    ctx.running = 1;
    
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    
    if (observer_start(&ctx) != 0) goto cleanup;
    if (http_server_start(&ctx) != 0) goto cleanup;
    if (mqtt_client_start(&ctx) != 0) goto cleanup;
    
    if (pthread_create(&ctx.thread_watchdog, NULL, thread_watchdog_run, &ctx) != 0) {
        LOG_ERROR("Failed to start watchdog thread");
        goto cleanup;
    }
    
    if (strlen(scenario_file) > 0) {
        scenario_t s;
        if (scenario_load_file(scenario_file, &s) == 0) {
            scenario_start(&ctx, &s);
        } else {
            LOG_ERROR("Failed to load scenario file: %s", scenario_file);
        }
    }
    
    time_t start_time = time(NULL);
    
    while (ctx.running) {
        sleep(1);
        if (duration > 0 && (time(NULL) - start_time) >= duration) {
            LOG_INFO("Duration %u seconds reached, stopping", duration);
            ctx.running = 0;
        }
    }
    
cleanup:
    LOG_INFO("ChaosNet shutting down...");
    
    net_inject_clear(ctx.iface);
    cpu_inject_clear(ctx.target_pid);
    mem_inject_clear();
    
    scenario_stop(&ctx);
    http_server_stop(&ctx);
    mqtt_client_stop(&ctx);
    
    /* Wait for threads */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += THREAD_JOIN_TIMEOUT_SEC;
    
    pthread_timedjoin_np(ctx.thread_observer, NULL, &ts);
    pthread_timedjoin_np(ctx.thread_http, NULL, &ts);
    pthread_timedjoin_np(ctx.thread_mqtt, NULL, &ts);
    pthread_timedjoin_np(ctx.thread_watchdog, NULL, &ts);
    
    if (duration > 0 || g_signal_received == SIGINT) {
        resilience_report_t r;
        report_generate(&ctx, &r);
        report_print(&r, &ctx);
    }
    
    pthread_mutex_destroy(&ctx.obs_mutex);
    pthread_mutex_destroy(&ctx.findings_mutex);
    pthread_mutex_destroy(&ctx.scenario_mutex);
    
    log_close();
    return 0;
}