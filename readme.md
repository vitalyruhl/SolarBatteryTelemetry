# SolarBatteryTelemetry

Firmware **0.3.0** reads a JBD 14-cell BMS and passive Victron SmartSolar Instant Readout on one ESP32 and publishes change-only MQTT telemetry. Runtime pages separate Bluetooth, MQTT, BMS and Victron. Battery and charger controls remain disabled.

The collector in the shed returned fresh BMS and Victron data simultaneously after the latest OTA installation. Ten observations over 95.6 seconds showed both sources available, MQTT online, continuous uptime and no additional reconnects. The owner subsequently confirmed incoming data. This is bounded acceptance, not a long-duration stability claim. The OTA sender missed its final acknowledgment even though the new firmware booted; see [BMS acceptance](docs/BMS.md).

## Configuration

Local credentials stay in ignored `src/secret/secrets.h`; only placeholders are tracked in `secrets.example.h`. The local reserved IPv4 address is 192.168.2.21.

- Bluetooth > Victron: Instant Readout MAC and encryption key.
- Bluetooth > BMS > Readout: JBD peer MAC; empty disables that source.
- MQTT: enabled, broker, credentials, base topic and publication interval.

`VICTRON_READOUT_MAC`, `VICTRON_READOUT_KEY` and `BMS_READOUT_MAC` seed initial settings. Persisted edits remain authoritative. Bluetooth is enabled once for the first configured readout upgrade; later user disables survive reboot. Existing OTA authentication is provisioned only if missing.

See [MQTT contract](docs/MQTT.md), [BMS readout](docs/BMS.md), [Victron readout](docs/Victron.md) and [project evidence](project.info.md). Unsupported or stale values stay unavailable rather than becoming fabricated zero measurements.

## Local ConfigManager dependency

This source requires the reviewed **local ConfigManager 4.6.0 development tree**, including the BMS transport additions on `feature/jbd-bms-transport`: `clearGattPeer`, `clearAdvertisementPeer` and characteristic-aware GATT writes. Ignored `platformio.local.ini` supplies `custom_cm_path` and an absolute `symlink://` dependency. The current local build works against that checkout.

At this SBT integration checkpoint, CM commit `4e9c31a` is published on `feature/demand-driven-runtime`, but the BMS transport additions are intentionally still uncommitted and unpublished at the owner's request. A fresh checkout of the published CM branch alone is insufficient to reproduce this SBT build. Preserve the local CM work until its separate checkpoint; do not substitute a registry package or assume the 4.6.0 version string proves those APIs exist.

The tracked consumer dependencies are NimBLE-Arduino 2.5.1 and PubSubClient 2.8. The stack is espressif32 7.0.1 / Arduino-ESP32 2.0.17 / nodemcu-32s with dual 1792 KiB OTA slots. See [local setup](docs/ble-smoke-test.md); its older 4.5.0 checkpoint is historical.

## Focused validation

```powershell
./tools/test/run-smoke-host-tests.ps1
python tools/test/run-settings-registration-tests.py
./tools/test/run-victron-host-tests.ps1
./tools/test/run-jbd-host-tests.ps1
pio run -e usb
```

The final USB-profile build used 1,342,481 bytes of flash and 80,360 bytes of static RAM. That exact build artifact was used for network OTA; no separate OTA-profile rebuild was needed. Firmware version remains 0.3.0 by owner request during debugging. Known upstream deprecation/LTO warnings remain.

Secrets, local build bindings, tooling state and detailed device captures are excluded from Git. Earlier smoke/preparation acceptance documents are retained as historical evidence.
