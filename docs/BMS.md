# JBD BMS telemetry and guarded MOS control

The collector uses the shared ConfigManager BLE worker to connect to the configured JBD/Xiaoxiang peer. The documented target is an SP15S020 14-series-cell BMS used with Overkill Solar. Actual device compatibility requires hardware acceptance.

## Configuration

Settings > Bluetooth > BMS > Readout exposes `BmsReadMac` and `BmsDataTimeout` (BMS data timeout, 1-9999 seconds, default 60). The timeout controls telemetry validity from the last complete valid cycle; it is separate from the two-second query deadline and the 30-second MOS safety freshness limit. The optional local `BMS_READOUT_MAC` secret macro supplies only the initial default; persisted settings win. Empty disables this source without stopping Victron. Bluetooth must also be enabled. Close other BMS client apps when testing: the module may allow only one connection.

## Protocol boundary

Service FF00, notifications FF01, requests FF02. Ordinary polling uses seven-byte requests for basic information (03) and cell information (04). The guarded control path additionally supports only the two charge/discharge MOS permissions through E1. There is no authentication fallback, factory mode, calibration, protection-setting or generic register write. A subscription descriptor write and telemetry read-request writes are necessary BLE transport operations.

A cycle queries basic information then cells, one outstanding query at a time, with a two-second response deadline and approximately five seconds between completed cycles. A bounded 262-byte parser validates status, checksum, register and end marker; only a complete valid 14-cell cycle publishes a new sample. A transient disconnect, query error or response timeout discards partial frames and cancels command intent, but preserves the last complete valid cycle until the configured data timeout expires. Reconnecting alone does not refresh it: its timestamp and sequence stay unchanged, and source health reports `cached`. Reconfiguration or an empty BMS MAC and OTA/pause clear telemetry immediately. Global Bluetooth disable is treated as a transport loss, with the same bounded telemetry validity and immediate command interlock. Expiry makes the source unavailable; invalid or unavailable values remain null. Reducing or increasing the setting takes effect on the next application loop, always against the original sample time.

Reported current is signed: positive means charging and negative discharging. Power is battery voltage times net battery current. Charging/discharging activity follows current direction; Actual conducting MOS state is reported separately. Minimum and maximum temperature use all validated available NTC readings; cell minimum, maximum and spread use all 14 cell readings. Cell and NTC counts are exposed. Raw alarm bits remain available alongside named active protection flags; unknown high bits remain visible. No protection thresholds are inferred.

## Guarded MOS permissions

`BMS control enabled` (`BmsControl`, default false) is the single persistent control gate. HA changes that same setting through `bms/control_enabled/set`; the ESP settings page and HA mirror the same value. MQTT gate requests are session-bound, non-retained and debounced for 150 ms. Persistence failure is reported, never acknowledged as saved. The last settings value wins on failure; a new manual click is required.

With the gate off, HA, ESP actions and SIL emergency commands cannot write MOS permissions. OFF also invalidates already buffered BLE commands under the shared worker's final-write fence. A physical write already started cannot be recalled; cancellation waits for that write to finish. The gate does not disable autonomous BMS hardware protection. Gate changes themselves never write a MOS register.

On boot, the collector reads and accepts actual conducting FET flags. HA Allow switches and ESP controls display these flags, with no historical requested-state baseline or joint initialization. A new explicit single-target command first reads fresh BasicInfo, preserves the actual opposite-direction bit and sends the E1 disable mask exactly once. Legacy envelope baseline fields are accepted for compatibility but have no control authority. Because actual FET feedback does not expose the persistent software disable bits, this approach can replace an invisible opposite-direction software lock. No automatic request runs at startup, reconnect, gate enable or periodic refresh.

A valid empty E1 acknowledgement is reported as `acknowledged: ok`: command acceptance, not proof of electrical isolation or a lasting FET change. Actual readback remains authoritative even if it differs. During charging, the observed BMS has reported both FETs on after acknowledging charge-on/discharge-off; see the [controlled observation](acceptance/2026-10-22-mos-software-request.md). Legacy `requested_charge_enabled`/`requested_discharge_enabled` runtime keys now alias actual readback. `permissions_known` means actual flags are available; `mos_readback` distinguishes `actual`, `cached` and `unavailable`.

