/* src/observer.c
 * Observer implementation
 * Author: Agent
 * Date: 2026-05-08
 * License: MIT
 */
#include "observer.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>

static unsigned long long last_utime = 0;
static unsigned long long last_stime = 0;
static struct timespec last_time = {0, 0};

static void add_finding(app_context_t *ctx, finding_severity_t sev, const char *msg) {
    pthread_mutex_lock(&ctx->findings_mutex);
    if (ctx->finding_count < MAX_FINDINGS) {
        ctx->findings[ctx->finding_count].severity = sev;
        strncpy(ctx->findings[ctx->finding_count].message, msg, FINDING_MSG_LEN - 1);
        ctx->findings[ctx->finding_count].message[FINDING_MSG_LEN - 1] = '\0';
        ctx->finding_count++;
    }
    pthread_mutex_unlock(&ctx->findings_mutex);
}

int observer_read_snapshot(pid_t pid, obs_snapshot_t *out) {
    char path[256];
    FILE *f;
    char line[256];

    memset(out, 0, sizeof(*out));
    out->target_pid = pid;
    out->timestamp = time(NULL);
    out->is_alive = (kill(pid, 0) == 0) ? 1 : 0;

    if (!out->is_alive) {
        return 0; /* process doesn't exist */
    }

    /* 1. Read /proc/<pid>/status */
    snprintf(path, sizeof(path), PROC_STATUS_FMT, pid);
    f = fopen(path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                sscanf(line, "VmRSS: %lu kB", &out->mem_rss_kb);
            } else if (strncmp(line, "voluntary_ctxt_switches:", 24) == 0) {
                sscanf(line, "voluntary_ctxt_switches: %u", &out->voluntary_ctxsw);
            } else if (strncmp(line, "nonvoluntary_ctxt_switches:", 27) == 0) {
                sscanf(line, "nonvoluntary_ctxt_switches: %u", &out->nonvoluntary_ctxsw);
            }
        }
        fclose(f);
    }

    /* 2. Read /proc/<pid>/stat */
    snprintf(path, sizeof(path), PROC_STAT_FMT, pid);
    f = fopen(path, "r");
    if (f) {
        if (fgets(line, sizeof(line), f)) {
            /* Fields: 1=pid, 2=comm, 3=state, ... 14=utime, 15=stime */
            char *p = strchr(line, ')'); /* skip comm */
            if (p) {
                p += 2; /* skip ") " */
                unsigned long long utime = 0, stime = 0;
                /* p points to state. We need to skip 11 fields to get to utime (14th field, but we start at 3rd) */
                /* 3 4 5 6 7 8 9 10 11 12 13 14 15 */
                sscanf(p, "%*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %llu %llu", &utime, &stime);

                struct timespec now;
                clock_gettime(CLOCK_MONOTONIC, &now);

                if (last_time.tv_sec != 0) {
                    double delta_t = (now.tv_sec - last_time.tv_sec) + 
                                     (now.tv_nsec - last_time.tv_nsec) / 1e9;
                    if (delta_t > 0) {
                        double delta_ticks = (utime - last_utime) + (stime - last_stime);
                        double ticks_per_sec = sysconf(_SC_CLK_TCK);
                        out->cpu_percent = (delta_ticks / ticks_per_sec) / delta_t * 100.0;
                    }
                }
                last_utime = utime;
                last_stime = stime;
                last_time = now;
            }
        }
        fclose(f);
    }

    /* 3. Count open FDs */
    snprintf(path, sizeof(path), PROC_FD_FMT, pid);
    DIR *d = opendir(path);
    if (d) {
        struct dirent *dir;
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_name[0] != '.') {
                out->open_fds++;
            }
        }
        closedir(d);
    }

    /* 4. Count TCP connections (simplification: we just count all established for the PID - 
       Note: /proc/net/tcp doesn't filter by PID easily without checking inodes. 
       For v1, as per comment, count all ESTABLISHED and CLOSE_WAIT rows) */
    /* V1 SIMPLIFICATION */
    f = fopen(PROC_NET_TCP, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            /* format: sl local_address rem_address st ... */
            int state;
            if (sscanf(line, "%*d: %*s %*s %x", &state) == 1) {
                if (state == 0x01) out->tcp_established++;
                else if (state == 0x08) out->tcp_close_wait++;
            }
        }
        fclose(f);
    }

    return 0;
}

