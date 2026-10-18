# SolarBatteryTelemetry

Firmware **1.1.0** uses compact CM 4.7.0 runtime metadata and reads a JBD 14-cell BMS and passive Victron SmartSolar Instant Readout on one ESP32 and publishes change-triggered MQTT telemetry with a full 60-second refresh. Live pages combine Bluetooth/MQTT under Connectivity and retain BMS and Victron. Guarded charge/discharge MOS control is implemented but disabled by default; Victron remains read-only.

Historical readout acceptance: the collector in the shed returned fresh BMS and Victron data simultaneously after the latest OTA installation. Ten observations over 95.6 seconds showed both sources available, MQTT online, continuous uptime and no additional reconnects. The owner subsequently confirmed incoming data. This is bounded acceptance, not a long-duration stability claim. The OTA sender missed its final acknowledgment even though the new firmware booted; see [BMS acceptance](docs/BMS.md).

## Configuration

Local credentials stay in ignored `src/secret/secrets.h`; only placeholders are tracked in `secrets.example.h`. The local reserved IPv4 address is 192.168.2.21.

- Bluetooth > Victron: Instant Readout MAC and encryption key.
- Bluetooth > BMS > Readout: JBD peer MAC; empty disables that source.
- BMS control enabled: commissioning gate, default false. Leave disabled until installation qualification.
- MQTT: enabled, broker, credentials, base topic and publication interval.

`VICTRON_READOUT_MAC`, `VICTRON_READOUT_KEY` and `BMS_READOUT_MAC` seed initial settings. Persisted edits remain authoritative. Bluetooth is enabled once for the first configured readout upgrade; later user disables survive reboot. Existing OTA authentication is provisioned only if missing.

See [MQTT contract](docs/MQTT.md), [BMS readout](docs/BMS.md), [Victron readout](docs/Victron.md) and [project evidence](project.info.md). Unsupported or stale values stay unavailable rather than becoming fabricated zero measurements.

## Local ConfigManager dependency

The active ignored `platformio.local.ini` binding selects the adjacent **ConfigManager 4.7.0** development checkout through an absolute local dependency. SBT now requires CM >=4.7.0 for its compact metadata API. This includes the existing shared BLE transport. Reproducing the build requires the compact implementation and coherent header/WebUI assets, not just the same version string; see the [compact migration handoff](docs/plans/2026-10-18-compact-runtime-metadata.md). No registry release or dependency publication is implied.

The tracked consumer dependencies are NimBLE-Arduino 2.5.1 and PubSubClient 2.8. The stack is espressif32 7.0.1 / Arduino-ESP32 2.0.17 / nodemcu-32s with dual 1792 KiB OTA slots. See [local setup](docs/ble-smoke-test.md); its older 4.5.0 checkpoint is historical.

## Focused validation

```powershell
./tools/test/run-smoke-host-tests.ps1
./tools/test/run-runtime-metadata-tests.ps1
python tools/test/run-settings-registration-tests.py
./tools/test/run-victron-host-tests.ps1
./tools/test/run-jbd-host-tests.ps1
./tools/test/run-bms-command-tests.ps1
pio run -e usb
```

The earlier JBD acceptance USB-profile build used 1,342,481 bytes of flash and 80,360 bytes of static RAM. That exact build artifact was used for network OTA; no separate OTA-profile rebuild was needed. That historical acceptance used firmware 0.3.0; the current compact metadata candidate is 1.1.0. Known upstream deprecation/LTO warnings remain.

Secrets, local build bindings, tooling state and detailed device captures are excluded from Git. Earlier smoke/preparation acceptance documents are retained as historical evidence.

The current feature adds minimum NTC temperature, cell/NTC counts, named protection causes, independent source-health heartbeats and guarded MOS commands. See [inventory](docs/telemetry-value-inventory.md), [command contract](docs/MQTT.md#guarded-command-contract) and [HA additions](docs/home-assistant-bms-controls.yaml). Live MOS switching is not qualified by host tests or telemetry readback.

### Fixed 60-second MQTT refresh

At the owner's request on October 16, every 60 seconds the collector republishes source/control availability, capabilities and all currently valid measurements, including unchanged values and differences below the deadband. This timer is independent of the normal publication interval. Between refreshes the 0.1-unit/1-W deadbands still apply. Unsupported fields remain unavailable; no fabricated values are emitted. Admission retry, bounded cursor work and OTA pause remain in force. A restarted Home Assistant recovers unchanged values within approximately one minute while the collector, sources and broker are available. Network failures or OTA pauses can extend that interval. This refresh does not replace the 5-second source-health heartbeat.

## Planned isolated UART wiring

See the [BMS and Victron ADuM1201 wiring plan](docs/isolated-uart-wiring.md) for the proposed SBT ESP pin assignment and harness preparation. The current firmware still uses BLE; UART transport is not implemented by this documentation change.


## Compact runtime metadata

The 50 application descriptors use compact Float, Int, Bool, Text or Checkbox
metadata. Connectivity combines the Bluetooth/MQTT live cards. Redundant MQTT
identity/OTA/validation fields and source availability displays are omitted;
Source reports ok or the concrete failure reason. Internal validation and OTA
behavior are retained. Two observed-state MOS checkboxes replace four action
buttons and keep the guarded request mailbox; an HTTP acknowledgment is not
hardware confirmation. Unknown or stale provider values remain null.

The MQTT channel list now has 24 entries. Cell/NTC counts and the three unused
Victron placeholders are removed from values, capabilities and publication.
Required JBD frame/cell/sensor-count validation remains. Numeric telemetry keeps
its units, with three decimal places for the cell-voltage fields.

CM's BLE configuration/IO split and the explicit SBT TX maximum of 9 bytes retain
all 272-byte RX messages and both queue depths. Separate status buffers match
the existing 256/384-byte producers. The normal USB build uses 80,464 static RAM
bytes versus 85,232 at the checkpoint: 4,768 bytes recovered before startup.
This is linker evidence, separate from measured runtime heap and stability.

The [separate SBT plan and handoff](docs/plans/2026-10-18-compact-runtime-metadata.md)
records implementation/build evidence and the conditional COM7 rollout. BME
acceptance and SBT upload are separate gates. The owner performs subsequent
behavior acceptance; no long-duration stability or HTTP-panic fix is claimed.

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