Commands require a connected healthy source with a complete sample younger than 30 seconds. Reconnection requires a new complete cycle; cached telemetry alone never permits control. Busy/debounced, stale, disabled, expired and cancelled requests are rejected without automatic retry. A 500 ms settled-command cooldown and separate state/command topics prevent publication echoes. A queued manual request may replace an unsent emergency request.

SIL may send only emergency BothOff on `bms/emergency_off/set`, while the common gate is enabled and a fresh BMS hardware fault bit 0-11 is present. SBT rechecks the fault in the immediate pre-read. Software-lock bit 12 alone is not an emergency. SIL makes one attempt per continuously observed fault episode, rearms only on a fresh clear and never automatically enables a MOS. Economic planning and SOC reserve control use inverter setpoints, never MOS commands.

Actual MOS scalar status is retained; result messages are non-retained and current-session only. Changes publish in a bounded cycle without the former delta-interval wait; unchanged data is refreshed every `SBTMQTTRefresh` seconds (default 60). Source availability remains separate, so retained values are not evidence of fresh readiness. See [HA MQTT examples](home-assistant-bms-controls.yaml) and [gate template](home-assistant-bms-gate.yaml).

## Reference and validation

Protocol fields and BLE UUIDs were checked against the [esphome-jbd-bms implementation](https://github.com/syssi/esphome-jbd-bms/tree/main/components/jbd_bms_ble). This adapter is independently implemented and imports no external authentication or control implementation. Host fixtures are synthetic, not captured device data.

Run `tools/test/run-bms-readout-tests.py`, `tools/test/run-bms-command-tests.ps1` and `tools/test/run-jbd-host-tests.ps1` and the existing Victron/MQTT regression tests. Device acceptance is recorded separately; passing host tests does not establish radio compatibility.

## Hardware acceptance, 2026-10-15

The firmware was compiled using the USB environment and transferred over network OTA to the owner-authorized collector. Both BMS and Victron returned available simultaneously. Observed BMS values included 46.83 V, 0 A, 0% SOC, 131.25 Ah configured full capacity, maximum temperature 17.7 C and cell range 3.074-3.393 V. These are device-reported values, not a calibration or battery-health assessment. The MQTT connection remained online; independent broker receipt was not tested.

Page index, selected BMS layout, selected metadata and settings returned complete valid JSON; the BMS MAC setting was present. Device observations and firmware hashes are saved locally under `.local/`.

The OTA sender authenticated and sent the image, but timed out waiting for the final response. Its installed espota.py then raised a NameError in its exception handler. The device rebooted into the new JBD-enabled firmware and returned measurements; no upload retry was performed. No global PlatformIO tool was modified.

The final observation covered 10 snapshots over 95.6 seconds. Both sources remained available and MQTT stayed online in every snapshot; uptime increased continuously, queue drops stayed zero and the reconnect counter did not increase. A historical startup timeout (-1005) and two connection attempts remained recorded; this is not a zero-error startup claim.


## Fourteen-cell telemetry (1.5.0)

The 14 voltage channels are series-group measurements of a 14S40P LG M29 Li-ion
pack (560 physical cells), not 560 individually measured cells. The pack label
rates 114 Ah; the separately reported BMS full-capacity setting remains
telemetry/configuration evidence. See [battery label evidence](BATTERY.md).

Every completed checked 03/04 pair retains 14 raw uint16_t millivolt readings
alongside scalar minimum/maximum/spread. Partial, wrong-length, out-of-range or
interrupted frames cannot replace any part of the completed array. Expiry clears
its validity, and MQTT never represents invalid cells as zero or live. Exact mV
deltas use independent fixed storage and shared elapsed-time pacing. See
[MQTT](MQTT.md#coherent-cell-array-150) and [HA entities](home-assistant-bms-cells.yaml).
No MOS command, automatic recovery or protection threshold changed.
