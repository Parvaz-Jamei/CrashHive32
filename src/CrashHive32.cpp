#include "CrashHive32.h"

#include "internal/CH32BlackBox.h"
#include "internal/CH32BootState.h"
#include "internal/CH32Config.h"
#include "internal/CH32EspNow.h"
#include "internal/CH32Heartbeat.h"
#include "internal/CH32Key.h"
#include "internal/CH32NeighborWatchdog.h"
#include "internal/CH32Types.h"

#include <esp_system.h>
#include <stddef.h>
#include <string.h>

static CH32BlackBox g_ch32_blackbox;
static CH32EspNow g_ch32_espnow;
static CH32Heartbeat g_ch32_heartbeat;
static CH32NeighborWatchdog g_ch32_watchdog;
static CH32BootState g_ch32_boot_state;
CrashHive32Class CrashHive32;

typedef struct {
  bool active;
  char key[CH32_BLACKBOX_KEY_MAX];
  float minValue;
  float maxValue;
  CH32MetricAlertCallback callback;
  bool alertActive;
} ch32_metric_alert_t;

static ch32_metric_alert_t g_ch32_metric_alerts[CH32_MAX_METRIC_ALERTS];

static uint8_t ch32_clip_count(uint16_t value) {
  return static_cast<uint8_t>(value > 255U ? 255U : value);
}

static uint8_t ch32_clip_u8_int(int value) {
  if (value < 0) {
    return 0U;
  }
  if (value > 255) {
    return 255U;
  }
  return static_cast<uint8_t>(value);
}

static int8_t ch32_hex_value(char c) {
  if (c >= '0' && c <= '9') {
    return static_cast<int8_t>(c - '0');
  }
  if (c >= 'a' && c <= 'f') {
    return static_cast<int8_t>(10 + (c - 'a'));
  }
  if (c >= 'A' && c <= 'F') {
    return static_cast<int8_t>(10 + (c - 'A'));
  }
  return -1;
}


static bool ch32_key_equals(const char* a, const char* b) {
  if (a == nullptr || b == nullptr) {
    return false;
  }
  return strncmp(a, b, CH32_BLACKBOX_KEY_MAX) == 0;
}

static bool ch32_make_metric_key(char* destination, size_t destinationSize, const char* source) {
  return ch32_make_key(destination, destinationSize, source);
}

static bool ch32_parse_mac_text(const char* text, uint8_t mac[6]) {
  if (text == nullptr || mac == nullptr) {
    return false;
  }

  for (uint8_t i = 0U; i < 17U; ++i) {
    if (text[i] == '\0') {
      return false;
    }
  }
  if (text[17] != '\0') {
    return false;
  }

  const char separator = text[2];
  if (separator != ':' && separator != '-') {
    return false;
  }

  for (uint8_t i = 0U; i < 6U; ++i) {
    const uint8_t offset = static_cast<uint8_t>(i * 3U);
    const int8_t high = ch32_hex_value(text[offset]);
    const int8_t low = ch32_hex_value(text[offset + 1U]);

    if (high < 0 || low < 0) {
      return false;
    }

    mac[i] = static_cast<uint8_t>(
      (static_cast<uint8_t>(high) << 4U) | static_cast<uint8_t>(low)
    );

    if (i < 5U && text[offset + 2U] != separator) {
      return false;
    }
  }

  return true;
}

static ch32_metric_alert_t* ch32_find_metric_alert(const char* key) {
  for (uint8_t i = 0U; i < CH32_MAX_METRIC_ALERTS; ++i) {
    if (g_ch32_metric_alerts[i].active && ch32_key_equals(g_ch32_metric_alerts[i].key, key)) {
      return &g_ch32_metric_alerts[i];
    }
  }
  return nullptr;
}

static ch32_metric_alert_t* ch32_find_free_metric_alert() {
  for (uint8_t i = 0U; i < CH32_MAX_METRIC_ALERTS; ++i) {
    if (!g_ch32_metric_alerts[i].active) {
      return &g_ch32_metric_alerts[i];
    }
  }
  return nullptr;
}

bool CrashHive32Class::begin() {
  g_ch32_heartbeat.attach(&g_ch32_espnow, &g_ch32_blackbox);
  g_ch32_watchdog.attach(&g_ch32_blackbox);
  const bool ok = g_ch32_blackbox.begin();
  if (ok) {
    g_ch32_boot_state.begin(&g_ch32_blackbox);
  }
  return ok;
}

