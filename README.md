# Chaos-Net

ChaosNet là một framework chaos engineering tối giản cho Embedded Linux, giúp chủ động gây lỗi có kiểm soát để tìm điểm yếu của ứng dụng trước khi đưa ra production.

## Thành phần chính

- **Control API (CLI/daemon stdin)**: nhận lệnh inject fault.
- **Chaos Engine**: mô phỏng 4 loại fault:
  - `packet_loss` / `network_loss`
  - `memory_pressure`
  - `cpu_throttle`
  - `packet_corrupt`
- **Observer & Analyzer**: quan sát phản ứng ứng dụng và sinh **Resilience Report**.

## Build

```bash
cd /home/runner/work/Chaos-Net/Chaos-Net
make
```

## Chạy one-shot

```bash
./chaosnet --type packet_loss --intensity 20 --duration 10
```

Ví dụ output:

```text
[Resilience Report]
Fault: packet_loss
Intensity: 20%
Duration: 10s
App bị crash sau 2.3s khi packet loss > 15%
Observation: Network stack exhausted retry budget.
Timestamp: 2026-05-08T00:00:00Z
```

## Chạy daemon mode

```bash
./chaosnet
inject packet_loss 20 10
inject cpu_throttle 50 4
exit
```

## Test

```bash
make test
```
