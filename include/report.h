/* include/report.h
 * Report Generator module
 * Author: Agent
 * Date: 2026-05-08
 * License: MIT
 */
#ifndef REPORT_H
#define REPORT_H

#include "chaosnet.h"

typedef struct {
    uint32_t   total_score;          /* 0-100 */
    uint32_t   score_memory;         /* 0-40 */
    uint32_t   score_cpu;            /* 0-30 */
    uint32_t   score_network;        /* 0-30 */
    uint32_t   critical_count;
    uint32_t   warning_count;
    uint32_t   pass_count;
    char       summary[512];
} resilience_report_t;

/* Generate report from ctx findings + obs ring */
int report_generate(app_context_t *ctx, resilience_report_t *out);

/* Print report to stdout in human-readable format */
void report_print(const resilience_report_t *r, const app_context_t *ctx);

/* Serialize report to JSON string (caller must free) */
char *report_to_json(const resilience_report_t *r, const app_context_t *ctx);

#endif /* REPORT_H */