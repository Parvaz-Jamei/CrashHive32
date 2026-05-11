# CrashHive32

ESP32 nodes that remember their last moments — and neighbors that watch closer.

CrashHive32 is a small Arduino library for crash-aware ESP32 sensor networks. It combines best-effort RTC/NOINIT BlackBox logging, ESP-NOW heartbeat monitoring, neighbor silence detection, field diagnostics, health scoring, packet loss estimates, boot storm detection, metric threshold alerts, and adaptive response callbacks.

## What CrashHive32 is not

CrashHive32 is not safety-certified, not a permanent data logger, not a full mesh routing stack, and not a self-healing network system. It does not include cloud, MQTT, AI, dashboards, OTA, persistent Flash/NVS/LittleFS logging, or RSSI scoring in v1.1.1.

## Features

- Best-effort RTC/NOINIT BlackBox for recent reset and diagnostic events.
- ESP-NOW heartbeat transport with small packets under the ESP-NOW v1 250-byte payload limit.
- Neighbor watchdog with alive, degraded, silent, and recovered states.
- Neighbor age, silent duration, health score, and estimated packet loss from heartbeat sequence gaps.
- Boot storm detection using best-effort RTC/NOINIT state.
- Metric threshold alerts with fixed-size, allocation-free storage.
- System health summary API for quick diagnostics.
- Chunked JSON export and Stream-based JSON printing without Arduino `String` storage.

## Installation

### Arduino IDE / Arduino CLI

CrashHive32 is designed as a standard Arduino library. Until it is available in Arduino Library Manager, install it manually by placing the `CrashHive32` folder inside your Arduino `libraries` directory.

```cpp
#include <CrashHive32.h>
```

Then open one of the examples from the Arduino IDE Examples menu.

## Hardware Requirements

- ESP32 board supported by the Arduino ESP32 core.
- Arduino IDE 2.x or Arduino CLI.
- ESP-NOW examples require at least two ESP32 boards for real peer delivery.
- `03_Neighbor_Watchdog` and `06_Field_Diagnostics` can compile and demonstrate logic with simulated heartbeats.

## Quick Start

```cpp
#include <CrashHive32.h>

void setup() {
  Serial.begin(115200);
  CrashHive32.begin();
  CrashHive32.recordMetric("temp", 42.5F);
  CrashHive32.printReport(Serial);
}

void loop() {
  CrashHive32.tick();
}
```

## ESP-NOW peers

Both colon and dash MAC text helpers are accepted. Mixed separators are rejected.

```cpp
uint8_t peerMac[6] = {0x24, 0x6F, 0x28, 0xAA, 0xBB, 0x01};
CrashHive32.addPeer(peerMac);
CrashHive32.addPeer("24:6F:28:AA:BB:01");
CrashHive32.addPeer("24-6F-28-AA-BB-01");
CrashHive32.removePeer(peerMac);
CrashHive32.removePeer("24:6F:28:AA:BB:01");
```

`removePeer()` returns `true` when a known peer is removed. It returns `false` for invalid MAC text, ESP-NOW delete failure, or a peer that is not currently known.

## Field Diagnostics

### Reset reason

```cpp
Serial.println(CrashHive32.getResetReasonName());
uint8_t code = CrashHive32.getResetReasonCode();
```

### Boot storm detection

```cpp
if (CrashHive32.isBootStorm()) {
  Serial.println("Boot storm detected");
}
// Normally let CrashHive32.tick() mark the boot stable automatically
// after CH32_BOOT_STABLE_MS. Only call markBootStable() manually
// after your own application-specific stability criteria have passed.
```

Boot storm detection uses best-effort RTC/NOINIT memory and is not durable across all power-loss or brownout conditions. Calling `markBootStable()` too early can hide real boot storm or crash-loop symptoms.

### Neighbor diagnostics

