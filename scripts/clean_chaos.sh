#!/usr/bin/env bash
tc qdisc del dev "${IFACE:-eth0}" root 2>/dev/null || true
iptables -F OUTPUT 2>/dev/null || true
rmdir /sys/fs/cgroup/chaosnet 2>/dev/null || true
echo "Chaos cleaned."
