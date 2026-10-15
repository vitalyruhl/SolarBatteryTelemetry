# SolarBatteryTelemetry

Version **0.3.0** prepares a dedicated ESP32 with WiFi/UI/OTA, an optional BLE
worker and working MQTT connectivity plus all 25 fixed BMS/Victron telemetry
channels. Device adapters are absent: values are null, source availability is
unavailable and controls remain disabled. The editable default root is
`SolarBatteryTelemetry`. See [the MQTT contract](docs/MQTT.md) for channel types,
limits, root transitions, settings ownership and acceptance boundaries.

The local reserved static IPv4 is **192.168.2.124**; credentials remain ignored.
BluetoothEnable defaults off. Without verified peers it remains Disabled/Idle,
without scanning, connecting or initializing NimBLE. There are no battery/charger
writes, decoder, Home Assistant discovery, activity LED, display, external GPIO,
RS485 or inverter behavior. SolarInverterLimiter is unchanged.

## Development dependency and local setup

The prepared 4.2.2 scaffold built successfully before this runtime. Current preparation
source requires the independently reviewed **local ConfigManager 4.5.0 development
checkout**, which is not claimed to be published. Do not use an older registry
package as a fallback. The tracked configuration declares the actual consumer
NimBLE-Arduino **2.5.1** and PubSubClient **2.8** dependencies; ignored `platformio.local.ini` binds CM through
an absolute local symlink. See [the smoke guide](docs/ble-smoke-test.md) for the
exact local configuration and build commands.

Only `src/secret/secrets.example.h` is tracked. Local credentials and OTA upload
authentication follow the limiter, with the collector's unique app identity and
reserved address. The placeholder file is never automatically provisioned as real
WiFi credentials. Valid persisted WiFi settings and OTA UI edits survive reboot;
local network defaults are applied only when stored WiFi credentials are missing
or structurally invalid. The OTA secret is the existing setting's initial NVS
default, not a password overwrite on every boot.

## Software validation and manual acceptance

```powershell
./tools/test/run-smoke-host-tests.ps1
pio run -e usb -e ota
```

These commands run focused host checks and build firmware only. The selected
stack remains espressif32 7.0.1 / Arduino-ESP32 2.0.17 / nodemcu-32s, with the
dual-slot OTA layout. The MQTT candidate requires at least 10% actual binary
reserve per slot; see the MQTT guide for partition migration constraints. Consumer-only central/observer role flags
and LTO are used as verified in ConfigManager. The known upstream NimBLE LTO
warning is preserved; see the smoke guide for its exact-image limitations.

On 2026-10-15 the owner authorized USB flashing on COM7. The detected ESP32-D0WD-V3
with 4 MB flash received the verified 0.2.0 application, bootloader and partition
files; all upload hashes passed and the device was reset. The owner subsequently
reported basic operation with no observed crash during the initial short test.
This is initial smoke feedback, not long-duration, OTA or BLE-radio acceptance;
those checks and device protocols remain pending.

Read [project.info.md](project.info.md) for installation evidence and future scope.
Local governance/tooling and `.local/acceptance/` records are intentionally ignored.
No release, commit, push or publication is implied by these build instructions.