```cpp
CrashHive32.watchNeighbor(2, 15000UL);
CrashHive32.watchNeighborEvery(3, 5000UL);  // timeout = 15000 ms
uint8_t state = CrashHive32.getNeighborState(2);
Serial.println(CrashHive32.getNeighborStateName(2));
uint32_t ageMs = CrashHive32.getNeighborAgeMs(2);
uint32_t silentMs = CrashHive32.getNeighborSilentMs(2);
uint8_t health = CrashHive32.getNeighborHealth(2);
uint8_t safeHealth = 0;
if (CrashHive32.getNeighborHealth(2, &safeHealth)) {
  Serial.println(safeHealth);
}
uint8_t loss = CrashHive32.getNeighborPacketLoss(2);
if (!CrashHive32.isNeighborHealthy(2)) {
  Serial.println("Node 2 needs attention");
}
```

Health meaning:

- `100`: excellent
- `70..99`: healthy
- `40..69`: degraded
- `1..39`: critical
- `0`: unknown, missing, dead, or silent too long

A watched neighbor remains `CH32_NEIGHBOR_UNKNOWN` only during its first timeout window. If no heartbeat arrives by `timeoutMs`, it transitions to `CH32_NEIGHBOR_SILENT`, health becomes `0`, and silent/adaptive callbacks fire once. For safe user code, check `getNeighborState()` or use `getNeighborHealth(nodeId, &outHealth)`, which returns `false` while health is not yet meaningful. Unknown neighbors before timeout are ignored by system-health warning calculations so registration alone does not create a synthetic warning. Missing-after-timeout neighbors are included as unhealthy.

Recommended timeout: set `timeoutMs` to at least 2.5x to 3x the expected heartbeat interval to avoid normal late heartbeats appearing degraded.

Developer UX helpers:

- `watchNeighborEvery(nodeId, expectedHeartbeatMs, missedBeats)`: configures timeout from the expected heartbeat interval. The default `missedBeats` value is `3`, so `watchNeighborEvery(2, 5000UL)` creates a 15000 ms timeout.
- `getNeighborStateName(nodeId)`: returns `"unknown"`, `"alive"`, `"degraded"`, or `"silent"` for simple Serial logs.
- `getAdaptiveReasonName(reason)`: converts adaptive reason codes to readable strings.
- `isNeighborHealthy(nodeId, minHealth)`: safe yes/no helper that returns `false` if health is unknown.

### Metric threshold alerts

```cpp
void onTempAlert(const char* key, float value, float minValue, float maxValue) {
  Serial.println(key);
}

CrashHive32.setMetricAlert("motor_temp", 0.0F, 70.0F, onTempAlert);
CrashHive32.recordMetric("motor_temp", 72.0F);
```

Metric alerts fire once per out-of-range episode. They reset only after the metric returns to the configured range.

### System health

```cpp
uint8_t health = CrashHive32.getSystemHealth();
if (CrashHive32.hasActiveDiagnosticWarning()) {
  Serial.println("Diagnostic warning active");
}
```

## JSON output

Use one JSON output method at a time:

- `exportJson(writer)`: chunk callback, useful for custom sinks.
- `printJson(Stream&)`: convenience helper for `Serial`, files, or other Stream-like outputs.

Neither method builds one large Arduino `String` internally.

## Callback model

- `onNeighborSilent()` and `onNeighborRecovered()` are precise neighbor state callbacks.
- `onAdaptiveResponse()` is a higher-level callback that receives a reason code.
- If you want one simpler callback, use `onAdaptiveResponse()`.
- If you need exact state control, use the silent/recovered callbacks.

User callbacks are not called directly from ESP-NOW send/receive callbacks; pending heartbeat data is drained from `tick()`.

## Public API

