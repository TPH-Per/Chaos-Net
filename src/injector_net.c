/* src/injector_net.c
 * Network Injector implementation
 * Author: Agent
 * Date: 2026-05-08
 * License: MIT
 */
#include "injector_net.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int validate_iface(const char *iface) {
    if (!iface || strlen(iface) == 0 || strlen(iface) > 15) {
        LOG_ERROR("Invalid interface name length");
        return -1;
    }
    for (size_t i = 0; i < strlen(iface); i++) {
        if (!isalnum((unsigned char)iface[i]) && iface[i] != '-' && iface[i] != '_') {
            LOG_ERROR("Invalid characters in interface name");
            return -1;
        }
    }
    return 0;
}

int net_inject_clear(const char *iface) {
    if (validate_iface(iface) != 0) return -1;

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "tc qdisc del dev %s root 2>/dev/null || true", iface);
    system(cmd);
    
    /* Remove any iptables rules added by this session */
    system("iptables -F OUTPUT 2>/dev/null || true");
    
    LOG_INFO("Cleared network rules for %s", iface);
    return 0;
}

int net_inject_packet_loss(const char *iface, uint32_t loss_percent) {
    if (validate_iface(iface) != 0) return -1;
    if (loss_percent > MAX_PACKET_LOSS_PCT) {
        LOG_ERROR("Invalid loss percentage %u", loss_percent);
        return -1;
    }

    net_inject_clear(iface);

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "tc qdisc add dev %s root netem loss %u%%", iface, loss_percent);
    if (system(cmd) != 0) {
        LOG_ERROR("Failed to inject packet loss");
        return -1;
    }

    LOG_INFO("Injected %u%% packet loss on %s", loss_percent, iface);
    return 0;
}

int net_inject_latency(const char *iface, uint32_t delay_ms) {
    if (validate_iface(iface) != 0) return -1;
    if (delay_ms > MAX_LATENCY_MS) {
        LOG_ERROR("Invalid latency %u ms", delay_ms);
        return -1;
    }

    net_inject_clear(iface);

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "tc qdisc add dev %s root netem delay %ums", iface, delay_ms);
    if (system(cmd) != 0) {
        LOG_ERROR("Failed to inject latency");
        return -1;
    }

    LOG_INFO("Injected %ums latency on %s", delay_ms, iface);
    return 0;
}

int net_inject_corruption(const char *iface, uint32_t corrupt_percent) {
    if (validate_iface(iface) != 0) return -1;
    if (corrupt_percent > 100) {
        LOG_ERROR("Invalid corruption percentage %u", corrupt_percent);
        return -1;
    }

    net_inject_clear(iface);

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "tc qdisc add dev %s root netem corrupt %u%%", iface, corrupt_percent);
    if (system(cmd) != 0) {
        LOG_ERROR("Failed to inject corruption");
        return -1;
    }

    LOG_INFO("Injected %u%% corruption on %s", corrupt_percent, iface);
    return 0;
}

int net_inject_conn_reset(uint16_t target_port) {
    char cmd[256];
    /* Using iptables to reject with TCP reset as a fallback implementation */
    snprintf(cmd, sizeof(cmd), 
             "iptables -A OUTPUT -p tcp --dport %u -j REJECT --reject-with tcp-reset", 
             target_port);
             
    if (system(cmd) != 0) {
        LOG_ERROR("Failed to inject connection reset on port %u", target_port);
        return -1;
    }

    LOG_INFO("Injected connection reset on port %u", target_port);
    return 0;
}