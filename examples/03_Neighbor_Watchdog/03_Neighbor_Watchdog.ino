#include <CrashHive32.h>

static const uint32_t neighborNodeId = 1UL;
static uint32_t lastSimulatedHeartbeatMs = 0UL;
static bool pauseSimulation = false;
static bool pauseStarted = false;
static bool recoveryDone = false;

static void onSilent(uint32_t nodeId) {
  Serial.print("Neighbor silent: ");
  Serial.println(nodeId);
}

static void onRecovered(uint32_t nodeId) {
  Serial.print("Neighbor recovered: ");
  Serial.println(nodeId);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("CrashHive32 Phase 03 Neighbor Watchdog example");
  Serial.println("This example simulates heartbeats without requiring a second ESP32.");

  if (!CrashHive32.begin()) {
    Serial.println("BlackBox init failed");
  }

  CrashHive32.onNeighborSilent(onSilent);
  CrashHive32.onNeighborRecovered(onRecovered);

  if (CrashHive32.watchNeighbor(neighborNodeId, 15000UL)) {
    Serial.println("Watching neighbor node 1 with 15 second timeout");
  } else {
    Serial.println("Could not register neighbor node 1");
  }

  CrashHive32.noteHeartbeat(neighborNodeId);
  lastSimulatedHeartbeatMs = millis();
}

void loop() {
  const uint32_t now = millis();

  // Simulate regular heartbeats, pause once to demonstrate the silent
  // transition, then recover once and continue normal simulated heartbeats.
  if (!pauseSimulation && (now - lastSimulatedHeartbeatMs >= 5000UL)) {
    lastSimulatedHeartbeatMs = now;
    CrashHive32.noteHeartbeat(neighborNodeId);
    Serial.println("Simulated heartbeat noted for node 1");
  }

  if (!pauseStarted && now >= 20000UL) {
    pauseStarted = true;
    pauseSimulation = true;
    Serial.println("Heartbeat simulation paused; watchdog should mark node silent.");
  }

  if (pauseStarted && !recoveryDone && now >= 40000UL) {
    recoveryDone = true;
    pauseSimulation = false;
    lastSimulatedHeartbeatMs = now;
    CrashHive32.noteHeartbeat(neighborNodeId);
    Serial.println("Heartbeat simulation resumed.");
  }

  CrashHive32.tick();
}
