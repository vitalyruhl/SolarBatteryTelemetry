# SolarBatteryTelemetry - Project Information

Date: 2026-10-15 (Europe/Berlin)
Working tree: the owner authorized fixing duplicate MQTT/OTA settings keys and connecting the configured SmartSolar Instant Readout MAC/key as editable settings. The original CM 4.5.0 dependency and app version 0.3.0 are retained. New hardware/OTA acceptance remains pending.

Installed checkpoint: 0.3.0 MQTT/channel preparation passed focused host tests, independent review and USB/OTA builds, then was installed on COM11 with all upload hashes verified. Live MQTT delivery, a complete OTA transfer and BLE-device acceptance remain unverified. See docs/acceptance/2026-10-15-mqtt-preparation-0.3.0.md.

## Purpose and approved scope

Create an independent collector before restructuring SolarInverterLimiter.
Read the battery management system (BMS; called "BMC" in the discussion) and
the Victron solar charge controller, then publish telemetry via MQTT for Home
Assistant. Use the owner's ESP32 Configuration Manager for runtime settings.

The initial checkpoint created the folder, PlatformIO scaffold, local tooling and
this decision record; it was reviewed and published privately. On 2026-10-15 the
owner separately approved the ConfigManager BLE/OTA foundation and, after its
software gate passed, this limited collector smoke runtime. That 0.2.0 scope was
WiFi/UI/OTA using the limiter's standard groups/lifecycle plus optional BLE worker
Disabled/Idle diagnostics. It supplies no peers, device adapters, measurements,
MQTT telemetry or LED behavior. The owner subsequently authorized USB flashing
on COM7 and, after the initial short smoke test, local commits of both projects.
The detected ESP32-D0WD-V3 has 4 MB flash. Application/bootstrap upload hashes
passed; the owner reports basic operation without an observed crash. This does
not establish OTA, RF, telemetry or long-duration stability.

The current working tree retains version 0.3.0 as requested. MQTT UI settings use
unique application keys with non-destructive migration from legacy keys. OTA uses
the existing setting with missing-only provisioning. The newly authorized Victron
adapter reads passive encrypted Solar Charger advertisements from the confirmed
SmartSolar model; MAC and key are editable, and supported values feed MQTT.
BMS decoding remains deferred. Unknown, unsupported and stale values stay null.
Both charge/discharge control paths remain disabled and read-only.

The reviewed local ConfigManager 4.5.0 checkout
is an ignored absolute dependency override, not a claimed registry release;
NimBLE-Arduino 2.5.1 is consumer-owned. The owner moved the reserved address to 192.168.2.21; local
network/OTA defaults match the limiter except this unique address/app identity.
Valid persisted UI settings remain authoritative after initial provisioning.
When valid Victron configuration is present, the first readout upgrade enables
Bluetooth once, then honors future user disables. See docs/Victron.md for the
passive receive path and remaining hardware acceptance; older smoke evidence is
retained in docs/ble-smoke-test.md.

## Hardware and reported installation

- PV array: 1.8 kWp.
- Newly installed battery: approximately 5 kWh (owner-reported). Exact battery
  model, chemistry, and nominal voltage remain unknown. Screenshot evidence
  identifies a 14S BMS and the Overkill Solar app; see the evidence below.
- Solar charger: Victron SmartSolar MPPT 150/35, product SCC115035210,
  confirmed by the label photo. Firmware remains pending.
- Owner enabled broadcast-style Bluetooth readout on the charger. Its temporary
  connection PIN is not recorded here. An Instant Readout encryption key, if
  applicable, is distinct from the connection PIN and must remain local.
- The battery is visible over Bluetooth and readable in its BMS app. Actual
  broadcast telemetry versus active polling has not been established.
- Devices are in the shed, outside Bluetooth range of the current workstation.
- Existing inverter: unbranded RS485-controlled unit, already controlled by
  SolarInverterLimiter; owner reports a 900 W maximum in battery mode.
- Photo displayed `1200W-PV: 55-90V TO AC220`, `PV Limit`, and 51.2 V DC.
  These are display observations, not confirmation of battery mode or a battery
  specification. Do not infer a specific inverter model from appearance.
- Collector on COM11: owner identifies NodeMCU ESP-32S v1.1; esptool confirms
  ESP32-D0WD-V3 revision3.1, 4 MB flash, MAC88:57:21:8f:66:4c. The build target
  remains `nodemcu-32s`. LED pin and polarity remain unverified.
- Owner also has a UICPAL ESP32-S3-N16R8 and ESP32-C3 Super Mini V1601 available.
  Neither spare was probed or flashed; no board migration is part of this stage.

## Existing limiter baseline (owner-reported)

