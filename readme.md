# SolarBatteryTelemetry

## ESP32-C3 replacement collector

The owner-authorized `c3` environment runs normal SBT behavior on the confirmed
ESP32-C3 Super Mini V1601. It replaces the disconnected original collector at
192.168.2.21 with the existing `SolarBatteryTelemetry` MQTT root, client identity,
real BMS/Victron settings and previous control policy. BLE is saved enabled so
readout restarts after a reboot. Build/upload the USB replacement with
`pio run -e c3 -t upload --upload-port COM10`.

The separate `testesp_c3` environment below always retains its isolated
namespace and command denial. Switching to the replacement role requires the
normal `c3` firmware, not only a UI setting change. Actual BMS/Victron reception
must still be validated at the installation site.

## ESP32-C3 recovery test candidate 1.5.2

The `testesp_c3` environment targets the owner-confirmed ESP32-C3 with 4 MB
flash on COM10. It uses DHCP and the real configured BMS/Victron readout, but
publishes exclusively under `SBTTest/C3`; MOS and control-gate requests remain
unconditionally blocked. It does not replace production MQTT telemetry.
Build with `pio run -e testesp_c3`; upload only the confirmed test board.
The classic ESP32 controller override is intentionally absent on C3.

The selected `esp32-c3-supermini-v1601` board profile supplies GPIO8 and
active-low polarity to CM's default `PulseOutput()` constructor. Its GPIO
initialization happens at runtime, so no RGB/RMT operation occurs during global
construction. The classic ESP32 application's explicit LED configuration stays
unchanged.

The consumer build derives two checked integer-type fixes and only the two
CM 4.11.0 LED helper files for C3 from the unchanged frozen CM package. This is
an explicit helper backport, not a complete CM upgrade. The SDK panic summary uses the correct
architecture fields; C3 raw stack bytes are not reported as decoded backtraces.
SBT redirects the frozen CM NTP entrypoint to Espressif's thread-safe SNTP APIs,
serializing whole reconfigurations. This removes a demonstrated unsafe API
boundary; it is not proof of the exact historical crash trigger.
See the [investigation and device-test record](docs/acceptance/2026-11-06-c3-ble-recovery.md).

## Bluetooth controller test candidate 1.5.1

The separate `fix/esp32-bt-controller` candidate links Espressif's IDF4.4.7-specific
controller0b40e99 from a hash-verified, worktree-local archive. Run
`python tools/bt_controller.py --prepare` before building. The shared SDK remains
unchanged, and post-link checks record the actual controller provenance.
See the [candidate and rollback record](docs/acceptance/2026-10-31-bt-controller-candidate.md).
The owner requested production testing immediately after the short bench gate;
production1.5.1 is installed with a confirmed BLE-off recovery boot and RAM-only
BLE activation. The initial real-device observation passed; long-term stability
is unproven. Do not Save all during the recovery experiment.

## Cell telemetry and paced diagnostics 1.5.0

Production 1.5.0 was confirmed after the owner-authorized five-minute test on
2026-10-31. See the [hardware gate](docs/acceptance/2026-10-31-cell-telemetry-hardware-gate.md)
for actual readings and remaining stability limits.

A later production panic matched an intentional Bluetooth controller assertion
at `llm_util.c:215`. The exact rejected packet and trigger remain unproven; see
the [matched ELF analysis](docs/acceptance/2026-10-31-controller-panic-1.5.0.md).
No controller fix is claimed by 1.5.0.

The compatible 1.5.0 feature adds all 14 checked BMS cell voltages on
`bms/cells`, with shared source sequence, acquisition age and optional acquisition
UTC. Exact millivolt changes use the existing minimum publication interval.
Missing or expired arrays become unavailable. Ordinary application MQTT work
shares one elapsed 100 ms grant across six fair owners. Urgent control results
and lifecycle cleanup keep their own path. Live UI remains disabled; no
automatic MOS operation was added.

