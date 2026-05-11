# CrashHive32 BlackBox Format

CrashHive32 v1.1.1 uses BlackBox schema version 3.

## Storage model

The BlackBox uses best-effort `RTC_NOINIT_ATTR` memory. It is intended to preserve a small ring buffer across some reset scenarios, but it is not guaranteed across all power loss, brownout, bootloader, or memory initialization cases. Brownout does not automatically clear valid storage; if RTC/NOINIT memory and CRC survive, events remain available.

## Header

The header stores metadata only:

- `magic`
- `version`
- `eventCount`
- `writeIndex`
- `resetReason`
- `crc8`

The header CRC covers only header metadata up to the CRC field.

## Event

Each event has its own CRC:

```cpp
typedef struct __attribute__((packed)) {
  uint32_t t_ms;
  uint8_t  type;
  char     key[CH32_BLACKBOX_KEY_SIZE];
  float    value;
  uint8_t  code;
  uint8_t  crc8;
} ch32_blackbox_event_t;
```

This avoids recomputing a full array CRC on every event append. `recordMetric()` and `recordFlag()` update only the new event CRC and the header metadata CRC.

## Event types

- `metric`
- `flag`
- `boot`
- `reset`

## Schema migration

Schema v3 safely invalidates older RTC/NOINIT buffers. If magic, version, bounds, header CRC, or any active event CRC is invalid, CrashHive32 resets the BlackBox storage safely and does not emit a misleading report.

## Key length

BlackBox keys use `CH32_BLACKBOX_KEY_SIZE = 16`, which means up to 15 useful characters plus the null terminator.

## Flag code range

`recordFlag(name, code)` stores code as an 8-bit value. Values below 0 are clipped to 0; values above 255 are clipped to 255. Do not use the flag code to store full `uint32_t` node IDs.
