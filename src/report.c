/* src/report.c
 * Report Generator implementation
 * Author: Agent
 * Date: 2026-05-08
 * License: MIT
 */
#include "report.h"
#include "logger.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int report_generate(app_context_t *ctx, resilience_report_t *out) {
    memset(out, 0, sizeof(*out));
    
    out->score_memory = SCORE_WEIGHT_MEMORY;
    out->score_cpu = SCORE_WEIGHT_CPU;
    out->score_network = SCORE_WEIGHT_NETWORK;

    pthread_mutex_lock(&ctx->findings_mutex);
    
    for (uint32_t i = 0; i < ctx->finding_count; i++) {
        finding_t *f = &ctx->findings[i];
        
        if (f->severity == FIND_SEVERITY_CRITICAL) out->critical_count++;
        else if (f->severity == FIND_SEVERITY_WARNING) out->warning_count++;
        else if (f->severity == FIND_SEVERITY_PASS) out->pass_count++;
        
        if (strstr(f->message, "Memory Leak Detected") != NULL) {
            if (out->score_memory >= 20) out->score_memory -= 20; else out->score_memory = 0;
        } else if (strstr(f->message, "FD Leak Detected") != NULL) {
            if (out->score_memory >= 10) out->score_memory -= 10; else out->score_memory = 0;
        } else if (strstr(f->message, "CPU Spike Detected") != NULL) {
            if (out->score_cpu >= 15) out->score_cpu -= 15; else out->score_cpu = 0;
        } else if (strstr(f->message, "Process crashed") != NULL) {
            if (out->score_network >= 15) out->score_network -= 15; else out->score_network = 0;
        } else if (strstr(f->message, "Possible socket leak") != NULL) {
            if (out->score_network >= 10) out->score_network -= 10; else out->score_network = 0;
        }
    }
    
    pthread_mutex_unlock(&ctx->findings_mutex);
    
    out->total_score = out->score_memory + out->score_cpu + out->score_network;
    
    if (out->total_score >= 90) {
        strncpy(out->summary, "Excellent", sizeof(out->summary)-1);
    } else if (out->total_score >= 70) {
        strncpy(out->summary, "Good", sizeof(out->summary)-1);
    } else if (out->total_score >= 50) {
        strncpy(out->summary, "Needs Work", sizeof(out->summary)-1);
    } else {
        strncpy(out->summary, "Critical", sizeof(out->summary)-1);
    }
    
    return 0;
}

static void print_bar(char *buf, size_t size, uint32_t score, uint32_t max_score) {
    int max_blocks = 20;
    int filled = (int)((float)score / max_score * max_blocks);
    
    char *p = buf;
    for (int i = 0; i < max_blocks && (size_t)(p - buf) < size - 4; i++) {
        if (i < filled) {
            strcpy(p, "#"); // using '#' instead of unicode block for compatibility
            p++;
        } else {
            strcpy(p, "-");
            p++;
        }
    }
    *p = '\0';
}

void report_print(const resilience_report_t *r, const app_context_t *ctx) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
    
    char mem_bar[64], cpu_bar[64], net_bar[64];
    print_bar(mem_bar, sizeof(mem_bar), r->score_memory, SCORE_WEIGHT_MEMORY);
    print_bar(cpu_bar, sizeof(cpu_bar), r->score_cpu, SCORE_WEIGHT_CPU);
    print_bar(net_bar, sizeof(net_bar), r->score_network, SCORE_WEIGHT_NETWORK);

    const char *scenario_name = "None";
    pthread_mutex_lock((pthread_mutex_t*)&ctx->scenario_mutex);
    if (strlen(ctx->active_scenario.name) > 0) {
        scenario_name = ctx->active_scenario.name;
    }
    pthread_mutex_unlock((pthread_mutex_t*)&ctx->scenario_mutex);

    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  CHAOSNET RESILIENCE REPORT\n");
    printf("  Target PID : %d   Scenario: %s\n", ctx->target_pid, scenario_name);
    printf("  Generated  : %s\n", time_buf);
    printf("═══════════════════════════════════════════════════════════\n\n");
    
    printf("  MEMORY SCORE   : %2u / %u  %s\n", r->score_memory, SCORE_WEIGHT_MEMORY, mem_bar);
    printf("  CPU SCORE      : %2u / %u  %s\n", r->score_cpu, SCORE_WEIGHT_CPU, cpu_bar);
    printf("  NETWORK SCORE  : %2u / %u  %s\n\n", r->score_network, SCORE_WEIGHT_NETWORK, net_bar);
    
    printf("  TOTAL          : %2u / 100  [ %s ]\n\n", r->total_score, r->summary);
    
    printf("───────────────────────────────────────────────────────────\n");
    printf("  FINDINGS (%u total)\n", ctx->finding_count);
    printf("───────────────────────────────────────────────────────────\n");
    
    pthread_mutex_lock((pthread_mutex_t*)&ctx->findings_mutex);
    for (uint32_t i = 0; i < ctx->finding_count; i++) {
        const char *sev_str = "INFO";
        if (ctx->findings[i].severity == FIND_SEVERITY_CRITICAL) sev_str = "CRITICAL";
        else if (ctx->findings[i].severity == FIND_SEVERITY_WARNING) sev_str = "WARNING";
        else if (ctx->findings[i].severity == FIND_SEVERITY_PASS) sev_str = "PASS";
        
        printf("  [%s] %s\n", sev_str, ctx->findings[i].message);
    }
    pthread_mutex_unlock((pthread_mutex_t*)&ctx->findings_mutex);
    
    printf("\n═══════════════════════════════════════════════════════════\n\n");
}

char *report_to_json(const resilience_report_t *r, const app_context_t *ctx) {
    size_t size = 4096 + (ctx->finding_count * 256);
    char *json = malloc(size);
    if (!json) return NULL;
    
    const char *scenario_name = "None";
    pthread_mutex_lock((pthread_mutex_t*)&ctx->scenario_mutex);
    if (strlen(ctx->active_scenario.name) > 0) {
        scenario_name = ctx->active_scenario.name;
    }
    pthread_mutex_unlock((pthread_mutex_t*)&ctx->scenario_mutex);
    
    int len = snprintf(json, size,
        "{\n"
        "  \"target_pid\": %d,\n"
        "  \"scenario\": \"%s\",\n"
        "  \"total_score\": %u,\n"
        "  \"summary\": \"%s\",\n"
        "  \"scores\": {\n"
        "    \"memory\": %u,\n"
        "    \"cpu\": %u,\n"
        "    \"network\": %u\n"
        "  },\n"
        "  \"findings\": [\n",
        ctx->target_pid, scenario_name, r->total_score, r->summary,
        r->score_memory, r->score_cpu, r->score_network);
        
    pthread_mutex_lock((pthread_mutex_t*)&ctx->findings_mutex);
    for (uint32_t i = 0; i < ctx->finding_count; i++) {
        char entry[512];
        snprintf(entry, sizeof(entry), "    {\"severity\": %d, \"message\": \"%s\"}%s\n",
                 ctx->findings[i].severity, ctx->findings[i].message,
                 (i == ctx->finding_count - 1) ? "" : ",");
        strncat(json + len, entry, size - len - 1);
        len += strlen(entry);
    }
    pthread_mutex_unlock((pthread_mutex_t*)&ctx->findings_mutex);
    
    strncat(json + len, "  ]\n}\n", size - len - 1);
    
    return json;
}