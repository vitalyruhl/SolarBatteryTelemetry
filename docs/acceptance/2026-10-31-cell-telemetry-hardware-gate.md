# Cell telemetry: authorized hardware gate and deployment

Date: 2026-10-31, Europe/Berlin. This updates the earlier preparation-only status.

The owner required SBT first, followed by SIL: a five-minute panic-free test on
the test ESP before each production installation.

- SBT 1.5.0 built and flashed on COM8. The isolated test uses DHCP and
  `SBTTest/COM8`, clears physical peers and blocks BMS control.
- Serial observation lasted 300 seconds from an explicit reset: one expected
  initial boot, no panic/fault matches, no unexpected boots or observer errors.
  Live MQTT uptime samples advanced during observation.
- Production OTA to `192.168.2.21` succeeded. `/version` confirmed
  `SolarBatteryTelemetry 1.5.0`.
- Production supplied real, available arrays for all 14 series groups. A further
  300-second passive capture had monotonic uptime; the final capture exceeded
  3,060 seconds uptime without a reset in the observed sequence.
- Final diagnostics included loop-stack minimum free 2,020 bytes and historical
  heap minimum free 1,868 bytes, with current free heap approximately 48 KB.
  These measurements do not establish that the earlier panic cause is fixed.

The bench has no physical BMS/Victron peers, so its gate is not a complete BLE
or production-load test. Actual readings were verified separately after upload.
Live UI stayed disabled. No production MOS command, capacity change, commit,
push or release publication was performed. Multi-day stability remains open.

Evidence is in `C:/Temp/energy-rollout-20261031`: SBT build/upload logs,
`sbt-testesp-gate.json`, serial/MQTT captures, `sbt-production-after.json`,
`sbt-production-during-sil-gate.json` and `final-production-state.json`.
