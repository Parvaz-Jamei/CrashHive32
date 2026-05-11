#include <CrashHive32.h>

// Replace this MAC address with the receiver ESP32 peer MAC.
// The sketch compiles without a real peer; sends may fail until a peer exists.
static const uint8_t peerMac[6] = {0x24, 0x6F, 0x28, 0x00, 0x00, 0x01};

static const uint32_t nodeId = 0x00000001UL;
static const uint8_t nodeStatusOk = 1U;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("CrashHive32 Phase 02 ESP-NOW heartbeat example");

  if (!CrashHive32.begin()) {
    Serial.println("BlackBox init failed");
  }

  if (CrashHive32.beginEspNow()) {
    Serial.println("ESP-NOW initialized in station mode");
  } else {
    Serial.println("ESP-NOW init failed");
  }

  if (CrashHive32.addPeer(peerMac)) {
    Serial.println("ESP-NOW peer registered");
  } else {
    Serial.println("ESP-NOW peer registration failed");
  }

  CrashHive32.setHeartbeatInterval(5000UL);

  if (CrashHive32.sendHeartbeat(nodeId, nodeStatusOk)) {
    Serial.println("Initial heartbeat queued");
  } else {
    Serial.println("Initial heartbeat could not be queued");
  }

  CrashHive32.printReport(Serial);
}

void loop() {
  CrashHive32.tick();

  static uint32_t lastPrintMs = 0;
  const uint32_t now = millis();
  if (now - lastPrintMs >= 5000UL) {
    lastPrintMs = now;
    Serial.println("Heartbeat tick processed");
  }
}
