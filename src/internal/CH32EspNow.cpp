#include "CH32EspNow.h"

#include <Arduino.h>
#include <stddef.h>
#include <string.h>

#include "CH32CRC.h"
#include "CH32Packet.h"

#if defined(ESP_PLATFORM)
#include <WiFi.h>
#include <esp_now.h>
#include <esp_idf_version.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#endif

#if defined(ESP_PLATFORM)
static portMUX_TYPE ch32EspNowMux = portMUX_INITIALIZER_UNLOCKED;
#endif

CH32EspNow* CH32EspNow::instance_ = nullptr;

#if defined(ESP_PLATFORM)
#if ESP_IDF_VERSION_MAJOR > 5 || \
    (ESP_IDF_VERSION_MAJOR == 5 && ESP_IDF_VERSION_MINOR >= 5)
static void ch32EspNowSendCallback(const esp_now_send_info_t* txInfo, esp_now_send_status_t status) {
  (void)txInfo;
  CH32EspNow::dispatchSendComplete(status == ESP_NOW_SEND_SUCCESS);
}

static void ch32EspNowReceiveCallback(const esp_now_recv_info_t* info, const uint8_t* data, int length) {
  (void)info;
  CH32EspNow::dispatchReceive(data, length);
}
#else
static void ch32EspNowSendCallback(const uint8_t* macAddr, esp_now_send_status_t status) {
  (void)macAddr;
  CH32EspNow::dispatchSendComplete(status == ESP_NOW_SEND_SUCCESS);
}

#if ESP_IDF_VERSION_MAJOR >= 5
static void ch32EspNowReceiveCallback(const esp_now_recv_info_t* info, const uint8_t* data, int length) {
  (void)info;
  CH32EspNow::dispatchReceive(data, length);
}
#else
static void ch32EspNowReceiveCallback(const uint8_t* macAddr, const uint8_t* data, int length) {
  (void)macAddr;
  CH32EspNow::dispatchReceive(data, length);
}
#endif
#endif
#endif

CH32EspNow::CH32EspNow()
    : initialized_(false),
      peerCount_(0U),
      peers_{},
      pendingSendSuccess_(0U),
      pendingSendFailure_(0U),
      pendingHeartbeats_{},
      pendingHeartbeatRead_(0U),
      pendingHeartbeatWrite_(0U),
      pendingReceiveDrops_(0U),
      lastRemoveStatus_(CH32_PEER_REMOVE_OK) {}

bool CH32EspNow::begin() {
  if (initialized_) {
    return true;
  }

#if defined(ESP_PLATFORM)
  WiFi.mode(WIFI_STA);

  const esp_err_t initResult = esp_now_init();
  if (initResult != ESP_OK) {
    return false;
  }

  instance_ = this;

  const esp_err_t sendCallbackResult = esp_now_register_send_cb(ch32EspNowSendCallback);
  if (sendCallbackResult != ESP_OK) {
    instance_ = nullptr;
    esp_now_deinit();
    return false;
  }

  const esp_err_t receiveCallbackResult = esp_now_register_recv_cb(ch32EspNowReceiveCallback);
  if (receiveCallbackResult != ESP_OK) {
    instance_ = nullptr;
    esp_now_deinit();
    return false;
  }

  initialized_ = true;
  return true;
#else
  return false;
#endif
}

bool CH32EspNow::addPeer(const uint8_t mac[6]) {
  if (mac == nullptr) {
    return false;
  }
  if (!initialized_ && !begin()) {
    return false;
  }
  if (isKnownPeer(mac)) {
    return true;
  }
  if (peerCount_ >= CH32_ESPNOW_MAX_PEERS) {
    return false;
  }

#if defined(ESP_PLATFORM)
  if (esp_now_is_peer_exist(mac)) {
    return rememberPeer(mac);
  }

  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, mac, 6U);
  peerInfo.channel = 0U;
  peerInfo.encrypt = false;

  const esp_err_t addResult = esp_now_add_peer(&peerInfo);
  if (addResult != ESP_OK && addResult != ESP_ERR_ESPNOW_EXIST) {
    return false;
  }
  return rememberPeer(mac);
#else
  (void)mac;
  return false;
#endif
}

