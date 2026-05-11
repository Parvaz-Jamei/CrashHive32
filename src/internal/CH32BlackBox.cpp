#include "CH32BlackBox.h"

#include <Arduino.h>
#include <esp_system.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "CH32CRC.h"
#include "CH32Config.h"
#include "CH32Key.h"

#ifndef RTC_NOINIT_ATTR
#define RTC_NOINIT_ATTR
#endif

RTC_NOINIT_ATTR static ch32_blackbox_header_t g_ch32_blackbox_header;
RTC_NOINIT_ATTR static ch32_blackbox_event_t g_ch32_blackbox_events[CH32_BLACKBOX_CAPACITY];

static bool ch32_is_cold_boot_reason(uint8_t reason) {
  return reason == static_cast<uint8_t>(ESP_RST_POWERON);
}

static const char* ch32_reset_reason_name(uint8_t reason) {
  switch (static_cast<esp_reset_reason_t>(reason)) {
    case ESP_RST_UNKNOWN:   return "unknown";
    case ESP_RST_POWERON:   return "power_on";
    case ESP_RST_EXT:       return "external";
    case ESP_RST_SW:        return "software";
    case ESP_RST_PANIC:     return "panic";
    case ESP_RST_INT_WDT:   return "interrupt_watchdog";
    case ESP_RST_TASK_WDT:  return "task_watchdog";
    case ESP_RST_WDT:       return "watchdog";
    case ESP_RST_DEEPSLEEP: return "deep_sleep";
    case ESP_RST_BROWNOUT:  return "brownout";
    case ESP_RST_SDIO:      return "sdio";
    default:                return "unknown";
  }
}

static const char* ch32_event_type_name(uint8_t type) {
  switch (type) {
    case CH32_EVENT_TYPE_METRIC: return "metric";
    case CH32_EVENT_TYPE_FLAG:   return "flag";
    case CH32_EVENT_TYPE_BOOT:   return "boot";
    case CH32_EVENT_TYPE_RESET:  return "reset";
    default:                     return "unknown";
  }
}


CH32BlackBox::CH32BlackBox()
    : previousEvents_(false),
      resetReason_(static_cast<uint8_t>(ESP_RST_UNKNOWN)),
      storageReady_(false) {}

bool CH32BlackBox::begin() {
  resetReason_ = static_cast<uint8_t>(esp_reset_reason());

  const bool valid = isStorageValid();
  const bool coldBoot = ch32_is_cold_boot_reason(resetReason_);
  previousEvents_ = valid && !coldBoot && g_ch32_blackbox_header.eventCount > 0U;

  if (!valid || coldBoot) {
    resetStorage();
  }
  storageReady_ = true;

  appendEvent(CH32_EVENT_TYPE_RESET, "reset", 0.0F, resetReason_);
  appendEvent(CH32_EVENT_TYPE_BOOT, "boot", static_cast<float>(millis()), 0U);
  return true;
}

bool CH32BlackBox::recordMetric(const char* name, float value) {
  if (name == nullptr || name[0] == '\0') {
    return false;
  }
  return appendEvent(CH32_EVENT_TYPE_METRIC, name, value, 0U);
}

bool CH32BlackBox::recordFlag(const char* name, int code) {
  if (name == nullptr || name[0] == '\0') {
    return false;
  }
  if (code < 0) {
    code = 0;
  }
  if (code > 255) {
    code = 255;
  }
  return appendEvent(CH32_EVENT_TYPE_FLAG, name, 0.0F, static_cast<uint8_t>(code));
}

bool CH32BlackBox::hasPreviousEvents() const {
  return previousEvents_;
}

void CH32BlackBox::printReport(Stream& out) const {
  out.println(F("CrashHive32 BlackBox Report"));
  out.print(F("schema_version: "));
  out.println(g_ch32_blackbox_header.version);
  out.print(F("reset_reason: "));
  out.print(ch32_reset_reason_name(resetReason_));
  out.print(F(" ("));
  out.print(resetReason_);
  out.println(F(")"));
  out.print(F("previous_events: "));
  out.println(previousEvents_ ? F("yes") : F("no"));
  out.print(F("event_count: "));
  out.println(g_ch32_blackbox_header.eventCount);
  out.print(F("write_index: "));
  out.println(g_ch32_blackbox_header.writeIndex);

  for (uint16_t i = 0U; i < g_ch32_blackbox_header.eventCount; ++i) {
    const ch32_blackbox_event_t& event = g_ch32_blackbox_events[eventIndexAt(i)];
    out.print(F("["));
    out.print(i);
    out.print(F("] t_ms="));
    out.print(event.t_ms);
    out.print(F(" type="));
    out.print(ch32_event_type_name(event.type));
    out.print(F(" key="));
    out.print(event.key);
    out.print(F(" value="));
    out.print(event.value, 3);
    out.print(F(" code="));
    out.println(event.code);
  }
}

