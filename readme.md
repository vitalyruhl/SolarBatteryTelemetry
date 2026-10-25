# SolarBatteryTelemetry

Installed firmware **1.4.6** uses published ConfigManager **4.9.0** with **Settings enabled and Live disabled**. It reads a JBD 14-cell BMS and passive Victron SmartSolar Instant Readout on one ESP32 and publishes change-triggered MQTT telemetry with a configurable full refresh (60 seconds by default). Guarded charge/discharge MOS control is implemented but disabled by default; Victron remains read-only. This build change does not establish a fix for the historical panic or HTTP-delivery issue.

## Current integration work

The [single SIL/SBT/HA integration plan](../SolarInverterLimiter/docs/superpowers/plans/2026-10-20-sil-sbt-ha-integration.md) and [integration specification](../SolarInverterLimiter/docs/control-planning/2026-10-20-integration-spec.md) are the current cross-repository entry points. SBT keeps collector responsibilities; SIL owns control policy. Older SBT plans now live under `docs/acceptance/` as historical evidence and do not create a second execution queue.

Firmware 1.4.6 was installed on 2026-10-25 after an isolated three-minute COM8 check. Production identity, version, Settings-only capabilities, inactive OTA and all 32 exposed settings were verified after the authenticated upload; masked secrets permit comparison of their exposed masks only. Fresh production BMS and Victron sequences were subsequently observed. The [rollout record](docs/acceptance/2026-10-22-mos-software-request.md#146-settings-only-registry-rollout) records hashes, bounded checks and open limits. The earlier 1.4.5 actual-state contract remains unchanged. The historical SBT panic/HTTP delivery issue and CM 4.9.0 synchronous MQTT risks remain unresolved; this is not long-run acceptance. The canonical plan remains the cross-repository execution record; historical acceptance documents do not authorize new device actions.

Historical readout acceptance: the collector in the shed returned fresh BMS and Victron data simultaneously after the latest OTA installation. Ten observations over 95.6 seconds showed both sources available, MQTT online, continuous uptime and no additional reconnects. The owner subsequently confirmed incoming data. This is bounded acceptance, not a long-duration stability claim. The OTA sender missed its final acknowledgment even though the new firmware booted; see [BMS acceptance](docs/BMS.md).

## Configuration

Local credentials stay in ignored `src/secret/secrets.h`; only placeholders are tracked in `secrets.example.h`. The local reserved IPv4 address is 192.168.2.21.

- Bluetooth > Victron: Instant Readout MAC and encryption key.
- Bluetooth > BMS > Readout: JBD peer MAC and data timeout (1-9999 seconds, default 60); an empty MAC disables that source.
- BMS control enabled: commissioning gate, default false. Leave disabled until installation qualification.
- MQTT: enabled, broker, credentials, base topic, delta publication interval and full refresh interval (5-3600 seconds, default 60).

`VICTRON_READOUT_MAC`, `VICTRON_READOUT_KEY` and `BMS_READOUT_MAC` seed initial settings. Persisted edits remain authoritative. Bluetooth is enabled once for the first configured readout upgrade; later user disables survive reboot. Existing OTA authentication is provisioned only if missing.

See [MQTT contract](docs/MQTT.md), [BMS readout](docs/BMS.md), [Victron readout](docs/Victron.md) and [project evidence](project.info.md). Unsupported or stale values stay unavailable rather than becoming fabricated zero measurements.

## Published ConfigManager dependency

The consumer pins `vitaly.ruhl/ESP32 Configuration Manager@4.9.0` from the PlatformIO Registry. Local CM source/symlink overrides are rejected. The precompile check validates registry owner/version, coherent mirrors and the generated Settings-only asset proof. The package's official generator creates derived WebUI assets only inside the consumer-local dependency directory; it does not use the unpublished sibling checkout or change PlatformIO's shared cache.

Both production profiles explicitly set `CM_ENABLE_LIVE_UI=0` and `CM_ENABLE_SETTINGS_UI=1`. Application metadata registration treats CM's disabled Live API as an intentional no-op, so disabling Live cannot abort BLE/MQTT initialization. Network credentials and OTA defaults remain in ignored local files. Remove obsolete `custom_cm_path` and CM `symlink://` entries from private overrides; preserve their other settings. Build outputs and dependencies are isolated under `.pio/registry-settings`.

The tracked consumer dependencies are NimBLE-Arduino 2.5.1 and PubSubClient 2.8. The stack is espressif32 7.0.1 / Arduino-ESP32 2.0.17 / nodemcu-32s with dual 1792 KiB OTA slots. See [local setup](docs/ble-smoke-test.md); its older 4.5.0 checkpoint is historical.

## Focused validation

```powershell
python tools/test/run-bms-readout-tests.py
./tools/test/run-smoke-host-tests.ps1
./tools/test/run-runtime-metadata-tests.ps1
python tools/test/run-settings-registration-tests.py
./tools/test/run-victron-host-tests.ps1
./tools/test/run-jbd-host-tests.ps1
./tools/test/run-bms-command-tests.ps1
pio run -e usb
pio run -e ota
```

## Isolated COM8 profile

`pio run -e testesp` builds a separate diagnostic artifact at `.pio/registry-settings/testesp/firmware.bin`; retain its matching ELF. It initializes the BLE worker with empty BMS/Victron peers, uses DHCP, and hard-denies BMS gate/MOS requests independently of settings. All MQTT root transitions, commands, availability, history and System-Info use `SBTTest/COM8/...` with client ID `SBTTest-COM8-collector`. The application emits no HA discovery. MQTT endpoint/authentication can still come from persisted settings; set only this verified test board's endpoint/auth/enable settings from the existing private source when an older SIL broker configuration was inherited.

The profile reports absent peers as unavailable. It qualifies initialization and lifecycle only, not real BLE reception or physical control. The test profile does not modify the production binary. ConfigManager 4.9.0 still has synchronous MQTT transport paths; the unpublished 4.9.1 bounded-transport changes are deliberately excluded. Device testing, OTA and restoration of the previous COM8 image require owner authorization and matching artifact evidence.

The earlier JBD acceptance USB-profile build used 1,342,481 bytes of flash and 80,360 bytes of static RAM. That exact build artifact was used for network OTA; no separate OTA-profile rebuild was needed. That historical acceptance used firmware 0.3.0 and predates the current 1.4.6 rollout. Known upstream deprecation/LTO warnings remain.

Secrets, local build bindings, tooling state and detailed device captures are excluded from Git. Earlier smoke/preparation acceptance documents are retained as historical evidence.

The current feature adds minimum NTC temperature, cell/NTC counts, named protection causes, independent source-health heartbeats and guarded MOS commands. See [inventory](docs/telemetry-value-inventory.md), [command contract](docs/MQTT.md#guarded-command-contract) and [HA additions](docs/home-assistant-bms-controls.yaml). Live MOS switching is not qualified by host tests or telemetry readback.

### Configurable full MQTT refresh

The collector republishes source/control availability, capabilities and all currently valid measurements, including unchanged values and differences below the deadband. `SBTMQTTRefresh` sets this full-refresh period to 5-3600 seconds, default 60 (the original fixed period requested on October 16). This timer is independent of the normal publication interval. Between refreshes the 0.1-unit/1-W deadbands still apply. Unsupported fields remain unavailable; no fabricated values are emitted. Admission retry, bounded cursor work and OTA pause remain in force. A restarted Home Assistant recovers unchanged values within approximately the configured full-refresh period while the collector, sources and broker are available. Network failures or OTA pauses can extend that interval. This refresh does not replace the 5-second source-health heartbeat.

## Planned isolated UART wiring

See the [BMS and Victron ADuM1201 wiring plan](docs/isolated-uart-wiring.md) for the proposed SBT ESP pin assignment and harness preparation. The current firmware still uses BLE; UART transport is not implemented by this documentation change.


## Compact runtime metadata

The 52 application descriptors use compact Float, Int, Bool, Text or Checkbox
metadata. Connectivity combines the Bluetooth/MQTT live cards. Redundant MQTT
identity/OTA/validation fields and source availability displays are omitted;
Source reports ok or the concrete failure reason. Internal validation and OTA
behavior are retained. Two observed-state MOS checkboxes replace four action
buttons and keep the guarded request mailbox; an HTTP acknowledgment is not
hardware confirmation. Unknown or stale provider values remain null.

The MQTT channel list now has 26 entries. Cell/NTC counts and the three unused
Victron placeholders are removed from values, capabilities and publication.
Required JBD frame/cell/sensor-count validation remains. Numeric telemetry keeps
its units, with three decimal places for the cell-voltage fields.

CM's BLE configuration/IO split and the explicit SBT TX maximum of 9 bytes retain
all 272-byte RX messages and both queue depths. Separate status buffers match
the existing 256/384-byte producers. The normal USB build uses 80,464 static RAM
bytes versus 85,232 at the checkpoint: 4,768 bytes recovered before startup.
This is linker evidence, separate from measured runtime heap and stability.

The [historical SBT migration evidence](docs/acceptance/2026-10-18-compact-runtime-metadata-evidence.md)
records the earlier implementation/build evidence and conditional COM7 rollout.
Its original gates are historical; current test-device authorization is recorded
in the canonical integration plan above. No long-duration stability or HTTP-panic
fix is claimed by the earlier evidence.

## Local heap diagnostics

The 1.0.1 diagnostic candidate aligns the one-second serial Health sample with normal allocator capabilities: `heap`, `min` and `largest` use `MALLOC_CAP_DEFAULT`. `internal` is the broader internal-memory total and must not be interpreted as available malloc memory. These fields describe different capabilities, not an unexplained sudden loss between loggers.

With `CM_ENABLE_VERBOSE_LOGGING=1` or `SBT_ENABLE_HEAP_DIAGNOSTICS=1`, the application installs its process-wide allocation-failure hook after serial setup. The first 16 events per boot, every 64th subsequent large failure, and all small failures up to 64 bytes are printed with `[HEAP-FAIL]`, including requested size, capabilities, allocator API, eligible free/largest memory and default free/largest memory. It uses ROM output and fixed storage, bypassing the regular logger. No existing hook was present when this integration was added; consumers must coordinate ownership if another subsystem later registers one. This diagnostic path is not qualified as generally ISR/cache-off safe. It and the periodic Health helper are omitted when both flags are disabled.

The CM 4.6.1 diagnostic work added `[HTTP-HEAP]` request-boundary/integrity samples and removes avoidable app-info/theme-response buffers. Host tests and builds are software evidence only; the intermittent panic remains under investigation. Capture the first `[HEAP-FAIL]` event and its surrounding HTTP samples, and retain the matching ELF before rebuilding. No BMS command or device-control operation is needed for this investigation.

Focused diagnostic host check: `tools/test/run-heap-failure-host-tests.ps1` (mocked allocator/ROM APIs, verbose 0 and 1). Build with `pio run -e usb`; uploading and serial monitoring remain owner-operated.


Optional test instrumentation uses SBT_ENABLE_HEAP_DIAGNOSTICS=1 and
-Wl,--wrap=sys_thread_sem_get. The normal build compiles it out. The probe calls
the real getter once and reports NULL with task/caller and recent allocation
failures using fixed storage; it does not preinitialize or repair the semaphore.
The diagnostic profile remains local. See the handoff for exact firmware hashes
and test-device results.

## 1.2.0 changes

Health generation now copies each source snapshot before reading its clock, preventing
new BLE samples from appearing almost one uint32 clock period old. JBD NTC channels
`bms/temperature_1_c` and `bms/temperature_2_c` preserve original sensor order
(NTC 1 Top / NTC 2 Bottom); existing minimum/maximum topics remain. Missing sensors
stay unavailable. No extra BLE query or control operation was added. This timestamp
repair does not establish that historical overnight resets are fixed.

## 1.3.0 changes

BMS telemetry now retains the last complete valid cycle through transient BLE/query
failures until `BmsDataTimeout` expires (1-9999 seconds, default 60). Settings >
Bluetooth > BMS > Readout exposes the timeout. A new sample alone advances its
original timestamp and sequence; cached data reports `cached` source health.
Reconfiguration and OTA/pause clear the cache immediately. Guarded MOS operations
still require a healthy connected session and a sample younger than 30 seconds.

Settings > MQTT adds `SBTMQTTRefresh`, Full refresh interval (5-3600 seconds,
default 60). It resends all currently valid telemetry independently of the delta
publish interval. Changing either setting does not require a reboot. Invalid UI
values are rejected; out-of-range stored values are bounded before use. MQTT
retain flags, Victron freshness and all command deadlines are unchanged.

Host/build validation and deployment acceptance are separate: this source change
does not establish a repair of the underlying BLE interruptions or HTTP failures.

## 1.4.0 changes

Victron error code and daily yield are retained on their existing scalar MQTT topics. Separate retained `victron/last_valid/error_code` and `victron/last_valid/yield_today_wh` JSON topics preserve each last valid reading with its actual reception time for display. Live source availability remains strict; display history is never a control input. See [MQTT display history](docs/MQTT.md#140-victron-display-history) for clock and date handling.

HA Allow controls now follow actual MOS readback and use fresh, explicit commands after a restart. The common ESP/HA control gate is persistent and blocks all external MOS writes, including SIL emergency requests, when off. See [the current control contract](docs/BMS.md#guarded-mos-permissions), [MQTT examples](docs/home-assistant-bms-controls.yaml) and [gate template](docs/home-assistant-bms-gate.yaml).
