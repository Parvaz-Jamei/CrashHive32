# Release Checklist

- `library.properties` is present in root and uses SemVer-compatible `version=1.1.1`.
- Only `src/CrashHive32.h` is public in the `src` root.
- Internal headers remain under `src/internal/`.
- All examples compile with ESP32 FQBN.
- `.github/workflows/arduino-compile.yml` is present.
- No `DELIVERY_NOTE.md` in the public package.
- No `.development`, `.exe`, build output, cache output, or symlink.
- No MQTT, AI, cloud, dashboard, OTA, full mesh routing, persistent Flash logging, or RSSI scoring.
- README and docs do not claim safety certification, guaranteed crash recovery, or full mesh routing.
