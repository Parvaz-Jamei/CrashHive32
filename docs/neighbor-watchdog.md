# Neighbor Watchdog

CrashHive32 tracks fixed-size neighbor entries without dynamic allocation.

## States

- `CH32_NEIGHBOR_UNKNOWN`: node is not known, not initialized, or watched but still inside its first timeout window before any heartbeat.
- `CH32_NEIGHBOR_ALIVE`: recent heartbeat received.
- `CH32_NEIGHBOR_DEGRADED`: heartbeat age or packet loss suggests the link is weak.
- `CH32_NEIGHBOR_SILENT`: timeout exceeded.

## APIs

```cpp
CrashHive32.watchNeighbor(2, 15000UL);
CrashHive32.watchNeighborEvery(3, 5000UL);
CrashHive32.removeNeighbor(2);
CrashHive32.noteHeartbeat(2);
CrashHive32.getNeighborState(2);
CrashHive32.getNeighborStateName(2);
CrashHive32.getNeighborAgeMs(2);
CrashHive32.getNeighborSilentMs(2);
CrashHive32.getNeighborHealth(2);
uint8_t health = 0;
CrashHive32.getNeighborHealth(2, &health);
CrashHive32.isNeighborHealthy(2);
CrashHive32.isNeighborDegraded(2);
CrashHive32.getNeighborPacketLoss(2);
```

## Health score

- `100`: excellent
- `70..99`: healthy
- `40..69`: degraded
- `1..39`: critical
- `0`: unknown, missing, dead, or silent too long

`getNeighborHealth(nodeId)` never returns `255`; it returns `0` when the node is unknown or missing. Use `getNeighborState()` or `getNeighborHealth(nodeId, &outHealth)` to distinguish "not yet meaningful" from a real 0-health missing/silent node.

Health is derived from heartbeat age and sequence-gap packet loss. Watched neighbors that have not produced any heartbeat yet remain `CH32_NEIGHBOR_UNKNOWN` only during the first timeout window. If no heartbeat arrives by `timeoutMs`, the neighbor becomes `CH32_NEIGHBOR_SILENT`, health becomes `0`, and silent/adaptive callbacks fire once. Health is not RSSI-based in v1.1.1.

Recommended timeout: configure `timeoutMs` at least 2.5x to 3x the expected heartbeat interval. Degraded state is an early warning, not a replacement for the hard silent timeout.

## Packet loss

Packet loss is estimated from heartbeat sequence gaps. If there is not enough packet history, the result is `0`.

## Callbacks

`onNeighborSilent()` and `onNeighborRecovered()` report precise state transitions. `onAdaptiveResponse()` provides a higher-level reason code for adaptive behavior. Degraded and healthy events are emitted once per episode and are not repeated every tick.

## Developer UX helpers

`watchNeighborEvery(nodeId, expectedHeartbeatMs, missedBeats)` helps users configure timeouts from heartbeat intervals. The default allows three missed heartbeats. `getNeighborStateName()` and `getAdaptiveReasonName()` make Serial logs readable without switch statements. `isNeighborHealthy()` uses the safe health query internally and returns `false` for unknown health.