bool CH32EspNow::removePeer(const uint8_t mac[6]) {
  lastRemoveStatus_ = CH32_PEER_REMOVE_OK;
  if (mac == nullptr || !initialized_) {
    lastRemoveStatus_ = CH32_PEER_REMOVE_INVALID;
    return false;
  }

  const bool knownLocal = isKnownPeer(mac);

#if defined(ESP_PLATFORM)
  const bool knownEspNow = esp_now_is_peer_exist(mac);
  if (!knownLocal && !knownEspNow) {
    lastRemoveStatus_ = CH32_PEER_REMOVE_NOT_FOUND;
    return false;
  }
  if (knownEspNow) {
    const esp_err_t delResult = esp_now_del_peer(mac);
    if (delResult != ESP_OK) {
      lastRemoveStatus_ = CH32_PEER_REMOVE_ESPNOW_FAIL;
      return false;
    }
  }
#else
  if (!knownLocal) {
    lastRemoveStatus_ = CH32_PEER_REMOVE_NOT_FOUND;
    return false;
  }
#endif

  forgetPeer(mac);
  lastRemoveStatus_ = CH32_PEER_REMOVE_OK;
  return true;
}

uint8_t CH32EspNow::lastRemoveStatus() const {
  return lastRemoveStatus_;
}

bool CH32EspNow::sendToPeers(const uint8_t* data, size_t length) {
  if (data == nullptr || length == 0U || length > 250U) {
    return false;
  }
  if (!initialized_ || peerCount_ == 0U) {
    return false;
  }

#if defined(ESP_PLATFORM)
  bool anyQueued = false;
  for (uint8_t i = 0U; i < peerCount_; ++i) {
    const esp_err_t sendResult = esp_now_send(peers_[i], data, length);
    if (sendResult == ESP_OK) {
      anyQueued = true;
    } else {
      onSendComplete(false);
    }
  }
  return anyQueued;
#else
  (void)data;
  (void)length;
  return false;
#endif
}

bool CH32EspNow::isReady() const {
  return initialized_;
}

uint8_t CH32EspNow::peerCount() const {
  return peerCount_;
}

uint16_t CH32EspNow::consumeSendSuccessCount() {
#if defined(ESP_PLATFORM)
  taskENTER_CRITICAL(&ch32EspNowMux);
#endif
  const uint16_t count = pendingSendSuccess_;
  pendingSendSuccess_ = 0U;
#if defined(ESP_PLATFORM)
  taskEXIT_CRITICAL(&ch32EspNowMux);
#endif
  return count;
}

uint16_t CH32EspNow::consumeSendFailureCount() {
#if defined(ESP_PLATFORM)
  taskENTER_CRITICAL(&ch32EspNowMux);
#endif
  const uint16_t count = pendingSendFailure_;
  pendingSendFailure_ = 0U;
#if defined(ESP_PLATFORM)
  taskEXIT_CRITICAL(&ch32EspNowMux);
#endif
  return count;
}

bool CH32EspNow::consumeHeartbeat(ch32_received_heartbeat_t* heartbeat) {
  if (heartbeat == nullptr) {
    return false;
  }
#if defined(ESP_PLATFORM)
  taskENTER_CRITICAL(&ch32EspNowMux);
#endif
  const uint8_t readIndex = pendingHeartbeatRead_;
  if (readIndex == pendingHeartbeatWrite_) {
#if defined(ESP_PLATFORM)
    taskEXIT_CRITICAL(&ch32EspNowMux);
#endif
    return false;
  }
  *heartbeat = pendingHeartbeats_[readIndex];
  pendingHeartbeatRead_ = static_cast<uint8_t>((readIndex + 1U) % CH32_ESPNOW_PENDING_HEARTBEATS);
#if defined(ESP_PLATFORM)
  taskEXIT_CRITICAL(&ch32EspNowMux);
#endif
  return true;
}

uint16_t CH32EspNow::consumeReceiveDropCount() {
#if defined(ESP_PLATFORM)
  taskENTER_CRITICAL(&ch32EspNowMux);
#endif
  const uint16_t count = pendingReceiveDrops_;
  pendingReceiveDrops_ = 0U;
#if defined(ESP_PLATFORM)
  taskEXIT_CRITICAL(&ch32EspNowMux);
#endif
  return count;
}

bool CH32EspNow::isKnownPeer(const uint8_t mac[6]) const {
  if (mac == nullptr) {
    return false;
  }
  for (uint8_t i = 0U; i < peerCount_; ++i) {
    if (memcmp(peers_[i], mac, 6U) == 0) {
      return true;
    }
  }
  return false;
}

