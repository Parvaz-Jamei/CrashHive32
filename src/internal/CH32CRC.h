#pragma once

#include <stddef.h>
#include <stdint.h>

static inline uint8_t ch32_crc8_update(uint8_t crc, uint8_t data) {
  crc ^= data;
  for (uint8_t i = 0; i < 8U; ++i) {
    if ((crc & 0x80U) != 0U) {
      crc = static_cast<uint8_t>((crc << 1U) ^ 0x07U);
    } else {
      crc = static_cast<uint8_t>(crc << 1U);
    }
  }
  return crc;
}

static inline uint8_t ch32_crc8_buffer(const uint8_t* data, size_t length, uint8_t seed = 0U) {
  uint8_t crc = seed;
  if (data == nullptr) {
    return crc;
  }
  for (size_t i = 0; i < length; ++i) {
    crc = ch32_crc8_update(crc, data[i]);
  }
  return crc;
}
