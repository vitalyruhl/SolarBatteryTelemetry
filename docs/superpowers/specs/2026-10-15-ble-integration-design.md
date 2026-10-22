# BMS and Victron Integration Design Proposal

Date: 2026-10-15. Status: proposed for owner review; no implementation authorized.
Companion: [historical implementation-plan evidence](../../acceptance/2026-10-15-ble-integration-evidence.md).
Context: [project information](../../../project.info.md).

## Approved intent and boundaries

Use one dedicated ESP32 of the same board type as SolarInverterLimiter. Test that
physical unit locally, then place it in the shed near the BMS and Victron and
update it over OTA. Keep ConfigManager and MQTT/Home Assistant integration.
The owner permits planning an optional Bluetooth module in ConfigurationsManager,
provided existing consumers remain compatible. No external IO or display; only
an onboard LED pulse on successful telemetry publication. No automatic commits.

BMS/Victron access is currently possible only from the phone in the shed. The
owner can obtain the BMS address. Neither address type/GATT compatibility nor the
Victron address/encryption key has been established. These block live device
acceptance, not offline implementation or local radio stability checks.

This document proposes software contracts and engineering test budgets for
review. They are not existing interfaces or previously approved domain rules.
The associated plan may be approved as a whole before implementation. No energy
scheduling, inverter control, battery protection changes, charging commands,
Home Assistant automation, or modification of SolarInverterLimiter is included.

## Verified baseline and documentation discrepancies

| Repository or component | Verified state |
| --- | --- |
| SolarBatteryTelemetry | main/origin/main 784319b8d8612317644141340d09d996a5cdaabe; inert preparation scaffold |
| ConfigurationsManager | main/origin/main fc171e07a2081dcfff722e527e144ddf4d92fe5b; library.json 4.4.10 |
| Collector dependency | ConfigManager 4.2.2, PubSubClient 2.8 |
| Platform | platformio/espressif32 7.0.1; installed Arduino package 3.20017.241212+sha.dcc1105b, Arduino core 2.0.17 |
| Board target | nodemcu-32s; board manifest 4 MB flash; variant LED_BUILTIN=2; physical size/polarity still to verify |
| Default partitions | app0 and app1 each 0x140000 = 1310720 bytes; OTA must retain two app slots |
| Previous build evidence | preparation firmware 489721 bytes; static RAM 27508 bytes; not a BLE runtime budget |

Serena resolved setup/loop; Atlas exact slice src/main.cpp:7-19 confirms there is
no runtime ConfigManager initialization, WiFi, MQTT, BLE or OTA service yet.
An OTA PlatformIO environment alone does not make the current device OTA-ready.

ConfigManager source findings at the recorded revision:

- src/mqtt/MQTTManager.h:134 says attach is layout-only; lines 786-790 and
  815-828 actually register settings and callbacks. docs/MQTT.md:8 conflicts.
  Follow code and correct the narrow comment/documentation drift during work.
- MQTT is an explicitly included module, not a general module-registry entry.
  PubSubClient is supplied by consumers. Do not invent a plugin registry.
- src/ConfigManager.h:1906 loadAll follows settings registration;
  handleClient at 2579 does not drive MQTT or an arbitrary module loop.
- src/ota/OTAManager.h:31-64 exposes reboot/logger callbacks, not lifecycle
  observers. ConfigManager.h:2843 replaces those during setupOTA.
  getOTAManager().isActive() exists, but its implementation also requires
  initialized/enabled flags and does not distinguish HTTP success/error/abort.
- ConfigManager.h:650 masks password JSON; toJSON(false) omits secrets and
  toJSON(true) still masks them. Reveal is separately authenticated. NVS storage
  is not encrypted merely because a field is marked as a password.
- ConfigManager's Atlas index could not publish: invalid graph identity text:
  identity text exceeds the byte limit. Bounded source reads were used instead.
  Existing .serena/project.yml changes were preserved. Tool state stays local.

## Reusable solutions and selected proposal

| Candidate | Pin and license | Decision |
| --- | --- | --- |
| h2zero/NimBLE-Arduino | 2.5.1, 5ae2c43f62c5a588a25c1de68f7d17b6f3b75adb; Apache-2.0 | Trial transport pin; exact combined build is an implementation gate |
| SH3D/VictronBLE | 0.7.1, 318e221170f6fac991e6f68e244b1323c0fa33ce; MIT; PIO scottp/victronble | Reuse only portable C decoder and software AES with scoped corrections |
| syssi/esphome-jbd-bms | 2.5.0, 31fc9ac0b2d339af6381f7ba29fcbce18cd8fa22; Apache-2.0 | Adapt telemetry-only framing/decoding; ESPHome component is not drop-in Arduino code |
| Fabian-Schmidt/esphome-victron_ble | 3e317e637735e36115bdb415e373200929f89bb3; GPL-3.0 | Independent protocol cross-check; no source copying in selected design |
| FurTrader/Overkill-Solar-BMS-Arduino-Library | 87d64873cb6a5dfec7c7e4ab02d5a4a30059543d; BSD-3-Clause | UART library; would still require BLE transport and control-path exclusion |
| ethycS0/jbd_bms | 4163675640096064639997ea55d9af8886c81d6a; no reuse license found | Reject: unresolved license and verified parser/notification assumptions |

