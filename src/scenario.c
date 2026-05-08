/* src/scenario.c
 * Scenario Engine implementation
 * Author: Agent
 * Date: 2026-05-08
 * License: MIT
 */
#include "scenario.h"
#include "logger.h"
#include "injector_net.h"
#include "injector_mem.h"
#include "injector_cpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

static volatile int scenario_cancel = 0;

static chaos_type_t parse_chaos_type(const char *type_str) {
    if (strstr(type_str, "packet_loss")) return CHAOS_PACKET_LOSS;
    if (strstr(type_str, "net_latency")) return CHAOS_NET_LATENCY;
    if (strstr(type_str, "packet_corrupt")) return CHAOS_PACKET_CORRUPT;
    if (strstr(type_str, "memory_pressure")) return CHAOS_MEMORY_PRESSURE;
    if (strstr(type_str, "cpu_throttle")) return CHAOS_CPU_THROTTLE;
    if (strstr(type_str, "conn_reset")) return CHAOS_CONN_RESET;
    return CHAOS_NONE;
}

int scenario_parse(const char *json, scenario_t *out) {
    memset(out, 0, sizeof(*out));
    
    char *p = strstr((char*)json, "\"name\"");
    if (p) sscanf(p, "\"name\"%*[ \t:]\"%127[^\"]\"", out->name);
    
    p = strstr((char*)json, "\"total_duration_sec\"");
    if (p) sscanf(p, "\"total_duration_sec\"%*[ \t:]%u", &out->total_duration_sec);
    
    p = strstr((char*)json, "\"steps\"");
    if (!p) return -1;
    
    char *step_start = strchr(p, '{');
    while (step_start && out->step_count < SCENARIO_MAX_STEPS) {
        scenario_step_t *step = &out->steps[out->step_count];
        
        char *step_end = strchr(step_start, '}');
        if (!step_end) break;
        
        char type_str[32] = {0};
        
        char *field = strstr(step_start, "\"at_sec\"");
        if (field && field < step_end) sscanf(field, "\"at_sec\"%*[ \t:]%u", &step->at_sec);
        
        field = strstr(step_start, "\"type\"");
        if (field && field < step_end) {
            if (sscanf(field, "\"type\"%*[ \t:]\"%31[^\"]\"", type_str) == 1) {
                step->cmd.type = parse_chaos_type(type_str);
            } else if (sscanf(field, "\"type\"%*[ \t:]%d", (int*)&step->cmd.type) != 1) {
                step->cmd.type = CHAOS_NONE;
            }
        }
        
        field = strstr(step_start, "\"param_a\"");
        if (field && field < step_end) sscanf(field, "\"param_a\"%*[ \t:]%u", &step->cmd.param_a);
        
        field = strstr(step_start, "\"duration_sec\"");
        if (field && field < step_end) sscanf(field, "\"duration_sec\"%*[ \t:]%u", &step->cmd.duration_sec);
        
        field = strstr(step_start, "\"iface\"");
        if (field && field < step_end) {
            sscanf(field, "\"iface\"%*[ \t:]\"%31[^\"]\"", step->cmd.iface);
        }
        
        out->step_count++;
        step_start = strchr(step_end, '{');
    }
    
    return (out->step_count > 0) ? 0 : -1;
}

int scenario_load_file(const char *filepath, scenario_t *out) {
    FILE *f = fopen(filepath, "r");
    if (!f) {
        LOG_ERROR("Failed to open scenario file: %s", filepath);
        return -1;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size <= 0) {
        fclose(f);
        return -1;
    }
    
    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        return -1;
    }
    
    size_t read_bytes = fread(buf, 1, size, f);
    buf[read_bytes] = '\0';
    fclose(f);
    
    int ret = scenario_parse(buf, out);
    free(buf);
    return ret;
}

static void dispatch_chaos_internal(app_context_t *ctx, chaos_cmd_t *cmd) {
    ctx->chaos_active = 1;
    /* Use default iface if empty */
    const char *iface = (strlen(cmd->iface) > 0) ? cmd->iface : ctx->iface;
    
    switch (cmd->type) {
        case CHAOS_PACKET_LOSS:
            net_inject_packet_loss(iface, cmd->param_a);
            break;
        case CHAOS_NET_LATENCY:
            net_inject_latency(iface, cmd->param_a);
            break;
        case CHAOS_PACKET_CORRUPT:
            net_inject_corruption(iface, cmd->param_a);
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

void *thread_scenario_run(void *arg) {
    app_context_t *ctx = (app_context_t *)arg;
    scenario_t s;

    pthread_mutex_lock(&ctx->scenario_mutex);
    s = ctx->active_scenario;  /* copy to local */
    pthread_mutex_unlock(&ctx->scenario_mutex);

    time_t start = time(NULL);
    LOG_INFO("Starting scenario '%s' with %u steps", s.name, s.step_count);

    for (uint32_t i = 0; i < s.step_count && ctx->running && !scenario_cancel; i++) {
        /* Sleep until step's at_sec */
        time_t now = time(NULL);
        int32_t wait = (int32_t)(start + s.steps[i].at_sec) - (int32_t)now;
        
        while (wait > 0 && ctx->running && !scenario_cancel) {
            sleep(1);
            now = time(NULL);
            wait = (int32_t)(start + s.steps[i].at_sec) - (int32_t)now;
        }

        if (!ctx->running || scenario_cancel) break;

        LOG_INFO("Scenario step %u: executing chaos type %d", i, s.steps[i].cmd.type);
        dispatch_chaos_internal(ctx, &s.steps[i].cmd);
    }

    if (scenario_cancel) {
        LOG_INFO("Scenario '%s' canceled", s.name);
    } else {
        LOG_INFO("Scenario '%s' completed", s.name);
    }
    
    return NULL;
}

int scenario_start(app_context_t *ctx, const scenario_t *s) {
    pthread_mutex_lock(&ctx->scenario_mutex);
    ctx->active_scenario = *s;
    scenario_cancel = 0;
    pthread_mutex_unlock(&ctx->scenario_mutex);
    
    if (pthread_create(&ctx->thread_scenario, NULL, thread_scenario_run, ctx) != 0) {
        LOG_ERROR("Failed to start scenario thread");
        return -1;
    }
    pthread_detach(ctx->thread_scenario);
    return 0;
}

void scenario_stop(app_context_t *ctx) {
    (void)ctx;
    scenario_cancel = 1;
}