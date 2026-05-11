#include <Arduino.h>
#include <CrashHive32.h>

// CrashHive32 Phase 04 full demo.
// This sketch is intentionally compile-ready without a real peer. Real ESP-NOW
// delivery requires multiple ESP32 boards and replacing the peer MAC values.
// This is not a full mesh routing or self-healing network example.

static const uint32_t localNodeId = 1UL;
static const uint8_t localStatusOk = 1U;

// Replace these with the peer ESP32 station MAC addresses for real delivery.
static const uint8_t peerNode2Mac[6] = {0x24, 0x6F, 0x28, 0x00, 0x00, 0x02};
static const uint8_t peerNode3Mac[6] = {0x24, 0x6F, 0x28, 0x00, 0x00, 0x03};

static const uint32_t node2Id = 2UL;
static const uint32_t node3Id = 3UL;

static uint32_t lastSimulatedNode2Ms = 0UL;
static uint32_t lastSimulatedNode3Ms = 0UL;
static bool pauseNode2 = false;

static void onNeighborSilent(uint32_t nodeId) {
  Serial.print("Neighbor silent: ");
  Serial.println(nodeId);
}

static void onNeighborRecovered(uint32_t nodeId) {
  Serial.print("Neighbor recovered: ");
  Serial.println(nodeId);
}

static void onAdaptiveResponse(uint32_t nodeId, uint8_t reason) {
  Serial.print("Adaptive response hook: node=");
  Serial.print(nodeId);
  Serial.print(" reason=");
  if (reason == CH32_ADAPTIVE_REASON_NEIGHBOR_SILENT) {
    Serial.println("neighbor_silent");
  } else if (reason == CH32_ADAPTIVE_REASON_NEIGHBOR_RECOVERED) {
    Serial.println("neighbor_recovered");
  } else {
    Serial.println(reason);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("CrashHive32 Phase 04 full demo with three logical nodes");
  Serial.println("Hardware note: real ESP-NOW delivery needs at least two ESP32 boards.");

  if (!CrashHive32.begin()) {
    Serial.println("BlackBox init failed");
  }

  if (CrashHive32.beginEspNow()) {
    Serial.println("ESP-NOW initialized");
  } else {
    Serial.println("ESP-NOW init failed or not available in this build");
  }

  if (CrashHive32.addPeer(peerNode2Mac)) {
    Serial.println("Peer node 2 registered");
  } else {
    Serial.println("Peer node 2 not registered; sketch still demonstrates local logic");
  }

  if (CrashHive32.addPeer(peerNode3Mac)) {
    Serial.println("Peer node 3 registered");
  } else {
    Serial.println("Peer node 3 not registered; sketch still demonstrates local logic");
  }

  CrashHive32.setHeartbeatInterval(5000UL);
  CrashHive32.watchNeighbor(node2Id, 15000UL);
  CrashHive32.watchNeighbor(node3Id, 15000UL);

  CrashHive32.onNeighborSilent(onNeighborSilent);
  CrashHive32.onNeighborRecovered(onNeighborRecovered);
  CrashHive32.onAdaptiveResponse(onAdaptiveResponse);

  CrashHive32.noteHeartbeat(node2Id);
  CrashHive32.noteHeartbeat(node3Id);
  lastSimulatedNode2Ms = millis();
  lastSimulatedNode3Ms = millis();

  CrashHive32.sendHeartbeat(localNodeId, localStatusOk);
}

void loop() {
  const uint32_t now = millis();

  // Simulate node 2 becoming silent and later recovering. This keeps the demo
  // understandable even when only one ESP32 is available for compile checks.
  if (!pauseNode2 && (now - lastSimulatedNode2Ms >= 5000UL)) {
    lastSimulatedNode2Ms = now;
    CrashHive32.noteHeartbeat(node2Id);
    Serial.println("Simulated heartbeat from node 2");
  }

  if (now >= 20000UL && now < 40000UL) {
    pauseNode2 = true;
  } else if (pauseNode2 && now >= 40000UL) {
    pauseNode2 = false;
    lastSimulatedNode2Ms = now;
    CrashHive32.noteHeartbeat(node2Id);
    Serial.println("Simulated node 2 recovery heartbeat");
  }

  if (now - lastSimulatedNode3Ms >= 7000UL) {
    lastSimulatedNode3Ms = now;
    CrashHive32.noteHeartbeat(node3Id);
    Serial.println("Simulated heartbeat from node 3");
  }

  CrashHive32.tick();
}
