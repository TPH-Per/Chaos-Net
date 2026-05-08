#!/usr/bin/env bash
# scripts/smoke_test.sh
set -euo pipefail

PASS=0
FAIL=0
IFACE="${IFACE:-lo}"

check() {
    local desc="$1"
    local result="$2"
    if [ "$result" = "0" ]; then
        echo "  [PASS] $desc"
        PASS=$((PASS+1))
    else
        echo "  [FAIL] $desc"
        FAIL=$((FAIL+1))
    fi
}

echo "=== ChaosNet Integration Test ==="

# Start broker
mosquitto -d -p 11883
sleep 1

# Start target app in background
./build/tools/target_app/target_app &
TARGET_PID=$!
sleep 1

# Start ChaosNet
./build/chaosnet \
    --pid "$TARGET_PID" \
    --iface "$IFACE" \
    --http-port 18080 \
    --mqtt-host localhost \
    --mqtt-port 11883 \
    --duration 30 &
CHAOS_PID=$!
sleep 2

# Test 1: HTTP status endpoint
STATUS=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:18080/status)
check "HTTP /status returns 200" $([ "$STATUS" = "200" ] && echo 0 || echo 1)

# Test 2: Inject via HTTP
INJECT=$(curl -s -o /dev/null -w "%{http_code}" -X POST http://localhost:18080/chaos \
    -H "Content-Type: application/json" \
    -d '{"type":1,"param_a":20,"duration_sec":5,"iface":"lo"}')
check "HTTP POST /chaos returns 200" $([ "$INJECT" = "200" ] && echo 0 || echo 1)

# Test 3: MQTT inject
mosquitto_pub -p 11883 -t "chaosnet/inject" \
    -m '{"type":4,"param_a":30,"duration_sec":3,"iface":""}'
sleep 1
check "MQTT inject does not crash ChaosNet" $(kill -0 $CHAOS_PID 2>/dev/null && echo 0 || echo 1)

# Test 4: Wait for scenario to complete
sleep 20

# Test 5: Report generated
REPORT=$(curl -s http://localhost:18080/report)
check "Report JSON contains 'total_score'" $(echo "$REPORT" | grep -q "total_score" && echo 0 || echo 1)

# Cleanup
kill $CHAOS_PID 2>/dev/null || true
kill $TARGET_PID 2>/dev/null || true
pkill mosquitto 2>/dev/null || true
bash scripts/clean_chaos.sh

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
[ "$FAIL" = "0" ] && exit 0 || exit 1
