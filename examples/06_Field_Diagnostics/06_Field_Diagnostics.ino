#include <CrashHive32.h>

static const uint32_t MOTOR_NODE_ID = 2;
static const uint32_t PUMP_NODE_ID = 3;

void printHealth(const char* label, uint32_t nodeId) {
  uint8_t health = 0;
  Serial.print(label);
  Serial.print(" state=");
  Serial.print(CrashHive32.getNeighborStateName(nodeId));
  Serial.print(" health=");
  if (CrashHive32.getNeighborHealth(nodeId, &health)) {
    Serial.print(health);
  } else {
    Serial.print("unknown");
  }
  Serial.print(" ageMs=");
  Serial.print(CrashHive32.getNeighborAgeMs(nodeId));
  Serial.print(" silentMs=");
  Serial.println(CrashHive32.getNeighborSilentMs(nodeId));
}

void onTempAlert(const char* key, float value, float minValue, float maxValue) {
  Serial.print("[METRIC ALERT] ");
  Serial.print(key);
  Serial.print(" value=");
  Serial.print(value);
  Serial.print(" range=");
  Serial.print(minValue);
  Serial.print("..");
  Serial.println(maxValue);
}

void onNeighborSilent(uint32_t nodeId) {
  Serial.print("[SILENT/MISSING] node=");
  Serial.print(nodeId);
  Serial.print(" silentMs=");
  Serial.println(CrashHive32.getNeighborSilentMs(nodeId));
}

void onNeighborRecovered(uint32_t nodeId) {
  Serial.print("[RECOVERED] node=");
  Serial.println(nodeId);
}

void onAdaptive(uint32_t nodeId, uint8_t reason) {
  Serial.print("[ADAPTIVE] node=");
  Serial.print(nodeId);
  Serial.print(" reason=");
  Serial.println(CrashHive32.getAdaptiveReasonName(reason));
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  CrashHive32.begin();

  Serial.print("Reset reason: ");
  Serial.println(CrashHive32.getResetReasonName());

  if (CrashHive32.isBootStorm()) {
    Serial.println("Boot storm detected");
  }

  // Normally let CrashHive32.tick() call markBootStable() automatically
  // after CH32_BOOT_STABLE_MS. Only call markBootStable() manually after
  // your own application-specific stability criteria have passed.

  CrashHive32.setMetricAlert("motor_temp", 0.0F, 70.0F, onTempAlert);

  // Single-board simulation: MOTOR receives simulated heartbeats, while
  // PUMP starts UNKNOWN and becomes SILENT/MISSING after its first timeout.
  CrashHive32.watchNeighborEvery(MOTOR_NODE_ID, 5000UL);
  CrashHive32.watchNeighborEvery(PUMP_NODE_ID, 4000UL);

  CrashHive32.onNeighborSilent(onNeighborSilent);
  CrashHive32.onNeighborRecovered(onNeighborRecovered);
  CrashHive32.onAdaptiveResponse(onAdaptive);
}

void loop() {
  CrashHive32.tick();

  static uint32_t last = 0;
  static uint8_t cycle = 0;
  const uint32_t now = millis();

  if (now - last >= 5000UL) {
    last = now;
    ++cycle;

    const float motorTemp = cycle < 3U ? 68.0F : 72.0F;
    CrashHive32.recordMetric("motor_temp", motorTemp);

    // MOTOR is alive for the first two cycles, then becomes late/silent.
    if (cycle <= 2U) {
      CrashHive32.noteHeartbeat(MOTOR_NODE_ID);
    }

    // PUMP demonstrates recovery after it first became missing/silent.
    if (cycle == 5U) {
      CrashHive32.noteHeartbeat(PUMP_NODE_ID);
    }

    Serial.print("System health=");
    Serial.println(CrashHive32.getSystemHealth());

    printHealth("Motor", MOTOR_NODE_ID);
    printHealth("Pump", PUMP_NODE_ID);

    if (!CrashHive32.isNeighborHealthy(MOTOR_NODE_ID)) {
      Serial.println("Motor node needs attention");
    }

    Serial.print("Motor packet loss=");
    Serial.println(CrashHive32.getNeighborPacketLoss(MOTOR_NODE_ID));

    Serial.println("JSON:");
    CrashHive32.printJson(Serial);
    Serial.println();
  }
}
