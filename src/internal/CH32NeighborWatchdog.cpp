#include "CH32NeighborWatchdog.h"

#include <Arduino.h>
#include <stddef.h>
#include <string.h>

CH32NeighborWatchdog::CH32NeighborWatchdog()
    : blackBox_(nullptr),
      entries_{},
      silentCallback_(nullptr),
      recoveredCallback_(nullptr),
      adaptiveCallback_(nullptr) {}

void CH32NeighborWatchdog::attach(CH32BlackBox* blackBox) {
  blackBox_ = blackBox;
}

bool CH32NeighborWatchdog::watchNeighbor(uint32_t nodeId, uint32_t timeoutMs) {
  if (nodeId == 0U) {
    return false;
  }
  if (timeoutMs < CH32_NEIGHBOR_MIN_TIMEOUT_MS) {
    timeoutMs = CH32_NEIGHBOR_MIN_TIMEOUT_MS;
  }

  ch32_neighbor_entry_t* entry = find(nodeId);
  if (entry == nullptr) {
    entry = findFreeSlot();
  }
  if (entry == nullptr) {
    if (blackBox_ != nullptr) {
      blackBox_->recordFlag("_ch32_wd_full", 1);
    }
    return false;
  }

  memset(entry, 0, sizeof(*entry));
  entry->nodeId = nodeId;
  entry->timeoutMs = timeoutMs;
  entry->lastSeenMs = millis();
  entry->state = CH32_NEIGHBOR_UNKNOWN;
  entry->active = true;
  entry->health = 0U;
  entry->seenOnce = false;

#if CH32_LOG_INTERNAL_SUCCESS_EVENTS
  if (blackBox_ != nullptr) {
    blackBox_->recordFlag("_ch32_wd_watch", eventCode(nodeId));
  }
#endif
  return true;
}

bool CH32NeighborWatchdog::removeNeighbor(uint32_t nodeId) {
  ch32_neighbor_entry_t* entry = find(nodeId);
  if (entry == nullptr) {
    return false;
  }
  memset(entry, 0, sizeof(*entry));
  return true;
}

bool CH32NeighborWatchdog::noteHeartbeat(uint32_t nodeId) {
  ch32_neighbor_entry_t* entry = find(nodeId);
  if (entry == nullptr || !entry->active) {
    return false;
  }

  const uint8_t previousState = entry->state;
  const bool wasDegraded = entry->degraded;
  const bool wasSeenOnce = entry->seenOnce;
  entry->seenOnce = true;
  entry->lastSeenMs = millis();
  entry->state = CH32_NEIGHBOR_ALIVE;

  if (!wasSeenOnce || previousState == CH32_NEIGHBOR_SILENT) {
    entry->health = 80U;
  } else if (entry->health < 100U) {
    const uint16_t raised = static_cast<uint16_t>(entry->health) + 15U;
    entry->health = static_cast<uint8_t>(raised > 100U ? 100U : raised);
  }
  if (entry->health >= 70U) {
    entry->degraded = false;
    entry->degradedEventFired = false;
    if (wasDegraded && !entry->healthyEventFired) {
      fireAdaptive(entry->nodeId, CH32_ADAPTIVE_REASON_NEIGHBOR_HEALTHY);
      entry->healthyEventFired = true;
    }
  }
  if (previousState == CH32_NEIGHBOR_SILENT) {
    entry->pendingRecovered = true;
  }
  return true;
}

