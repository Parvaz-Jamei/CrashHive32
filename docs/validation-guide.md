# Validation Guide

Developer-side hardware validation is not performed for this package.

## Compile validation

Run on the repository root when Arduino CLI is available:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 examples/01_BlackBox_Basic
arduino-cli compile --fqbn esp32:esp32:esp32 examples/02_Heartbeat_Monitor
arduino-cli compile --fqbn esp32:esp32:esp32 examples/03_Neighbor_Watchdog
arduino-cli compile --fqbn esp32:esp32:esp32 examples/04_Full_Demo_Three_Nodes
arduino-cli compile --fqbn esp32:esp32:esp32 examples/06_Field_Diagnostics
```

## Owner-side hardware checklist

- Board: ESP32 DevKit or target board name.
- Arduino ESP32 core version: record exact version.
- Example 01 BlackBox: compile and run.
- Example 02 Heartbeat: compile and test with two ESP32 boards if available.
- Example 03 Neighbor Watchdog: run simulated heartbeat flow.
- Example 04 Full Demo: compile and inspect Serial output.
- Example 06 Field Diagnostics: inspect reset reason, metric alert, health, packet loss, and JSON output.
- Hardware ESP-NOW delivery: PASS / PARTIAL / NOT TESTED.
- Date: YYYY-MM-DD.

## Expected notes

`recordFlag()` code is small status/code data and clips values to `0..255`; do not use it to store a full `uint32_t` node ID.

Hardware validation is intentionally owner-side because it depends on the board, Arduino core version, power conditions, and ESP-NOW environment.