| Setting | Value |
| --- | --- |
| Maximum output | 900 W |
| Minimum output | 100 W |
| Input correction | -100 |
| PID enabled | yes |
| Kp / Ki / Kd | 0.45 / 0.05 / 0.02 |
| `set0onnegativeprice` | enabled |
| Observed average grid import | approximately 70 W |

These values were not changed or hardware-verified during preparation.
The limiter already provides RS485, MQTT, grid-power feedback, and price inputs.
Its README records about 98.8% flash use in an older build; this is not a current
measurement. The collector avoids adding BLE load to that existing firmware now.

## Energy-management intent for later work

"Feed in" in the discussion means producing AC power for the home's consumption,
not intentionally exporting to the public grid. Household idle demand is about
600 W. The owner has a dynamic electricity tariff and observes high prices
commonly around 17:00-19:00; use actual prices rather than hard-coding this window.

- Cheap electricity, sun, and available battery capacity: prioritize storing PV
  and deliberately allow cheap grid import for household demand.
- Expensive electricity: use PV and battery to supply actual household demand,
  up to the inverter's 900 W limit and subject to a battery reserve.
- Full battery with ongoing sunshine: release inverter capacity for household
  demand, still limited by demand and the inverter maximum.
- Low PV and cheap electricity: preserve stored energy for expensive periods.
- Distinguish power allowance from forced production: allowing 900 W does not
  mean producing 900 W when the house only needs 600 W.
- A full battery plus PV above usable household demand/inverter capacity cannot
  be fully utilized without another energy sink; curtailment may be necessary.
- Price planning, PV/load forecasts, reserve policy, losses, charge/discharge
  limits, and stale-data behavior belong to a later design. Existing PID tuning
  is a baseline, not an approved change.
- True zero-output behavior despite the 100 W minimum and priority of
  `set0onnegativeprice` remain later limiter investigation items.
- Grid charging capability has not been established. Do not assume an AC charger
  or bidirectional inverter exists.

## Collector decisions

- Separate project and dedicated collector first; keep the limiter unchanged.
- Initial scaffold used ConfigManager 4.2.2; current smoke runtime uses the reviewed local 4.5.0 development checkout after the separate CM gate.
- No display, external GPIO functions, wired sensors, relays, or RS485 control.
- Built-in LED only: requested future indication of successful MQTT sends,
  nonblocking; do not copy unrelated WiFi/AP/status blinking patterns.
- Copy local WiFi/MQTT credentials from the limiter without printing them.
- Give this device its own application name and MQTT root: SolarBatteryTelemetry.
- The owner reserved static IPv4 192.168.2.124 for this collector; local DHCP default is false. Actual unit/port and upload authorization remain pending.
- Keep credentials, governance, Serena, ProjectAtlas, and editor/generated state
  outside Git. The initial checkpoint was local only. On 2026-10-15 the owner
  authorized integrating all reviewed product changes into main and publishing
  `vitalyruhl/SolarBatteryTelemetry` as a private GitHub repository. Credentials,
  screenshots, governance, and tool state remain excluded. Public visibility
  requires a separate owner decision; full device telemetry implementation is still pending.

## Research findings, not implementation commitments

- Victron Instant Readout can be received as encrypted BLE advertisements without
  a persistent connection. MAC address and encryption key are required.
- Whether the battery requires an active BLE connection depends on its BMS.
  Bluetooth visibility alone is not proof of broadcast measurement availability.
- Multiple BLE devices do not automatically require multiple ESP32 boards.
  Resource use, coexistence with WiFi, reconnect behavior, and reliability still
  require validation with the actual devices.
- ESPHome integrations are references/candidates, not the selected firmware base;
  this collector uses PlatformIO/Arduino with ConfigManager as requested.
- Home Assistant-based price planning is a candidate for later architecture,
  not an implemented or finally selected dependency of this collector.

References consulted during brainstorming:

- https://github.com/Fabian-Schmidt/esphome-victron_ble
- https://github.com/patman15/BMS_BLE-HA
- https://esphome.io/components/bluetooth_proxy/
- https://emhass.readthedocs.io/en/latest/main_core_concepts.html
- https://www.victronenergy.com/solar-charge-controllers/smartsolar-150-35

## Information needed before device telemetry or physical acceptance

1. Battery model/chemistry and confirmed BMS vendor/read protocol. The observed
   model string, firmware, app, and readings are recorded below; validate BLE
   compatibility, SOC accuracy, current sign, temperatures, and alarm fields.
2. Victron firmware, verified Instant Readout status, and locally stored
   MAC/encryption key. Do not commit pairing or encryption secrets.
3. Physical wiring and any AC charging path; no assumptions about energy flow.
4. Collector board and available flash/RAM, built-in LED behavior, placement,
   WiFi reception, and Bluetooth reception near both devices.
5. MQTT schema, units, timestamps, freshness/availability, discovery strategy,
   update intervals, and handling of reconnects or missing readings.