NimBLE's tagged CI includes stock-espressif32 legacy builds; that is candidacy
evidence, not proof of this exact combination. Do not combine NimBLE with the
Victron Arduino wrapper, which initializes Bluedroid. Importing that whole
library through lib_deps is not the selected implementation. Vendor the minimal
MIT C subset, preserve its license and pinned provenance, and track a small
reviewable patch; no custom cryptography. Pin upstream revisions, not branches.

Alternatives considered: a full ESPHome collector reduces integration work but
replaces the requested ConfigManager application; a second gateway adds hardware
and deployment. Keep both as explicit fallback decisions if memory/coexistence
fails; do not silently switch architecture.

## Proposed architecture and responsibilities

ConfigurationsManager owns generic BLE transport, its optional settings/status,
and additive OTA lifecycle notifications. SolarBatteryTelemetry owns device
identities/keys, JBD and Victron adapters, validity/freshness, MQTT mapping and LED.
BLE-only; one stack instance, one BMS client, one advertisement scanner.

Add src/bluetooth/BluetoothManager.h as an explicitly included, header-only
module, analogous to MQTT. Do not include it from ConfigManager.h or add NimBLE
to the main library's dependencies. The BLE consumer declares NimBLE explicitly.
A default consumer must compile without a NimBLE installation. Module attach
registers settings exactly once; settings load before begin. No global radio
initialization during static construction. No shared-library device profiles.

Use one bounded BLE worker for potentially blocking connect/discovery operations;
never block the application loop or AsyncWeb callbacks on connect/discovery.
A finite command/event queue transfers owned copies (advertisement events contain
manufacturer data including company ID, notification events contain raw fragments); no borrowed notification
pointers. Callbacks only validate bounds, copy, count and return. They must not
publish MQTT, save NVS, decrypt packets, change UI state or restart the stack.
The application loop consumes events, decodes and publishes. All dynamic device
objects have one owner; no reconnect-time accumulation of clients/callbacks.

Proposed starting budgets (verify on hardware, change only with recorded reason):
16 event slots, each up to 272 bytes; one outstanding BMS query; 2 s reply and
fragment deadlines; reconnect delay 1,2,4,8,16,30 s capped at 30 s; scan windows
1 s every 5 s, passive initially, stored scan-result count disabled. Pause scan
while establishing/discovering a GATT link, then resume bounded scanning.
If passive discovery is insufficient, investigate with a bounded scan; do not
permanently enable aggressive active scanning by assumption.

The module runtime exposes enabled/state, connected/scanning, queue drops,
reconnect count and last error code. A bool enabled defaults false; the collector
can provision its own enabled default. No arbitrary device-address enumeration
or unbounded result history is retained. User configuration changes enqueue
reconfiguration and invalidate old-generation events; no teardown in callbacks.

## Read-only device contracts

JBD: verify service FF00, notify FF01, command FF02 and address type on this
specific device. Allow only status 0x03, cells 0x04 and identification 0x05 reads.
A GATT write carrying a read request and subscribing to notifications is allowed;
configuration/control writes, factory access and password changes are not.
No automatic FF AA authentication/root-password path. If required, stop for a
new device-specific design decision. No guessed PIN or pairing brute force.

Assemble fragmented/coalesced responses in a bounded buffer. Reply length is
payload length + 7; validate DD/end77, status, expected register and checksum
before access. Maximum wire frame 262 bytes fits the 272-byte envelope. The 14S
cell reply is 28 payload bytes/35 total bytes. Reset on disconnect, timeout,
malformation or generation change; never update freshness from a rejected frame.

Victron: receive only the configured address and Solar record type 0x01.
Manufacturer data includes company ID 0x02E1; product type is 0x10. Use the
16-byte Instant Readout key, not the connection PIN. The API is
victronble_decode(mfg, len, key, out). Solar requires 22-31 manufacturer bytes.
Patch the portable decoder's actual-length handling and unavailable sentinels;
its current zero-padding and blanket fixed parser length admit short packets.
Keep invalid fields absent/null, never turn them into zero. The key check is
one byte, not cryptographic authentication: a matching byte cannot prove the
entire key. Reject wrong header/type/length, apply bounded plausibility checks,
and verify the complete key through real-device comparison. Do not claim
spoof-proof or authenticated measurements. Suppress immediate duplicates but
accept legitimate counter wrap/restart. No permanent monotonic-counter lockout.

Available Solar broadcast fields: battery voltage, charger current, PV power,
yield today, charge state/error and optional load current. PV voltage/current
from the phone screenshot are not broadcast fields and must not be fabricated.
JBD supplies battery current/SOC, pack and cell voltage, temperatures and alarms.
Use positive battery current for charging, negative for discharging; compare to
the app in live acceptance. Charger output current is not net battery current.

## Settings, MQTT and secrets