bool CrashHive32Class::tick() {
  const bool heartbeatActive = g_ch32_heartbeat.tick();
  bool receiveActive = false;

  ch32_received_heartbeat_t received = {};
  while (g_ch32_espnow.consumeHeartbeat(&received)) {
    receiveActive = true;
    if (!g_ch32_watchdog.noteHeartbeat(received.nodeId, received.seq, received.status)) {
      g_ch32_blackbox.recordFlag("_ch32_rx_drop", ch32_clip_count(static_cast<uint16_t>(received.nodeId & 0xFFU)));
    }
  }

  const uint16_t receiveDrops = g_ch32_espnow.consumeReceiveDropCount();
  if (receiveDrops > 0U) {
    receiveActive = true;
    g_ch32_blackbox.recordFlag("_ch32_rx_drop", ch32_clip_count(receiveDrops));
  }

  const bool watchdogActive = g_ch32_watchdog.tick();
  const bool bootActive = g_ch32_boot_state.tick();
  return heartbeatActive || receiveActive || watchdogActive || bootActive;
}

bool CrashHive32Class::recordMetric(const char* name, float value) {
  if (!g_ch32_blackbox.recordMetric(name, value)) {
    return false;
  }

  char canonicalKey[CH32_BLACKBOX_KEY_MAX];
  if (!ch32_make_metric_key(canonicalKey, sizeof(canonicalKey), name)) {
    return true;
  }

  ch32_metric_alert_t* alert = ch32_find_metric_alert(canonicalKey);
  if (alert == nullptr) {
    return true;
  }

  const bool outside = value < alert->minValue || value > alert->maxValue;
  if (outside && !alert->alertActive) {
    alert->alertActive = true;
    const uint8_t code = value < alert->minValue ? 1U : 2U;
    g_ch32_blackbox.recordFlag("_ch32_met_alert", code);
    if (alert->callback != nullptr) {
      alert->callback(alert->key, value, alert->minValue, alert->maxValue);
    }
  } else if (!outside) {
    alert->alertActive = false;
  }
  return true;
}

bool CrashHive32Class::recordFlag(const char* name, int code) {
  return g_ch32_blackbox.recordFlag(name, ch32_clip_u8_int(code));
}

bool CrashHive32Class::hasPreviousEvents() const {
  return g_ch32_blackbox.hasPreviousEvents();
}

void CrashHive32Class::printReport(Stream& out) const {
  g_ch32_blackbox.printReport(out);
}

bool CrashHive32Class::exportJson(CH32ChunkWriter writer) const {
  return g_ch32_blackbox.exportJson(writer);
}

void CrashHive32Class::printJson(Stream& out) const {
  g_ch32_blackbox.printJson(out);
}

uint8_t CrashHive32Class::getResetReasonCode() const {
  return g_ch32_blackbox.getResetReasonCode();
}

const char* CrashHive32Class::getResetReasonName() const {
  return g_ch32_blackbox.getResetReasonName();
}

bool CrashHive32Class::beginEspNow() {
  const bool ok = g_ch32_espnow.begin();
#if CH32_LOG_INTERNAL_SUCCESS_EVENTS
  if (ok) {
    g_ch32_blackbox.recordFlag("_ch32_now_ok", 1);
  }
#endif
  if (!ok) {
    g_ch32_blackbox.recordFlag("_ch32_now_fail", 2);
  }
  return ok;
}

bool CrashHive32Class::addPeer(const uint8_t mac[6]) {
  const bool ok = g_ch32_espnow.addPeer(mac);
#if CH32_LOG_INTERNAL_SUCCESS_EVENTS
  if (ok) {
    g_ch32_blackbox.recordFlag("_ch32_peer_ok", 1);
  }
#endif
  if (!ok) {
    g_ch32_blackbox.recordFlag("_ch32_peer_fail", 2);
  }
  return ok;
}

bool CrashHive32Class::addPeer(const char* macText) {
  uint8_t mac[6];
  if (!ch32_parse_mac_text(macText, mac)) {
    g_ch32_blackbox.recordFlag("_ch32_peer_fail", 3);
    return false;
  }
  return addPeer(mac);
}

bool CrashHive32Class::removePeer(const uint8_t mac[6]) {
  const bool ok = g_ch32_espnow.removePeer(mac);
  if (ok) {
#if CH32_LOG_INTERNAL_SUCCESS_EVENTS
    g_ch32_blackbox.recordFlag("_ch32_pr_rm_ok", 1);
#endif
  } else {
    const uint8_t status = g_ch32_espnow.lastRemoveStatus();
    if (status == CH32_PEER_REMOVE_NOT_FOUND) {
      g_ch32_blackbox.recordFlag("_ch32_pr_miss", 1);
    } else {
      g_ch32_blackbox.recordFlag("_ch32_peer_fail", status == 0U ? 4U : status);
    }
  }
  return ok;
}

