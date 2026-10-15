# JBD BMS readout

The collector uses the shared ConfigManager BLE worker to connect to the configured JBD/Xiaoxiang peer. The documented target is an SP15S020 14-series-cell BMS used with Overkill Solar. Actual device compatibility requires hardware acceptance.

## Configuration

Settings > Bluetooth > BMS > Readout exposes `BmsReadMac`. The optional local `BMS_READOUT_MAC` secret macro supplies only the initial default; persisted settings win. Empty disables this source without stopping Victron. Bluetooth must also be enabled. Close other BMS client apps when testing: the module may allow only one connection.

## Protocol boundary

Service FF00, notifications FF01, requests FF02. Only the seven-byte read requests for basic information (03) and cell information (04) are emitted. No authentication fallback, factory mode, charger/MOSFET commands or BMS configuration writes exist. A subscription descriptor write and telemetry read-request writes are necessary BLE transport operations.

A cycle queries basic information then cells, one outstanding query at a time, with a two-second response deadline and approximately five seconds between completed cycles. A bounded 262-byte parser validates status, checksum, register and end marker; only a complete valid 14-cell cycle publishes a new sample. Disconnect, reconfiguration and OTA pause invalidate availability. Samples expire after 30 seconds without a valid cycle. Invalid or unavailable values remain null.

Reported current is signed: positive means charging and negative discharging. Power is battery voltage times net battery current. Charging/discharging activity follows current direction; MOS permission is reported separately. Temperature is the maximum of the validated available NTC readings; cell minimum, maximum and spread use all 14 cell readings. Alarm bits are raw; no protection-setting interpretation is applied.

## Reference and validation

Protocol fields and BLE UUIDs were checked against the [esphome-jbd-bms implementation](https://github.com/syssi/esphome-jbd-bms/tree/main/components/jbd_bms_ble). This adapter is independently implemented and imports no authentication/control code. Host fixtures are synthetic, not captured device data.

Run `tools/test/run-jbd-host-tests.ps1` and the existing Victron/MQTT regression tests. Device acceptance is recorded separately; passing host tests does not establish radio compatibility.

## Hardware acceptance, 2026-10-15

The firmware was compiled using the USB environment and transferred over network OTA to the owner-authorized collector. Both BMS and Victron returned available simultaneously. Observed BMS values included 46.83 V, 0 A, 0% SOC, 131.25 Ah configured full capacity, maximum temperature 17.7 C and cell range 3.074-3.393 V. These are device-reported values, not a calibration or battery-health assessment. The MQTT connection remained online; independent broker receipt was not tested.

Page index, selected BMS layout, selected metadata and settings returned complete valid JSON; the BMS MAC setting was present. Device observations and firmware hashes are saved locally under `.local/`.

The OTA sender authenticated and sent the image, but timed out waiting for the final response. Its installed espota.py then raised a NameError in its exception handler. The device rebooted into the new JBD-enabled firmware and returned measurements; no upload retry was performed. No global PlatformIO tool was modified.

The final observation covered 10 snapshots over 95.6 seconds. Both sources remained available and MQTT stayed online in every snapshot; uptime increased continuously, queue drops stayed zero and the reconnect counter did not increase. A historical startup timeout (-1005) and two connection attempts remained recorded; this is not a zero-error startup claim.
