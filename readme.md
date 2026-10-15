# SolarBatteryTelemetry

The current **0.3.0 working tree** adds passive Victron SmartSolar Instant Readout
and repairs MQTT/OTA settings registration while retaining the existing CM 4.5.0
dependency and the owner's requested version. The installed 0.3.0 checkpoint is
historical; the additions below are not claimed to have been uploaded or verified
on the charger yet.

Seven direct SmartSolar readings can populate the fixed 25-channel MQTT contract.
BMS data remains unavailable; unsupported PV voltage/current and deferred derived
battery power stay null. Controls remain disabled. The editable default root is
`SolarBatteryTelemetry`. See [MQTT](docs/MQTT.md) and [Victron](docs/Victron.md).

The local reserved static IPv4 is **192.168.2.21**; credentials remain ignored.
Set `VICTRON_READOUT_MAC` and `VICTRON_READOUT_KEY` in the ignored secret header
for first-boot defaults, or edit Bluetooth / Victron / Instant Readout in Settings.
On the first configured readout upgrade, a persistent migration enables Bluetooth
once; a later explicit user disable survives reboot. Readout is passive: no GATT
connection, battery/charger writes, external GPIO or inverter behavior is added.

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
python tools/test/run-settings-registration-tests.py
./tools/test/run-victron-host-tests.ps1
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
