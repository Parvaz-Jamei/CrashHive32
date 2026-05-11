#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "CH32Types.h"

#ifndef CH32_CHUNK_WRITER_DEFINED
#define CH32_CHUNK_WRITER_DEFINED
typedef void (*CH32ChunkWriter)(const char* chunk);
#endif

class CH32BlackBox {
 public:
  CH32BlackBox();

  bool begin();
  bool recordMetric(const char* name, float value);
  bool recordFlag(const char* name, int code);
  bool hasPreviousEvents() const;
  void printReport(Stream& out) const;
  bool exportJson(CH32ChunkWriter writer) const;
  void printJson(Stream& out) const;
  uint8_t getResetReasonCode() const;
  const char* getResetReasonName() const;

 private:
  bool isStorageValid() const;
  bool isEventValid(const ch32_blackbox_event_t& event) const;
  void resetStorage();
  void updateStorageCrc();
  void updateEventCrc(ch32_blackbox_event_t& event) const;
  uint8_t computeHeaderCrc() const;
  uint8_t computeEventCrc(const ch32_blackbox_event_t& event) const;
  bool appendEvent(uint8_t type, const char* key, float value, uint8_t code);
  uint16_t oldestIndex() const;
  uint16_t eventIndexAt(uint16_t chronologicalIndex) const;
  void printJsonEvent(Stream& out, const ch32_blackbox_event_t& event) const;

  bool previousEvents_;
  uint8_t resetReason_;
  bool storageReady_;
};
