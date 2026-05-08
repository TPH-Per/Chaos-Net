#!/usr/bin/env bash
set -euo pipefail

echo "[ChaosNet] Installing dependencies..."
sudo apt-get update -qq
sudo apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libmosquitto-dev \
    libmicrohttpd-dev \
    mosquitto \
    mosquitto-clients \
    iproute2 \
    iptables \
    valgrind \
    cgroup-tools \
    jq

echo "[ChaosNet] Setting up cgroup v2..."
sudo mkdir -p /sys/fs/cgroup/chaosnet || true

echo "[ChaosNet] Done. Run: mkdir build && cd build && cmake .. && make"
