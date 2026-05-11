# CrashHive32

> ESP32 nodes that remember their last moments — and neighbors that watch closer.

CrashHive32 is a small Arduino library for crash-aware ESP32 sensor networks. It combines best-effort RTC/NOINIT BlackBox logging, ESP-NOW heartbeat monitoring, neighbor silence detection, field diagnostics, health scoring, packet loss estimates, boot storm detection, metric threshold alerts, and adaptive response callbacks.

---

## What CrashHive32 is not

CrashHive32 is not safety-certified, not a permanent data logger, not a full mesh routing stack, and not a self-healing network system. It does not include cloud, MQTT, AI, dashboards, OTA, persistent Flash/NVS/LittleFS logging, or RSSI scoring in v1.1.1.

---

## Features

- Best-effort RTC/NOINIT BlackBox for recent reset and diagnostic events.
- ESP-NOW heartbeat transport with small packets under the ESP-NOW v1 250-byte payload limit.
- Neighbor watchdog with `alive`, `degraded`, `silent`, and `recovered` states.
- Neighbor age, silent duration, health score, and estimated packet loss from heartbeat sequence gaps.
- Boot storm detection using best-effort RTC/NOINIT state.
- Metric threshold alerts with fixed-size, allocation-free storage.
- System health summary API for quick diagnostics.
- Chunked JSON export and Stream-based JSON printing without Arduino `String` storage.

---

## Installation

### Arduino Library Manager ✅

CrashHive32 is available in the **Arduino Library Manager**.

Open **Arduino IDE 2.x**, go to **Sketch → Include Library → Manage Libraries**, search for **CrashHive32**, and click **Install**.

```cpp
#include <CrashHive32.h>
```

### Arduino CLI

```bash
arduino-cli lib install CrashHive32
```

### Manual installation