```cpp
bool begin();
bool tick();

bool recordMetric(const char* key, float value);
bool recordFlag(const char* key, int code);
bool hasPreviousEvents() const;
void printReport(Stream& out) const;
bool exportJson(CH32ChunkWriter writer) const;
void printJson(Stream& out) const;

uint8_t getResetReasonCode() const;
const char* getResetReasonName() const;

bool beginEspNow();
bool addPeer(const uint8_t mac[6]);
bool addPeer(const char* mac);
bool removePeer(const uint8_t mac[6]);
bool removePeer(const char* mac);
bool sendHeartbeat(uint32_t nodeId, uint8_t status);
void setHeartbeatInterval(uint32_t intervalMs);

bool watchNeighbor(uint32_t nodeId, uint32_t timeoutMs);
bool watchNeighborEvery(uint32_t nodeId, uint32_t expectedHeartbeatMs, uint8_t missedBeats = 3U);
bool removeNeighbor(uint32_t nodeId);
bool noteHeartbeat(uint32_t nodeId);
uint8_t getNeighborState(uint32_t nodeId) const;
const char* getNeighborStateName(uint32_t nodeId) const;
uint32_t getNeighborAgeMs(uint32_t nodeId) const;
uint32_t getNeighborSilentMs(uint32_t nodeId) const;
uint8_t getNeighborHealth(uint32_t nodeId) const;
bool getNeighborHealth(uint32_t nodeId, uint8_t* outHealth) const;
bool isNeighborHealthy(uint32_t nodeId, uint8_t minHealth = 70U) const;
bool isNeighborDegraded(uint32_t nodeId) const;
uint8_t getNeighborPacketLoss(uint32_t nodeId) const;

void onNeighborSilent(CH32NeighborCallback callback);
void onNeighborRecovered(CH32NeighborCallback callback);
void onAdaptiveResponse(CH32AdaptiveCallback callback);
const char* getAdaptiveReasonName(uint8_t reason) const;

bool isBootStorm() const;
uint8_t getBootStreak() const;
void markBootStable();

bool setMetricAlert(const char* key, float minValue, float maxValue, CH32MetricAlertCallback callback);
bool clearMetricAlert(const char* key);
void clearMetricAlerts();

uint8_t getSystemHealth() const;
bool hasActiveDiagnosticWarning() const;
```

## API notes

- `recordFlag(name, code)`: `code` is stored as an 8-bit value. Values below `0` are clipped to `0`; values above `255` are clipped to `255`.
- Metric alert keys use the same canonicalization as BlackBox keys. Valid visible characters are `a-z`, `A-Z`, `0-9`, `_`, `-`, and `.`; other characters become `_`. Metric alert keys that would exceed the visible key length are rejected to avoid ambiguous matches.
- `sendHeartbeat(nodeId, status)`: `status` is an application-defined 8-bit value. CrashHive32 does not impose a fixed status enum in v1.1.1.
- If you need to preserve a full `uint32_t nodeId`, use neighbor APIs and Serial output; the BlackBox flag code is small status/code data, not a full node ID store.

## Examples

- `01_BlackBox_Basic`: basic BlackBox usage and JSON/report output.
- `02_Heartbeat_Monitor`: ESP-NOW peer registration and heartbeat send.
- `03_Neighbor_Watchdog`: simulated heartbeat watchdog.
- `04_Full_Demo_Three_Nodes`: three-node conceptual demo.
- `06_Field_Diagnostics`: single-board simulated diagnostics for reset reason, boot storm, metric alert, UNKNOWN-to-SILENT missing node behavior, recovery, neighbor health, packet loss, system health, and JSON.

## Limitations

- RTC/NOINIT memory is best-effort only.
- CrashHive32 is not a permanent data logger.
- CrashHive32 is not safety-certified.
- ESP-NOW send callback indicates MAC-layer send status, not guaranteed application-level delivery.
- Packet loss is estimated from received sequence gaps.
- No RSSI scoring in this version.
- No full mesh routing.
- No persistent Flash/NVS/LittleFS logging.
- Hardware validation must be performed by the integrator. Brownout preservation is best-effort: if RTC/NOINIT memory and CRC survive, CrashHive32 preserves valid BlackBox history; if memory is corrupt or lost, it resets safely.

## Validation status

Developer-side hardware validation is not performed for this package. Arduino CLI/GitHub Actions compile evidence should be collected on the target repository before public release.

## Version Status

`1.1.1` is a release candidate for field diagnostics, Developer UX polish, and Arduino Library Manager readiness.

## License

MIT License.