bool CH32BlackBox::exportJson(CH32ChunkWriter writer) const {
  if (writer == nullptr) {
    return false;
  }

  char chunk[96];
  writer("{\"schema\":\"crashhive32.blackbox.v3\"");
  writer(",\"version\":");
  snprintf(chunk, sizeof(chunk), "%u", static_cast<unsigned>(g_ch32_blackbox_header.version));
  writer(chunk);
  writer(",\"reset_reason\":\"");
  writer(ch32_reset_reason_name(resetReason_));
  writer("\"");
  writer(",\"reset_code\":");
  snprintf(chunk, sizeof(chunk), "%u", static_cast<unsigned>(resetReason_));
  writer(chunk);
  writer(",\"previous_events\":");
  writer(previousEvents_ ? "true" : "false");
  writer(",\"event_count\":");
  snprintf(chunk, sizeof(chunk), "%u", static_cast<unsigned>(g_ch32_blackbox_header.eventCount));
  writer(chunk);
  writer(",\"events\":[");

  for (uint16_t i = 0U; i < g_ch32_blackbox_header.eventCount; ++i) {
    if (i > 0U) {
      writer(",");
    }
    const ch32_blackbox_event_t& event = g_ch32_blackbox_events[eventIndexAt(i)];
    writer("{\"t_ms\":");
    snprintf(chunk, sizeof(chunk), "%lu", static_cast<unsigned long>(event.t_ms));
    writer(chunk);
    writer(",\"type\":\"");
    writer(ch32_event_type_name(event.type));
    writer("\",\"key\":\"");
    writer(event.key);
    writer("\",\"value\":");
    snprintf(chunk, sizeof(chunk), "%.3f", static_cast<double>(event.value));
    writer(chunk);
    writer(",\"code\":");
    snprintf(chunk, sizeof(chunk), "%u", static_cast<unsigned>(event.code));
    writer(chunk);
    writer("}");
  }

  writer("]}");
  return true;
}

void CH32BlackBox::printJson(Stream& out) const {
  char chunk[96];
  out.print(F("{\"schema\":\"crashhive32.blackbox.v3\""));
  out.print(F(",\"version\":"));
  snprintf(chunk, sizeof(chunk), "%u", static_cast<unsigned>(g_ch32_blackbox_header.version));
  out.print(chunk);
  out.print(F(",\"reset_reason\":\""));
  out.print(ch32_reset_reason_name(resetReason_));
  out.print(F("\""));
  out.print(F(",\"reset_code\":"));
  snprintf(chunk, sizeof(chunk), "%u", static_cast<unsigned>(resetReason_));
  out.print(chunk);
  out.print(F(",\"previous_events\":"));
  out.print(previousEvents_ ? F("true") : F("false"));
  out.print(F(",\"event_count\":"));
  snprintf(chunk, sizeof(chunk), "%u", static_cast<unsigned>(g_ch32_blackbox_header.eventCount));
  out.print(chunk);
  out.print(F(",\"events\":["));

  for (uint16_t i = 0U; i < g_ch32_blackbox_header.eventCount; ++i) {
    if (i > 0U) {
      out.print(F(","));
    }
    printJsonEvent(out, g_ch32_blackbox_events[eventIndexAt(i)]);
  }
  out.print(F("]}"));
}

bool CH32BlackBox::isStorageValid() const {
  if (g_ch32_blackbox_header.magic != CH32_BLACKBOX_MAGIC) {
    return false;
  }
  if (g_ch32_blackbox_header.version != CH32_BLACKBOX_VERSION) {
    return false;
  }
  if (g_ch32_blackbox_header.eventCount > CH32_BLACKBOX_CAPACITY) {
    return false;
  }
  if (g_ch32_blackbox_header.writeIndex >= CH32_BLACKBOX_CAPACITY) {
    return false;
  }
  if (g_ch32_blackbox_header.eventCount < CH32_BLACKBOX_CAPACITY &&
      g_ch32_blackbox_header.writeIndex != g_ch32_blackbox_header.eventCount) {
    return false;
  }
  if (g_ch32_blackbox_header.resetReason > static_cast<uint8_t>(ESP_RST_SDIO)) {
    return false;
  }
  if (g_ch32_blackbox_header.crc8 != computeHeaderCrc()) {
    return false;
  }
  for (uint16_t i = 0U; i < g_ch32_blackbox_header.eventCount; ++i) {
    if (!isEventValid(g_ch32_blackbox_events[eventIndexAt(i)])) {
      return false;
    }
  }
  return true;
}