`Diagnostics/Panic` retains the bounded historical stored-dump report.
`Diagnostics/Runtime` samples loop-stack minimum free bytes and default-heap
free/largest/minimum bytes every 30 seconds without adding an allocator hook.
Build-only checks do not establish device or multi-day stability. See
[MQTT](docs/MQTT.md), [BMS](docs/BMS.md) and the additive
[HA configuration](docs/home-assistant-bms-cells.yaml).

The 14 BMS voltage channels measure the pack's 14 series groups, each containing
40 parallel LG M29 Li-ion cells; they do not measure all 560 physical cells.
See [battery label and capacity evidence](docs/BATTERY.md). The label's 114 Ah
rating and the BMS-reported full-capacity value remain distinct; this feature
does not silently change capacity or planning settings.

## BLE-owned HTTP reboot baseline 1.4.12

This patch selects the unpublished immutable CM 4.10.5 package at
`file://.local/cm-ble-reboot-4.10.5-final-package`. Before starting the web server,
the application registers a nonblocking reboot-preparation callback: request
terminal BLE worker shutdown and return only its completed acknowledgment.
No SDK shutdown or restart runs in this application callback. CM owns timeout,
failure handling, native controller checks, and the eventual main-loop restart.
The WiFi coexistence lease remains conservative until reboot. The task watchdog
remains enabled; no BLE reinitialization is attempted after terminal shutdown.

Build outputs and dependency copies use `.pio/cell-telemetry-1.5.0`. The `testesp`
profile enables bounded CM shutdown phase markers, excluded from `usb`; neither
`usb` nor normal
`testesp` enables deliberate panic injection. The isolated test namespace and
BMS control gates remain unchanged. The 1.4.11 BLE
self-OTA failed with a watchdog while the same image without BLE restarted
normally. The initialized-BLE self-OTA and the separate production restart validation passed; see [the 1.4.12 acceptance report](docs/acceptance/2026-10-27-http-ota-ble-shutdown.md).
Previous firmware and source freezes remain unchanged.

When switching a shared fixture between SIL and SBT, use a complete USB upload (pio run -e testesp -t upload) that writes the bootloader, partition table and OTA selection as well as the application. An application-only upload cannot replace a different partition layout. Production HTTP OTA continues to require the existing compatible SBT layout.

## HTTP OTA reboot correction candidate 1.4.11

This patch selects the unpublished, immutable CM 4.10.4 source package at
`file://.local/cm-http-reboot-4.10.4-final-package`. CM defers HTTP-success reboot
from the AsyncTCP disconnect callback to its existing main-loop OTA handler.
The task watchdog remains enabled. The bounded stored-panic diagnostics from
1.4.10, Settings-only UI, telemetry, BMS control gates, and isolated test identity
remain unchanged. A stored dump can be replaced during OTA and is not timestamped;
this change does not establish the cause of the earlier unrelated runtime panic.

Build and dependency outputs use `.pio/http-reboot-1.4.11`. Validate `usb` and
normal `testesp`; neither may contain the intentional panic marker. The separate
`testesp_panic` profile remains an explicitly selected fixture, not a production
or normal test image. Previous frozen packages and firmware artifacts stay intact.
COM8 HTTP OTA with active passive BLE and production acceptance remain separate
owner-controlled gates; this source update is not a long-duration stability claim.

## Bounded abort-origin diagnostics candidate 1.4.10

The existing one-shot panic report adds `abort_status`, `abort_caller_pc`, and
`abort_core`. After successful SDK summary and CRC/bounds checks, a read-only
ELF32 resolver follows only the saved abort-details pointer into one unambiguous,
file-backed `PT_LOAD` segment. It reads at most 64 bytes there and accepts only
the exact SDK `abort()` message grammar. Only the parsed caller PC/core leave
the device; no saved pointer, registers, text, arbitrary memory, or HTTP endpoint
are exposed. Unsupported context, invalid structure, ambiguity, truncation,
missing NUL, format mismatch, or read failure produce null result fields and a
fixed status. No whole-image string scan or live-RAM dereference is performed.

