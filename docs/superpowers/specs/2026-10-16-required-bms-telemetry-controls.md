# Required BMS telemetry and MOS control design

Status: implemented and software-validated on 2026-10-16. Real telemetry and charge MOS disable/restoration have bounded acceptance; discharge MOS commissioning remains pending. See docs/acceptance/2026-10-16-required-bms-telemetry-controls.md.
Branch: `feature/bms-required-telemetry-controls`; baseline `861068793d3abf064e8d2c1df0888aa1a686d5a2`.

## Requested result

Extend the existing collector, MQTT contract and Home Assistant cards with the missing Required telemetry and only the two Required BMS write operations. Keep all existing values and topics. Do not add Victron configuration/control writes. The separate SIL task only upgrades ConfigManager and validates USB-before-OTA deployment; it does not implement economic dispatch or BMS orchestration.

## Telemetry scope

| Area | Existing | Addition or verification |
| --- | --- | --- |
| BMS measurements | Voltage, signed current, power, SOC, maximum temperature, minimum/maximum cell voltage, alarm bits | Expose minimum of validated NTC measurements, cell count and NTC count |
| BMS protection | Raw alarm bitfield | Add decoded protection descriptions; retain raw and unknown bits |
| Owner-selected optional values | Remaining capacity, charging, discharging and cell spread | Preserve fields/topics and verify presence on ESP and HA; do not create duplicate sensors |
| BMS control feedback | Observed charge/discharge MOS flags | Add command readiness, pending/result/error state and fresh actual readback |
| Source health | Retained availability and combined capability summary | Publish per-source reason and fresh-sample health independently of measurement deadbands |
| Victron Required values | PV power, charger output current, charge state and error code | Preserve and verify ESP/MQTT/HA coverage and source health |
| PV voltage/current | Unsupported BLE placeholders | Owner confirmed no VE.Direct connection; hardware-dependent follow-up, never synthetic zero or inferred PV voltage |

Minimum NTC temperature is the lowest received sensor reading. It must not be labelled as proven cell temperature until placement is known. No chemistry-dependent cutoff, SOC calibration or battery-capacity correction is inferred. Existing thresholds of 0.1 per real unit, except 1 W, remain as explicitly selected by the owner. Alarm, availability and command-result changes bypass deadbands.

## Proposed command behavior

- Only `bms/charge_enabled/set` and `bms/discharge_enabled/set` may cause a device write. No generic register, reset, balancing, calibration or protection-setting write endpoint.
- Commands represent absolute desired permission, not toggle operations. A fresh basic-info response is required before composing register `0xE1`; preserve the other observed MOS state. Serialize reads and writes through the existing CM worker with one outstanding query.
- Do not report success on MQTT/GATT admission. After the write response, read fresh basic/cell information and report whether the observed MOS state matches. Expose both MOS flags and net current. A state readback confirms the permission only, not zero electrical flow or isolation.
- Reject control when disconnected, stale, OTA-paused or another request is pending. Expire pending work rather than saving commands for later. Boot, reconnect and source loss discard command intent and never automatically re-enable a MOS.
- Add an explicit BMS-control enable setting, initially disabled, so installation qualification can precede operation. This is separate from telemetry/Bluetooth enablement.
- MQTT requests use a bounded envelope containing desired state, request ID, the current connection/session token, and an expiry time. Old-session, duplicate, expired and malformed requests are rejected. The current PubSubClient callback does not expose the retained flag; do not claim retained-bit filtering. Session binding and expiry prevent old broker replay from becoming a new command. Unsynchronized time makes remote command handling unavailable.
- HA controls use actual state feedback rather than optimistic switches. The ESP runtime uses the same command mailbox and validation path. No MQTT/web callback directly performs BLE work.
- No automatic economic control, fault-reset policy or emergency shutdown sequence is introduced. Hardware BMS protections remain authoritative. Live switching tests need a defined operating condition that does not abruptly remove a loaded battery from charger/inverter.

## Hardware boundary

The owner confirmed Bluetooth-only Victron connectivity. Victron documents the conventional MPPT VE.Direct port as 5 V UART and recommends galvanic isolation. A future ESP link needs compatible 5 V/3.3 V interfacing; it is not a direct RS232-to-GPIO connection. VE.Direct power is limited to 10 mA average and is not an ESP supply. No pins, cable, UART peripheral or wiring are assigned by this task without the hardware decision.

## Verification and rollout

1. Focused decoder tests: NTC extremes and missing sensors, counts, named/unknown alarm bits; preserve existing JBD validation.
2. Command-policy tests: exact allowlist and checksum, preserved opposite MOS, error responses, fresh-read requirement, timeout, disconnect/OTA cancellation, replay/expiry rejection, actual readback and failed admission.
3. MQTT/UI tests: existing topics and deadbands preserved; status/protection events unfiltered; bounded source-health messages; HA displays and controls use real availability.
4. Build the firmware and review the final diff. Keep the existing owner-requested debugging version freeze unless changed by the owner.
5. OTA to the authorized SBT device only after software validation. Verify boot, continuous uptime, fresh BMS/Victron samples and independent MQTT receipt. Confirm new HA fields and cell spread. Verify rejected/control-disabled requests without changing hardware state.
6. Qualify live MOS switching separately against the installation and record actual flags/current, including the final restored state. Never claim a host test proves hardware control.

## Evidence

- `src/bms/JbdProtocol.h`, `src/bms/BmsReadout.h`, `src/telemetry/TelemetryChannels.h`, `src/telemetry/TelemetryPublisher.h`.
- [JBD implementation reference](https://github.com/syssi/esphome-jbd-bms/blob/main/components/jbd_bms_ble/jbd_bms_ble.cpp): MOS register `0xE1`, two-byte value, inverted disable bits 0/1; actual device acceptance remains necessary.
- [Victron VE.Direct FAQ](https://www.victronenergy.com/live/vedirect_protocol:faq): voltage levels, isolation and supply limits.
- Live pre-change ESP sample: BMS available, remaining capacity 7.53 Ah, charging true, discharging false, spread 0.038 V. HA subsequently showed the four requested optional fields, including spread 0.04 V. Values are dated observations, not synchronized calibration evidence.