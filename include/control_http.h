/* include/control_http.h
 * HTTP Control Plane module
 * Author: Agent
 * Date: 2026-05-08
 * License: MIT
 */
#ifndef CONTROL_HTTP_H
#define CONTROL_HTTP_H

#include "chaosnet.h"

int  http_server_start(app_context_t *ctx);
void http_server_stop(app_context_t *ctx);

/* Thread entry point */
void *thread_http_run(void *arg);

#endif /* CONTROL_HTTP_H */