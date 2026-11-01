# Isolated SBT / AsyncTCP compatibility build

Private local evidence. Do not publish the consumer, firmware, ignored inputs or raw logs.

USB and OTA-profile compilation succeed with released AsyncTCP 3.5.0 and the exact CM freeze on Arduino 2.0.17 / ESP-IDF 4.4.7. The unchanged baseline succeeds. No source API patch or SDK upgrade was needed.

## Version matrix

| Component | Version |
| --- | --- |
| SBT | 1.4.12 |
| CM | 4.10.5 exact unchanged freeze |
| ESPAsyncWebServer-esphome | 3.4.0 |
| Baseline AsyncTCP-esphome | 2.1.4 |
| Candidate ESP32Async/AsyncTCP | 3.5.0 |
| PlatformIO espressif32 | 7.0.1 |
| Arduino ESP32 | 2.0.17 (3.20017.241212+sha.dcc1105b) |
| ESP-IDF | 4.4.7 |
| Xtensa toolchain | 8.4.0+2021r2-patch5 |
| NimBLE-Arduino | 2.5.1 |
| PubSubClient | 2.8.0 |
| ArduinoJson | 7.4.3 |

Board nodemcu-32s; partition dual-ota-1792k.csv; C++17 and LTO retained. All stages confirmed WebUI Live0 Settings1 and identical UI asset hashes.

## Builds

| Environment | Result | RAM / 327680 | Flash / 1835008 | Warnings | Seconds |
| --- | --- | --- | --- | --- | --- |
| usb | PASS | 83220 | 1316613 | 165 | 169.6 |
| compat_async35_usb | PASS | 83148 | 1316617 | 173 | 128.9 |
| compat_async35_ota | PASS | 83148 | 1316617 | 173 | 163.4 |

Each build compiles 21 CM and 7 ESPAsyncWebServer units. Candidates compile only successor AsyncTCP, never AsyncTCP-esphome. Registry source matches official v3.5.0 after newline normalization. Candidate USB static RAM is 72 bytes smaller and flash 4 bytes larger; these are linker estimates, not runtime heap measurements.

## Alias and integrity checks

CM and WebServer metadata still request the old package. Only the temporary consumer profile adds exact ESP32Async/AsyncTCP@3.5.0 and lib_ignore = AsyncTCP-esphome. The old transitive package is downloaded but excluded from compilation/linking. WebServer 3.4.0 and CM remain compiled. This experiment does not adopt a product dependency policy.

The consumer is a git archive of main commit 48726cad93ee822deac96c7631634cd38c28209b. All 194 freeze files match their originals by SHA-256 after building. Owning source remains clean. Two ignored inputs were copied without printing contents and remain private. No owning CM/SBT/SIL files or feature worktrees changed. Version 1.4.12 remains for this disposable experiment.

New distinct warning messages compared with baseline:

- warning: 'void AsyncClient::close(bool)' is deprecated: Use AsyncClient::close() instead [-Wdeprecated-declarations]

## Evidence and limits

Compile-only PlatformIO environments: usb, compat_async35_usb and compat_async35_ota, stopping on any failure. No upload, serial monitor, device operation or live UI access. Files: compat-build-status.json, three compat-build-*.log files, compat-consumer-inputs.json, COMPATIBILITY-RESULTS.json, and consumer platformio.local.ini.

This proves build compatibility only. No runtime/HIL or multi-day stability test; production panic and MQTT failure causes remain unproven. Product migration needs a reviewable dependency change and requested multi-day acceptance. The v3.5.0 release notes describe changed abort-callback task context; compilation does not validate application assumptions about that behavior. Unreleased main 4.0.0 was not substituted for this release.
