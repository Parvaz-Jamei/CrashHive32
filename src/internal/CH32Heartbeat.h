#pragma once

#include <stdint.h>

#include "CH32BlackBox.h"
#include "CH32EspNow.h"
#include "CH32Packet.h"

class CH32Heartbeat {
 public:
  CH32Heartbeat();

  void attach(CH32EspNow* espNow, CH32BlackBox* blackBox);
  void setInterval(uint32_t intervalMs);
  bool sendHeartbeat(uint32_t nodeId, uint8_t status);
  bool tick();

 private:
  uint8_t buildHeartbeatPacket(uint32_t nodeId, uint8_t status, ch32_heartbeat_packet_t* packet);
  void drainSendStatus();
  uint8_t clippedCountCode(uint16_t value) const;

  CH32EspNow* espNow_;
  CH32BlackBox* blackBox_;
  uint32_t intervalMs_;
  uint32_t lastSendMs_;
  uint32_t lastNodeId_;
  uint8_t lastStatus_;
  uint16_t seq_;
  bool hasHeartbeatProfile_;
};
