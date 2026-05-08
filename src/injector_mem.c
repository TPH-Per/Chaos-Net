/* src/injector_mem.c
 * Memory Injector implementation
 * Author: Agent
 * Date: 2026-05-08
 * License: MIT
 */
#include "injector_mem.h"
#include "logger.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

typedef struct {
    uint32_t mb;
    uint32_t duration_sec;
} mem_thread_arg_t;

static pthread_t mem_thread;
static volatile int mem_thread_active = 0;
static volatile int mem_cancel = 0;

static void *thread_mem_pressure_run(void *arg) {
    mem_thread_arg_t *a = (mem_thread_arg_t *)arg;
    size_t bytes = (size_t)a->mb * 1024 * 1024;

    /* mmap anonymous private memory */
    void *ptr = mmap(NULL, bytes, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
        LOG_ERROR("mmap failed: %s", strerror(errno));
        free(a);
        mem_thread_active = 0;
        return NULL;
    }

    /* mlock to prevent swap — requires CAP_IPC_LOCK or root */
    if (mlock(ptr, bytes) != 0) {
        LOG_WARN("mlock failed (will still consume RSS): %s", strerror(errno));
    }

    /* Touch every page to ensure physical allocation */
    /* Write pattern so compiler cannot optimize away */
    memset(ptr, 0xAB, bytes);

    LOG_INFO("Memory pressure: holding %u MB for %u seconds", a->mb, a->duration_sec);
    
    uint32_t slept = 0;
    while (slept < a->duration_sec && !mem_cancel) {
        sleep(1);
        slept++;
    }

    munlock(ptr, bytes);
    munmap(ptr, bytes);
    
    if (mem_cancel) {
        LOG_INFO("Memory pressure: canceled, released %u MB", a->mb);
    } else {
        LOG_INFO("Memory pressure: released %u MB", a->mb);
    }

    free(a);
    mem_thread_active = 0;
    return NULL;
}

int mem_inject_pressure(uint32_t mb, uint32_t duration_sec) {
    if (mem_thread_active) {
        LOG_ERROR("Memory injection already active");
        return -1;
    }

    if (mb > MAX_MEMORY_PRESSURE_MB) {
        LOG_ERROR("Requested memory %u MB exceeds limit %u MB", mb, MAX_MEMORY_PRESSURE_MB);
        return -1;
    }

    /* Check available memory from /proc/meminfo */
    FILE *f = fopen("/proc/meminfo", "r");
    if (f) {
        char line[256];
        unsigned long mem_available_kb = 0;
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "MemAvailable:", 13) == 0) {
                sscanf(line, "MemAvailable: %lu kB", &mem_available_kb);
                break;
            }
        }
        fclose(f);

        if (mem_available_kb > 0) {
            uint32_t avail_mb = mem_available_kb / 1024;
            if (avail_mb < mb * 1.2) {
                uint32_t new_mb = avail_mb * 0.8;
                LOG_WARN("Available memory (%u MB) is too low for %u MB request. Reducing to %u MB.", 
                         avail_mb, mb, new_mb);
                mb = new_mb;
            }
        }
    }

    mem_thread_arg_t *arg = malloc(sizeof(mem_thread_arg_t));
    if (!arg) {
        LOG_ERROR("malloc failed");
        return -1;
    }
    
    arg->mb = mb;
    arg->duration_sec = duration_sec;
    mem_cancel = 0;
    mem_thread_active = 1;

    if (pthread_create(&mem_thread, NULL, thread_mem_pressure_run, arg) != 0) {
        LOG_ERROR("Failed to create memory pressure thread");
        free(arg);
        mem_thread_active = 0;
        return -1;
    }

    pthread_detach(mem_thread); /* Detach so we don't have to join */
    return 0;
}

void mem_inject_clear(void) {
    if (mem_thread_active) {
        mem_cancel = 1;
        /* It might take up to 1 second for the thread to notice and clear */
        LOG_INFO("Signaled memory pressure thread to clear");
    }
}