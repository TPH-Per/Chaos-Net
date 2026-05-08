/* src/control_http.c
 * HTTP Control Plane implementation
 * Author: Agent
 * Date: 2026-05-08
 * License: MIT
 */
#include "control_http.h"
#include "logger.h"
#include "config.h"
#include "injector_net.h"
#include "injector_mem.h"
#include "injector_cpu.h"
#include "scenario.h"
#include "report.h"
#include <microhttpd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct connection_info_struct {
    char *data;
    size_t data_size;
};

static int send_json_response(struct MHD_Connection *conn, unsigned int status_code, const char *json_body) {
    struct MHD_Response *response = MHD_create_response_from_buffer(
        strlen(json_body), (void *)json_body, MHD_RESPMEM_MUST_COPY);
    
    if (!response) return MHD_NO;
    
    MHD_add_response_header(response, "Content-Type", "application/json");
    int ret = MHD_queue_response(conn, status_code, response);
    MHD_destroy_response(response);
    return ret;
}

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

static enum MHD_Result request_handler(
    void *cls,
    struct MHD_Connection *connection,
    const char *url,
    const char *method,
    const char *version,
    const char *upload_data,
    size_t *upload_data_size,
    void **con_cls)
{
    (void)version;
    app_context_t *ctx = (app_context_t *)cls;
    
    if (NULL == *con_cls) {
        struct connection_info_struct *con_info;
        con_info = malloc(sizeof(struct connection_info_struct));
        if (NULL == con_info) return MHD_NO;
        con_info->data = NULL;
        con_info->data_size = 0;
        *con_cls = (void *)con_info;
        return MHD_YES;
    }

    struct connection_info_struct *con_info = *con_cls;

    if (*upload_data_size != 0) {
        char *new_data = realloc(con_info->data, con_info->data_size + *upload_data_size + 1);
        if (!new_data) return MHD_NO;
        con_info->data = new_data;
        memcpy(con_info->data + con_info->data_size, upload_data, *upload_data_size);
        con_info->data_size += *upload_data_size;
        con_info->data[con_info->data_size] = '\0';
        *upload_data_size = 0;
        return MHD_YES;
    }

    if (strcmp(method, "GET") == 0 && strcmp(url, "/status") == 0) {
        char json[512];
        pthread_mutex_lock(&ctx->obs_mutex);
        obs_snapshot_t *last = &ctx->obs_ring[(ctx->obs_head - 1) % OBS_RING_SIZE];
        snprintf(json, sizeof(json), 
                 "{\"pid\":%d,\"alive\":%d,\"cpu\":%.2f,\"mem_kb\":%lu,\"findings\":%u}",
                 last->target_pid, last->is_alive, last->cpu_percent, 
                 (unsigned long)last->mem_rss_kb, ctx->finding_count);
        pthread_mutex_unlock(&ctx->obs_mutex);
        return send_json_response(connection, MHD_HTTP_OK, json);
    }
    
    else if (strcmp(method, "GET") == 0 && strcmp(url, "/findings") == 0) {
        char json[4096] = "[";
        pthread_mutex_lock(&ctx->findings_mutex);
        for (uint32_t i = 0; i < ctx->finding_count; i++) {
            char entry[512];
            snprintf(entry, sizeof(entry), "{\"severity\":%d,\"message\":\"%s\"}%s",
                     ctx->findings[i].severity, ctx->findings[i].message,
                     (i == ctx->finding_count - 1) ? "" : ",");
            strncat(json, entry, sizeof(json) - strlen(json) - 1);
        }
        pthread_mutex_unlock(&ctx->findings_mutex);
        strncat(json, "]", sizeof(json) - strlen(json) - 1);
        return send_json_response(connection, MHD_HTTP_OK, json);
    }
    
    else if (strcmp(method, "POST") == 0 && strcmp(url, "/chaos") == 0) {
        if (!con_info->data) {
            return send_json_response(connection, MHD_HTTP_BAD_REQUEST, "{\"error\":\"Empty body\"}");
        }
        
        chaos_cmd_t cmd;
        memset(&cmd, 0, sizeof(cmd));
        strncpy(cmd.iface, ctx->iface, sizeof(cmd.iface)-1); /* default to configured iface */
        
        /* Basic JSON parsing */
        char type_str[32] = {0};
        char *p;
        
        p = strstr(con_info->data, "\"type\"");
        if (p) {
            /* Support both string types and integer types */
            if (sscanf(p, "\"type\":\"%31[^\"]\"", type_str) == 1) {
                cmd.type = parse_chaos_type(type_str);
            } else if (sscanf(p, "\"type\":%d", (int*)&cmd.type) != 1) {
                cmd.type = CHAOS_NONE;
            }
        }
        
        p = strstr(con_info->data, "\"param_a\"");
        if (p) sscanf(p, "\"param_a\":%u", &cmd.param_a);
        
        p = strstr(con_info->data, "\"duration_sec\"");
        if (p) sscanf(p, "\"duration_sec\":%u", &cmd.duration_sec);
        
        p = strstr(con_info->data, "\"iface\"");
        if (p) sscanf(p, "\"iface\":\"%31[^\"]\"", cmd.iface);

        dispatch_chaos(ctx, &cmd);
        return send_json_response(connection, MHD_HTTP_OK, "{\"status\":\"ok\"}");
    }
    
    else if (strcmp(method, "POST") == 0 && strcmp(url, "/scenario") == 0) {
        if (!con_info->data) {
            return send_json_response(connection, MHD_HTTP_BAD_REQUEST, "{\"error\":\"Empty body\"}");
        }
        scenario_t s;
        if (scenario_parse(con_info->data, &s) == 0) {
            scenario_start(ctx, &s);
            return send_json_response(connection, MHD_HTTP_OK, "{\"status\":\"started\"}");
        }
        return send_json_response(connection, MHD_HTTP_BAD_REQUEST, "{\"error\":\"Parse failed\"}");
    }
    
    else if (strcmp(method, "GET") == 0 && strcmp(url, "/report") == 0) {
        resilience_report_t r;
        report_generate(ctx, &r);
        char *json = report_to_json(&r, ctx);
        if (json) {
            int ret = send_json_response(connection, MHD_HTTP_OK, json);
            free(json);
            return ret;
        }
        return send_json_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, "{\"error\":\"Report failed\"}");
    }
    
    else if (strcmp(method, "DELETE") == 0 && strcmp(url, "/chaos") == 0) {
        net_inject_clear(ctx->iface);
        cpu_inject_clear(ctx->target_pid);
        mem_inject_clear();
        ctx->chaos_active = 0;
        return send_json_response(connection, MHD_HTTP_OK, "{\"status\":\"cleared\"}");
    }

    return send_json_response(connection, MHD_HTTP_NOT_FOUND, "{\"error\":\"Not found\"}");
}

