#pragma once

#include <stddef.h>
#include <stdint.h>

#include "CH32Config.h"
#include "CH32Types.h"

enum {
  CH32_PEER_REMOVE_OK = 0U,
  CH32_PEER_REMOVE_INVALID = 1U,
  CH32_PEER_REMOVE_NOT_FOUND = 2U,
  CH32_PEER_REMOVE_ESPNOW_FAIL = 3U
};

class CH32EspNow {
 public:
  CH32EspNow();

  bool begin();
  bool addPeer(const uint8_t mac[6]);
  bool removePeer(const uint8_t mac[6]);
  uint8_t lastRemoveStatus() const;
  bool sendToPeers(const uint8_t* data, size_t length);
  bool isReady() const;
  uint8_t peerCount() const;
  uint16_t consumeSendSuccessCount();
  uint16_t consumeSendFailureCount();
  bool consumeHeartbeat(ch32_received_heartbeat_t* heartbeat);
  uint16_t consumeReceiveDropCount();

#if defined(ESP_PLATFORM)
  static void dispatchSendComplete(bool success);
  static void dispatchReceive(const uint8_t* data, int length);
#endif

 private:
  bool isKnownPeer(const uint8_t mac[6]) const;
  bool rememberPeer(const uint8_t mac[6]);
  bool forgetPeer(const uint8_t mac[6]);
  void onSendComplete(bool success);
  void onReceive(const uint8_t* data, int length);
  bool validateHeartbeatPacket(const uint8_t* data, int length, ch32_received_heartbeat_t* heartbeat) const;
  bool queueHeartbeat(const ch32_received_heartbeat_t& heartbeat);
  void incrementReceiveDrop();

  bool initialized_;
  uint8_t peerCount_;
  uint8_t peers_[CH32_ESPNOW_MAX_PEERS][6];
  uint16_t pendingSendSuccess_;
  uint16_t pendingSendFailure_;
  ch32_received_heartbeat_t pendingHeartbeats_[CH32_ESPNOW_PENDING_HEARTBEATS];
  uint8_t pendingHeartbeatRead_;
  uint8_t pendingHeartbeatWrite_;
  uint16_t pendingReceiveDrops_;
  uint8_t lastRemoveStatus_;

  static CH32EspNow* instance_;
};