Download the latest release ZIP from [GitHub Releases](https://github.com/Parvaz-Jamei/CrashHive32/releases) and use **Sketch → Include Library → Add .ZIP Library** in Arduino IDE.

---

## Hardware Requirements

- ESP32 board supported by the Arduino ESP32 core.
- Arduino IDE 2.x or Arduino CLI.
- ESP-NOW examples require at least two ESP32 boards for real peer delivery.
- `03_Neighbor_Watchdog` and `06_Field_Diagnostics` can compile and demonstrate logic with simulated heartbeats.

---

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

---

## ESP-NOW Peers

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

---

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

> Boot storm detection uses best-effort RTC/NOINIT memory and is not durable across all power-loss or brownout conditions. Calling `markBootStable()` too early can hide real boot storm or crash-loop symptoms.

### Neighbor diagnostics

```cpp
CrashHive32.watchNeighbor(2, 15000UL);
CrashHive32.watchNeighborEvery(3, 5000UL);  // timeout = 15000 ms

uint8_t state   = CrashHive32.getNeighborState(2);
uint32_t ageMs  = CrashHive32.getNeighborAgeMs(2);
uint32_t silMs  = CrashHive32.getNeighborSilentMs(2);
uint8_t  loss   = CrashHive32.getNeighborPacketLoss(2);

Serial.println(CrashHive32.getNeighborStateName(2));

uint8_t safeHealth = 0;
if (CrashHive32.getNeighborHealth(2, &safeHealth)) {
  Serial.println(safeHealth);
}

if (!CrashHive32.isNeighborHealthy(2)) {
  Serial.println("Node 2 needs attention");
}
```

**Health score meaning:**

| Value   | Meaning   |
|---------|-----------|
| `100`   | Excellent |
| `70–99` | Healthy   |
| `40–69` | Degraded  |
| `1–39`  | Critical  |
| `0`     | Unknown / dead / silent too long |

A watched neighbor stays `CH32_NEIGHBOR_UNKNOWN` only during its first timeout window. If no heartbeat arrives by `timeoutMs`, it transitions to `CH32_NEIGHBOR_SILENT`, health drops to `0`, and silent/adaptive callbacks fire once.

**Recommended timeout:** at least 2.5× to 3× the expected heartbeat interval to avoid treating normal late heartbeats as degraded.

**Developer UX helpers:**

- `watchNeighborEvery(nodeId, expectedHeartbeatMs, missedBeats)` — configures timeout from expected interval. Default `missedBeats = 3`, so `watchNeighborEvery(2, 5000UL)` creates a 15 000 ms timeout.
- `getNeighborStateName(nodeId)` — returns `"unknown"`, `"alive"`, `"degraded"`, or `"silent"` for Serial logs.
- `getAdaptiveReasonName(reason)` — converts adaptive reason codes to readable strings.
- `isNeighborHealthy(nodeId, minHealth)` — safe yes/no helper; returns `false` if health is unknown.

### Metric threshold alerts

```cpp
void onTempAlert(const char* key, float value, float minValue, float maxValue) {
  Serial.println(key);
}

CrashHive32.setMetricAlert("motor_temp", 0.0F, 70.0F, onTempAlert);
CrashHive32.recordMetric("motor_temp", 72.0F);
```

Alerts fire once per out-of-range episode and reset only after the metric returns to the configured range.

### System health

```cpp
uint8_t health = CrashHive32.getSystemHealth();
if (CrashHive32.hasActiveDiagnosticWarning()) {
  Serial.println("Diagnostic warning active");
}
```

---

## JSON Output

Use one JSON output method at a time:

- `exportJson(writer)` — chunk callback, useful for custom sinks.
- `printJson(Stream&)` — convenience helper for `Serial`, files, or other Stream-like outputs.

Neither method builds a large Arduino `String` internally.

---

## Callback Model

- `onNeighborSilent()` and `onNeighborRecovered()` — precise neighbor state callbacks.
- `onAdaptiveResponse()` — higher-level callback that receives a reason code.

Use `onAdaptiveResponse()` for simplicity; use silent/recovered callbacks for exact state control.

> User callbacks are not called from ESP-NOW send/receive callbacks directly. Pending heartbeat data is drained from `tick()`.

---

## Public API

```cpp
bool begin();
bool tick();

// BlackBox
bool recordMetric(const char* key, float value);
bool recordFlag(const char* key, int code);
bool hasPreviousEvents() const;
void printReport(Stream& out) const;
bool exportJson(CH32ChunkWriter writer) const;
void printJson(Stream& out) const;

// Reset & boot
uint8_t     getResetReasonCode() const;
const char* getResetReasonName() const;
bool        isBootStorm() const;
uint8_t     getBootStreak() const;
void        markBootStable();

// ESP-NOW
bool beginEspNow();
bool addPeer(const uint8_t mac[6]);
bool addPeer(const char* mac);
bool removePeer(const uint8_t mac[6]);
bool removePeer(const char* mac);
bool sendHeartbeat(uint32_t nodeId, uint8_t status);
void setHeartbeatInterval(uint32_t intervalMs);

// Neighbor watchdog
bool        watchNeighbor(uint32_t nodeId, uint32_t timeoutMs);
bool        watchNeighborEvery(uint32_t nodeId, uint32_t expectedHeartbeatMs, uint8_t missedBeats = 3U);
bool        removeNeighbor(uint32_t nodeId);
bool        noteHeartbeat(uint32_t nodeId);
uint8_t     getNeighborState(uint32_t nodeId) const;
const char* getNeighborStateName(uint32_t nodeId) const;
uint32_t    getNeighborAgeMs(uint32_t nodeId) const;
uint32_t    getNeighborSilentMs(uint32_t nodeId) const;
uint8_t     getNeighborHealth(uint32_t nodeId) const;
bool        getNeighborHealth(uint32_t nodeId, uint8_t* outHealth) const;
bool        isNeighborHealthy(uint32_t nodeId, uint8_t minHealth = 70U) const;
bool        isNeighborDegraded(uint32_t nodeId) const;
uint8_t     getNeighborPacketLoss(uint32_t nodeId) const;

// Neighbor callbacks
void        onNeighborSilent(CH32NeighborCallback callback);
void        onNeighborRecovered(CH32NeighborCallback callback);
void        onAdaptiveResponse(CH32AdaptiveCallback callback);
const char* getAdaptiveReasonName(uint8_t reason) const;

// Metric alerts
bool setMetricAlert(const char* key, float minValue, float maxValue, CH32MetricAlertCallback callback);
bool clearMetricAlert(const char* key);
void clearMetricAlerts();

// System health
uint8_t getSystemHealth() const;
bool    hasActiveDiagnosticWarning() const;
```

### API notes

- `recordFlag(name, code)`: `code` is stored as an 8-bit value. Values below `0` are clipped to `0`; values above `255` are clipped to `255`.
- Metric alert keys use the same canonicalization as BlackBox keys. Valid characters: `a-z A-Z 0-9 _ - .`; others become `_`. Keys exceeding the visible key length limit are rejected.
- `sendHeartbeat(nodeId, status)`: `status` is an application-defined 8-bit value. CrashHive32 does not impose a fixed status enum in v1.1.1.
- To preserve a full `uint32_t nodeId`, use neighbor APIs and Serial output; the BlackBox flag code is small status/code data, not a full node ID store.

---

## Examples

| Example | Description |
|---------|-------------|
| `01_BlackBox_Basic` | Basic BlackBox usage and JSON/report output. |
| `02_Heartbeat_Monitor` | ESP-NOW peer registration and heartbeat send. |
| `03_Neighbor_Watchdog` | Simulated heartbeat watchdog. |
| `04_Full_Demo_Three_Nodes` | Three-node conceptual demo. |
| `06_Field_Diagnostics` | Single-board simulated diagnostics: reset reason, boot storm, metric alert, UNKNOWN-to-SILENT transition, recovery, health, packet loss, and JSON. |

---

## Limitations

- RTC/NOINIT memory is best-effort only and is not durable across all power-loss or brownout conditions.
- CrashHive32 is not a permanent data logger.
- CrashHive32 is not safety-certified.
- ESP-NOW send callback indicates MAC-layer send status, not guaranteed application-level delivery.
- Packet loss is estimated from received sequence gaps.
- No RSSI scoring in this version.
- No full mesh routing.
- No persistent Flash/NVS/LittleFS logging.

---

## License

MIT License — see [LICENSE](LICENSE) for details.
