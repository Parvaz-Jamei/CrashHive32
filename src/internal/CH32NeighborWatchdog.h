#pragma once

#include <stdint.h>

#include "CH32BlackBox.h"
#include "CH32Config.h"

#ifndef CH32_NEIGHBOR_CALLBACK_DEFINED
#define CH32_NEIGHBOR_CALLBACK_DEFINED
typedef void (*CH32NeighborCallback)(uint32_t nodeId);
#endif

#ifndef CH32_ADAPTIVE_CALLBACK_DEFINED
#define CH32_ADAPTIVE_CALLBACK_DEFINED
typedef void (*CH32AdaptiveCallback)(uint32_t nodeId, uint8_t reason);
#endif

typedef struct {
  uint32_t nodeId;
  uint32_t timeoutMs;
  uint32_t lastSeenMs;
  uint8_t state;
  bool active;
  bool pendingRecovered;
  uint8_t health;
  bool degraded;
  bool degradedEventFired;
  bool healthyEventFired;
  uint16_t expectedSeq;
  uint16_t receivedPackets;
  uint16_t missedPackets;
  bool seqInitialized;
  bool seenOnce;
} ch32_neighbor_entry_t;

class CH32NeighborWatchdog {
 public:
  CH32NeighborWatchdog();

  void attach(CH32BlackBox* blackBox);
  bool watchNeighbor(uint32_t nodeId, uint32_t timeoutMs);
  bool removeNeighbor(uint32_t nodeId);
  bool noteHeartbeat(uint32_t nodeId);
  bool noteHeartbeat(uint32_t nodeId, uint16_t seq, uint8_t status);
  uint8_t getNeighborState(uint32_t nodeId) const;
  uint32_t getNeighborAgeMs(uint32_t nodeId) const;
  uint32_t getNeighborSilentMs(uint32_t nodeId) const;
  uint8_t getNeighborHealth(uint32_t nodeId) const;
  bool getNeighborHealth(uint32_t nodeId, uint8_t* outHealth) const;
  bool isNeighborDegraded(uint32_t nodeId) const;
  uint8_t getNeighborPacketLoss(uint32_t nodeId) const;
  uint8_t minimumKnownHealth() const;
  bool hasActiveWarning() const;
  void onSilent(CH32NeighborCallback callback);
  void onRecovered(CH32NeighborCallback callback);
  void onAdaptiveResponse(CH32AdaptiveCallback callback);
  bool tick();

 private:
  ch32_neighbor_entry_t* find(uint32_t nodeId);
  const ch32_neighbor_entry_t* find(uint32_t nodeId) const;
  ch32_neighbor_entry_t* findFreeSlot();
  uint8_t eventCode(uint32_t nodeId) const;
  void fireAdaptive(uint32_t nodeId, uint8_t reason);
  void updatePacketLoss(ch32_neighbor_entry_t& entry, uint16_t seq);
  void normalizePacketCounters(ch32_neighbor_entry_t& entry);
  void updateHealthFromAge(ch32_neighbor_entry_t& entry, uint32_t ageMs);
  uint8_t packetLossFor(const ch32_neighbor_entry_t& entry) const;

  CH32BlackBox* blackBox_;
  ch32_neighbor_entry_t entries_[CH32_NEIGHBOR_MAX_ENTRIES];
  CH32NeighborCallback silentCallback_;
  CH32NeighborCallback recoveredCallback_;
  CH32AdaptiveCallback adaptiveCallback_;
};