bool CrashHive32Class::removePeer(const char* macText) {
  uint8_t mac[6];
  if (!ch32_parse_mac_text(macText, mac)) {
    g_ch32_blackbox.recordFlag("_ch32_peer_fail", 5);
    return false;
  }
  return removePeer(mac);
}

bool CrashHive32Class::sendHeartbeat(uint32_t nodeId, uint8_t status) {
  return g_ch32_heartbeat.sendHeartbeat(nodeId, status);
}

void CrashHive32Class::setHeartbeatInterval(uint32_t intervalMs) {
  g_ch32_heartbeat.setInterval(intervalMs);
}

bool CrashHive32Class::watchNeighbor(uint32_t nodeId, uint32_t timeoutMs) {
  return g_ch32_watchdog.watchNeighbor(nodeId, timeoutMs);
}

bool CrashHive32Class::watchNeighborEvery(uint32_t nodeId,
                                          uint32_t expectedHeartbeatMs,
                                          uint8_t missedBeats) {
  if (expectedHeartbeatMs == 0UL || missedBeats == 0U) {
    return false;
  }
  if (expectedHeartbeatMs > (UINT32_MAX / static_cast<uint32_t>(missedBeats))) {
    return false;
  }
  uint32_t timeoutMs = expectedHeartbeatMs * static_cast<uint32_t>(missedBeats);
  if (timeoutMs < CH32_NEIGHBOR_MIN_TIMEOUT_MS) {
    timeoutMs = CH32_NEIGHBOR_MIN_TIMEOUT_MS;
  }
  return watchNeighbor(nodeId, timeoutMs);
}

bool CrashHive32Class::removeNeighbor(uint32_t nodeId) {
  return g_ch32_watchdog.removeNeighbor(nodeId);
}

bool CrashHive32Class::noteHeartbeat(uint32_t nodeId) {
  return g_ch32_watchdog.noteHeartbeat(nodeId);
}

uint8_t CrashHive32Class::getNeighborState(uint32_t nodeId) const {
  return g_ch32_watchdog.getNeighborState(nodeId);
}

const char* CrashHive32Class::getNeighborStateName(uint32_t nodeId) const {
  switch (getNeighborState(nodeId)) {
    case CH32_NEIGHBOR_ALIVE:
      return "alive";
    case CH32_NEIGHBOR_DEGRADED:
      return "degraded";
    case CH32_NEIGHBOR_SILENT:
      return "silent";
    case CH32_NEIGHBOR_UNKNOWN:
    default:
      return "unknown";
  }
}

uint32_t CrashHive32Class::getNeighborAgeMs(uint32_t nodeId) const {
  return g_ch32_watchdog.getNeighborAgeMs(nodeId);
}

uint32_t CrashHive32Class::getNeighborSilentMs(uint32_t nodeId) const {
  return g_ch32_watchdog.getNeighborSilentMs(nodeId);
}

uint8_t CrashHive32Class::getNeighborHealth(uint32_t nodeId) const {
  return g_ch32_watchdog.getNeighborHealth(nodeId);
}

bool CrashHive32Class::getNeighborHealth(uint32_t nodeId, uint8_t* outHealth) const {
  return g_ch32_watchdog.getNeighborHealth(nodeId, outHealth);
}

bool CrashHive32Class::isNeighborHealthy(uint32_t nodeId, uint8_t minHealth) const {
  uint8_t health = 0U;
  if (!getNeighborHealth(nodeId, &health)) {
    return false;
  }
  return health >= minHealth;
}

bool CrashHive32Class::isNeighborDegraded(uint32_t nodeId) const {
  return g_ch32_watchdog.isNeighborDegraded(nodeId);
}

uint8_t CrashHive32Class::getNeighborPacketLoss(uint32_t nodeId) const {
  return g_ch32_watchdog.getNeighborPacketLoss(nodeId);
}

void CrashHive32Class::onNeighborSilent(CH32NeighborCallback callback) {
  g_ch32_watchdog.onSilent(callback);
}

void CrashHive32Class::onNeighborRecovered(CH32NeighborCallback callback) {
  g_ch32_watchdog.onRecovered(callback);
}

