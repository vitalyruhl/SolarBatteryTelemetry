# JBD BMS telemetry and guarded MOS control

The collector uses the shared ConfigManager BLE worker to connect to the configured JBD/Xiaoxiang peer. The documented target is an SP15S020 14-series-cell BMS used with Overkill Solar. Actual device compatibility requires hardware acceptance.

## Configuration

Settings > Bluetooth > BMS > Readout exposes `BmsReadMac`. The optional local `BMS_READOUT_MAC` secret macro supplies only the initial default; persisted settings win. Empty disables this source without stopping Victron. Bluetooth must also be enabled. Close other BMS client apps when testing: the module may allow only one connection.

## Protocol boundary

Service FF00, notifications FF01, requests FF02. Ordinary polling uses seven-byte requests for basic information (03) and cell information (04). The guarded control path additionally supports only the two charge/discharge MOS permissions through E1. There is no authentication fallback, factory mode, calibration, protection-setting or generic register write. A subscription descriptor write and telemetry read-request writes are necessary BLE transport operations.

A cycle queries basic information then cells, one outstanding query at a time, with a two-second response deadline and approximately five seconds between completed cycles. A bounded 262-byte parser validates status, checksum, register and end marker; only a complete valid 14-cell cycle publishes a new sample. Disconnect, reconfiguration and OTA pause invalidate availability. Samples expire after 30 seconds without a valid cycle. Invalid or unavailable values remain null.

Reported current is signed: positive means charging and negative discharging. Power is battery voltage times net battery current. Charging/discharging activity follows current direction; MOS permission is reported separately. Minimum and maximum temperature use all validated available NTC readings; cell minimum, maximum and spread use all 14 cell readings. Cell and NTC counts are exposed. Raw alarm bits remain available alongside named active protection flags; unknown high bits remain visible. No protection thresholds are inferred.

## Guarded MOS permissions

`BMS control enabled` (`BmsControl`) defaults to false. The firmware default remains disabled. On October 16 the owner accepted manual charge/discharge testing and requested that the installed gate remain enabled for manual HA operation while SIL automation is unfinished. See the [acceptance record](acceptance/2026-10-16-required-bms-telemetry-controls.md#discharge-test-and-owner-acceptance-later-on-october-16) for the observed readback fluctuation and the separate owner confirmation. This does not authorize autonomous SIL switching. Desired MOS states are never persisted. Both the ESP actions and MQTT use a bounded mailbox; only the application loop calls the shared BLE worker.

A command reads fresh basic status, preserves the opposite MOS permission, sends the absolute E1 disable-bit mask, requires a successful acknowledgement, then reads a complete new basic/cell pair. Only matching observed flags produce `verified`. The result also reports observed net current; MOS readback is not proof of electrical isolation or current interruption. A write is never retried automatically. Disconnect, reconfiguration, OTA pause, disablement and expiry cancel intent. An already transmitted write cannot be undone by cancellation; subsequent telemetry remains the source of truth.

The ESP exposes separate Enable/Disable charge MOS and Enable/Disable discharge MOS actions. `control_admission` shows rejected/accepted mailbox admission separately from transaction phase and result. Disabled, stale, expired, cancelled and busy requests are rejected at arrival; taking a request keeps its reservation until completion. Existing CM runtime-action endpoints are accessible within the local network; the control gate is a commissioning interlock, not authentication. MQTT broker access control and network boundaries remain deployment responsibilities. See [MQTT command contract](MQTT.md#guarded-command-contract).

## Reference and validation

Protocol fields and BLE UUIDs were checked against the [esphome-jbd-bms implementation](https://github.com/syssi/esphome-jbd-bms/tree/main/components/jbd_bms_ble). This adapter is independently implemented and imports no external authentication or control implementation. Host fixtures are synthetic, not captured device data.

Run `tools/test/run-bms-command-tests.ps1` and `tools/test/run-jbd-host-tests.ps1` and the existing Victron/MQTT regression tests. Device acceptance is recorded separately; passing host tests does not establish radio compatibility.

## Hardware acceptance, 2026-10-15

The firmware was compiled using the USB environment and transferred over network OTA to the owner-authorized collector. Both BMS and Victron returned available simultaneously. Observed BMS values included 46.83 V, 0 A, 0% SOC, 131.25 Ah configured full capacity, maximum temperature 17.7 C and cell range 3.074-3.393 V. These are device-reported values, not a calibration or battery-health assessment. The MQTT connection remained online; independent broker receipt was not tested.

Page index, selected BMS layout, selected metadata and settings returned complete valid JSON; the BMS MAC setting was present. Device observations and firmware hashes are saved locally under `.local/`.

The OTA sender authenticated and sent the image, but timed out waiting for the final response. Its installed espota.py then raised a NameError in its exception handler. The device rebooted into the new JBD-enabled firmware and returned measurements; no upload retry was performed. No global PlatformIO tool was modified.

The final observation covered 10 snapshots over 95.6 seconds. Both sources remained available and MQTT stayed online in every snapshot; uptime increased continuously, queue drops stayed zero and the reconnect counter did not increase. A historical startup timeout (-1005) and two connection attempts remained recorded; this is not a zero-error startup claim.
