#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

report_one="$($repo_root/chaosnet --type packet_loss --intensity 20 --duration 10)"
echo "$report_one" | grep -F "[Resilience Report]"
echo "$report_one" | grep -F "App crashed after 2.3s"

report_two="$($repo_root/chaosnet --type cpu_throttle --intensity 50 --duration 4)"
echo "$report_two" | grep -F "App survived fault injection"
echo "$report_two" | grep -F "Increased response latency without process crash."
