# Contributing

Thank you for considering a contribution to CrashHive32.

## Scope

CrashHive32 is currently focused on:

- ESP32 Arduino library compatibility
- RTC/NOINIT BlackBox diagnostics
- Lightweight ESP-NOW heartbeat transport
- Fixed-size neighbor watchdog monitoring
- Small callback hooks for adaptive behavior

Please keep contributions within this scope unless an issue explicitly expands it.

## Development Rules

- Keep the public API small.
- Keep only `src/CrashHive32.h` as the public root header.
- Place internal headers under `src/internal/`.
- Avoid Arduino `String` in `src/`.
- Avoid heap allocation in `tick()` and `loop()` paths.
- Keep ESP-NOW packets small and documented.
- Do not add AI/API keys/MQTT/dashboard code to firmware.
- Do not claim full mesh routing or self-healing behavior.

## Validation

Run, when available:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 examples/01_BlackBox_Basic
arduino-cli compile --fqbn esp32:esp32:esp32 examples/02_Heartbeat_Monitor
arduino-cli compile --fqbn esp32:esp32:esp32 examples/03_Neighbor_Watchdog
arduino-cli compile --fqbn esp32:esp32:esp32 examples/04_Full_Demo_Three_Nodes
```

If hardware was not tested, mark it as NOT TESTED instead of implying success.
