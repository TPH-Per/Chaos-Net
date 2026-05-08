/* include/observer.h
 * Observer module for ChaosNet
 * Author: Agent
 * Date: 2026-05-08
 * License: MIT
 */
#ifndef OBSERVER_H
#define OBSERVER_H

#include "chaosnet.h"

/* Start observer thread. ctx->target_pid must be set. */
int  observer_start(app_context_t *ctx);

/* Stop observer thread gracefully */
void observer_stop(app_context_t *ctx);

/* Read one snapshot synchronously (used internally & in tests) */
int  observer_read_snapshot(pid_t pid, obs_snapshot_t *out);

/* Analyze ring buffer and populate ctx->findings */
void observer_analyze(app_context_t *ctx);

/* Thread entry point (exposed for testing) */
void *thread_observer_run(void *arg);

#endif /* OBSERVER_H */