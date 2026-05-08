/* src/injector_cpu.c
 * CPU Throttle Injector implementation
 * Author: Agent
 * Date: 2026-05-08
 * License: MIT
 */
#include "injector_cpu.h"
#include "logger.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>

typedef struct {
    pid_t target_pid;
    uint32_t duration_sec;
} cpu_thread_arg_t;

static pthread_t cpu_thread;
static volatile int cpu_thread_active = 0;
static volatile int cpu_cancel = 0;

static int write_to_file(const char *path, const char *content) {
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        LOG_ERROR("Cannot open %s: %s", path, strerror(errno));
        return -1;
    }
    ssize_t written = write(fd, content, strlen(content));
    close(fd);
    return (written < 0) ? -1 : 0;
}

static void *thread_cpu_throttle_run(void *arg) {
    cpu_thread_arg_t *a = (cpu_thread_arg_t *)arg;
    
    uint32_t slept = 0;
    while (slept < a->duration_sec && !cpu_cancel) {
        sleep(1);
        slept++;
    }

    if (!cpu_cancel) {
        cpu_inject_clear(a->target_pid);
    }
    
    free(a);
    cpu_thread_active = 0;
    return NULL;
}

int cpu_inject_throttle(pid_t target_pid, uint32_t cpu_limit_percent, uint32_t duration_sec) {
    if (cpu_thread_active) {
        LOG_ERROR("CPU throttle already active");
        return -1;
    }

    if (cpu_limit_percent < MIN_CPU_LIMIT_PCT || cpu_limit_percent > 95) {
        LOG_ERROR("Invalid CPU limit %u%% (must be %u - 95)", cpu_limit_percent, MIN_CPU_LIMIT_PCT);
        return -1;
    }

    /* 1. mkdir /sys/fs/cgroup/chaosnet */
    if (mkdir(CGROUP_BASE, 0755) != 0 && errno != EEXIST) {
        LOG_ERROR("Failed to create cgroup directory: %s", strerror(errno));
        return -1;
    }

    /* 2. Write quota to cpu.max */
    char max_val[64];
    /* quota = cpu_limit_percent * 1000, period = 100000 */
    snprintf(max_val, sizeof(max_val), "%u 100000\n", cpu_limit_percent * 1000);
    if (write_to_file(CGROUP_CPU_MAX, max_val) != 0) {
        LOG_ERROR("Failed to set cpu.max");
        return -1;
    }

    /* 3. Write target_pid to cgroup.procs */
    char procs_path[256];
    snprintf(procs_path, sizeof(procs_path), "%s/cgroup.procs", CGROUP_BASE);
    
    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d\n", target_pid);
    if (write_to_file(procs_path, pid_str) != 0) {
        LOG_ERROR("Failed to move process to cgroup");
        return -1;
    }

    LOG_INFO("Injected CPU throttle to %u%% for PID %d", cpu_limit_percent, target_pid);

    if (duration_sec > 0) {
        cpu_thread_arg_t *arg = malloc(sizeof(cpu_thread_arg_t));
        if (!arg) {
            LOG_ERROR("malloc failed");
            cpu_inject_clear(target_pid);
            return -1;
        }
        arg->target_pid = target_pid;
        arg->duration_sec = duration_sec;
        cpu_cancel = 0;
        cpu_thread_active = 1;

        if (pthread_create(&cpu_thread, NULL, thread_cpu_throttle_run, arg) != 0) {
            LOG_ERROR("Failed to create CPU throttle thread");
            free(arg);
            cpu_thread_active = 0;
            cpu_inject_clear(target_pid);
            return -1;
        }
        pthread_detach(cpu_thread);
    }

    return 0;
}

void cpu_inject_clear(pid_t target_pid) {
    if (cpu_thread_active) {
        cpu_cancel = 1;
    }

    /* Move process back to root cgroup */
    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d\n", target_pid);
    
    /* /sys/fs/cgroup/cgroup.procs is the root */
    if (write_to_file("/sys/fs/cgroup/cgroup.procs", pid_str) != 0) {
        LOG_WARN("Failed to move PID %d back to root cgroup", target_pid);
    } else {
        LOG_INFO("Cleared CPU throttle for PID %d", target_pid);
    }

    /* Attempt to remove our cgroup */
    if (rmdir(CGROUP_BASE) != 0) {
        LOG_WARN("Failed to remove cgroup %s: %s", CGROUP_BASE, strerror(errno));
    }
}