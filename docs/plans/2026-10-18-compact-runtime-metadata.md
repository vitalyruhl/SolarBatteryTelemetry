# SBT compact runtime metadata migration and device handoff

Date: 2026-10-18. Status: implementation authorized; hardware acceptance pending.

## Restart context and authorization

The owner approved compact ConfigManager 4.7.0 metadata, requested a checkpoint
before migrating CM-owned fields, and then explicitly authorized migrating SBT
and uploading it to the test ESP on COM7 after the BME smoke gate. The owner will
test subsequent behavior. This is the separate SBT plan previously deferred until
CM was ready. No final commit, push, production-device OTA, or BMS operation is
authorized by this implementation handoff.

The test ESP is reachable at http://192.168.2.126/. Preserve its persisted WiFi
and other settings. Parent agent owns the BME gate, flash backup, app-only upload,
serial observation and settings verification. Do not use the separately configured
OTA destination: it refers to a different device and its local binding contains
credentials. No secrets or flash/NVS captures belong in Git.

## Baseline and boundaries

Repository: C:/Daten/_Codding/ESP32/SolarBatteryTelemetry.
Branch: feature/move-sil-planning; initial HEAD
18d5ad90e29b0bf75ddba8dd449ce2a806bf6589.
Existing owner changes in platformio.ini disable CM_ENABLE_LOGGING and
CM_ENABLE_VERBOSE_LOGGING. Preserve those exact edits.

Use adjacent C:/Daten/_Codding/ESP32/ConfigurationsManager through the existing
ignored custom_cm_path/symlink override. Require coherent CM >=4.7.0. Do not
change CM from this task. Local governance is ignored and absent from origin/main;
its historical preparation restrictions are superseded by the current explicit
migration/upload authorization. Do not stage local governance or tooling.

## Bounded implementation

1. Move the 64 existing application field definitions to a small production
   registration header using CM's RuntimeFieldBase composition and compact types.
   Keep every sourceGroup, key, label, page, card, group and order unchanged.
2. main.cpp: convert eleven smoke/Bluetooth descriptors to one text, four bool
   and six int fields. Keep the provider values unchanged.
3. MqttService.h: register forty-nine telemetry descriptors using actual provider
   types. Keep textual diagnostics textual. Map channel Boolean/Real/Unsigned/
   State/Protection to Bool/Float/Int/Text/Text. Use two decimals for real values
   except cell min/max/spread volts, which use three. Providers, MQTT payloads,
   freshness, null values, publication policy and settings remain unchanged.
4. BmsReadout.h: register the four existing RuntimeButtonMeta descriptors. Keep
   registerRuntimeButton and every guarded MOS callback/policy unchanged. Do not
   actuate buttons or MQTT commands during acceptance.
5. Check compact registration failures and stop the existing attach/setup path;
   do not silently install incomplete metadata or fall back to legacy allocations.
6. Require CM >=4.7.0 in precompile validation, including matching test fixtures
   and hints. Preserve its read-only local dependency validation mechanism.
7. Scan version mirrors, bump APP_VERSION 1.0.1 to 1.0.2, retain VERSION_DATE
   2026-10-18, and synchronize current README claims. Historical acceptance
   versions remain historical. No settings keys/NVS layout changes.

The expected UI change is deliberate: former isString numeric and boolean
telemetry now uses numeric unit/precision and bool presentation. Raw runtime
provider values and MQTT payloads do not change. Unknown/stale values remain null.
No new fields, sparse alarms, styles, labels, controls or features are added.

## Validation and hardware gates

- Add a focused host contract test that executes the actual production field
  registration functions with a clearly marked mocked sink and real CM field
  types. Assert all 64 identities, placement, kinds, units, relevant precision,
  uniqueness and failed-registration propagation. This is registration evidence,
  not radio, callback-actuation or full CM allocator evidence.
- Update and run existing precompile dependency tests: reject coherent 4.6.x,
  allow coherent 4.7.0, reject mirror/binding mismatches, perform no writes.
- Run the relevant smoke host runner and pio run -e usb; retain logs in ignored
  .local. No unrelated full test matrix.
- Parent verifies BME gate, snapshots the device, and uploads only the active app
  partition. Preserve bootloader, partition table, NVS and OTA selection.