bool CH32NeighborWatchdog::noteHeartbeat(uint32_t nodeId, uint16_t seq, uint8_t status) {
  (void)status;
  ch32_neighbor_entry_t* entry = find(nodeId);
  if (entry == nullptr || !entry->active) {
    return false;
  }

  const uint8_t previousState = entry->state;
  const bool wasDegraded = entry->degraded;
  const bool wasSeenOnce = entry->seenOnce;
  entry->seenOnce = true;
  entry->lastSeenMs = millis();
  entry->state = CH32_NEIGHBOR_ALIVE;
  updatePacketLoss(*entry, seq);

  if (!wasSeenOnce || previousState == CH32_NEIGHBOR_SILENT) {
    entry->health = 80U;
  } else if (entry->health < 100U) {
    const uint16_t raised = static_cast<uint16_t>(entry->health) + 15U;
    entry->health = static_cast<uint8_t>(raised > 100U ? 100U : raised);
  }

  const uint8_t loss = packetLossFor(*entry);
  if (loss > 20U && entry->health > 40U) {
    entry->health = 40U;
  } else if (loss > 5U && entry->health > 65U) {
    entry->health = 65U;
  }

  if (entry->health >= 70U) {
    entry->degraded = false;
    entry->degradedEventFired = false;
    if (wasDegraded && !entry->healthyEventFired) {
      fireAdaptive(entry->nodeId, CH32_ADAPTIVE_REASON_NEIGHBOR_HEALTHY);
      entry->healthyEventFired = true;
    }
  }

  if (previousState == CH32_NEIGHBOR_SILENT) {
    entry->pendingRecovered = true;
  }
  return true;
}

uint8_t CH32NeighborWatchdog::getNeighborState(uint32_t nodeId) const {
  const ch32_neighbor_entry_t* entry = find(nodeId);
  if (entry == nullptr || !entry->active) {
    return CH32_NEIGHBOR_UNKNOWN;
  }
  if (entry->state == CH32_NEIGHBOR_UNKNOWN) {
    return CH32_NEIGHBOR_UNKNOWN;
  }
  if (entry->state == CH32_NEIGHBOR_SILENT) {
    return CH32_NEIGHBOR_SILENT;
  }
  if (entry->degraded || entry->state == CH32_NEIGHBOR_DEGRADED) {
    return CH32_NEIGHBOR_DEGRADED;
  }
  return CH32_NEIGHBOR_ALIVE;
}

uint32_t CH32NeighborWatchdog::getNeighborAgeMs(uint32_t nodeId) const {
  const ch32_neighbor_entry_t* entry = find(nodeId);
  if (entry == nullptr || !entry->active) {
    return UINT32_MAX;
  }
  return millis() - entry->lastSeenMs;
}

uint32_t CH32NeighborWatchdog::getNeighborSilentMs(uint32_t nodeId) const {
  const ch32_neighbor_entry_t* entry = find(nodeId);
  if (entry == nullptr || !entry->active) {
    return UINT32_MAX;
  }
  if (entry->state != CH32_NEIGHBOR_SILENT) {
    return 0U;
  }
  return millis() - entry->lastSeenMs;
}

uint8_t CH32NeighborWatchdog::getNeighborHealth(uint32_t nodeId) const {
  uint8_t health = 0U;
  if (!getNeighborHealth(nodeId, &health)) {
    return 0U;
  }
  return health;
}

bool CH32NeighborWatchdog::getNeighborHealth(uint32_t nodeId, uint8_t* outHealth) const {
  if (outHealth == nullptr) {
    return false;
  }
  const ch32_neighbor_entry_t* entry = find(nodeId);
  if (entry == nullptr || !entry->active || !entry->seenOnce || entry->state == CH32_NEIGHBOR_UNKNOWN) {
    *outHealth = 0U;
    return false;
  }
  *outHealth = entry->health;
  return true;
}

bool CH32NeighborWatchdog::isNeighborDegraded(uint32_t nodeId) const {
  const ch32_neighbor_entry_t* entry = find(nodeId);
  return entry != nullptr && entry->active && entry->degraded;
}

uint8_t CH32NeighborWatchdog::getNeighborPacketLoss(uint32_t nodeId) const {
  const ch32_neighbor_entry_t* entry = find(nodeId);
  if (entry == nullptr || !entry->active) {
    return 0U;
  }
  return packetLossFor(*entry);
}

uint8_t CH32NeighborWatchdog::minimumKnownHealth() const {
  uint8_t minimum = 255U;
  for (uint8_t i = 0U; i < CH32_NEIGHBOR_MAX_ENTRIES; ++i) {
    if (!entries_[i].active || !entries_[i].seenOnce || entries_[i].state == CH32_NEIGHBOR_UNKNOWN) {
      continue;
    }
    if (entries_[i].health < minimum) {
      minimum = entries_[i].health;
    }
  }
  return minimum;
}

