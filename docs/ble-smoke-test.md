# BLE worker smoke-test preparation (0.2.0)

## Scope and prerequisites

This prepares a new ESP32 for WiFi, configuration UI and HTTP/Arduino OTA servicing,
plus the optional ConfigManager BLE worker. No peer addresses, device profiles or
UUIDs are supplied. It therefore validates software construction/build readiness
only: no RF initialization, scan, GATT connection, real telemetry, MQTT or LED.
Do not interpret this as radio coexistence or shed acceptance.

The actual board, flash capacity, USB port and later device identities must be
verified before any separately authorized upload. A fresh board has no OTA server;
USB bootstrap is necessary first. Both build environments retain the standard
1,310,720-byte application slots. No partition change is part of this stage.

## Local development binding

The reviewed local CM 4.5.0 source is required and remains unpublished. The build
fails early when its local binding is missing, older than 4.5, mismatched between
library/header/WebUI versions, or missing its prepared WebUI asset. The helper
never generates assets in or modifies the reviewed sibling checkout. Development
output/dependencies use `.pio/ble-smoke`, separate from the preserved 4.2.2 cache.

Create/retain ignored `platformio.local.ini` with the appropriate absolute path:

```ini
[env]
custom_cm_path = C:/path/to/reviewed/ConfigurationsManager

[env:usb]
lib_deps =
    ${env.lib_deps}
    symlink://C:/path/to/reviewed/ConfigurationsManager

[env:ota]
lib_deps =
    ${env.lib_deps}
    symlink://C:/path/to/reviewed/ConfigurationsManager
upload_port = 192.168.2.124
; Retain your local OTA authentication in upload_flags; never commit it.
```

The tracked consumer dependency is NimBLE-Arduino 2.5.1. No registry CM version
is advertised as compatible with this development API until it is published and
reviewed. PlatformIO is pinned to espressif32 7.0.1 / Arduino core 2.0.17; the board
target remains nodemcu-32s. Only central/observer roles are retained. Consumer LTO
removes the inherited linker `-fno-lto`; no framework/toolchain files are patched.

NimBLE 2.5.1 emits an LTO declaration mismatch for
`is_ble_hs_resolv_enabled` (`int` versus `uint8_t`). Reviewed CM example images
discarded the incompatible `ble_gap_host_check_status` checker and the dependency
has no source callers of that checker. Preserve the warning and assess the exact
collector ELF; this is not a universal guarantee for other consumers or flags.

Local `src/secret/secrets.h` supplies WiFi/OTA defaults matching the limiter;
MY_WIFI_IP is 192.168.2.124 and MY_USE_DHCP is false. Other credentials/network
values and OTA upload authentication remain local. The app identity is unique.
Without local secrets, placeholders are not applied and UI/AP recovery remains.

## Startup and settings

Registration order is CoreSettings WiFi, standard System OTA enable/password,
CoreSettings NTP, Bluetooth and runtime fields, then the OTA lifecycle observer.
Settings load before defaults, BLE begin and web-server startup. Normal
onWiFiConnected/onWiFiDisconnected/onWiFiAPMode use CoreWiFiServices; OTA starts
after IP readiness. Core settings remain operator-configurable.

Inherited CoreWiFiServices limitation: when OTAEn was disabled at boot, enabling
it in the UI during the same WiFi connection does not initialize ArduinoOTA and
its reboot callback. Restart after enabling it before the ArduinoOTA smoke test;
do not infer readiness from the checkbox alone. This stage does not change that
shared helper or claim to repair this existing initialization limitation.

Valid stored WiFi credentials preserve UI edits, including DHCP/static addressing.
Only missing/invalid stored credentials trigger local defaults without a forced
reboot. The password shape check matches the limiter (SSID 1..32, password <=63);
it does not authenticate connectivity. The standard OTA password's default is
seeded before registration/loadAll, so existing stored edits, including an empty
password, are preserved. An already configured reused board may therefore retain
another address: inspect/correct it through the UI; this firmware does not erase
NVS or force the reserved address over operator choices.

BluetoothEnable is the reviewed module's persisted switch, default false. It owns
its atomic setting callback; this application does not replace it or add another
first-boot key. No peer/profile UI keys are invented. Once verified identities are
available, a later approved application stage can call configureAdvertisementPeer
or configureGattPeer with explicit address types and verified protocol UUIDs,
then requestConnect as appropriate. No transport initialization is done directly
through NimBLE by this application.

The Bluetooth live page reports the smoke scope, worker-ready flag, reset reason,
enabled/state/connected/scanning, drops/errors/late events and stack watermark.
State codes follow CM BleState: 0 Disabled, 1 Idle, 2 Scanning, 3 Connecting,
4 Connected, 5 Paused, 6 Error. Without peers, expected state is Disabled or Idle;
connected/scanning remain false. Worker-ready only confirms task creation, not RF
or device connectivity. Optional BLE failure leaves WiFi/UI/OTA available.

OTA observers only request atomic pause/resume. Starting and success pause BLE;
failure/abort/timeout resume only the OTA reason. Reviewed CM serializes sessions,
retains the successful owner through its response/reboot handoff and latches
success pause until reboot. Observer return is not radio-quiescence proof.

## Build checks and subsequent manual acceptance

```powershell
./tools/test/run-smoke-host-tests.ps1
pio run -e usb -e ota
```

No upload target or serial session is invoked. Preserve the exact ELF, resolved
versions, size report and hashes locally before eventual physical tests.

After separate authorization for the actual unit/port and test stage:

1. Bootstrap over USB and verify board/flash/boot behavior without exposing secrets.
2. Verify WiFi at the expected reserved address or explain retained NVS settings;
   open the UI and confirm editable WiFi and System OTA fields.
3. Check system/reset/heap and Bluetooth smoke fields; enable/disable Bluetooth
   and confirm Disabled/Idle without claiming RF or telemetry.
4. Verify OTA success/reboot, failed/aborted updates and recovery through the
   reviewed HTTP/Arduino paths. Do not infer this from compilation.
5. Preserve UI settings through reboot; investigate unexpected resets or memory
   trends before device/radio testing.

BMS/Victron addresses, address types, GATT UUIDs, Victron key/firmware, actual
packet/fragment behavior, RF range, phone contention and live heap/stack stability
are still missing evidence. Device polling, decoding, MQTT freshness/schema and
LED behavior need a later approved scope. Nothing here authorizes control writes.

## Initial hardware observation - 2026-10-15

The owner authorized USB flashing on COM7. The detected ESP32-D0WD-V3 revision
v3.1 has 4 MB flash. The reviewed 0.2.0 application and matching bootstrap files
were written with successful hash checks, followed by a hard reset. The owner
then reported basic operation with no observed crash during the initial short
test and authorized local commits. No test duration or measured heap/stack trend
was supplied. Actual OTA success/failure recovery, BLE RF/peer operation and
long-duration stability remain unverified.