bool CH32EspNow::rememberPeer(const uint8_t mac[6]) {
  if (mac == nullptr) {
    return false;
  }
  if (isKnownPeer(mac)) {
    return true;
  }
  if (peerCount_ >= CH32_ESPNOW_MAX_PEERS) {
    return false;
  }
  memcpy(peers_[peerCount_], mac, 6U);
  ++peerCount_;
  return true;
}

bool CH32EspNow::forgetPeer(const uint8_t mac[6]) {
  if (mac == nullptr) {
    return false;
  }

  for (uint8_t i = 0U; i < peerCount_; ++i) {
    if (memcmp(peers_[i], mac, 6U) == 0) {
      for (uint8_t j = i; j + 1U < peerCount_; ++j) {
        memcpy(peers_[j], peers_[j + 1U], 6U);
      }
      --peerCount_;
      memset(peers_[peerCount_], 0, 6U);
      return true;
    }
  }
  return false;
}

void CH32EspNow::onSendComplete(bool success) {
#if defined(ESP_PLATFORM)
  taskENTER_CRITICAL(&ch32EspNowMux);
#endif
  uint16_t& counter = success ? pendingSendSuccess_ : pendingSendFailure_;
  if (counter < 65535U) {
    ++counter;
  }
#if defined(ESP_PLATFORM)
  taskEXIT_CRITICAL(&ch32EspNowMux);
#endif
}

void CH32EspNow::onReceive(const uint8_t* data, int length) {
  ch32_received_heartbeat_t heartbeat = {};
  if (!validateHeartbeatPacket(data, length, &heartbeat)) {
    incrementReceiveDrop();
    return;
  }
  if (!queueHeartbeat(heartbeat)) {
    incrementReceiveDrop();
  }
}

bool CH32EspNow::validateHeartbeatPacket(const uint8_t* data, int length, ch32_received_heartbeat_t* heartbeat) const {
  if (data == nullptr || heartbeat == nullptr || length != static_cast<int>(sizeof(ch32_heartbeat_packet_t))) {
    return false;
  }

  ch32_heartbeat_packet_t packet;
  memcpy(&packet, data, sizeof(packet));
  if (packet.magic != CH32_PACKET_MAGIC ||
      packet.version != CH32_PACKET_VERSION ||
      packet.packetType != CH32_PACKET_HEARTBEAT) {
    return false;
  }

  const uint8_t expectedCrc = ch32_crc8_buffer(reinterpret_cast<const uint8_t*>(&packet),
                                              offsetof(ch32_heartbeat_packet_t, crc8),
                                              0U);
  if (packet.crc8 != expectedCrc) {
    return false;
  }

  if (packet.nodeId == 0U) {
    return false;
  }
  heartbeat->nodeId = packet.nodeId;
  heartbeat->seq = packet.seq;
  heartbeat->status = packet.status;
  return true;
}

bool CH32EspNow::queueHeartbeat(const ch32_received_heartbeat_t& heartbeat) {
#if defined(ESP_PLATFORM)
  taskENTER_CRITICAL(&ch32EspNowMux);
#endif
  const uint8_t writeIndex = pendingHeartbeatWrite_;
  const uint8_t nextIndex = static_cast<uint8_t>((writeIndex + 1U) % CH32_ESPNOW_PENDING_HEARTBEATS);
  if (nextIndex == pendingHeartbeatRead_) {
#if defined(ESP_PLATFORM)
    taskEXIT_CRITICAL(&ch32EspNowMux);
#endif
    return false;
  }
  pendingHeartbeats_[writeIndex] = heartbeat;
  pendingHeartbeatWrite_ = nextIndex;
#if defined(ESP_PLATFORM)
  taskEXIT_CRITICAL(&ch32EspNowMux);
#endif
  return true;
}

void CH32EspNow::incrementReceiveDrop() {
#if defined(ESP_PLATFORM)
  taskENTER_CRITICAL(&ch32EspNowMux);
#endif
  if (pendingReceiveDrops_ < 65535U) {
    ++pendingReceiveDrops_;
  }
#if defined(ESP_PLATFORM)
  taskEXIT_CRITICAL(&ch32EspNowMux);
#endif
}

#if defined(ESP_PLATFORM)
void CH32EspNow::dispatchSendComplete(bool success) {
  if (instance_ != nullptr) {
    instance_->onSendComplete(success);
  }
}

void CH32EspNow::dispatchReceive(const uint8_t* data, int length) {
  if (instance_ != nullptr) {
    instance_->onReceive(data, length);
  }
}
#endif
