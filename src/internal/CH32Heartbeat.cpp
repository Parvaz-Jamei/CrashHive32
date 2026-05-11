#include "CH32Heartbeat.h"

#include <Arduino.h>
#include <stddef.h>
#include <string.h>

#include "CH32CRC.h"
#include "CH32Config.h"

CH32Heartbeat::CH32Heartbeat()
    : espNow_(nullptr),
      blackBox_(nullptr),
      intervalMs_(CH32_HEARTBEAT_DEFAULT_INTERVAL_MS),
      lastSendMs_(0U),
      lastNodeId_(0U),
      lastStatus_(0U),
      seq_(0U),
      hasHeartbeatProfile_(false) {}

void CH32Heartbeat::attach(CH32EspNow* espNow, CH32BlackBox* blackBox) {
  espNow_ = espNow;
  blackBox_ = blackBox;
}

void CH32Heartbeat::setInterval(uint32_t intervalMs) {
  if (intervalMs < CH32_HEARTBEAT_MIN_INTERVAL_MS) {
    intervalMs = CH32_HEARTBEAT_MIN_INTERVAL_MS;
  }
  intervalMs_ = intervalMs;
}

bool CH32Heartbeat::sendHeartbeat(uint32_t nodeId, uint8_t status) {
  if (espNow_ == nullptr || !espNow_->isReady()) {
    if (blackBox_ != nullptr) {
      blackBox_->recordFlag("_ch32_hb_fail", 1);
    }
    return false;
  }
  if (espNow_->peerCount() == 0U) {
    if (blackBox_ != nullptr) {
      blackBox_->recordFlag("_ch32_hb_fail", 2);
    }
    return false;
  }

  ch32_heartbeat_packet_t packet;
  const uint8_t packetSize = buildHeartbeatPacket(nodeId, status, &packet);
  const bool queued = espNow_->sendToPeers(reinterpret_cast<const uint8_t*>(&packet), packetSize);

  lastNodeId_ = nodeId;
  lastStatus_ = status;
  hasHeartbeatProfile_ = true;
  lastSendMs_ = millis();

  if (blackBox_ != nullptr) {
#if CH32_LOG_INTERNAL_SUCCESS_EVENTS
    if (queued) {
      blackBox_->recordFlag("_ch32_hb_ok", 1);
    }
#endif
    if (!queued) {
      blackBox_->recordFlag("_ch32_hb_fail", 3);
    }
  }
  return queued;
}

bool CH32Heartbeat::tick() {
  drainSendStatus();

  if (espNow_ == nullptr || !espNow_->isReady() || espNow_->peerCount() == 0U) {
    return false;
  }

  const uint32_t now = millis();
  if (lastSendMs_ != 0U && (now - lastSendMs_) < intervalMs_) {
    return true;
  }

  uint32_t nodeId = lastNodeId_;
  uint8_t status = lastStatus_;
  if (!hasHeartbeatProfile_) {
#if defined(ESP_PLATFORM)
    const uint64_t mac = ESP.getEfuseMac();
    nodeId = static_cast<uint32_t>(mac & 0xFFFFFFFFUL);
#else
    nodeId = 0U;
#endif
    status = 0U;
  }

  return sendHeartbeat(nodeId, status);
}

void CH32Heartbeat::drainSendStatus() {
  if (espNow_ == nullptr || blackBox_ == nullptr) {
    return;
  }

  const uint16_t okCount = espNow_->consumeSendSuccessCount();
  const uint16_t failCount = espNow_->consumeSendFailureCount();

#if CH32_LOG_INTERNAL_SUCCESS_EVENTS
  if (okCount > 0U) {
    blackBox_->recordFlag("_ch32_cb_ok", clippedCountCode(okCount));
  }
#else
  (void)okCount;
#endif
  if (failCount > 0U) {
    blackBox_->recordFlag("_ch32_cb_fail", clippedCountCode(failCount));
  }
}

uint8_t CH32Heartbeat::buildHeartbeatPacket(uint32_t nodeId,
                                            uint8_t status,
                                            ch32_heartbeat_packet_t* packet) {
  if (packet == nullptr) {
    return 0U;
  }

  memset(packet, 0, sizeof(*packet));
  packet->magic = CH32_PACKET_MAGIC;
  packet->version = CH32_PACKET_VERSION;
  packet->packetType = CH32_PACKET_HEARTBEAT;
  packet->nodeId = nodeId;
  packet->uptimeMs = millis();
  packet->seq = seq_;
  packet->status = status;
  packet->crc8 = 0U;
  packet->crc8 = ch32_crc8_buffer(reinterpret_cast<const uint8_t*>(packet),
                                  offsetof(ch32_heartbeat_packet_t, crc8),
                                  0U);
  ++seq_;
  return static_cast<uint8_t>(sizeof(*packet));
}

uint8_t CH32Heartbeat::clippedCountCode(uint16_t value) const {
  if (value > 255U) {
    return 255U;
  }
  return static_cast<uint8_t>(value);
}