Register collector-owned settings through ConfigManager: bms.enabled/address,
victron.enabled/address/key, poll/publish intervals and stale timeouts. Stable
storage keys must be explicit and documented before first deployment. Treat
victron.key as a password, validate exactly 32 hex characters, and never include
it in runtime status or raw logs. Test masked round-trips so *** cannot overwrite
the saved key. Missing credentials disable only that adapter with a clear state;
WiFi, UI and OTA remain available. Bluetooth address type is captured separately.
Defaults/captures/real keys remain in ignored local files; fixtures use generated
addresses and keys. Compiled firmware can contain provisioned secrets and stays
local too. Do not attach binaries or logs to GitHub automatically.

Use the existing MQTT module, one connection, and root SolarBatteryTelemetry.
Proposed schema version 1: /status (online/offline, retained LWT), /bms/state,
/victron/state (non-retained JSON), and per-source /availability (retained).
State includes schema, boot_id, sequence, sample_uptime_ms, age_ms and valid;
no wall-clock claim until time sync is verified. Invalid fields serialize null.
Publish every 5 s at most, only fresh snapshots; BMS polling every 5 s and source
stale timeout 30 s initially. Stale/disconnected adapters publish unavailable,
never a synthetic zero. Reconnect publishes availability before current data;
no queued historical telemetry flood. Two source availability topics prevent a
missing BMS from hiding valid Victron data. HA discovery is a later separate
feature; provide an explicit MQTT sensor YAML example with availability/expiry
for this scope. No HA service/device configuration is applied automatically.

MQTT QoS0 publish success means accepted by the local client, not broker receipt.
Pulse onboard LED 30 ms only after a successful source-state publish; no delays,
no blinking on failure/connection attempts/keepalive. Verify pin/polarity locally.
Keep one unambiguous serialized payload limit (1024 bytes); verify MQTT packet
buffer includes topic and protocol overhead and fail visibly on truncation.

## OTA, diagnostics and deployment gates

Add a generic OTA lifecycle observer without replacing reboot/logger callbacks.
It must cover ArduinoOTA and HTTP uploads: Starting, Succeeded, Failed, Aborted,
TimedOut. Serialize upload sessions, identify them with session IDs, reject
overlap, and ignore stale terminal events. A pre-start authentication failure
must never release another session. Emit Starting before first write; callbacks only set atomic flags or
queue commands. BLE worker stops new requests/scan and handles cancellation in
its own context. Queues are invalidated; success stays paused until reboot,
failure resumes only if configuration allows it. No stack deinit/init loop and
no irreversible Bluetooth memory release as a pause mechanism. Test timeout,
client disconnect and settings disable paths. HTTP and ArduinoOTA semantics
must agree; an isActive() polling shortcut is insufficient.

Register the observer once; initialize OTA after IP readiness using the existing
WiFi connection hook. Retain WiFi, ConfigManager servicing and OTA in recovery mode. Record reset
reason, uptime, heap/minimum heap/largest block, queue drops and worker stack
high-water mark without secrets. Capture serial panic output and preserve the
matching ELF locally for decoding. A flash coredump partition alone is not
proof that core dumps are enabled. No deliberate panic/brownout in production.

Local acceptance cannot prove the real BMS protocol or RF coverage. Stage gates:
compile/parser tests -> actual-board USB bootstrap -> 24 h local stability with
bounded synthetic decoder inputs plus real WiFi/MQTT -> local OTA A/B/A -> shed
24 h actual telemetry and two OTA cycles. Initial scaffold cannot be updated
remotely until a functioning OTA build is first installed over USB. Retain USB
recovery access. Dual OTA slots are not proof of automatic rollback after a
bad boot; no rollback guarantee is made without configuring and testing it.

## Primary evidence links

- [NimBLE pin and legacy CI](https://github.com/h2zero/NimBLE-Arduino/blob/2.5.1/.github/workflows/build.yml)
- [Platform mapping](https://github.com/platformio/platform-espressif32/blob/v7.0.1/platform.json)
- [Victron C decoder](https://github.com/SH3D/VictronBLE/blob/318e221170f6fac991e6f68e244b1323c0fa33ce/src/victronble_core.c)
- [Victron C API](https://github.com/SH3D/VictronBLE/blob/318e221170f6fac991e6f68e244b1323c0fa33ce/include/victronble_core.h)
- [JBD BLE reference](https://github.com/syssi/esphome-jbd-bms/blob/31fc9ac0b2d339af6381f7ba29fcbce18cd8fa22/components/jbd_bms_ble/jbd_bms_ble.cpp)
- [Victron unavailable-value reference](https://github.com/Fabian-Schmidt/esphome-victron_ble/blob/3e317e637735e36115bdb415e373200929f89bb3/components/victron_ble/sensor/victron_sensor.h)
- [VictronConnect manual/key retrieval](https://www.victronenergy.com/upload/documents/VictronConnect_Manual/8778-VictronConnect_manual-pdf-en.pdf)
- [PIO dependency semantics](https://docs.platformio.org/en/latest/manifests/library-json/fields/dependencies.html)
- [ESP32 coexistence](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/coexist.html)
- [Panic diagnosis](https://docs.espressif.com/projects/esp-idf/en/v4.4.8/esp32/api-guides/fatal-errors.html)
