/* include/chaosnet.h
 * Master typedefs and structs for ChaosNet
 * Author: Agent
 * Date: 2026-05-08
 * License: MIT
 */

#ifndef CHAOSNET_H
#define CHAOSNET_H

#include <pthread.h>
#include <stdint.h>
#include <time.h>
#include <sys/types.h>

/* ── Chaos type identifiers ── */
typedef enum {
    CHAOS_NONE           = 0,
    CHAOS_PACKET_LOSS    = 1,
    CHAOS_NET_LATENCY    = 2,
    CHAOS_PACKET_CORRUPT = 3,
    CHAOS_MEMORY_PRESSURE= 4,
    CHAOS_CPU_THROTTLE   = 5,
    CHAOS_CONN_RESET     = 6
} chaos_type_t;

/* ── Single chaos injection command ── */
typedef struct {
    chaos_type_t type;
    uint32_t     param_a;      /* meaning depends on type:
                                  PACKET_LOSS    → loss percent (0-100)
                                  NET_LATENCY    → delay ms
                                  MEMORY_PRESSURE→ MB to consume
                                  CPU_THROTTLE   → CPU% limit (0-100)
                                  CONN_RESET     → target port */
    uint32_t     duration_sec; /* how long to apply this chaos */
    char         iface[32];    /* network interface, e.g. "eth0" */
} chaos_cmd_t;

/* ── Observation snapshot (taken every OBSERVE_INTERVAL_MS) ── */
typedef struct {
    time_t   timestamp;
    pid_t    target_pid;
    float    cpu_percent;
    uint64_t mem_rss_kb;
    uint32_t open_fds;
    uint32_t tcp_established;
    uint32_t tcp_close_wait;
    uint32_t voluntary_ctxsw;   /* from /proc/<pid>/status */
    uint32_t nonvoluntary_ctxsw;
    int      is_alive;          /* 1 = process exists */
} obs_snapshot_t;

/* ── Scenario step ── */
typedef struct {
    uint32_t   at_sec;          /* seconds from scenario start */
    chaos_cmd_t cmd;            /* which chaos to inject at this time */
} scenario_step_t;

/* ── Scenario definition ── */
#define SCENARIO_MAX_STEPS 64
#define SCENARIO_NAME_LEN  128

typedef struct {
    char             name[SCENARIO_NAME_LEN];
    uint32_t         total_duration_sec;
    uint32_t         step_count;
    scenario_step_t  steps[SCENARIO_MAX_STEPS];
} scenario_t;

/* ── Findings from the observer ── */
typedef enum {
    FIND_SEVERITY_PASS     = 0,
    FIND_SEVERITY_WARNING  = 1,
    FIND_SEVERITY_CRITICAL = 2
} finding_severity_t;

#define FINDING_MSG_LEN 256
#define MAX_FINDINGS    64

typedef struct {
    finding_severity_t severity;
    char               message[FINDING_MSG_LEN];
} finding_t;

/* ── Global application context (the only allowed global) ── */
typedef struct {
    /* Config */
    pid_t            target_pid;
    char             iface[32];
    uint16_t         http_port;
    char             mqtt_host[128];
    uint16_t         mqtt_port;

    /* State */
    volatile int     running;       /* set to 0 to stop all threads */
    volatile int     chaos_active;

    /* Observation history ring buffer */
    obs_snapshot_t   obs_ring[1024];
    uint32_t         obs_head;
    pthread_mutex_t  obs_mutex;

    /* Findings */
    finding_t        findings[MAX_FINDINGS];
    uint32_t         finding_count;
    pthread_mutex_t  findings_mutex;

    /* Active scenario */
    scenario_t       active_scenario;
    pthread_mutex_t  scenario_mutex;

    /* Threads */
    pthread_t        thread_observer;
    pthread_t        thread_http;
    pthread_t        thread_mqtt;
    pthread_t        thread_scenario;
    pthread_t        thread_watchdog;
} app_context_t;

#endif /* CHAOSNET_H */