- SBT's existing missing-only OTA/MQTT/readout initialization can create absent
  application NVS keys at first start. Compare WiFi keys and report any app-only
  additions rather than assuming whole-NVS byte identity is required.
- Verify identity/version, field routing/types, settings preservation and bounded
  serial health; leave behavior acceptance to the owner. The inherited intermittent
  CM HTTP-delivery finding remains separate and must not be declared fixed.

## Execution ledger

- Initial plan written before product edits. Implementation complete; device
  acceptance remains with the parent/owner.
- Product registration functions now live in src/runtime/AppRuntimeMetadata.h,
  used by the three original registration paths. No RuntimeFieldMeta references
  remain under src. Type counts: 19 Float, 12 Int, 11 Bool, 18 Text, 4 Button.
- The contract test executes these production functions with a mocked sink and
  real CM public field definitions. All 64 keys/labels/placement/order, kinds,
  numeric units, cell precision, uniqueness, invalid button index and first/middle/
  last registration-failure propagation pass.
- Dependency regression first failed because coherent CM 4.6.3 was accepted;
  all six dependency tests now pass, including the read-only coherent 4.7.0 case.
- The existing smoke runner passed provisioning, dependency validation, telemetry
  contract, MQTT mailbox and lifecycle checks. No radio, MOS or broker command
  was run by the implementation agent.
- pio run -e usb passed against frozen CM 4.7.0 in 110.20 seconds. Static RAM:
  85,232 bytes; flash: 1,375,525 / 1,835,008 bytes. These linker figures are not a
  runtime heap-savings measurement. Existing upstream deprecation/LTO warnings,
  including NimBLE type-mismatch warnings, remain in the ignored build log.
- Build log: .local/compact-runtime-metadata/usb-build.log. Host contract log:
  .local/compact-runtime-metadata/runtime-metadata-host.log. Artifact identities:
  .local/compact-runtime-metadata/build-artifacts.json.
- No tracked changelog exists; current behavior and dependency changes are
  recorded in readme.md and docs/MQTT.md. Historical acceptance/version-freeze
  statements remain as historical records. Current APP_VERSION is 1.0.2 and
  VERSION_DATE remains 2026-10-18.
- Existing owner platformio.ini logging changes are unchanged. No commit, push,
  upload, serial session, browser operation or CM source change by this agent.
- Parent must append the conditional BME gate/SBT upload and settings acceptance
  results after hardware work. Do not infer device acceptance from this build.

firmware.bin: 1382096 bytes; SHA-256 `6f3f308ad09394688a7c4c7228f730fabb07d84bf7f4b181253e18c7de7ad475`.

firmware.elf: 12264664 bytes; SHA-256 `7ded81181f7c496f6cffebb6c1a3303d00dced284b920247190fd6026e3c8e01`.

## Hardware execution and remaining acceptance

BME prerequisite passed the short follow-up gate on COM7: sensor ready, browser
sensor rendering and an explicit reload, 18 compact System descriptions with
identical historical precision, complete HTTP root (61,595 bytes), and no panic
markers during 70 seconds. The known intermittent HTTP-delivery issue remains
open; this is bounded smoke evidence, not a stability certification.

SBT application-only upload completed and esptool verified the image at active
app0 offset 0x10000. A fresh 4,194,304-byte full flash backup was captured before
the write. Image: 1,382,096 bytes, SHA-256
`6f3f308ad09394688a7c4c7228f730fabb07d84bf7f4b181253e18c7de7ad475`.
NVS, partition table and OTA selection were byte-identical immediately after
upload. Private backup/logs/results: `.local/compact-runtime-metadata/device-COM7/`.

The first browser load triggered a real Core 0 LoadProhibited panic. Decode used
the exact deployed ELF (`7ded81181f7c496f6cffebb6c1a3303d00dced284b920247190fd6026e3c8e01`):

```text
Guru Meditation Error: Core  0 panic'ed (LoadProhibited). Exception was unhandled.
Backtrace: 0x40142326:0x3ffdf390 0x401328be:0x3ffdf3b0
ELF file SHA256: 7ded81181f7c496f
```