Context recognition is limited to the inspected original 1.4.8 and normal 1.4.9
ELF identities at their known `panic_abort` instruction, or the currently running
ELF identity at the pinned SDK's inspected abort instruction. The resolver accepts
only the classic ESP32 CRC32 format: 20-byte outer header, version `0x100`, four-byte
checksum footer, ELF32 little-endian Xtensa, at most 160 program headers, and a
64 KiB image. The original dump can predate the most recent reset or have been
replaced during OTA; no crash time or firmware version is inferred from the task
name. An extracted abort caller identifies a call site, not automatically its
underlying cause.

CM remains the exact unpublished 4.10.1 freeze. Build outputs use
`.pio/abort-origin-1.4.10`. Validate with the existing panic-summary host runner,
`pio run -e usb`, and `pio run -e testesp_panic`; the latter remains COM8-only.
The earlier 1.4.9 and 1.4.8 artifacts remain immutable historical evidence.
The 1.4.10 panic fixture uses a new RTC latch generation so the prior 1.4.9
fixture latch does not suppress this one new test; subsequent warm reboots remain
suppressed. This policy is compiled out of the normal profile.

## Stored panic diagnostics candidate 1.4.9

This diagnostic patch reads the existing coredump once at the start of `setup`,
before BLE workers start. It verifies partition bounds and the SDK CRC before
decoding the bounded SDK summary. It never erases the dump or exports raw memory,
registers, credentials, or a new HTTP endpoint. Normal BLE startup ordering and
MOS controls are unchanged; the exact unpublished CM 4.10.1 dependency is retained.

The existing MQTT owner publishes `<root>/Diagnostics/Panic` without retention.
The immutable report identifies a `historical_stored_panic`, SDK image/check/summary
status, dump address/size, original crashed ELF SHA256 prefix when valid, task name, PC,
exception cause, and at most sixteen backtrace addresses. `dump_valid` means the
partition bounds and SDK CRC passed; it does not mean the backtrace is uncorrupted
or that decoding succeeded. Missing or corrupt dumps produce status-only reports.
No crash timestamp is inferred. The report can refer to an earlier firmware boot;
symbolication must use its original ELF identity, not this diagnostic build.
This Arduino SDK exposes only sixteen hex characters of the stored ELF hash;
`elf_sha256_hex_chars` reports that length and `elf_sha256_complete` is false.
Match the prefix against the preserved original ELF, never invent the missing suffix.

The payload is bounded to 1024 bytes, with the complete topic and MQTT header
fitting the existing 1536-byte buffer. There are at most twelve local publish
attempts per boot, spaced at least five seconds apart while connected. Successful
QoS0 local acceptance ends retries, including across reconnects; broker receipt is
not guaranteed. No repeated flash reads occur on reconnect. `testesp` keeps the
report under `SBTTest/COM8/Diagnostics/Panic` and retains its existing command block.

Build outputs use `.pio/panic-summary-1.4.9`. Focused offline validation:
`powershell -File tools/test/run-panic-summary-host-tests.ps1`; firmware validation:
`pio run -e usb` and `pio run -e testesp`. This candidate adds diagnostic evidence;
it does not identify or fix the observed 1.4.8 panic, and is not a long-run pass.
The stored coredump and original 1.4.8 ELF must remain preserved for diagnosis.

`testesp_panic` is a separate, explicitly destructive COM8-only diagnostic profile.
It inherits every isolated test guard and requires `SBT_ISOLATED_TEST_PROFILE=1`
at compile time. After sixty continuous seconds with the application, BLE worker,
and MQTT connection ready, it writes a two-word RTC no-init latch before one
deliberate `abort()`. After the panic reboot, early capture can report the real
SDK coredump; the retained latch prevents a second deliberate panic. A warm reboot
does not re-arm it. A power cycle can lose the RTC latch. No NVS flag is written.
Normal `usb` and `testesp` profiles compile out the trigger and its marker.
Only the owner may flash/run this special profile, after preserving the complete
COM8 flash image, and must restore that image after the bounded diagnostic test.
Never upload `testesp_panic` to production.