void observer_analyze(app_context_t *ctx) {
    pthread_mutex_lock(&ctx->obs_mutex);
    uint32_t head = ctx->obs_head;
    uint32_t count = (head < OBS_RING_SIZE) ? head : OBS_RING_SIZE;
    
    if (count < 2) {
        pthread_mutex_unlock(&ctx->obs_mutex);
        return;
    }

    /* Rule 4 - Process Death */
    obs_snapshot_t *last = &ctx->obs_ring[(head - 1) % OBS_RING_SIZE];
    obs_snapshot_t *prev = &ctx->obs_ring[(head - 2) % OBS_RING_SIZE];
    if (prev->is_alive && !last->is_alive) {
        add_finding(ctx, FIND_SEVERITY_CRITICAL, "Process crashed");
    }

    /* Rule 1 - Memory Leak */
    if (count >= 20) {
        int monotonic = 1;
        uint64_t start_mem = ctx->obs_ring[(head - 20) % OBS_RING_SIZE].mem_rss_kb;
        uint64_t end_mem = last->mem_rss_kb;
        
        for (int i = 19; i > 0; i--) {
            uint64_t m1 = ctx->obs_ring[(head - i - 1) % OBS_RING_SIZE].mem_rss_kb;
            uint64_t m2 = ctx->obs_ring[(head - i) % OBS_RING_SIZE].mem_rss_kb;
            if (m2 < m1) {
                monotonic = 0;
                break;
            }
        }
        if (monotonic && (end_mem > start_mem + 5120)) { /* > 5MB */
            add_finding(ctx, FIND_SEVERITY_CRITICAL, "Memory Leak Detected");
        }
    }

    /* Rule 2 - CPU Spike */
    if (count >= 10) {
        int spiking = 1;
        for (int i = 10; i > 0; i--) {
            if (ctx->obs_ring[(head - i) % OBS_RING_SIZE].cpu_percent <= 90.0) {
                spiking = 0;
                break;
            }
        }
        if (spiking) {
            add_finding(ctx, FIND_SEVERITY_WARNING, "CPU Spike Detected");
        }
    }

    /* Rule 3 - FD Leak */
    /* 30 seconds = 60 snapshots */
    if (count >= 60) {
        uint32_t start_fd = ctx->obs_ring[(head - 60) % OBS_RING_SIZE].open_fds;
        if (last->open_fds > start_fd + 50) {
            add_finding(ctx, FIND_SEVERITY_CRITICAL, "FD Leak Detected");
        }
    }

    /* Rule 5 - High CLOSE_WAIT */
    if (last->tcp_close_wait > 10) {
        add_finding(ctx, FIND_SEVERITY_WARNING, "Possible socket leak");
    }

    pthread_mutex_unlock(&ctx->obs_mutex);
}

void *thread_observer_run(void *arg) {
    app_context_t *ctx = (app_context_t *)arg;
    obs_snapshot_t snap;

    LOG_INFO("Observer thread started for PID %d", ctx->target_pid);

    while (ctx->running) {
        observer_read_snapshot(ctx->target_pid, &snap);

        pthread_mutex_lock(&ctx->obs_mutex);
        ctx->obs_ring[ctx->obs_head % OBS_RING_SIZE] = snap;
        ctx->obs_head++;
        pthread_mutex_unlock(&ctx->obs_mutex);

        observer_analyze(ctx);

        usleep(OBSERVE_INTERVAL_MS * 1000);
    }

    LOG_INFO("Observer thread stopped");
    return NULL;
}

int observer_start(app_context_t *ctx) {
    if (pthread_create(&ctx->thread_observer, NULL, thread_observer_run, ctx) != 0) {
        LOG_ERROR("Failed to create observer thread");
        return -1;
    }
    return 0;
}

void observer_stop(app_context_t *ctx) {
    /* Main loop checks ctx->running, so we just join */
    /* Handled typically by main() joining all threads */
}