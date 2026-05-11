#pragma once

#include <Arduino.h>
#include <stdint.h>

#ifndef CH32_NEIGHBOR_UNKNOWN
#define CH32_NEIGHBOR_UNKNOWN 0U
#endif
#ifndef CH32_NEIGHBOR_ALIVE
#define CH32_NEIGHBOR_ALIVE 1U
#endif
#ifndef CH32_NEIGHBOR_SILENT
#define CH32_NEIGHBOR_SILENT 2U
#endif
#ifndef CH32_NEIGHBOR_DEGRADED
#define CH32_NEIGHBOR_DEGRADED 3U
#endif

#ifndef CH32_ADAPTIVE_REASON_NEIGHBOR_SILENT
#define CH32_ADAPTIVE_REASON_NEIGHBOR_SILENT     1U
#endif
#ifndef CH32_ADAPTIVE_REASON_NEIGHBOR_RECOVERED
#define CH32_ADAPTIVE_REASON_NEIGHBOR_RECOVERED  2U
#endif
#ifndef CH32_ADAPTIVE_REASON_NEIGHBOR_DEGRADED
#define CH32_ADAPTIVE_REASON_NEIGHBOR_DEGRADED   3U
#endif
#ifndef CH32_ADAPTIVE_REASON_NEIGHBOR_HEALTHY
#define CH32_ADAPTIVE_REASON_NEIGHBOR_HEALTHY    4U
#endif

#ifndef CH32_CHUNK_WRITER_DEFINED
#define CH32_CHUNK_WRITER_DEFINED
typedef void (*CH32ChunkWriter)(const char* chunk);
#endif

#ifndef CH32_NEIGHBOR_CALLBACK_DEFINED
#define CH32_NEIGHBOR_CALLBACK_DEFINED
typedef void (*CH32NeighborCallback)(uint32_t nodeId);
#endif

#ifndef CH32_ADAPTIVE_CALLBACK_DEFINED
#define CH32_ADAPTIVE_CALLBACK_DEFINED
typedef void (*CH32AdaptiveCallback)(uint32_t nodeId, uint8_t reason);
#endif

#ifndef CH32_METRIC_ALERT_CALLBACK_DEFINED
#define CH32_METRIC_ALERT_CALLBACK_DEFINED
typedef void (*CH32MetricAlertCallback)(const char* key,
                                        float value,
                                        float minValue,
                                        float maxValue);
#endif

class CrashHive32Class {
 public:
  bool begin();
  bool tick();

  bool recordMetric(const char* name, float value);
  bool recordFlag(const char* name, int code);
  bool hasPreviousEvents() const;
  void printReport(Stream& out) const;
  bool exportJson(CH32ChunkWriter writer) const;
  void printJson(Stream& out) const;

  uint8_t getResetReasonCode() const;
  const char* getResetReasonName() const;

  bool beginEspNow();
  bool addPeer(const uint8_t mac[6]);
  bool addPeer(const char* macText);
  bool removePeer(const uint8_t mac[6]);
  bool removePeer(const char* macText);
  bool sendHeartbeat(uint32_t nodeId, uint8_t status);
  void setHeartbeatInterval(uint32_t intervalMs);

  bool watchNeighbor(uint32_t nodeId, uint32_t timeoutMs);
  bool watchNeighborEvery(uint32_t nodeId,
                          uint32_t expectedHeartbeatMs,
                          uint8_t missedBeats = 3U);
  bool removeNeighbor(uint32_t nodeId);
  bool noteHeartbeat(uint32_t nodeId);
  uint8_t getNeighborState(uint32_t nodeId) const;
  const char* getNeighborStateName(uint32_t nodeId) const;
  uint32_t getNeighborAgeMs(uint32_t nodeId) const;
  uint32_t getNeighborSilentMs(uint32_t nodeId) const;
  uint8_t getNeighborHealth(uint32_t nodeId) const;
  bool getNeighborHealth(uint32_t nodeId, uint8_t* outHealth) const;
  bool isNeighborHealthy(uint32_t nodeId, uint8_t minHealth = 70U) const;
  bool isNeighborDegraded(uint32_t nodeId) const;
  uint8_t getNeighborPacketLoss(uint32_t nodeId) const;
  void onNeighborSilent(CH32NeighborCallback callback);
  void onNeighborRecovered(CH32NeighborCallback callback);
  void onAdaptiveResponse(CH32AdaptiveCallback callback);
  const char* getAdaptiveReasonName(uint8_t reason) const;

  bool isBootStorm() const;
  uint8_t getBootStreak() const;
  void markBootStable();

  bool setMetricAlert(const char* key,
                      float minValue,
                      float maxValue,
                      CH32MetricAlertCallback callback);
  bool clearMetricAlert(const char* key);
  void clearMetricAlerts();

  uint8_t getSystemHealth() const;
  bool hasActiveDiagnosticWarning() const;
};

extern CrashHive32Class CrashHive32;
