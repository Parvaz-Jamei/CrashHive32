# Limitations

## Safety

CrashHive32 is not a safety-certified industrial protection system. It is a developer library for observability, diagnostics, and fault awareness.

## Storage

CrashHive32 is not a permanent data logger. RTC/NOINIT BlackBox memory is best-effort only and may be lost on cold boot, brownout, power loss, bootloader behavior, or memory initialization. Brownout does not automatically clear a valid BlackBox; if RTC/NOINIT storage and CRC survive, previous events are preserved, but this is not guaranteed.

No persistent Flash, NVS, FAT, or LittleFS logging is included in v1.1.1.

## ESP-NOW

ESP-NOW delivery is not guaranteed application-level delivery. The send callback reports MAC-layer send status only. User callbacks are not executed inside ESP-NOW callbacks.

`beginEspNow()` initializes ESP-NOW in station mode and may call `WiFi.mode(WIFI_STA)`. If the user application already manages WiFi mode or an active WiFi connection, this may affect the existing WiFi state. Advanced users should initialize WiFi and ESP-NOW in a controlled order.

## Diagnostics

Packet loss is estimated from heartbeat sequence gaps. No RSSI scoring is included in this version. Neighbor health is a lightweight diagnostic estimate, not a certified network quality metric.

`recordFlag(name, code)` stores small status/code data only. Values are clipped to `0..255`; use Serial output or neighbor APIs for full node IDs.

## Network scope

CrashHive32 is not full mesh routing, does not perform automatic rerouting, and does not claim self-healing behavior.

## Validation

Hardware validation must be performed by the integrator/owner on the target ESP32 board and Arduino core version.

## Boot Storm Stability

`CrashHive32.tick()` automatically marks the boot stable after `CH32_BOOT_STABLE_MS`. Users normally should not call `markBootStable()` in `setup()`. Manual `markBootStable()` is only for advanced cases where the application has completed its own stability criteria; calling it too early can hide real boot storm or crash-loop symptoms.
