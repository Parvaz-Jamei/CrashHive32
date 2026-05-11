#include "CH32BootState.h"

#include <Arduino.h>
#include <stddef.h>
#include <string.h>

#include "CH32CRC.h"

#ifndef RTC_NOINIT_ATTR
#define RTC_NOINIT_ATTR
#endif

#define CH32_BOOT_STATE_MAGIC 0x43484253UL
#define CH32_BOOT_STATE_VERSION 1U

typedef struct __attribute__((packed)) {
  uint32_t magic;
  uint8_t version;
  uint8_t bootStreak;
  bool bootStorm;
  bool stableMarked;
  uint8_t crc8;
} ch32_boot_state_t;

RTC_NOINIT_ATTR static ch32_boot_state_t g_ch32_boot_state;

CH32BootState::CH32BootState()
    : blackBox_(nullptr),
      bootMs_(0U) {}

void CH32BootState::begin(CH32BlackBox* blackBox) {
  blackBox_ = blackBox;
  bootMs_ = millis();

  if (!isValid()) {
    reset();
  }

  if (g_ch32_boot_state.bootStreak < 255U) {
    ++g_ch32_boot_state.bootStreak;
  }
  g_ch32_boot_state.stableMarked = false;
  if (g_ch32_boot_state.bootStreak >= CH32_BOOT_STORM_THRESHOLD) {
    g_ch32_boot_state.bootStorm = true;
    if (blackBox_ != nullptr) {
      blackBox_->recordFlag("_ch32_bootstorm", g_ch32_boot_state.bootStreak);
    }
  }
  updateCrc();
}

bool CH32BootState::tick() {
  if (!isValid()) {
    reset();
    return false;
  }
  if (!g_ch32_boot_state.stableMarked && (millis() - bootMs_) >= CH32_BOOT_STABLE_MS) {
    markBootStable();
    return true;
  }
  return false;
}

bool CH32BootState::isBootStorm() const {
  return isValid() && g_ch32_boot_state.bootStorm;
}

uint8_t CH32BootState::getBootStreak() const {
  return isValid() ? g_ch32_boot_state.bootStreak : 0U;
}

void CH32BootState::markBootStable() {
  if (!isValid()) {
    reset();
  }
  g_ch32_boot_state.bootStreak = 0U;
  g_ch32_boot_state.bootStorm = false;
  g_ch32_boot_state.stableMarked = true;
  updateCrc();
}

bool CH32BootState::isValid() const {
  if (g_ch32_boot_state.magic != CH32_BOOT_STATE_MAGIC) {
    return false;
  }
  if (g_ch32_boot_state.version != CH32_BOOT_STATE_VERSION) {
    return false;
  }
  return g_ch32_boot_state.crc8 == computeCrc();
}

void CH32BootState::reset() {
  memset(&g_ch32_boot_state, 0, sizeof(g_ch32_boot_state));
  g_ch32_boot_state.magic = CH32_BOOT_STATE_MAGIC;
  g_ch32_boot_state.version = CH32_BOOT_STATE_VERSION;
  updateCrc();
}

void CH32BootState::updateCrc() {
  g_ch32_boot_state.crc8 = 0U;
  g_ch32_boot_state.crc8 = computeCrc();
}

uint8_t CH32BootState::computeCrc() const {
  return ch32_crc8_buffer(reinterpret_cast<const uint8_t*>(&g_ch32_boot_state),
                          offsetof(ch32_boot_state_t, crc8),
                          0U);
}
