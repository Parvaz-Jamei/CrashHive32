# Arduino Library Manager Readiness

CrashHive32 is structured as a standard Arduino library:

- `library.properties` in the root.
- Source code under `src/`.
- Single public header: `src/CrashHive32.h`.
- Internal implementation headers under `src/internal/`.
- Examples under `examples/`.
- CI workflow for compiling examples with ESP32 FQBN.

Before submission, collect real Arduino CLI or GitHub Actions compile evidence for all examples.