## WiFi power-policy checkpoint 1.4.8

Companion CM implementation checkpoint: `801e3cf76dc0c8801a4176241a600aa92be9336f`
(on `fix/mqtt-session-recovery`). The version-specific archived inputs below,
including their original manifests, remain the exact firmware dependency source.

This installed checkpoint uses an independently frozen, unpublished CM 4.10.1 package
through `file://.local/cm-wifi-coex-4.10.1-package`. Run
`python tools/prepare_cm_development.py --source-package <frozen-package-path>`
to copy and verify that package and its original manifest without importing the
mutable CM checkout or deriving new Git provenance. Without an explicit source,
the helper looks under the neighboring CM repository's `.local` directory.
Build/dependency outputs use `.pio/wifi-coex-1.4.8`;
deployed 1.4.6 artifacts and private settings are preserved. The historical
published CM 4.9.0 binding remains guarded and tested separately.

CM adds one setting, `WiFiPowerSave`, default false, and reports requested mode,
native mode and application/readback errors in Settings JSON with Live disabled.
On classic ESP32, initializing BLE also acquires a controller-lifetime coexistence
lease: CM confirms native MIN_MODEM before starting the controller, retains it
through pause/reset, and explains an overridden OFF preference without changing
its stored value. WiFi-only operation can still use NONE. The earlier unreleased
1.4.7 / CM 4.10.0 artifact aborted during BLE initialization with native NONE;
it is preserved as crash evidence and is not the current candidate.
SBT adds no direct sleep override and retains legacy MQTT transport to isolate
the WiFi/BLE comparison; bounded MQTT I/O is not enabled. BLE algorithms, MOS
semantics and testesp's permanent control block/initial empty peers are unchanged.
Source freshness uses the complete immutable manifest, with only consumer-local
generated UI assets and scoped caches excepted. This is not a public CM release
or a long-term stability claim.

The exact owner-authorized OTA was accepted at 21:50:21.698503 UTC on 2026-10-25.
Binary SHA256: `0bc36ac5cf5d5e9532c450b16856ad210c606c86c5ffb06e291803ce895d09fd`.
Fresh identity, version 1.4.8, Live disabled/Settings enabled and inactive OTA were
verified. All 32 prior settings matched their readback, with exactly one new
WiFiPowerSave=false setting; masked secrets compare only as returned. Native
MIN_MODEM with the explicit BLE coexistence reason and successful apply/read
confirmed the intended policy. At 21:53:53 UTC, real BMS/Victron health sequences
had advanced to 35/69 and both reported available. Uptime reached 190099 ms at
21:53:37 UTC without a further observed drop. COM8 was outside real peer range,
and the owner explicitly accepted the transition to production acquisition.

The 188-file CM freeze's manifest SHA256 is
`6c04701572f9d45e10277ea6aa8eefa9763ab98d57bd6d6676ff3308ad1a6fe7`.
The restored `.local` package remains ignored, but its exact source ZIP, SHA256
and original manifest are tracked under `dependencies/cm/`, including LICENSE.
The companion CM commit is not a Registry release. Later README/changelog notes
are documentation-only differences from the frozen inputs. Original packages and
OTA binaries were not regenerated for this checkpoint.

From the repository root, restore the package before building:

```powershell
$archive = 'dependencies/cm/cm-4.10.1-source.zip'
$expected = '3e39252b3eca6bebf5d2fc73dd97ef72a16daa6d573a10865d4c7ecd31d3e606'
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $archive).Hash.ToLowerInvariant() -ne $expected) { throw 'CM archive hash mismatch' }
$destination = '.local/cm-wifi-coex-4.10.1-package'
if (Test-Path -LiteralPath $destination) { throw 'Preserve the existing package; choose a clean checkout for restore' }
Expand-Archive -LiteralPath $archive -DestinationPath $destination
```