bool CH32NeighborWatchdog::hasActiveWarning() const {
  for (uint8_t i = 0U; i < CH32_NEIGHBOR_MAX_ENTRIES; ++i) {
    if (!entries_[i].active || !entries_[i].seenOnce || entries_[i].state == CH32_NEIGHBOR_UNKNOWN) {
      continue;
    }
    if (entries_[i].state == CH32_NEIGHBOR_SILENT || entries_[i].degraded || entries_[i].health < 70U) {
      return true;
    }
  }
  return false;
}

void CH32NeighborWatchdog::onSilent(CH32NeighborCallback callback) {
  silentCallback_ = callback;
}

void CH32NeighborWatchdog::onRecovered(CH32NeighborCallback callback) {
  recoveredCallback_ = callback;
}

void CH32NeighborWatchdog::onAdaptiveResponse(CH32AdaptiveCallback callback) {
  adaptiveCallback_ = callback;
}

bool CH32NeighborWatchdog::tick() {
  const uint32_t now = millis();
  bool anyActive = false;

  for (uint8_t i = 0U; i < CH32_NEIGHBOR_MAX_ENTRIES; ++i) {
    ch32_neighbor_entry_t& entry = entries_[i];
    if (!entry.active) {
      continue;
    }
    anyActive = true;

    const uint32_t ageMs = now - entry.lastSeenMs;
    updateHealthFromAge(entry, ageMs);

    // A watched neighbor is UNKNOWN only during its first timeout window.
    // If the expected node never appears, report it as SILENT/MISSING once.
    if (!entry.seenOnce && entry.state == CH32_NEIGHBOR_UNKNOWN) {
      if (ageMs < entry.timeoutMs) {
        continue;
      }
      entry.state = CH32_NEIGHBOR_SILENT;
      entry.seenOnce = true;
      entry.health = 0U;
      entry.degraded = false;
      if (blackBox_ != nullptr) {
        blackBox_->recordFlag("_ch32_wd_silent", eventCode(entry.nodeId));
      }
      if (silentCallback_ != nullptr) {
        silentCallback_(entry.nodeId);
      }
      fireAdaptive(entry.nodeId, CH32_ADAPTIVE_REASON_NEIGHBOR_SILENT);
    }

    if (entry.pendingRecovered) {
      entry.pendingRecovered = false;
      if (blackBox_ != nullptr) {
        blackBox_->recordFlag("_ch32_wd_recov", eventCode(entry.nodeId));
      }
      if (recoveredCallback_ != nullptr) {
        recoveredCallback_(entry.nodeId);
      }
      fireAdaptive(entry.nodeId, CH32_ADAPTIVE_REASON_NEIGHBOR_RECOVERED);
    }

    if (entry.state != CH32_NEIGHBOR_SILENT && ageMs >= entry.timeoutMs) {
      entry.state = CH32_NEIGHBOR_SILENT;
      entry.degraded = false;
      if (blackBox_ != nullptr) {
        blackBox_->recordFlag("_ch32_wd_silent", eventCode(entry.nodeId));
      }
      if (silentCallback_ != nullptr) {
        silentCallback_(entry.nodeId);
      }
      fireAdaptive(entry.nodeId, CH32_ADAPTIVE_REASON_NEIGHBOR_SILENT);
    }

    if (entry.state != CH32_NEIGHBOR_SILENT && entry.degraded && !entry.degradedEventFired) {
      fireAdaptive(entry.nodeId, CH32_ADAPTIVE_REASON_NEIGHBOR_DEGRADED);
      entry.degradedEventFired = true;
      entry.healthyEventFired = false;
    }
  }

  return anyActive;
}

ch32_neighbor_entry_t* CH32NeighborWatchdog::find(uint32_t nodeId) {
  for (uint8_t i = 0U; i < CH32_NEIGHBOR_MAX_ENTRIES; ++i) {
    if (entries_[i].active && entries_[i].nodeId == nodeId) {
      return &entries_[i];
    }
  }
  return nullptr;
}

const ch32_neighbor_entry_t* CH32NeighborWatchdog::find(uint32_t nodeId) const {
  for (uint8_t i = 0U; i < CH32_NEIGHBOR_MAX_ENTRIES; ++i) {
    if (entries_[i].active && entries_[i].nodeId == nodeId) {
      return &entries_[i];
    }
  }
  return nullptr;
}

