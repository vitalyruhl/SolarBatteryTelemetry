> **Historical evidence; superseded on 2026-10-20.** This document preserves a previous plan and its original execution/acceptance record. Its tasks, checkboxes, version targets, device instructions and permissions are historical, not current execution authority.
>
> The [single current SIL/SBT/HA integration plan](../../../SolarInverterLimiter/docs/superpowers/plans/2026-10-20-sil-sbt-ha-integration.md) owns all subsequent work and current owner approvals, including the test-ESP build/upload, Settings/Live checks and at least five minutes of observation for each consumer. Earlier COM7 restrictions or approvals below do not override that current scope. Production uploads are not authorized. This consolidation performs documentation work only; implementation and testing start in a later step.

---

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
## Resumed heap investigation after checkpoint b2e749a (2026-10-18)

The owner resumed function-preserving memory/crash work and explicitly permits
the test ESP and Chrome when needed. The prohibition on further NVS checks
remains. No hardware operations were performed during the following read-only
audit. Current firmware checkpoint is b2e749a; CM is 5133056.

Attribution: the last Core 0 failure is a NULL outer semaphore pointer in lwIP,
not a proven dangling BLE pointer. AsyncTCP's permanent async_tcp task executes
HTTP callbacks separately from setup/loopTask. AsyncClient write/output/receive-
acknowledge/close wrappers call tcpip_api_call from their caller's task. Initial
server bind/listen in setup therefore cannot establish semaphore readiness for
every later calling task. The visible BLE worker loop has no direct TCP call.
Memory pressure caused by BLE is possible; neither BLE corruption nor a CM-only
root cause is established. Do not combine this trace with the older Core 1 abort.

Primary source cross-checks:
- https://github.com/espressif/esp-idf/blob/v4.4.7/components/lwip/port/esp32/freertos/sys_arch.c#L470-L514
- https://github.com/espressif/esp-lwip/blob/2.1.3-esp/src/api/tcpip.c#L442-L470
The first source returns NULL on lazy per-thread semaphore allocation failure;
the second queues that return without a NULL check. Exact deployed ELF evidence
above remains stronger than attributing a runtime event from source alone.

Function-preserving candidates (not implemented or device-measured):
1. CM: separate BLE configuration from IO commands. Three retained configuration
   copies currently carry unused 272-byte arrays (816 payload bytes definitely
   unnecessary). Estimated ESP32 layouts/config and IO split save about 1,248
   bytes BSS, including the eight command slots. Keep event capacity 16, command
   capacity 8, RX maximum 272 and all generation/query ordering rules.
2. CM/SBT: retain CM's normal TX maximum 272, but allow an explicit SBT TX limit
   9. All SBT reads are 7 bytes and existing guarded MOS writes are 9 bytes
   (BmsReadout.h and BmsCommandPolicy.h). After the preceding split, estimated
   additional saving is 2,112 BSS bytes; combined BLE estimate 3,360 bytes.
   Target sizeof/linker comparison and complete transmit-contract coverage are
   required. Do not shrink RX or maximum JBD frame handling to obtain this gain.
3. SBT: StatusPublisher has four 512-byte slots. Current health producers bound
   their buffers at 256 and control/result producers at 384; matching per-slot
   capacities 256/256/384/384 recover 768 payload bytes with unchanged producer
   capacity and MQTT behavior. Test every producer boundary and retry path.
4. Later: source-specific snapshots or separate numeric arrays/validity masks
   can avoid unused fields/alignment overhead. Keep double precision, null,
   freshness and coherent-cycle semantics. No savings number approved yet.

Current ELF BSS objects: BluetoothManager 9,152 bytes; telemetry service 5,504;
BMS readout 2,640; Victron readout 1,400. BLE worker reserves 8,192 additional
heap bytes for its stack; AsyncTCP default stack is 16,384. These categories
must not be conflated. No stack reduction is justified before worst-case
high-water measurements. Peripheral/Broadcaster are already compiled out and
NimBLE scan results are already not retained; no gain can be claimed twice.

