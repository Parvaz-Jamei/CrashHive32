#pragma once

#include <stdint.h>

#include "CH32Config.h"

typedef struct __attribute__((packed)) {
  uint32_t t_ms;
  uint8_t  type;
  char     key[CH32_BLACKBOX_KEY_SIZE];
  float    value;
  uint8_t  code;
  uint8_t  crc8;
} ch32_blackbox_event_t;

typedef struct __attribute__((packed)) {
  uint32_t magic;
  uint16_t version;
  uint16_t eventCount;
  uint16_t writeIndex;
  uint8_t  resetReason;
  uint8_t  crc8;
} ch32_blackbox_header_t;

typedef struct {
  uint32_t nodeId;
  uint16_t seq;
  uint8_t  status;
} ch32_received_heartbeat_t;
