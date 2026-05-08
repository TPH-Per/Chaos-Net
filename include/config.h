/* include/config.h
 * All constants / magic numbers for ChaosNet
 * Author: Agent
 * Date: 2026-05-08
 * License: MIT
 */
#ifndef CONFIG_H
#define CONFIG_H

/* Timing */
#define OBSERVE_INTERVAL_MS      500    /* observer polls every 500ms */
#define WATCHDOG_INTERVAL_SEC    5      /* watchdog checks every 5s */
#define HTTP_TIMEOUT_SEC         10
#define MQTT_KEEPALIVE_SEC       60
#define THREAD_JOIN_TIMEOUT_SEC  3

/* Defaults */
#define DEFAULT_HTTP_PORT        8080
#define DEFAULT_MQTT_PORT        1883
#define DEFAULT_MQTT_HOST        "localhost"
#define DEFAULT_IFACE            "eth0"

/* Limits */
#define OBS_RING_SIZE            1024
#define MAX_PACKET_LOSS_PCT      100
#define MAX_LATENCY_MS           10000
#define MAX_MEMORY_PRESSURE_MB   512
#define MIN_CPU_LIMIT_PCT        5

/* Log levels */
#define LOG_LEVEL_DEBUG          0
#define LOG_LEVEL_INFO           1
#define LOG_LEVEL_WARN           2
#define LOG_LEVEL_ERROR          3

/* MQTT topics */
#define MQTT_TOPIC_INJECT        "chaosnet/inject"
#define MQTT_TOPIC_STATUS        "chaosnet/status"
#define MQTT_TOPIC_REPORT        "chaosnet/report"

/* Proc paths */
#define PROC_STATUS_FMT          "/proc/%d/status"
#define PROC_STAT_FMT            "/proc/%d/stat"
#define PROC_FD_FMT              "/proc/%d/fd"
#define PROC_NET_TCP             "/proc/net/tcp"

/* cgroup paths (v2) */
#define CGROUP_BASE              "/sys/fs/cgroup/chaosnet"
#define CGROUP_CPU_MAX           "/sys/fs/cgroup/chaosnet/cpu.max"

/* Resilience scoring weights */
#define SCORE_WEIGHT_MEMORY      40
#define SCORE_WEIGHT_CPU         30
#define SCORE_WEIGHT_NETWORK     30

#endif /* CONFIG_H */