Proposed diagnostic probe before selecting a panic remedy: wrap the actual
sys_thread_sem_get at link time, verify interception in the ELF, call the real
getter exactly once, and record result/task/caller without allocations. Correlate
with fixed-storage allocation-failure events and capability-specific largest
blocks. Do not add an extra getter as a probe because it can initialize the
resource itself. A matching NULL return and failed allocation would establish
the OOM route; absence of that evidence leaves corruption open. Controlled HTTP
reloads on the owner-authorized test ESP and Chrome can then reproduce the
baseline, followed by one change per comparison. Use app-only upload, no NVS
readback/write, no MOS actuation, and preserve the matching ELF and failure log.
The read-only audit does not establish that any candidate fixes the panic.
## Owner-approved GUI and heap implementation, final evidence (2026-10-18)

Owner approved the ranked memory changes and an explicit UI/schema reduction.
SBT remains on feature/move-sil-planning (checkpoint b2e749a); CM remains on
feature/ble-compact-runtime (checkpoint 5133056). All follow-up changes below
are uncommitted. No push, main integration or release was performed. SIL is
unchanged by this implementation. Current firmware is SBT 1.1.0 (minor: approved
GUI/control/schema revision; persistent settings/NVS layout unchanged). CM
remains 4.7.0 Unreleased and all version mirrors remain coherent.

Implemented: 50 compact application fields instead of 64; 24 telemetry channels
instead of 29; one Connectivity card/page combines Bluetooth and MQTT. Removed
GUI MQTT root/OTA/validation, duplicate source availability, cell/NTC count
channels and the three unused Victron placeholders. The mandatory JBD 03/04
reads still supply cell/sensor counts needed for frame validation; there never
were separate count-only device queries to remove. No count values are now
stored/published as telemetry. Source is ok or its concrete error reason.
Two compact MOS checkboxes replace four action buttons. They register setters
with empty CM getters: the existing BMS provider remains authoritative for
true/false/null. Requests retain all mailbox/session/freshness/control safeguards;
no MOS operation was triggered during the device test.

Browser acceptance found a same-page CM key-only lookup collision between
bluetooth.enabled and telemetry.enabled. A narrow SBT correction renamed only
the GUI diagnostic key to telemetry.mqttEnabled, with a regression proving all
Connectivity keys unique and Bluetooth Bool/MQTT Text labels preserved. The
public MQTT topics and settings keys are unaffected by that alias correction.
CM's checkbox acknowledgment toast is now neutral (Request sent), followed by
existing readback, rather than claiming the requested ON/OFF state as observed.
The actual Vue handler has five new execution tests; all 36 WebUI Node tests
pass. The generated CM HTML asset was intentionally rebuilt for this change.

Memory: CM config48/IO288(default)/IO24(TX9), old shared record336, RX320 unchanged
are compiler-verified ESP32 ABI sizes. SBT uses CM_BLE_TX_MAX_BYTES=9 consistently;
all existing writes are 7 or 9 bytes. CM default remains272, command/event queue
depths remain8/16 and RX272 stays intact. Retained BLE saving is3360 bytes.
Status payload capacity2048->1280 saves768; removal of five channels also shrinks
retained snapshots. Final normal build: RAM80464 versus baseline85232, exactly
4768 bytes recovered; flash1374129/1835008. Final normal build passed31.76s.
No blind stack reduction was performed (BLE8192/AsyncTCP16384 remain).

Independent SBT_ENABLE_HEAP_DIAGNOSTICS enables the failure hook/Health without
turning verbose CM logging on. A fixed four-entry failure history and a linker
wrapper observe the real sys_thread_sem_get exactly once, without allocations
or prewarming. Final ELF disassembly proves tcpip_api_call calls the wrapper,
which calls the real getter once. Normal firmware contains no wrapper,
recordAllocationFailure, allocationFailed or Health symbol. Host diagnostic
profiles0/0,0/1,1/0 and wrapper/task/ISR/no-allocation/compile-out tests pass.
Diagnostic build RAM80736 (272 bytes more), flash1376109; final build32.26s.
The diagnostic profile and its isolated build/libdeps roots are private under
.local/heap-investigation*.ini and .pio/heap-diagnostics-*; public usb flags
remain logging0/verbose0 with TX9. No SDK/dependency source was patched.