6. Static IP 192.168.2.124 and this limited smoke stage are approved; named hardware operations and full device telemetry need their later acceptance gates.

## Screenshot evidence recorded on 2026-10-15

The owner supplied nine images in `.temp/`. These are observations of labels and
app screens, not hardware acceptance tests or verified battery specifications.
Images and pairing secrets are local reference material and must not be staged.

### Device identification

| Item | Observed value | Evidence |
| --- | --- | --- |
| Charger | Victron SmartSolar MPPT 150/35; product SCC115035210 | `20261015_092516.jpg` |
| BMS device name | `15660030-SP15S020-P14S-60A-60AH` | `Screenshot_20261015_093808_Overkill Solar.jpg` |
| BMS firmware | 2.4 | Same BMS information screen |
| BMS manufacturer field | ABCDEF; not reliable vendor identification | Same BMS information screen |
| BMS app | Overkill Solar v2.0.6 | `Screenshot_20261015_093802_Overkill Solar.jpg` |
| Bluetooth module | DT-1902; pins GND, RXD, TXD, VCC | `20261015_093248.jpg` |
| Cell count | 14 series cells | Status and configuration screens |
| Configured total capacity | 131250 mAh (131.25 Ah) | `Screenshot_20261015_094016_Overkill Solar.jpg` |

The displayed name "Powerwall" does not establish the battery manufacturer.
The device-name suffix "60AH" and configured 131.25 Ah differ; neither establishes
measured usable capacity or supersedes the owner's approximate 5 kWh report.
The UART pin labels identify the module interface, not its electrical levels or
exact protocol. No wired connection is planned for this collector.
The Victron label includes pairing credentials; their values are deliberately
omitted from this document.

### Telemetry and configuration snapshots

- Victron at approximately 09:26: PV 128 W, 123.21 V, 1.0 A; battery-side
  voltage 48.07 V, charger current 2.60 A, charging stage Bulk.
- Victron configuration: 48 V system, maximum charge current 35 A, absorption
  58.80 V, float 57.00 V. Equalization voltage is configured as 64.80 V but
  automatic equalization is disabled; temperature compensation is disabled.
  These are recorded settings, not an endorsement for the unidentified battery.
- BMS at 09:38:01: 47.22 V, discharging 9.2 A, power -433.5 W, SOC 1%,
  remaining capacity 1.4 Ah; temperatures 14.8 and 13.1 degrees Celsius.
- Cell 10 is lowest at 3.231 V; cell 1 is highest at 3.392 V; spread 161 mV.
  This is one snapshot under load, not a diagnosis of a defective cell.
- BMS information reports 190 cycles and a manufacture date of 2019-12-04.
  These are device-reported fields, not independently verified history.
- BMS configuration: cell-full voltage 4150 mV, cell-minimal voltage 3000 mV;
  balancing starts at 4000 mV with a 40 mV delta, enabled only while charging.
  These fields must not be mistaken for verified protection trip thresholds.
- Charger current is not net battery current when the inverter draws from the
  same DC bus. The two app snapshots were taken at different times and must not
  be combined into a simultaneous energy balance.
- SOC and capacity accuracy remain unverified. Battery specifications and the
  complete protection settings are needed before assessing charging settings.

### Protocol inference and remaining evidence

The SP15S020 model string strongly suggests the JBD/Xiaoxiang family. The
[esphome-jbd-bms project](https://github.com/syssi/esphome-jbd-bms) provides BLE
and UART implementations and lists an SP15S020 13S variant. This supports using
it as a protocol reference, but does not confirm this exact 14S device works.
No BLE read, connection test, or protocol implementation has been performed.

The current candidate remains Victron advertisement reception plus BMS BLE
polling on one collector ESP32. The actual BMS access method and concurrent
WiFi/BLE reliability still require validation. No second board decision follows
from the screenshots alone. Neither the BMS BLE address nor the Victron Instant
Readout encryption key is available in the supplied screenshots.

The owner saved BMS settings in Overkill Solar but could not locate the file.
The settings screen shows "BMS read", "Open Config", "Save Config", and
"BMS write". No file-selection dialog or export location is visible, so no
filesystem path or successful external export is confirmed. Next evidence:
open "Open Config" and capture the dialog to identify the saved configuration
and any export option. "BMS write" is unrelated to locating a backup and is
outside the read-only collector scope.

The screenshots recorded evidence only at the initial preparation checkpoint.
The later approved 0.2.0 runtime extends software preparation as described above;
device settings and hardware communication remain outside its authorization.

## Next stage after smoke build review

Perform separately authorized named-unit smoke acceptance, then confirm device
protocols and implement the approved read-only adapters/telemetry contracts in a
later scope. The existing broad BLE design/plan is retained as planning context;
this smoke change does not claim those complete-collector tasks are implemented. No control commands to the BMS, charger, or inverter
are part of the collector's current scope.