Frames resolve to `sys_sem_signal` (lwIP ESP32 FreeRTOS sys_arch.c:152) and
`tcpip_thread_handle_msg` / `tcpip_thread` (lwIP tcpip.c:173/154). This differs
from the earlier Core 1 abort trace. Root cause is not proven; do not call the
migration stable, the original issue fixed, or attribute the panic to metadata
based on this stack alone. Private decoded evidence: `decoded-crash.txt`.
One initial POWERON_RESET and one subsequent SW_CPU_RESET were captured during
the 60-second monitor window. No automatic transport or dependency patch was made.

After reboot, appinfo confirmed SolarBatteryTelemetry 1.0.2; runtime_meta.json
returned all 9,853 bytes and 64 fields: 11 Bool, 18 Text, 4 Button, 31 numeric.
All three cell-voltage fields retained precision 3. Page index returned complete
HTTP 200. Browser subsequently displayed the proper SBT title, Bluetooth/MQTT/
BMS/Victron tabs, transport fields and WebSocket mode. No control was actuated.

Post-runtime NVS semantic comparison: 197 active keys before/after; no missing
or added keys. All application settings, including credentials/IP, unchanged.
Only ESP WiFi driver cache entries `nvs.net80211/bssid.set` and
`nvs.net80211/sta.apinfo` differed. Partition table and OTA selection remained
byte-identical. The readback intentionally rebooted the device afterward.
Agent browser tab and serial session were closed; COM7 is free for owner testing.

Status: software migration/build/review and requested test-device upload complete;
stability acceptance remains open with the owner. Implementation is uncommitted
on feature/move-sil-planning, including preserved pre-existing logging=0 edits.
No push, merge, OTA upload to another device or SIL migration occurred.

Independent read-only review found no installation-blocking product issue in
this SBT migration. Parent independently reran the production registration
contract test successfully and confirmed firmware hash, source diff and saved
logging flags. The contract uses a mocked sink; actual endpoint/browser output
above supplies device registration evidence, not allocator/long-run certification.

## Paused SBT diagnosis: exact NULL-semaphore evidence

Owner requested no further NVS checks, then steered to a compact-metadata SIL
comparison on the same test ESP with BLE disabled. Pause SBT-specific diagnosis
for that comparison; do not repeat flash/NVS readbacks.

The exact SBT ELF disassembly shows tcpip_api_call at 0x40132a6c calling
sys_thread_sem_get, storing its return in the queued message at offset 12,
posting it without checking NULL, and waiting indefinitely. The TCP/IP thread
loads that message field at 0x401328b9 and signals it. The actual fault PC
0x40142329 dereferences A2=0 (EXCVADDR=0), proving a NULL outer semaphore pointer.
Installed IDF 4.4.7 uses LWIP_NETCONN_SEM_PER_THREAD=1. Lazy initialization needs
a four-byte holder plus a binary semaphore; either allocation failure returns
NULL. This is a direct OOM-to-panic path, but the failed allocation event itself
was not captured in the logging=0 build. Memory corruption remains an alternative
until runtime evidence excludes it. Do not claim a proven BLE defect.

Source reference: https://github.com/espressif/esp-idf/blob/v4.4.7/components/lwip/port/esp32/freertos/sys_arch.c#L470

Separate read-only AsyncTCP 2.1.4 inspection also found potential RX pbuf leakage
when queued receive events are discarded, unchecked event malloc calls, and API
wrapper error propagation weaknesses. None is established as the direct cause
of this observed NULL pointer. Do not silently patch downloaded dependencies or
apply a broad upgrade while the SIL comparison is pending.

## Owner-authorized checkpoint before memory analysis (2026-10-18)

The owner requested a local intermediate commit before resuming SBT crash and
memory work. The checkpoint preserves all 64 compact application fields, the
CM >=4.7 validation and the owner's existing logging=0 configuration on
`feature/move-sil-planning`. The adjacent CM integration baseline is
`feature/ble-compact-runtime` at 51330563b2207a5453255aa6e9caa850f5de312c.

Fresh validation: production metadata registration host contract passed and all
six precompile dependency checks passed. `pio run -e usb` succeeded in 12.21
seconds: static RAM 85,232 bytes and flash 1,375,525 / 1,835,008 bytes. Build log
is private at `.local/compact-runtime-metadata/checkpoint-build.log`. Firmware
remains 1.0.2. No hardware, NVS, serial-monitor, browser, upload or push operation
is part of this checkpoint. Earlier panic evidence remains open; the successful
build does not establish crash-free runtime. Further memory/crash analysis starts
after this checkpoint; no new optimization is included here.