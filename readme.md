# SolarBatteryTelemetry

Firmware **0.3.0** reads a JBD 14-cell BMS and passive Victron SmartSolar Instant Readout on one ESP32 and publishes change-triggered MQTT telemetry with a full 60-second refresh. Runtime pages separate Bluetooth, MQTT, BMS and Victron. Guarded charge/discharge MOS control is implemented but disabled by default; Victron remains read-only.

The collector in the shed returned fresh BMS and Victron data simultaneously after the latest OTA installation. Ten observations over 95.6 seconds showed both sources available, MQTT online, continuous uptime and no additional reconnects. The owner subsequently confirmed incoming data. This is bounded acceptance, not a long-duration stability claim. The OTA sender missed its final acknowledgment even though the new firmware booted; see [BMS acceptance](docs/BMS.md).

## Configuration

Local credentials stay in ignored `src/secret/secrets.h`; only placeholders are tracked in `secrets.example.h`. The local reserved IPv4 address is 192.168.2.21.

- Bluetooth > Victron: Instant Readout MAC and encryption key.
- Bluetooth > BMS > Readout: JBD peer MAC; empty disables that source.
- BMS control enabled: commissioning gate, default false. Leave disabled until installation qualification.
- MQTT: enabled, broker, credentials, base topic and publication interval.

`VICTRON_READOUT_MAC`, `VICTRON_READOUT_KEY` and `BMS_READOUT_MAC` seed initial settings. Persisted edits remain authoritative. Bluetooth is enabled once for the first configured readout upgrade; later user disables survive reboot. Existing OTA authentication is provisioned only if missing.

See [MQTT contract](docs/MQTT.md), [BMS readout](docs/BMS.md), [Victron readout](docs/Victron.md) and [project evidence](project.info.md). Unsupported or stale values stay unavailable rather than becoming fabricated zero measurements.

## Local ConfigManager dependency

The active ignored `platformio.local.ini` binding selects **ConfigManager 4.6.0**, commit `554869da7efc98e0c1874e74bf0c78d4bcd1664a`, through an absolute local dependency. This includes the reviewed shared BLE transport additions. The tracked dependency declaration is unchanged; reproducing the local build requires an equivalent selected CM revision, not just the same version string.

The tracked consumer dependencies are NimBLE-Arduino 2.5.1 and PubSubClient 2.8. The stack is espressif32 7.0.1 / Arduino-ESP32 2.0.17 / nodemcu-32s with dual 1792 KiB OTA slots. See [local setup](docs/ble-smoke-test.md); its older 4.5.0 checkpoint is historical.

## Focused validation

```powershell
./tools/test/run-smoke-host-tests.ps1
python tools/test/run-settings-registration-tests.py
./tools/test/run-victron-host-tests.ps1
./tools/test/run-jbd-host-tests.ps1
./tools/test/run-bms-command-tests.ps1
pio run -e usb
```

The earlier JBD acceptance USB-profile build used 1,342,481 bytes of flash and 80,360 bytes of static RAM. That exact build artifact was used for network OTA; no separate OTA-profile rebuild was needed. Firmware version remains 0.3.0 by owner request during debugging. Known upstream deprecation/LTO warnings remain.

Secrets, local build bindings, tooling state and detailed device captures are excluded from Git. Earlier smoke/preparation acceptance documents are retained as historical evidence.

The current feature adds minimum NTC temperature, cell/NTC counts, named protection causes, independent source-health heartbeats and guarded MOS commands. See [inventory](docs/telemetry-value-inventory.md), [command contract](docs/MQTT.md#guarded-command-contract) and [HA additions](docs/home-assistant-bms-controls.yaml). Live MOS switching is not qualified by host tests or telemetry readback.

### Fixed 60-second MQTT refresh

At the owner's request on October 16, every 60 seconds the collector republishes source/control availability, capabilities and all currently valid measurements, including unchanged values and differences below the deadband. This timer is independent of the normal publication interval. Between refreshes the 0.1-unit/1-W deadbands still apply. Unsupported fields remain unavailable; no fabricated values are emitted. Admission retry, bounded cursor work and OTA pause remain in force. A restarted Home Assistant recovers unchanged values within approximately one minute while the collector, sources and broker are available. Network failures or OTA pauses can extend that interval. This refresh does not replace the 5-second source-health heartbeat.
