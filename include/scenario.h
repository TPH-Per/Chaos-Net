/* include/scenario.h
 * Scenario Engine module
 * Author: Agent
 * Date: 2026-05-08
 * License: MIT
 */
#ifndef SCENARIO_H
#define SCENARIO_H

#include "chaosnet.h"

/* Parse scenario from JSON string. Returns 0 on success. */
int scenario_parse(const char *json, scenario_t *out);

/* Load scenario from file */
int scenario_load_file(const char *filepath, scenario_t *out);

/* Start executing a scenario (spawns internal thread) */
int scenario_start(app_context_t *ctx, const scenario_t *s);

/* Stop current scenario mid-run */
void scenario_stop(app_context_t *ctx);

void *thread_scenario_run(void *arg);

#endif /* SCENARIO_H */