static void request_completed(
    void *cls,
    struct MHD_Connection *connection,
    void **con_cls,
    enum MHD_RequestTerminationCode toe)
{
    (void)cls; (void)connection; (void)toe;
    struct connection_info_struct *con_info = *con_cls;
    if (con_info) {
        if (con_info->data) free(con_info->data);
        free(con_info);
    }
}

static struct MHD_Daemon *daemon_handle = NULL;

void *thread_http_run(void *arg) {
    app_context_t *ctx = (app_context_t *)arg;
    uint16_t port = ctx->http_port ? ctx->http_port : DEFAULT_HTTP_PORT;
    
    daemon_handle = MHD_start_daemon(
        MHD_USE_INTERNAL_POLLING_THREAD, 
        port, 
        NULL, NULL, 
        &request_handler, ctx, 
        MHD_OPTION_NOTIFY_COMPLETED, request_completed, NULL,
        MHD_OPTION_END);
        
    if (!daemon_handle) {
        LOG_ERROR("Failed to start HTTP server on port %u", port);
        return NULL;
    }
    
    LOG_INFO("HTTP server started on port %u", port);
    
    while (ctx->running) {
        sleep(1);
    }
    
    return NULL;
}

int http_server_start(app_context_t *ctx) {
    if (pthread_create(&ctx->thread_http, NULL, thread_http_run, ctx) != 0) {
        LOG_ERROR("Failed to create HTTP thread");
        return -1;
    }
    return 0;
}

void http_server_stop(app_context_t *ctx) {
    (void)ctx;
    if (daemon_handle) {
        MHD_stop_daemon(daemon_handle);
        daemon_handle = NULL;
        LOG_INFO("HTTP server stopped");
    }
}