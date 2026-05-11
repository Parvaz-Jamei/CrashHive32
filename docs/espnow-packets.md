# ESP-NOW Packets

CrashHive32 v1.1.1 uses ESP-NOW for small heartbeat packets only. It does not implement full mesh routing or guaranteed application-level delivery.

## Heartbeat packet

```cpp
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
```

The packet remains below the ESP-NOW v1 250-byte payload limit.

## Sequence number

`seq` is used by the neighbor watchdog to estimate packet loss from received sequence gaps. Duplicate or out-of-order packets do not create fake packet loss.

## Status field

`sendHeartbeat(nodeId, status)` uses an application-defined 8-bit status value. CrashHive32 does not impose a fixed status enum in v1.1.1.

## MAC text helpers

MAC text helpers require exact length and one consistent separator:

- `24:6F:28:AA:BB:01`
- `24-6F-28-AA-BB-01`

Mixed separators such as `24:6F-28:AA:BB:01` are rejected. Uppercase and lowercase hex are accepted.

## ESP-NOW callback rule

ESP-NOW send/receive callbacks should stay lightweight. CrashHive32 receive callbacks validate heartbeat packets and queue a small pending heartbeat event. `tick()` drains the queue and calls neighbor logic. User callbacks and BlackBox writes are not performed directly from ESP-NOW callbacks.

The send callback indicates MAC-layer send status; it does not guarantee that the destination application processed the payload.
