# SolarBatteryTelemetry - Project Information

Date: 2026-10-15 (Europe/Berlin)
Status: owner-requested preparation checkpoint; telemetry implementation pending.

## Purpose and approved scope

Create an independent collector before restructuring SolarInverterLimiter.
Read the battery management system (BMS; called "BMC" in the discussion) and
the Victron solar charge controller, then publish telemetry via MQTT for Home
Assistant. Use the owner's ESP32 Configuration Manager for runtime settings.

This checkpoint creates the folder, local Git repository, PlatformIO scaffold,
local Serena and ProjectAtlas setup, local governance, and this decision record.
Stop after the local commit and governance adaptation so the owner can review.
No telemetry adapters, inverter changes, flashing, or device communication are
authorized in this preparation stage.

## Hardware and reported installation

- PV array: 1.8 kWp.
- Newly installed battery: approximately 5 kWh; exact model, chemistry, nominal
  voltage, BMS vendor/protocol, and app name are still unknown.
- Solar charger: Victron 150/35 with Bluetooth; exact variant/firmware pending.
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
- Collector board provisionally uses the limiter's `nodemcu-32s` build target;
  actual ESP32 board, flash capacity, LED pin, and polarity remain to be checked.

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
- Use ConfigManager; prepare against version 4.2.2 used by the limiter.
- No display, external GPIO functions, wired sensors, relays, or RS485 control.
- Built-in LED only: requested future indication of successful MQTT sends,
  nonblocking; do not copy unrelated WiFi/AP/status blinking patterns.
- Copy local WiFi/MQTT credentials from the limiter without printing them.
- Give this device its own application name and MQTT root: SolarBatteryTelemetry.
- The owner will choose and update a free static IP before any upload.
- Keep credentials, governance, Serena, ProjectAtlas, and editor/generated state
  outside Git. Create a local commit only; no GitHub remote or push requested.

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

## Information needed before implementation

1. Battery model, BMS vendor/model, exact app name, firmware, available readings,
   and read protocol. Confirm SOC, current sign, temperatures, and alarm fields.
2. Exact Victron variant, firmware, Instant Readout status, and locally stored
   MAC/encryption key. Do not commit pairing or encryption secrets.
3. Physical wiring and any AC charging path; no assumptions about energy flow.
4. Collector board and available flash/RAM, built-in LED behavior, placement,
   WiFi reception, and Bluetooth reception near both devices.
5. MQTT schema, units, timestamps, freshness/availability, discovery strategy,
   update intervals, and handling of reconnects or missing readings.
6. Owner-selected static IP and explicit approval of the implementation plan.

## Next stage after owner review

Confirm device protocols, design read-only adapters and telemetry contracts,
agree on an implementation plan, implement and build, then request a separate
hardware acceptance step. No control commands to the BMS, charger, or inverter
are part of the collector's current scope.
