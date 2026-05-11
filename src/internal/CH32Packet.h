#pragma once

#include <stddef.h>
#include <stdint.h>

#define CH32_PACKET_MAGIC        0x43483332UL
#define CH32_PACKET_VERSION      1U
#define CH32_PACKET_HEARTBEAT    1U

typedef struct __attribute__((packed)) {
  uint32_t magic;
  uint8_t  version;
  uint8_t  packetType;
  uint32_t nodeId;
  uint32_t uptimeMs;
  uint16_t seq;
  uint8_t  status;
  uint8_t  crc8;
} ch32_heartbeat_packet_t;

static_assert(sizeof(ch32_heartbeat_packet_t) <= 250U, "CH32 heartbeat packet must fit ESP-NOW v1 packet limit");
