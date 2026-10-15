# MQTT preparation 0.3.0 acceptance record

Date: 2026-10-15. Branch: feature/mqtt-telemetry. Checkpoint based on 47369d56bf653bb5d0bd7a92fbbb96f9e38348fe; the owner requested commit and push before a workstation restart.

## Verified software scope

All 25 fixed telemetry channels, labels, units, support/state strings and reserved command paths participate in production code. Unknown values are null and sources unavailable; command handling and actual JBD/Victron adapters are absent. The default editable root is SolarBatteryTelemetry. The operational root limit is 63 ASCII bytes; this does not add generic CM storage validation.

Focused production-helper tests cover the telemetry contract, bounded settings handoff, reconnect/root retirement, endpoint ownership and OTA phase boundaries. The controller independently ran the three changed host test cases successfully. Four P2 lifecycle findings were corrected and independently re-reviewed with no remaining blocker. All 17 frozen source hashes matched before and after the final affected-object rebuild. No source mutation followed that rebuild; subsequent edits are this acceptance documentation only.

Both USB and OTA builds passed. The exact BINs contain the SHA256 of their respective final ELFs. The controller independently parsed partition entries and MD5, verified the 4 MB boundary/no overlap, checked both OTA slots and located all 25 channel paths/labels and reserved command names in both actual images.

| Measurement | USB | OTA |
| --- | ---: | ---: |
| Actual firmware BIN bytes | 1,328,448 | 1,328,448 |
| Program sections reported by PlatformIO | 1,321,869 | 1,321,869 |
| Static RAM reported by PlatformIO | 78,920 / 327,680 | 78,920 / 327,680 |
| Bytes per application slot | 1,835,008 | 1,835,008 |
| Free bytes per slot | 506,560 | 506,560 |
| Free percentage per slot | 27.61% | 27.61% |

USB BIN SHA256: `e727c6a05b23eb31f85ba02214341b39c8cbfcb9b26e52528e4ba007d870a2ea`.
OTA BIN SHA256: `34dfddb007e188e1d39f14861874a13def56e5e5eafafb5b6c21dd3766e656f7`.
Partition BIN SHA256: `b8a579361a5c2bf99e12fb82332892b2cc6c1c2aba69aa043ed733af2b3b02b3`.

The previous default-slot candidate exceeded capacity. The applied dual-OTA table uses app0 at 0x10000 and app1 at 0x1d0000, each size0x1c0000; SPIFFS is 0x390000/0x60000. NVS, otadata and coredump extents remain unchanged. Logging was already disabled; it was not a new size saving.

## Authorized hardware installation

Immediately before flashing, esptool identified COM11 as ESP32-D0WD-V3 revision3.1, MAC88:57:21:8f:66:4c, 4 MB flash. A complete USB upload wrote bootloader0x1000, partitions0x8000, boot_app0 at0xe000 and application0x10000. Every segment reported `Hash of data verified.` and the tool exited successfully, then reset the device via RTS. No full-chip erase or NVS write was issued. OTA metadata was initialized by boot_app0 as part of partition migration.

Local evidence: `.local/acceptance/mqtt-030/flash-com11.log`, `fix1-exact-final-build.log`, `fix1-exact-build-before.json`, `fix1-exact-build-after.json` and `fix1-final/artifact-manifest.json`. Original review, reproductions and superseded builds remain separate from final acceptance evidence.

## Boundaries and repository state

This records verified software checks and transfer integrity, not a confirmed boot/runtime session, broker receipt, full physical OTA transfer, live BLE decoding, WiFi/BLE coexistence or long-duration stability. No serial monitor session or BMS/charger-write command was introduced. The known upstream NimBLE declaration/definition LTO warning and ArduinoJson deprecation warnings remain visible; the incompatible checker/resolver path is absent from both exact final ELFs, with no universal guarantee for future builds. MQTT TCP/DNS remains synchronous and an in-flight call cannot be cancelled by the OTA pause.

CM feature/bluetooth-module was pushed at65762bd22edddd0307427d01f056fbbe0ddc0b51; main stayedfc171e07a2081dcfff722e527e144ddf4d92fe5b. Its original Serena change is preserved. SBT PR2 merged the previous BLE smoke into main47369d56bf653bb5d0bd7a92fbbb96f9e38348fe; the merged feature/ble-smoke-test branch was safely removed locally/remotely. The MQTT work is saved as an explicitly unfinished checkpoint on feature/mqtt-telemetry; it is not approved for main integration.
## Open runtime findings and restart handoff

The owner reports repeated "Fehler: Request timeout" during page loading, with settings remaining empty even after a forced reload. The issue remains unresolved; debugging is paused at the owner's request. No repair was implemented after the validated firmware build.

Read-only HTTP checks returned complete, valid JSON for config.json (3014 bytes), runtime.json (3387 bytes), runtime_meta.json (9964 bytes), and appinfo. These checks do not reproduce or disprove the browser timeout. Chrome automation was blocked by the browser tool's request-header-policy initialization, so there is no browser network trace proving the timeout's cause.

The live configuration omits the MQTT category and OTA password setting. Static inspection found duplicate Config storage-key reservations caused by constructing additional settings and assigning temporary Config instances in MqttService.h and the existing OTA-password initialization in main.cpp. ConfigManager rejects settings carrying the collision error. Treat this as a separate confirmed registration defect until its relationship to the browser timeout is established. The current portable helper tests do not cover real Config construction/registration.

The runtime endpoint reported MQTT online and approximately 42.8 KB free heap; this is not proof of broker receipt or stable browser operation. Resume by correcting settings construction/default initialization while preserving persisted values, intentionally empty passwords, and the existing UI-to-loop synchronization. Validate actual Config registration, investigate the browser timeout independently, and repeat hardware acceptance before claiming a fix. No BMS/charger commands or device adapters were added.

Before this checkpoint, all 13 non-Markdown files in the final build's source/config/test hash manifest were rechecked unchanged. Both recorded USB and OTA builds succeeded. Build and helper-test evidence therefore remains applicable to those exact inputs, but does not override the open runtime defects above.