ch32_neighbor_entry_t* CH32NeighborWatchdog::findFreeSlot() {
  for (uint8_t i = 0U; i < CH32_NEIGHBOR_MAX_ENTRIES; ++i) {
    if (!entries_[i].active) {
      return &entries_[i];
    }
  }
  return nullptr;
}

uint8_t CH32NeighborWatchdog::eventCode(uint32_t nodeId) const {
  const uint32_t clipped = nodeId & 0xFFUL;
  return static_cast<uint8_t>(clipped == 0U ? 255U : clipped);
}

void CH32NeighborWatchdog::fireAdaptive(uint32_t nodeId, uint8_t reason) {
  if (adaptiveCallback_ != nullptr) {
    adaptiveCallback_(nodeId, reason);
  }
}

void CH32NeighborWatchdog::updatePacketLoss(ch32_neighbor_entry_t& entry, uint16_t seq) {
  if (!entry.seqInitialized) {
    entry.expectedSeq = static_cast<uint16_t>(seq + 1U);
    ++entry.receivedPackets;
    entry.seqInitialized = true;
    return;
  }

  const uint16_t diff = static_cast<uint16_t>(seq - entry.expectedSeq);
  if (diff == 0U) {
    if (entry.receivedPackets < 65535U) {
      ++entry.receivedPackets;
    }
    entry.expectedSeq = static_cast<uint16_t>(seq + 1U);
  } else if (diff < 32768U) {
    const uint32_t missed = static_cast<uint32_t>(entry.missedPackets) + diff;
    entry.missedPackets = static_cast<uint16_t>(missed > 65535U ? 65535U : missed);
    if (entry.receivedPackets < 65535U) {
      ++entry.receivedPackets;
    }
    entry.expectedSeq = static_cast<uint16_t>(seq + 1U);
  }
  normalizePacketCounters(entry);
}

void CH32NeighborWatchdog::normalizePacketCounters(ch32_neighbor_entry_t& entry) {
  const uint32_t total = static_cast<uint32_t>(entry.receivedPackets) + entry.missedPackets;
  if (total > 1000U) {
    entry.receivedPackets = static_cast<uint16_t>(entry.receivedPackets / 2U);
    entry.missedPackets = static_cast<uint16_t>(entry.missedPackets / 2U);
    if (entry.receivedPackets == 0U) {
      entry.receivedPackets = 1U;
    }
  }
}

void CH32NeighborWatchdog::updateHealthFromAge(ch32_neighbor_entry_t& entry, uint32_t ageMs) {
  if (entry.state == CH32_NEIGHBOR_UNKNOWN) {
    return;
  }
  const uint32_t degradedAgeMs = (entry.timeoutMs * 80UL) / 100UL;
  if (ageMs >= degradedAgeMs && ageMs < entry.timeoutMs) {
    if (entry.health > 60U) {
      entry.health = 60U;
    }
    entry.degraded = true;
    entry.state = CH32_NEIGHBOR_DEGRADED;
  } else if (ageMs >= entry.timeoutMs) {
    if (entry.health > 30U) {
      entry.health = 30U;
    }
  }
  if (ageMs >= (entry.timeoutMs * 2UL) && entry.health > 10U) {
    entry.health = 10U;
  }
  if (ageMs >= (entry.timeoutMs * 3UL)) {
    entry.health = 0U;
  }

  const uint8_t loss = packetLossFor(entry);
  if (loss > 20U && entry.health > 40U) {
    entry.health = 40U;
    entry.degraded = true;
  } else if (loss > 5U && entry.health > 65U) {
    entry.health = 65U;
    entry.degraded = true;
  }
}

uint8_t CH32NeighborWatchdog::packetLossFor(const ch32_neighbor_entry_t& entry) const {
  const uint32_t total = static_cast<uint32_t>(entry.receivedPackets) + entry.missedPackets;
  if (total == 0U) {
    return 0U;
  }
  const uint32_t percent = (static_cast<uint32_t>(entry.missedPackets) * 100UL) / total;
  return static_cast<uint8_t>(percent > 100U ? 100U : percent);
}