void CrashHive32Class::onAdaptiveResponse(CH32AdaptiveCallback callback) {
  g_ch32_watchdog.onAdaptiveResponse(callback);
}

const char* CrashHive32Class::getAdaptiveReasonName(uint8_t reason) const {
  switch (reason) {
    case CH32_ADAPTIVE_REASON_NEIGHBOR_SILENT:
      return "neighbor_silent";
    case CH32_ADAPTIVE_REASON_NEIGHBOR_RECOVERED:
      return "neighbor_recovered";
    case CH32_ADAPTIVE_REASON_NEIGHBOR_DEGRADED:
      return "neighbor_degraded";
    case CH32_ADAPTIVE_REASON_NEIGHBOR_HEALTHY:
      return "neighbor_healthy";
    default:
      return "unknown";
  }
}

bool CrashHive32Class::isBootStorm() const {
  return g_ch32_boot_state.isBootStorm();
}

uint8_t CrashHive32Class::getBootStreak() const {
  return g_ch32_boot_state.getBootStreak();
}

void CrashHive32Class::markBootStable() {
  g_ch32_boot_state.markBootStable();
}

bool CrashHive32Class::setMetricAlert(const char* key,
                                      float minValue,
                                      float maxValue,
                                      CH32MetricAlertCallback callback) {
  if (callback == nullptr || minValue > maxValue) {
    return false;
  }
  char canonicalKey[CH32_BLACKBOX_KEY_MAX];
  if (!ch32_make_metric_key(canonicalKey, sizeof(canonicalKey), key)) {
    return false;
  }
  ch32_metric_alert_t* alert = ch32_find_metric_alert(canonicalKey);
  if (alert == nullptr) {
    alert = ch32_find_free_metric_alert();
  }
  if (alert == nullptr) {
    return false;
  }
  memset(alert, 0, sizeof(*alert));
  alert->active = true;
  memcpy(alert->key, canonicalKey, sizeof(alert->key));
  alert->minValue = minValue;
  alert->maxValue = maxValue;
  alert->callback = callback;
  return true;
}

bool CrashHive32Class::clearMetricAlert(const char* key) {
  char canonicalKey[CH32_BLACKBOX_KEY_MAX];
  if (!ch32_make_metric_key(canonicalKey, sizeof(canonicalKey), key)) {
    return false;
  }
  ch32_metric_alert_t* alert = ch32_find_metric_alert(canonicalKey);
  if (alert == nullptr) {
    return false;
  }
  memset(alert, 0, sizeof(*alert));
  return true;
}

void CrashHive32Class::clearMetricAlerts() {
  memset(g_ch32_metric_alerts, 0, sizeof(g_ch32_metric_alerts));
}

uint8_t CrashHive32Class::getSystemHealth() const {
  int16_t health = 100;
  if (g_ch32_boot_state.isBootStorm()) {
    health -= 30;
  }

  const uint8_t neighborHealth = g_ch32_watchdog.minimumKnownHealth();
  if (neighborHealth != 255U && neighborHealth < static_cast<uint8_t>(health)) {
    health = neighborHealth;
  }

  for (uint8_t i = 0U; i < CH32_MAX_METRIC_ALERTS; ++i) {
    if (g_ch32_metric_alerts[i].active && g_ch32_metric_alerts[i].alertActive) {
      health -= 20;
      break;
    }
  }

  const uint8_t resetReason = g_ch32_blackbox.getResetReasonCode();
  if (resetReason == static_cast<uint8_t>(ESP_RST_PANIC) ||
      resetReason == static_cast<uint8_t>(ESP_RST_INT_WDT) ||
      resetReason == static_cast<uint8_t>(ESP_RST_TASK_WDT) ||
      resetReason == static_cast<uint8_t>(ESP_RST_WDT) ||
      resetReason == static_cast<uint8_t>(ESP_RST_BROWNOUT)) {
    health -= 10;
  }

  if (health < 0) {
    return 0U;
  }
  if (health > 100) {
    return 100U;
  }
  return static_cast<uint8_t>(health);
}

bool CrashHive32Class::hasActiveDiagnosticWarning() const {
  if (g_ch32_boot_state.isBootStorm() || g_ch32_watchdog.hasActiveWarning()) {
    return true;
  }
  for (uint8_t i = 0U; i < CH32_MAX_METRIC_ALERTS; ++i) {
    if (g_ch32_metric_alerts[i].active && g_ch32_metric_alerts[i].alertActive) {
      return true;
    }
  }
  return getSystemHealth() < 100U;
}
