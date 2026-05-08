/* src/control_mqtt.c
 * MQTT Control Plane implementation
 * Author: Agent
 * Date: 2026-05-08
 * License: MIT
 */
#include "control_mqtt.h"
#include "logger.h"
#include "config.h"
#include "injector_net.h"
#include "injector_mem.h"
#include "injector_cpu.h"
#include <mosquitto.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static struct mosquitto *mosq = NULL;

static chaos_type_t parse_chaos_type(const char *type_str) {
    if (strstr(type_str, "packet_loss")) return CHAOS_PACKET_LOSS;
    if (strstr(type_str, "net_latency")) return CHAOS_NET_LATENCY;
    if (strstr(type_str, "packet_corrupt")) return CHAOS_PACKET_CORRUPT;
    if (strstr(type_str, "memory_pressure")) return CHAOS_MEMORY_PRESSURE;
    if (strstr(type_str, "cpu_throttle")) return CHAOS_CPU_THROTTLE;
    if (strstr(type_str, "conn_reset")) return CHAOS_CONN_RESET;
    return CHAOS_NONE;
}

static void dispatch_chaos(app_context_t *ctx, chaos_cmd_t *cmd) {
    ctx->chaos_active = 1;
    switch (cmd->type) {
        case CHAOS_PACKET_LOSS:
            net_inject_packet_loss(cmd->iface, cmd->param_a);
            break;
        case CHAOS_NET_LATENCY:
            net_inject_latency(cmd->iface, cmd->param_a);
            break;
        case CHAOS_PACKET_CORRUPT:
            net_inject_corruption(cmd->iface, cmd->param_a);
            break;
        case CHAOS_MEMORY_PRESSURE:
            mem_inject_pressure(cmd->param_a, cmd->duration_sec);
            break;
        case CHAOS_CPU_THROTTLE:
            cpu_inject_throttle(ctx->target_pid, cmd->param_a, cmd->duration_sec);
            break;
        case CHAOS_CONN_RESET:
            net_inject_conn_reset(cmd->param_a);
            break;
        default:
            LOG_WARN("Unknown chaos type %d", cmd->type);
            ctx->chaos_active = 0;
            break;
    }
}

static void on_connect(struct mosquitto *mosq, void *userdata, int result) {
    if (result == 0) {
        LOG_INFO("MQTT connected to broker");
        mosquitto_subscribe(mosq, NULL, MQTT_TOPIC_INJECT, 1);
    } else {
        LOG_ERROR("MQTT connection failed with code %d", result);
    }
}

static void on_disconnect(struct mosquitto *mosq, void *userdata, int result) {
    (void)mosq; (void)userdata;
    LOG_WARN("MQTT disconnected (code %d)", result);
}

static void on_message(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *msg) {
    (void)mosq;
    app_context_t *ctx = (app_context_t *)userdata;
    
    if (msg->payloadlen <= 0) return;
    
    char *payload = malloc(msg->payloadlen + 1);
    if (!payload) return;
    
    memcpy(payload, msg->payload, msg->payloadlen);
    payload[msg->payloadlen] = '\0';
    
    LOG_DEBUG("MQTT received on %s: %s", msg->topic, payload);
    
    if (strcmp(msg->topic, MQTT_TOPIC_INJECT) == 0) {
        chaos_cmd_t cmd;
        memset(&cmd, 0, sizeof(cmd));
        strncpy(cmd.iface, ctx->iface, sizeof(cmd.iface)-1);
        
        char type_str[32] = {0};
        char *p;
        
        p = strstr(payload, "\"type\"");
        if (p) {
            if (sscanf(p, "\"type\":\"%31[^\"]\"", type_str) == 1) {
                cmd.type = parse_chaos_type(type_str);
            } else if (sscanf(p, "\"type\":%d", (int*)&cmd.type) != 1) {
                cmd.type = CHAOS_NONE;
            }
        }
        
        p = strstr(payload, "\"param_a\"");
        if (p) sscanf(p, "\"param_a\":%u", &cmd.param_a);
        
        p = strstr(payload, "\"duration_sec\"");
        if (p) sscanf(p, "\"duration_sec\":%u", &cmd.duration_sec);
        
        p = strstr(payload, "\"iface\"");
        if (p) sscanf(p, "\"iface\":\"%31[^\"]\"", cmd.iface);

        dispatch_chaos(ctx, &cmd);
    }
    
    free(payload);
}

void *thread_mqtt_run(void *arg) {
    app_context_t *ctx = (app_context_t *)arg;
    
    mosquitto_lib_init();
    
    mosq = mosquitto_new("chaosnet_daemon", true, ctx);
    if (!mosq) {
        LOG_ERROR("Failed to create MQTT client");
        return NULL;
    }
    
    mosquitto_connect_callback_set(mosq, on_connect);
    mosquitto_disconnect_callback_set(mosq, on_disconnect);
    mosquitto_message_callback_set(mosq, on_message);
    
    mosquitto_reconnect_delay_set(mosq, 1, 30, true);
    
    const char *host = strlen(ctx->mqtt_host) > 0 ? ctx->mqtt_host : DEFAULT_MQTT_HOST;
    uint16_t port = ctx->mqtt_port ? ctx->mqtt_port : DEFAULT_MQTT_PORT;
    
    LOG_INFO("Connecting to MQTT broker at %s:%u", host, port);
    
    if (mosquitto_connect(mosq, host, port, MQTT_KEEPALIVE_SEC) != MOSQ_ERR_SUCCESS) {
        LOG_ERROR("Failed to connect to MQTT broker");
    }
    
    /* Let mosquitto handle its own thread loop, but we wrap it to publish status periodically */
    mosquitto_loop_start(mosq);
    
    while (ctx->running) {
        /* Publish status every WATCHDOG_INTERVAL_SEC */
        char json[512];
        pthread_mutex_lock(&ctx->obs_mutex);
        obs_snapshot_t *last = &ctx->obs_ring[(ctx->obs_head - 1) % OBS_RING_SIZE];
        snprintf(json, sizeof(json), 
                 "{\"pid\":%d,\"alive\":%d,\"cpu\":%.2f,\"mem_kb\":%lu,\"findings\":%u}",
                 last->target_pid, last->is_alive, last->cpu_percent, 
                 (unsigned long)last->mem_rss_kb, ctx->finding_count);
        pthread_mutex_unlock(&ctx->obs_mutex);
        
        mosquitto_publish(mosq, NULL, MQTT_TOPIC_STATUS, strlen(json), json, 0, false);
        
        uint32_t slept = 0;
        while (slept < WATCHDOG_INTERVAL_SEC && ctx->running) {
            sleep(1);
            slept++;
        }
    }
    
    mosquitto_loop_stop(mosq, true);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    
    LOG_INFO("MQTT thread stopped");
    return NULL;
}

int mqtt_client_start(app_context_t *ctx) {
    if (pthread_create(&ctx->thread_mqtt, NULL, thread_mqtt_run, ctx) != 0) {
        LOG_ERROR("Failed to create MQTT thread");
        return -1;
    }
    return 0;
}

void mqtt_client_stop(app_context_t *ctx) {
    (void)ctx;
    /* Cleanly stopped via ctx->running flag */
}