bool CH32BlackBox::isEventValid(const ch32_blackbox_event_t& event) const {
  return event.crc8 == computeEventCrc(event);
}

void CH32BlackBox::resetStorage() {
  memset(g_ch32_blackbox_events, 0, sizeof(g_ch32_blackbox_events));
  g_ch32_blackbox_header.magic = CH32_BLACKBOX_MAGIC;
  g_ch32_blackbox_header.version = CH32_BLACKBOX_VERSION;
  g_ch32_blackbox_header.eventCount = 0U;
  g_ch32_blackbox_header.writeIndex = 0U;
  g_ch32_blackbox_header.resetReason = resetReason_;
  updateStorageCrc();
  storageReady_ = true;
}

void CH32BlackBox::updateStorageCrc() {
  g_ch32_blackbox_header.crc8 = 0U;
  g_ch32_blackbox_header.crc8 = computeHeaderCrc();
}

void CH32BlackBox::updateEventCrc(ch32_blackbox_event_t& event) const {
  event.crc8 = 0U;
  event.crc8 = computeEventCrc(event);
}

uint8_t CH32BlackBox::computeHeaderCrc() const {
  return ch32_crc8_buffer(reinterpret_cast<const uint8_t*>(&g_ch32_blackbox_header),
                          offsetof(ch32_blackbox_header_t, crc8),
                          0U);
}

uint8_t CH32BlackBox::computeEventCrc(const ch32_blackbox_event_t& event) const {
  return ch32_crc8_buffer(reinterpret_cast<const uint8_t*>(&event),
                          offsetof(ch32_blackbox_event_t, crc8),
                          0U);
}

bool CH32BlackBox::appendEvent(uint8_t type, const char* key, float value, uint8_t code) {
  if (!storageReady_) {
    resetStorage();
    previousEvents_ = false;
    storageReady_ = true;
  }

  const uint16_t index = g_ch32_blackbox_header.writeIndex;
  ch32_blackbox_event_t& event = g_ch32_blackbox_events[index];
  memset(&event, 0, sizeof(event));
  event.t_ms = millis();
  event.type = type;
  ch32_copy_key(event.key, sizeof(event.key), key);
  event.value = value;
  event.code = code;
  updateEventCrc(event);

  g_ch32_blackbox_header.writeIndex = static_cast<uint16_t>((index + 1U) % CH32_BLACKBOX_CAPACITY);
  if (g_ch32_blackbox_header.eventCount < CH32_BLACKBOX_CAPACITY) {
    ++g_ch32_blackbox_header.eventCount;
  }
  g_ch32_blackbox_header.resetReason = resetReason_;
  updateStorageCrc();
  return true;
}

uint8_t CH32BlackBox::getResetReasonCode() const {
  return resetReason_;
}

const char* CH32BlackBox::getResetReasonName() const {
  return ch32_reset_reason_name(resetReason_);
}

uint16_t CH32BlackBox::oldestIndex() const {
  if (g_ch32_blackbox_header.eventCount < CH32_BLACKBOX_CAPACITY) {
    return 0U;
  }
  return g_ch32_blackbox_header.writeIndex;
}

uint16_t CH32BlackBox::eventIndexAt(uint16_t chronologicalIndex) const {
  const uint16_t base = oldestIndex();
  return static_cast<uint16_t>((base + chronologicalIndex) % CH32_BLACKBOX_CAPACITY);
}

void CH32BlackBox::printJsonEvent(Stream& out, const ch32_blackbox_event_t& event) const {
  char chunk[96];
  out.print(F("{\"t_ms\":"));
  snprintf(chunk, sizeof(chunk), "%lu", static_cast<unsigned long>(event.t_ms));
  out.print(chunk);
  out.print(F(",\"type\":\""));
  out.print(ch32_event_type_name(event.type));
  out.print(F("\",\"key\":\""));
  out.print(event.key);
  out.print(F("\",\"value\":"));
  snprintf(chunk, sizeof(chunk), "%.3f", static_cast<double>(event.value));
  out.print(chunk);
  out.print(F(",\"code\":"));
  snprintf(chunk, sizeof(chunk), "%u", static_cast<unsigned>(event.code));
  out.print(chunk);
  out.print(F("}"));
}
