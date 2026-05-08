/* tests/test_scenario.c
 * Smoke test for Module 8
 * Author: Agent
 * Date: 2026-05-08
 * License: MIT
 */
#include "scenario.h"
#include <stdio.h>
#include <string.h>

const char *sample_json = "{"
  "\"name\": \"mqtt_gateway_stress\","
  "\"total_duration_sec\": 60,"
  "\"steps\": ["
    "{"
      "\"at_sec\": 0,"
      "\"type\": \"packet_loss\","
      "\"param_a\": 10,"
      "\"duration_sec\": 10,"
      "\"iface\": \"eth0\""
    "}"
  "]"
"}";

int main(void) {
    scenario_t s;
    if (scenario_parse(sample_json, &s) == 0) {
        if (strcmp(s.name, "mqtt_gateway_stress") == 0 && s.step_count == 1 &&
            s.steps[0].at_sec == 0 && s.steps[0].cmd.type == CHAOS_PACKET_LOSS &&
            s.steps[0].cmd.param_a == 10 && s.steps[0].cmd.duration_sec == 10 &&
            strcmp(s.steps[0].cmd.iface, "eth0") == 0) {
            printf("Scenario parsing test passed.\n");
            return 0;
        }
    }
    printf("Scenario parsing test failed.\n");
    return 1;
}