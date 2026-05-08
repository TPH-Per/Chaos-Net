/* include/injector_cpu.h
 * CPU Throttle Injector module
 * Author: Agent
 * Date: 2026-05-08
 * License: MIT
 */
#ifndef INJECTOR_CPU_H
#define INJECTOR_CPU_H

#include <stdint.h>
#include <sys/types.h>

/* Limit target_pid to cpu_limit_percent% CPU using cgroup v2.
   Duration 0 means "until clear is called". */
int  cpu_inject_throttle(pid_t target_pid, uint32_t cpu_limit_percent,
                         uint32_t duration_sec);

/* Remove throttle, move process back to root cgroup */
void cpu_inject_clear(pid_t target_pid);

#endif /* INJECTOR_CPU_H */