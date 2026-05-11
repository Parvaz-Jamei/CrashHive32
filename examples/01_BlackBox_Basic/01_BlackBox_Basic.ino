#include <CrashHive32.h>

static void writeChunk(const char* chunk) {
  Serial.print(chunk);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("CrashHive32 Phase 01 BlackBox basic example");

  if (!CrashHive32.begin()) {
    Serial.println("BlackBox init failed");
    return;
  }

  if (CrashHive32.hasPreviousEvents()) {
    Serial.println("Previous RTC/NOINIT events found");
  } else {
    Serial.println("No previous RTC/NOINIT events found");
  }

  CrashHive32.recordMetric("temp_c", 24.5F);
  CrashHive32.recordFlag("boot_ok", 1);

  CrashHive32.printReport(Serial);
  Serial.println();
  Serial.println("Chunked JSON export:");
  CrashHive32.exportJson(writeChunk);
  Serial.println();
}

void loop() {
}
