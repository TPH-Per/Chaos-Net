/* include/injector_net.h
 * Network Injector module
 * Author: Agent
 * Date: 2026-05-08
 * License: MIT
 */
#ifndef INJECTOR_NET_H
#define INJECTOR_NET_H

#include "chaosnet.h"

/* Apply network chaos. Returns 0 on success, -1 on error. */
int net_inject_packet_loss(const char *iface, uint32_t loss_percent);
int net_inject_latency(const char *iface, uint32_t delay_ms);
int net_inject_corruption(const char *iface, uint32_t corrupt_percent);

/* Remove ALL tc rules on interface. Always call this on cleanup. */
int net_inject_clear(const char *iface);

/* Reset a TCP connection to target port using RST injection */
int net_inject_conn_reset(uint16_t target_port);

#endif /* INJECTOR_NET_H */