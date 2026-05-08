/* include/control_mqtt.h
 * MQTT Control Plane module
 * Author: Agent
 * Date: 2026-05-08
 * License: MIT
 */
#ifndef CONTROL_MQTT_H
#define CONTROL_MQTT_H

#include "chaosnet.h"

int  mqtt_client_start(app_context_t *ctx);
void mqtt_client_stop(app_context_t *ctx);

void *thread_mqtt_run(void *arg);

#endif /* CONTROL_MQTT_H */