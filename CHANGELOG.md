# Changelog


## [1.1.2] - 2026-05-12

### Changed
- Updated README installation instructions to reflect Arduino Library Manager availability.
- Improved README formatting and public documentation polish.
- No code changes.



## [1.1.1] - 2026-05-11

Initial public release candidate of CrashHive32.

### Added
- ESP32 RTC/NOINIT crash blackbox.
- ESP-NOW heartbeat monitoring.
- Neighbor watchdog, state tracking, health scoring, silent duration, and packet-loss estimates.
- Boot storm detection and reset reason helpers.
- Metric threshold alerts and system health summary APIs.
- Developer-friendly helpers for neighbor timeout setup, state names, adaptive reason names, and healthy checks.
- Arduino examples and documentation for field diagnostics workflows.

### Changed
- Hardened BlackBox validation with schema v3 and per-event CRC.
- Kept ESP-NOW callback handling lightweight and drained through `tick()`.
- Improved MAC text parsing for colon and dash formats.
- Simplified the public changelog into a clean initial-release history.

### Notes
- Hardware validation is intentionally owner-side and should be documented with board, Arduino ESP32 core version, Serial Monitor evidence, and test date before claiming hardware-tested status.
- CrashHive32 is not a permanent data logger, not safety-certified, and not a full mesh routing system.
