/* include/injector_mem.h
 * Memory Injector module
 * Author: Agent
 * Date: 2026-05-08
 * License: MIT
 */
#ifndef INJECTOR_MEM_H
#define INJECTOR_MEM_H

#include <stdint.h>

/* Allocate and lock mb megabytes. Blocks until duration_sec expires, then frees. */
/* Runs in its own thread internally. Returns 0 if thread started, -1 on error. */
int  mem_inject_pressure(uint32_t mb, uint32_t duration_sec);

/* Immediately release any held memory */
void mem_inject_clear(void);

#endif /* INJECTOR_MEM_H */