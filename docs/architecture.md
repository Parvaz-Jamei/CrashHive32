# Architecture

CrashHive32 is a lightweight ESP32 diagnostics layer. It keeps the Arduino library structure simple and uses only `CrashHive32.h` as the public include.

## Layers

- BlackBox: best-effort RTC/NOINIT recent events with schema v3 per-event CRC.
- ESP-NOW transport: small heartbeat packets and lightweight callback queueing.
- Neighbor watchdog: fixed-size neighbor table, states, health, packet loss estimate, silent/recovered/degraded events.
- Boot state: best-effort boot storm detection using RTC/NOINIT state.
- Metric alerts: fixed-size threshold table with no heap allocation.
- System health: compact 0..100 summary.

No cloud, MQTT, AI, web server, OTA, full mesh routing, persistent Flash logging, or RSSI scoring is included in v1.1.1.