The ZIP contains exactly the 188 manifest entries plus `manifest.sha`. Its
roundtrip was hash-verified before this checkpoint; the wrapper verifies all
extracted source hashes again. The `.zip.sha256` and external `.manifest.sha`
record the archive digest and original source manifest. The preparation helper
above remains an alternative when the original frozen directory is available.

An earlier production 1.4.6 uptime fell from 20710524 ms at 21:27:54 UTC to
10118 ms at 21:28:57 UTC, proving a pre-OTA restart. The new image subsequently
reported resetReason=6 after OTA (TASK_WDT in the actual SDK). The initiating
cause, including any connection to the old firmware's OTA path, remains open.
This checkpoint does not claim to fix that restart or establish long-term runtime
stability.

Real COM8 BMS testing requires a separately verified production-controller release;
read-only GATT still opens a connection. Restoring the production peer must wait
until COM8 has stopped that controller. Passive Victron advertisements do not
require that GATT handoff. Report actual received frames or an observed RF limit;
empty-peer lifecycle checks and emulated frames do not qualify real acquisition.

The preceding firmware **1.4.6** used published ConfigManager **4.9.0** with **Settings enabled and Live disabled**. It reads a JBD 14-cell BMS and passive Victron SmartSolar Instant Readout on one ESP32 and publishes change-triggered MQTT telemetry with a configurable full refresh (60 seconds by default). Guarded charge/discharge MOS control is implemented but disabled by default; Victron remains read-only. This build change does not establish a fix for the historical panic or HTTP-delivery issue.

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

The previous 1.4.6 checkpoint pinned `vitaly.ruhl/ESP32 Configuration Manager@4.9.0` from the PlatformIO Registry. Checkpoint 1.4.8 selects only the explicit frozen local CM 4.10.1 URI above; other source/symlink overrides are rejected. The precompile check validates selected provenance/version, coherent mirrors and the generated Settings-only asset proof. The package's official generator creates derived WebUI assets only inside the consumer-local dependency directory; it does not use the mutable sibling checkout or change PlatformIO's shared cache.

Both production profiles explicitly set `CM_ENABLE_LIVE_UI=0` and `CM_ENABLE_SETTINGS_UI=1`. Application metadata registration treats CM's disabled Live API as an intentional no-op, so disabling Live cannot abort BLE/MQTT initialization. Network credentials and OTA defaults remain in ignored local files. Remove obsolete `custom_cm_path` and CM `symlink://` entries from private overrides; preserve their other settings. Candidate outputs use `.pio/wifi-coex-1.4.8`; `.pio/registry-settings` retains the earlier checkpoint.

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

`pio run -e testesp` builds a separate diagnostic artifact at `.pio/cell-telemetry-1.5.0/testesp/firmware.bin`; retain its matching ELF. It initializes the BLE worker with empty BMS/Victron peers, uses DHCP, and hard-denies BMS gate/MOS requests independently of settings. All MQTT root transitions, commands, availability, history and System-Info use `SBTTest/COM8/...` with client ID `SBTTest-COM8-collector`. The application emits no HA discovery. MQTT endpoint/authentication can still come from persisted settings; set only this verified test board's endpoint/auth/enable settings from the existing private source when an older SIL broker configuration was inherited.

The profile reports absent peers as unavailable. Empty-peer operation qualifies initialization and lifecycle only, not real BLE reception or physical control. The test profile does not modify the production binary. SBT retains synchronous legacy MQTT transport even with CM 4.10.1; it does not enable that library's optional bounded adapter. Device testing, OTA and restoration of the previous COM8 image require owner authorization and matching artifact evidence.

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

HA Allow controls now follow actual MOS readback and use fresh, explicit commands after a restart. The common ESP/HA control gate is persistent and blocks all external MOS writes when off. SIL 5.6.1 publishes no MOS commands; received directional protections and MOS feedback still qualify its inverter control. Existing manual SBT/HA controls and autonomous BMS hardware protection remain unchanged. See [the current control contract](docs/BMS.md#guarded-mos-permissions), [MQTT examples](docs/home-assistant-bms-controls.yaml) and [gate template](docs/home-assistant-bms-gate.yaml).
