# ChaosNet — Chaos Engineering for Embedded Linux

Chaos Engineering (cố tình phá hệ thống có kiểm soát để tìm điểm yếu) là tiêu chuẩn vàng ở cloud/microservices — Netflix có Chaos Monkey, AWS có Fault Injection Simulator... phát hiện lỗi mạng, memory leak, race condition. ChaosNet giải quyết điều đó bằng cách chủ động gây lỗi có kiểm soát ngay trong môi trường development.

## What It Is
ChaosNet is a C99-based daemon that injects controlled chaos (network latency, packet loss, CPU throttling, and memory pressure) into a target process running on embedded Linux systems. It actively monitors the target process, observing memory consumption, CPU utilization, file descriptor leaks, and crashes, ultimately producing a resilience report.

## Why It Matters
While tools like Chaos Monkey and AWS FIS exist for cloud environments, embedded Linux systems lack equivalent chaos engineering tools. ChaosNet addresses this gap by allowing engineers to discover and resolve memory leaks, CPU spikes, FD exhaustion, and crash loops before pushing software into production devices.

## Architecture Diagram (ASCII)
```
 +---------------------------------------------------------+
 |                      ChaosNet                           |
 |                                                         |
 |  [ HTTP API ] <----> [ Main / Watchdog ] <----> [ MQTT] |
 |                           |                             |
 |  [ Scenario Engine ] <----+----> [ Observer ]           |
 |          |                            |                 |
 |          v                            v                 |
 |  +---------------+               +-----------------+    |
 |  |  Injectors    |               |  /proc/<pid>    |    |
 |  | (net,mem,cpu) |               |  Stats & State  |    |
 |  +---------------+               +-----------------+    |
 +---------------------------------------------------------+
          |                              ^
          v                              |
    (tc, iptables, cgroups)              |
          |                              |
 +---------------------------------------------------------+
 |                 Target Process (PID)                    |
 +---------------------------------------------------------+
```

## Requirements
- Linux OS (tested on Ubuntu, Raspberry Pi OS)
- GCC or Clang (C99 standard)
- CMake 3.16+
- `libmosquitto-dev`
- `libmicrohttpd-dev`
- `iproute2` (`tc`)
- `iptables`
- `cgroup-tools` (v2 support)

## Build Instructions
```bash
# Install dependencies
./scripts/setup_deps.sh

# Build
mkdir build && cd build
cmake ..
make
```

## Quick Start
```bash
# 1. Start target application
./build/tools/target_app/target_app &
TARGET_PID=$!

# 2. Run ChaosNet daemon
./build/chaosnet --pid $TARGET_PID --duration 60 &

# 3. Inject packet loss
curl -X POST http://localhost:8080/chaos \
  -d '{"type":1,"param_a":20,"duration_sec":10,"iface":"lo"}'
```

## CLI Reference
```
Usage: ./chaosnet --pid <pid> [options]
Options:
  --pid <pid>           Target process PID (required)
  --iface <name>        Network interface (default: eth0)
  --http-port <port>    HTTP port (default: 8080)
  --mqtt-host <host>    MQTT broker host (default: localhost)
  --mqtt-port <port>    MQTT broker port (default: 1883)
  --scenario <file>     Load and run scenario file immediately
  --log-level <0-3>     Log verbosity (0=DEBUG, 1=INFO, 2=WARN, 3=ERROR)
  --log-file <path>     Log file path
  --duration <sec>      Auto-stop after N seconds (0=run forever)
  --no-inject           Do not start injectors (dry run)
```

## HTTP API Reference

- `GET /status`: Returns JSON snapshot of the latest system observations.
- `GET /findings`: Returns JSON array of detected anomalies (leaks, crashes).
- `POST /chaos`: Injects chaos.
  ```bash
  curl -X POST http://localhost:8080/chaos -d '{"type":"memory_pressure","param_a":100,"duration_sec":10}'
  ```
- `POST /scenario`: Upload a scenario JSON to execute automatically.
- `GET /report`: Fetches the resilience report.
- `DELETE /chaos`: Clears active chaos configurations.

## MQTT Topics Reference
- `chaosnet/inject`: Publish to this topic to trigger chaos. Format is same as POST `/chaos`.
- `chaosnet/status`: Subscribing to this topic yields live observations of the target process state.
- `chaosnet/report`: Emits the resilience report JSON when requested.

## Writing a Scenario
Create a JSON file with steps representing a series of failure injections:
```json
{
  "name": "mqtt_gateway_stress",
  "total_duration_sec": 60,
  "steps": [
    {
      "at_sec": 0,
      "type": "packet_loss",
      "param_a": 10,
      "duration_sec": 10,
      "iface": "eth0"
    }
  ]
}
```
Run it via CLI: `./chaosnet --pid 1234 --scenario my_scenario.json`

## Understanding the Report
At the end of a session, ChaosNet aggregates findings into a final Resilience Score out of 100, broken into Memory, CPU, and Network sub-scores. This pinpoints exact weak spots in your embedded Linux applications.

## Running Against Your Own App
Simply specify the PID of your running process:
```bash
./chaosnet --pid $(pgrep my_daemon) --duration 120
```

## Troubleshooting
- **No CPU Throttle:** Ensure `cgroup v2` is mounted at `/sys/fs/cgroup`.
- **TC Errors:** Verify the target interface exists using `ip link`. Default is `eth0`.
- **Permission Denied:** Most chaos injections (like memory locking and `tc`) require root/sudo access.

## License
MIT License