Final normal BIN SHA256:
529ac52e3b880621e54da82238434d063ef20eaf841ba5de47318cf7ab12af4c
Final normal ELF SHA256:
a21f85541f0adc1d6bf02560ba98805264a05c44f5e76fc7e9e641e46bc9fe10
Installed diagnostic BIN SHA256:
2e67da37d8b7e2a7fcaf6e0e4c01e21c866d86c0f23ba9b7e73a72c66079ec76
Installed diagnostic ELF SHA256:
086fc401ccb824272ef5edaed717ecbfa9f5c81f0077a8145a943e683615995d
Installed BIN size1382688 at verified active app0 offset0x10000. The deployment
read only the partition table0x8000+0x1000 and OTA selector0xe000+0x2000; it never
read/wrote NVS, partition table, OTA selection or bootloader. Runtime SDK writes
are outside that tool statement; no additional NVS comparison was performed.

Hardware evidence: first revision240.1s and final revision180.1s, each zero panic,
software-reset, SEM-NULL and HEAP-FAIL markers, one initial POWERON boot. Final
177 Health samples: uptime2755->178933ms, sampled default free>=22808, sampled
largest>=20468; allocator lifetime minimum16500, final free27644/largest23540.
The earlier revision's lifetime min9768 is historical, not the final result.
Chrome on existing test tab192.168.2.126 confirmed proper SBT1.1.0 title,
Connectivity/BMS/Victron navigation, correct Bluetooth/MQTT labels, simplified
BMS/Victron fields and WebSocket mode through repeated reloads/page changes.
Final HTTP200 complete: appinfo107 bytes; metadata7822 bytes/50 fields (old9853,
20.6% smaller), correct two checkbox keys and all removed fields absent; runtime
2785 bytes with unavailable MOS fields still null; root61598 bytes in8.67s.
MQTT showed online; no independent broker receipt acceptance was attempted.
BLE worker reported6100 free stack bytes/zero queue drops, but BMS was unavailable
(disconnected_or_timeout) and Victron awaited advertisements on this test ESP.
Thus full-load radio/readout, physical MOS control, OTA and long-run stability
are NOT established. No root-cause attribution to BLE is proven; no NULL failure
occurred to correlate with OOM. Keep the original intermittent crash issue open
until owner acceptance under real acquisition traffic.

Final regression: CM BLE standard/TX9 transport+memory suite, SBT registration
with actual CM null preservation, JBD/Victron, guarded MOS/envelope/status,
telemetry contract/mailbox/lifecycle and six precompile checks all passed.
Independent review found no remaining product finding; its three documentation
corrections were applied. Private artifacts/logs/HTTP/serial/partition-only
metadata are under .local/heap-test-COM7-20261018; revision-1 preserves the first
run separately. All serial sessions completed/COM7 released; claimed Chrome tab
is left on the test ESP. Subsequent validation must use the installed diagnostic
ELF above, not an older b2e749a image or another build profile.
## Owner-authorized intermediate commit and remote handoff

The owner requested a local intermediate commit before continuing acceptance.
The reviewed SBT1.1.0 GUI/heap/diagnostic changes above are now captured by this
checkpoint on feature/move-sil-planning. The corresponding CM checkpoint is
`dc40899f42f0ea76f7b5db6cfd03520f822eab15` on feature/ble-compact-runtime.
The existing local CM binding resolves that revision; no published dependency
pin, push, main integration or release was performed. Prior uncommitted state
statements remain historical. Product sources predate the successful final USB
and diagnostic builds; only handoff documentation changed afterwards.

The owner reports that the remote SBT at192.168.2.21 has just been updated via
OTA, COM7 is removed, and MQTT Explorer is open and authenticated for testing.
These are owner-provided deployment/access facts, not new independent radio or
OTA acceptance. Do not attach to COM7 or run another upload automatically.
Further browser/MQTT observation should target192.168.2.21 and retain the current
no-NVS-check/no-MOS-actuation boundary. Fresh-source and long-run acceptance are
still pending until evidence is gathered from this remote target.