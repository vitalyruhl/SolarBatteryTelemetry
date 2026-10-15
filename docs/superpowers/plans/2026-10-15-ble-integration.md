# BMS and Victron Telemetry Implementation Plan

> **For agentic workers:** Use superpowers:subagent-driven-development or superpowers:executing-plans after owner approval. Work task by task; checkbox completion requires evidence. Reuse bounded workers where context remains useful. No automatic commits, pushes, releases, uploads or device writes.

**Goal:** Deliver one ConfigManager-based ESP32 collector that reads JBD BMS and Victron BLE data, publishes MQTT telemetry, survives local stability tests and can be updated via OTA in the shed.

**Architecture:** One opt-in ConfigManager Bluetooth transport owns NimBLE. Collector adapters own device protocols, decoding, freshness and MQTT. Generic OTA lifecycle events pause/resume the transport without replacing existing OTA callbacks.

**Tech Stack:** PlatformIO espressif32 7.0.1 / Arduino-ESP32 2.0.17 / nodemcu-32s; ConfigManager development based on 4.4.10 after a separate 4.2.2 upgrade gate; PubSubClient 2.8; NimBLE-Arduino 2.5.1 trial pin; scoped MIT Victron C decoder and Apache-2.0 JBD protocol adaptation.

**Spec:** [design proposal](../specs/2026-10-15-ble-integration-design.md). Both files are proposed, not yet approved for execution. This is the requested planning deliverable, not authorization to start implementing it.

## Global constraints

- Preserve [project context](../../../project.info.md); no inverter, battery-control or energy-scheduling changes.
- Same board type as limiter; same physical test ESP32 later moves to shed.
- Preserve existing ConfigManager consumers, settings keys and callbacks. Bluetooth stays opt-in and BLE-only.
- Do not add NimBLE to ConfigManager library.json or include its headers from ConfigManager.h.
- Device profiles, cryptography and MQTT entities belong to the collector, not shared ConfigManager.
- Only protocol-read requests and subscription writes to BMS; no configuration/control writes. Victron reception is passive.
- No external IO/display. Built-in LED only, 30 ms pulse on source-state publish success.
- All repository artifacts English; normal conversation German. Credentials, real captures, firmware binaries, logs, governance and tool state stay local/ignored.
- Owner explicitly requested no automatic completion commits. Do not reuse previous publication authority for new commits or pushes.
- Read each repository's AGENTS.md and only the applicable role. Resume current task branches; do not overwrite existing .serena changes.
- Start with Atlas and exact source slices; use Serena symbols. If Atlas reports the known ConfigManager graph publication error, report it and use bounded source reads; do not repair tools as part of implementation.
- Tests only for changed components and affected consumers. No full repository sweep by default. If later release governance requires a wider gate, report that separately before release.
- Hardware operations need explicit authorization for the named unit, port/address and test stage. Approval of this plan alone does not authorize unattended device manipulation.

## Review focus and owning tests

1. Partial BLE notifications, bad lengths, dropped events and stale generations must never create valid measurements (Tasks 3-5).
2. Wrong/missing Victron key and unavailable sentinels must never become plausible zeros; matching key-check byte is not full authentication (Task 4).
3. MQTT outage, BLE peer disappearance and phone holding the GATT link must not starve WiFi/UI/OTA or leak clients (Tasks 3, 6, 8-9).
4. HTTP or ArduinoOTA failure/timeout must recover BLE; success must keep BLE paused through reboot; secrets must survive without exposure (Tasks 2, 6, 8).
5. Changes to enabled state/settings during callbacks and 32-bit millis/counter wrap must not cause use-after-free, stale publication or infinite reconnects (Tasks 3-6).

## Work allocation and dependencies

Lead owns Task 1, integration Task 6 and acceptance coordination. A shared-library
worker owns Tasks 2-3 serially. One protocol worker owns Tasks 4-5 serially (or two
only if it materially helps); these can run alongside library work after Task 1
freezes contracts. Lead reviews source diffs and focused results before Task 6.
Task 7 is software release-readiness, Task 8 local physical acceptance, Task 9
shed acceptance. Never let two workers edit the same repository simultaneously
without explicit file ownership. Do not create one fresh agent per checkbox.

At each handoff supply only: approved spec/plan, exact repository/branch/base SHA,
allowed paths, interface section, necessary governance and preceding evidence.
Workers report changed files, tests/commands, failures and unresolved assumptions.
The lead independently checks the relevant diffs and evidence. No agent may
quietly change the selected stack, backend version, protocol write allowlist,
MQTT contract, test thresholds or partition layout.

## Shared proposed interfaces

These names are a design contract for new code, not claims about existing APIs.
Confirm names do not collide at Task 1; if they do, update this contract once
before parallel work. Use uint32_t elapsed subtraction for wrap-safe deadlines.

**ConfigManager new transport:** src/bluetooth/BluetoothTypes.h defines
BleAddress (18-byte colon-hex text and explicit uint8_t address type),
BlePauseReason {Ota, Configuration}, BleEventType {Advertisement, Connected,
Disconnected, Notification, Error}, and BleEvent with type, peer, uint32_t
receivedMs/generation, int8_t rssi, int errorCode, uint16_t size and owned
uint8_t data[272]. Advertisement data means manufacturer bytes INCLUDING the
two company-ID bytes, not a complete AD element or whole advertisement.
Notification data means the exact received GATT fragment. size is the actual
copied length; empty/non-manufacturer advertisements are ignored. States:
Disabled, Idle, Scanning, Connecting, Connected, Paused, Error.
BluetoothManager.h defines a statically owned BluetoothManager with:

- void attach(ConfigManagerClass& cm);
- bool begin(); void loop(uint32_t nowMs);
- bool configureAdvertisementPeer(const BleAddress&);
- bool configureGattPeer(const BleAddress&, uint16_t serviceUuid, uint16_t notifyUuid, uint16_t writeUuid);
- bool requestConnect(); bool writeCommand(const uint8_t* data, size_t size);
- void requestPause(BlePauseReason); void requestResume(BlePauseReason);
- bool isPaused() const; uint32_t generation() const;
- void setEventHandler(std::function<void(const BleEvent&)> handler);
- Settings/layout/runtime helpers following existing MQTT helper conventions;
  exact overloads copied from the verified pattern, no new generic registry.

Only requestPause/requestResume cross callback/task boundaries; implement flags
atomically. Application loop invokes other methods and event handler. Worker
owns NimBLE objects; loop transfers commands/events. Reconfiguration increments
generation, discards old events and cancels outstanding queries. Queue full:
drop event, increment counter, invalidate affected query; never grow a vector.
All command copies use a separate finite queue (8 entries, each max 272 bytes).
Worker initial stack budget 8192 bytes; verify units for ESP32 task APIs and
normalize high-water reporting to bytes. No task core affinity assumption.

**ConfigManager new OTA contract:** src/ota/OTAManager.h adds OtaStage
{Starting, Succeeded, Failed, Aborted, TimedOut}, OtaTransport {Arduino, Http},
OtaLifecycleEvent {stage, transport, uint32_t sessionId, int errorCode}, and
void setLifecycleObserver(std::function<void(const OtaLifecycleEvent&)>).
Observer is separate from setCallbacks(RebootCallback, LogCallback), registered
before uploads, and survives setupOTA. It only requests transport pause/resume;
no waiting for the worker, no deinit or NVS write inside an OTA callback.
Serialize accepted upload sessions across transports; reject overlap through
the actual transport error path before accepting flash writes. IDs identify
accepted sessions, not unauthenticated probes. Only the active session may
release its pause after failure; Succeeded latches pause until reboot, even if
late cleanup/disable arrives. Test rejected concurrent attempts and stale IDs.

**Collector new model:** src/telemetry/TelemetryTypes.h defines validity-bearing
fields (value + bool valid), BmsSample, VictronSample and per-source status.
BmsSample has voltage/current/power/SOC/capacity, 14 cell values, bounded up to
4 temperatures and raw alarm bits; VictronSample has only broadcast fields
listed in the spec. Preserve reported counts and reject unsupported counts
instead of truncating silently. Both include receivedMs/generation/sequence.

JbdFrame contains uint8_t command/status/payloadLength and owned payload[255].
Its parser exposes setFrameHandler(std::function<void(const JbdFrame&)>); the
handler consumes a validated frame synchronously and must not retain a pointer.

Pure APIs:
- JbdFrameParser::consume(const uint8_t*, size_t, uint32_t nowMs) and reset();
  produces validated owned JbdFrame values through a synchronous handler.
- bool decodeJbdBasic(const JbdFrame&, BmsSample&);
  bool decodeJbdCells(const JbdFrame&, BmsSample&);
- DecodeResult decodeVictronSolar(const uint8_t*, size_t, const uint8_t key[16], VictronSample&);
  DecodeResult distinguishes Ok, Ignored, Malformed, KeyMismatch, Unsupported.
- Adapter onBleEvent(const BleEvent&), tick(uint32_t), reset(uint32_t generation);
  no adapter initializes the BLE stack or calls MQTT directly.
- TelemetryPublisher::tick(uint32_t, const BmsSample&, const VictronSample&);
  owns freshness/JSON publication; MqttActivityLed::pulse(uint32_t) and tick(uint32_t).

## Task 1: Freeze the baseline, dependency trial and contracts

**Owner:** lead. **Depends on:** owner approval of this spec and plan.
**Files:** collector platformio.ini, src/settings.h; ConfigManager existing
library.json/ConfigManager.h and examples/minimal build profile, read-only first.
Record findings in ignored .local/acceptance/; amend plan if a gate fails.

- [ ] Record clean/dirty states, branch names, SHA and existing user changes for both repositories. Create/select implementation side branches per governance; no automatic commits.
- [ ] Baseline collector build with 4.2.2 using `pio run -e usb`; retain size report and exact ELF locally. No upload. Do not reuse it as live OTA evidence.
- [ ] Build the unchanged scaffold against 4.4.10 in an isolated dependency profile. Verify required ConfigManager settings/module/OTA APIs; compile affected existing password/settings tests without upload or execution (`pio test -e usb --without-uploading --without-testing`) and build the affected MQTT example. Execute new host checks below; existing hardware-only tests remain pending until Task 8. The scaffold itself has no existing runtime behavior to regress.
- [ ] Run an opt-in compile probe against NimBLE 2.5.1 + core 2.0.17 + ConfigManager baseline; compile an ordinary consumer without NimBLE. Record resolved dependency versions and linker size. No broad upgrades to make a probe pass.
- [ ] Establish host test tooling before worker handoff. This planning host has clang++ at C:/Program Files/LLVM/bin/clang++.exe and CMake, but host linking/standard libraries were not exercised. Verify a minimal C++17 executable can compile and run. Create tools/test/run-ble-host-tests.ps1 in CM; it selects clang++ (or an explicitly verified compiler), builds isolated suites into ignored .local/host-tests and fails on any compile/assert failure. Do not install tools automatically.
- [ ] Freeze the integration against the reviewed ConfigManager development checkout using an ignored absolute local dependency override; do not mix 4.2.2 headers with 4.4.10 generated WebUI. Existing collector precompile helper must remain package-coherent.
- [ ] Confirm a small, compile-only no-BLE/MQTT consumer and the new BLE example as regression targets. Include WiFi-off and CM_ENABLE_OTA=0 only for shared paths actually changed; do not build every consumer.
- [ ] Classify version impact under each repo's governance. Proposed collector 0.2.0 and CM next compatible minor (4.5.0 only if still free at execution); synchronize required mirrors/changelog during implementation. Do not publish a dependency release automatically.

**Gate:** exact combined compile passes and fits a dual-slot layout. If baseline
upgrade or the selected backend fails, stop that dependent path and report a
bounded alternative/backport; do not claim library compatibility from upstream CI.

## Task 2: Add generic OTA lifecycle observation

**Owner:** shared-library worker. **Depends on:** Task 1.
**Modify CM:** src/ota/OTAManager.h, src/ota/OTAManager.cpp; affected OTA docs/tests.
**Create:** src/ota/detail/OtaLifecycleState.h and test/host/ota_lifecycle_test.cpp.
Extract only the real session transition logic into this hardware-free helper;
the production OTAManager uses it, so tests do not mirror a second implementation.
Helper API: bool tryStart(OtaTransport), void finish(uint32_t sessionId,
OtaStage, int errorCode), uint32_t activeSessionId() const. Inject the observer
at construction. IDs, single-terminal behavior and overlap are tested directly.

- [ ] First add failing lifecycle sequence tests: Arduino/HTTP start-success, authentication/begin/write/end failure, disconnect abort, stale timeout, disabled OTA and duplicate cleanup. Assert one terminal event per started session; no resume after Succeeded.
- [ ] Implement the shared OTA contract at existing callback/HTTP paths. Emit Starting before first flash write; preserve existing authentication, logging, reboot and route behavior. Unauthorized probes must not trigger a fake successful update.
- [ ] Observer callback only flips/queues pause intent. BLE cancellation runs on its worker; callback completion is not proof of quiescence. Count/report late radio events and ensure they cannot publish after pause. If physical testing shows flash-time instability, stop release and add an explicitly reviewed pre-upload quiescence handshake, not a blocking wait in AsyncWeb.
- [ ] Test callback preservation across setupOTA; observer empty must behave as before. Exercise disabled build. A separate observer must never overwrite ArduinoOTA's existing callbacks.
- [ ] Run `./tools/test/run-ble-host-tests.ps1 -Suite ota` with fake upload sources driving the REAL lifecycle helper, and build the minimum example. Assert event order, session IDs and failure coverage. Review production HTTP/Arduino hook wiring against the helper; host tests do not execute real flash/network callbacks, which remain Task 8 acceptance.

## Task 3: Optional BLE transport/settings module

**Owner:** shared-library worker. **Depends on:** Tasks 1-2.
**Create CM:** src/bluetooth/BluetoothTypes.h, BluetoothManager.h,
docs/BLUETOOTH.md, examples/bluetooth/platformio.ini and src/main.cpp;
src/bluetooth/detail/BluetoothScheduler.h, BoundedBleQueue.h and
test/host/bluetooth_transport_test.cpp. These hardware-free units implement the
ACTUAL deadlines, generations, pause mask and owned queue used by the production
module; fake backend operations record connect/scan/write/cancel requests.
Run via `./tools/test/run-ble-host-tests.ps1 -Suite bluetooth`; assert transitions
and ownership, not only successful compilation. Production NimBLE callbacks
remain subject to source review plus Task 8/9 physical tests. Update narrow MQTT attach
comment/docs mismatch and required version mirrors.

- [ ] Add tests for attach twice, enabled=false, missing peer, queue overflow, callback-buffer lifetime, stale generation, disconnect mid-query, deadline/millis wrap and two simultaneous pause reasons. Resuming OTA must not undo a Configuration pause.
- [ ] Implement the single-worker/finite-queue contract, scheduling and reconnect budgets from the spec. Store no unbounded scan results. One client object is reused or deleted exactly once; configure/reconnect does not append callbacks.
- [ ] Wire settings and runtime helpers following actual MQTT attach behavior: add settings once, load before begin; radio remains off if disabled. Keep all peer protocol UUIDs and decoders out of CM.
- [ ] Build BLE opt-in example with only this consumer declaring NimBLE. Build isolated default consumer with no NimBLE, plus affected MQTT case. Inspect dependency graph/link map: no NimBLE/BLE symbols in opt-out firmware. Runtime disabled is not a substitute for compile-time opt-out.
- [ ] Connect lifecycle observer in the example. Test scan/connect pending during OTA pause; error resumes only permitted state; success remains paused. Keep the stack initialized; no memory-release workaround.
- [ ] Record worker stack/queue limits, expected call context and lifetime rules in BLUETOOTH.md. Report exact test commands and results, no commits.

## Task 4: Victron decoder with bounded reuse

**Owner:** protocol worker. **Depends on:** frozen interfaces from Task 1; can run alongside Tasks 2-3.
**Create collector:** lib/VictronCore/library.json, include/victronble_core.h,
src/victronble_core.c, src/victronble_aes_sw.c and required crypto headers only;
LICENSE, UPSTREAM.md and recorded patch; src/telemetry/TelemetryTypes.h;
src/adapters/VictronAdapter.h/.cpp; test/test_victron_decoder/test_main.cpp.

- [ ] Vendor only the portable MIT C subset from the pinned SHA. Verify includes and license dependencies; never import src/esp32 or the C++ wrapper. Document each changed line and retain copyright/license.
- [ ] Add independently constructed synthetic known-answer tests: valid Solar, short header, 21-byte manufacturer data, 22-byte minimum, >31-byte input, wrong company/type/key-check, unsupported record, negative current, yield above 65535 Wh, every unavailable sentinel, duplicate nonce, wrap and restart. No real device keys/frames.
- [ ] Fix actual-length propagation in the reused decoder and Solar sentinel handling. Test voltage/current 0x7FFF, yield/power 0xFFFF and load 0x1FF become invalid, not zero. Never copy GPL implementation code; use protocol facts and independently authored tests.
- [ ] Use the existing AES implementation with known vectors. Document that a wrong key sharing the check byte cannot always be detected; test that limitation instead of claiming authentication. Filter address/type, apply explicit field bounds, and surface suspect samples without refreshing valid telemetry.
- [ ] Adapter consumes owned Advertisement events, keeps only the latest sample, and resets duplicate/freshness state on configuration generation changes. Do not infer PV voltage/current.
- [ ] Run `pio test -e native -f test_victron_decoder` after adding a hardware-free native test profile with a verified host compiler. Keep TelemetryTypes and pure parser/decoder units free of Arduino/NimBLE includes; build_src_filter includes only those units. Add tools/native_toolchain.py to select the verified host compiler for this environment only; on this Windows host use the verified clang/clang++ executables rather than assuming g++ exists. If no compiler exists, report the tooling prerequisite; do not install it automatically or silently skip parser tests.

## Task 5: JBD read-only adapter and framing

**Owner:** same protocol worker. **Depends on:** Task 1 and shared sample types from Task 4.
**Create collector:** src/adapters/JbdFrameParser.h/.cpp, JbdAdapter.h/.cpp;
test/test_jbd_parser/test_main.cpp; third-party notices for adapted Apache code.

- [ ] Add failing fixtures for valid 14S basic/cells/ID, every single split position of the 35-byte cell response, bytewise fragments, two frames per callback, checksum/status/register/end-byte errors, length overflow, truncated temperatures and disconnect/timeout reset.
- [ ] Implement length+7 frame assembly and checksum/status verification with maximum 262-byte frame, 272-byte buffer, 2 s fragment deadline. Incomplete or rejected replies cannot alter sample validity/freshness. Accept no unsolicited command as a response to the active query.
- [ ] Restrict outgoing commands to exact read requests: DD A5 03 00 FF FD 77; DD A5 04 00 FF FC 77; DD A5 05 00 FF FB 77. Tests compare all emitted bytes to this allowlist. No configuration writes, factory commands, root passwords, balancing/MOSFET actions or automatic authentication fallback.
- [ ] Verify signed current, voltage/current scales, Kelvin-to-Celsius conversion, capacity/SOC validity, alarm bits and exactly 14 cell values against pinned reference fixtures. Explicitly reject impossible or inconsistent counts; expose raw alarm bits rather than guessing unnamed meanings.
- [ ] Query 03 then 04 serially every 5 s; 05 once per connection. Only one outstanding query, 2 s reply deadline. Sample freshness advances only for a complete basic+cell cycle of the same connection generation; optional ID failure must not block measurements.
- [ ] Discover FF00/FF01/FF02 only on configured peer; subscribe, check properties, then read. Missing service/auth requirement is a reported unsupported state, never permission to change the BMS.
- [ ] Run `pio test -e native -f test_jbd_parser`; use fake transport for retry/fragment/disconnect tests. Record fixtures and notices. No BLE hardware session yet.

## Task 6: Collector runtime, ConfigManager UI, MQTT and diagnostics

**Owner:** lead. **Depends on:** reviewed Tasks 2-5.
**Modify collector:** src/main.cpp, src/settings.h, platformio.ini,
src/secret/secrets.example.h, readme.md and project.info.md (status only when true).
**Create:** src/telemetry/TelemetryPublisher.h/.cpp, CollectorSettings.h/.cpp,
MqttActivityLed.h/.cpp, diagnostics/HealthStatus.h/.cpp;
test/test_telemetry_contract/test_main.cpp; docs/MQTT.md and sanitized HA YAML.

- [ ] Replace inert setup/loop using verified ConfigManager initialization order. Register owned settings/modules before loadAll; start WiFi/UI and enabled BLE. Register the observer once; call setupOTA only after IP readiness through the existing WiFi connection hook, preserving it across reconnects. Then call handleClient, MQTT loop, BLE loop, adapters and publisher without delay(1000).
- [ ] Apply the MQTT schema and budgets in the spec: payload <=1024 bytes, topic <=256 bytes, configured MQTT packet buffer 1536 bytes, LWT and per-source availability. Reject oversize serialization without partial publish. Configure client timeouts so broker loss does not monopolize the loop; verify actual MQTT module reconnect behavior before reuse.
- [ ] Add deterministic contract tests: current-sign/unit mapping, missing/sentinel values null, retained availability vs non-retained samples, stale at 30 s, millis wrap, generation invalidation, reconnect sends only fresh latest data and wrong topic/serialization size cannot blink LED. Use separate sensor availability/expiry in HA example; no HA connector writes.
- [ ] Register device keys using password fields; validate format before replacing saved values. Test masked JSON, export omission, masked-value round-trip, no key in runtime/MQTT/logs, and configuration save/load after reboot. Authentication settings use existing CM mechanisms, not a parallel password system.
- [ ] Publish diagnostics at a bounded rate (60 s): firmware/build ID, reset reason, boot_id, uptime, free/min/largest heap, worker stack high-water bytes, queue drops and adapter ages. Secrets and device identities excluded from public logs by default.
- [ ] Connect OTA observer: atomically request pause; invalidate measurement generation, suppress stale data; on failure from the matching active OTA session resume only enabled adapters. Rejected probes or stale terminal callbacks never release a different session. Success remains paused until reboot. Ensure recovery mode can disable BLE while preserving WiFi/UI/OTA.
- [ ] Implement nonblocking 30 ms LED pulse. Verify board pin/polarity on USB before enabling default. Preserve no external IO requirement.
- [ ] Run only affected native tests, ConfigManager regression cases and `pio run -e usb -e ota`. Generate compiledb only for changed compiler flags/dependencies, then refresh affected Atlas/Serena indexes. No upload.

## Task 7: Pre-hardware release-readiness gate

**Owner:** lead; independent focused review of shared lifecycle and parser boundaries.
**Create:** docs/acceptance.md checklist; evidence templates under ignored .local/acceptance/.

- [ ] Review changes against scope, licenses and outgoing command allowlist. Run secret scanning on changed files and any intended Git objects; exclude real captures/binaries/settings and local absolute dependency overrides.
- [ ] Verify firmware file length against BOTH app-slot sizes and retain at least 10% headroom for the candidate. Check actual partition binary, not board maximum_size alone. If exceeded, stop and propose a separate reviewed partition adjustment retaining two slots; do not switch to huge_app/no-OTA. First layout change requires USB provisioning.
- [ ] Record exact sources, resolved libraries, flags, binary hash, ELF, partition table and test output locally. Different uncommitted candidates need distinct build IDs and binary hashes even without commits. Do not upload this evidence to GitHub automatically.
- [ ] Verify recovery plan: physical USB access, known-good local image, selected free static IP in secrets.h AND platformio.local.ini, matching unique hostname/client ID and tested OTA password. Do not reuse the limiter's IP.
- [ ] Stop for explicit Task 8 authorization with the exact board/USB port, commands, test duration, dedicated MQTT test root and permitted failure simulations. Missing BMS/Victron key is acceptable for local missing-device tests but recorded as blocking Task 9.

## Task 8: Local physical-board stability and OTA acceptance

**Owner:** lead/owner. **Depends on:** Task 7 plus explicit hardware authorization.
**Commands after authorization:** `pio run -e usb -t upload --upload-port <verified-port>`;
`pio device monitor --port <verified-port> --baud 115200 --filter esp32_exception_decoder`;
`pio run -e ota -t upload` using the verified ignored local address/auth settings.
Use actual authorized values at execution; never print or commit credentials.

- [ ] USB-install the first full OTA-capable candidate. Confirm actual chip, flash, LED polarity, IP, WiFi, settings, MQTT test-root output and recovery controls. Preserve continuous serial logs and matching ELF locally.
- [ ] Run 24 h locally: real WiFi/MQTT/UI/OTA servicing, missing BLE devices, bounded synthetic decoder/transport events in an explicitly separate test profile, and realistic scan/connect timeouts. This is not acceptance of actual BMS/Victron compatibility. Production builds exclude injection hooks/topics.
- [ ] Exercise 20 controlled broker reconnects and 20 permitted WiFi disconnect/reconnect cycles without changing router-wide settings. Exercise 100 fake GATT disconnect/reconnect sequences for ownership tests; record that these are simulated, not RF evidence.
- [ ] Pass criteria: zero unexpected reset/panic/WDT/assert/stack-overflow/brownout, zero corrupted/unauthorized outbound packets, no unbounded queue growth, no stale-as-valid publication. Track a 15-minute warm-up median and final 15-minute median free heap; loss <=5%, no sustained downward trend. Minimum free heap >=50000 bytes, largest free block >=32000 bytes, worker stack high-water >=1024 bytes are proposed release gates; failure requires explanation/fix/review, not silently lowering thresholds.
- [ ] Onboard LED pulses only on successful telemetry enqueue. Confirm ordinary UI use remains responsive, normal loop service gap <=250 ms outside the measured upload interval, and eventual MQTT recovery <=60 s after network restoration. A failed optional adapter must not reboot the system.
- [ ] Perform local firmware A -> B -> A via OTA (distinct build IDs). Test ArduinoOTA and HTTP route if both are enabled. Also test wrong-password rejection and one client-side transfer interruption without cutting device power. After failure, old firmware remains reachable and BLE resumes within 60 s (or after documented stale timeout up to 120 s); after success, verify new build ID, settings persistence and another OTA.
- [ ] Do not interpret OTA protocol success as application health. Wait at least 15 minutes and confirm diagnostics/UI/telemetry after every successful update. No automatic rollback claim; retain USB recovery.
- [ ] If Guru Meditation occurs, retain complete boot/backtrace log and EXACT ELF/hash, decode first failing frame, create a bounded fix task and rerun affected test plus stability gate. Do not suppress panic/watchdog/brownout checks to obtain a pass.

**Gate:** signed evidence checklist with elapsed times, counts, lowest resources,
expected reboot reasons/build IDs and outstanding device limitations. Owner
approves moving this exact unit to the shed; do not declare real-device success.

## Task 9: Shed commissioning and repeatable remote updates

**Owner:** lead with owner present for physical access; explicit stage authorization.
**Prerequisites:** Task 8 pass, verified WiFi coverage/unique IP, BMS address AND
address type, Victron address and 32-hex Instant Readout key in ignored settings.
Obtain Victron data from VictronConnect product information/Instant Readout
details. PIN is not the key; PIN changes may invalidate the key. User performs
any needed charger configuration. Close phone apps while testing BMS connection.

- [ ] Discover/verify only the authorized peer identities and JBD GATT profile. Do not infer model compatibility from the app name. Unsupported/auth-required behavior stops that adapter and retains OTA access.
- [ ] Observe actual status/cell replies and charger advertisements; compare concurrent app observations, allowing time differences. Verify 14 cells, current sign, charge state and sentinel behavior. Do not calibrate SOC or change charging/protection settings.
- [ ] Validate both sources independently: fresh valid samples within 30 s after initial connection, documented measured update rates, source stale after 30 s disappearance and correct recovery after reappearance. Use controlled radio/phone contention only; never disconnect battery/PV wiring for a test.
- [ ] Run a 24 h real-device soak with WiFi/MQTT and normal house operation. Apply Task 8 reset/resource criteria; record RSSI, reconnects, queue drops, stale periods and any phone interference. Lack of sun may limit charger-value comparison; mark unobserved states explicitly.
- [ ] Perform two remote OTA cycles from the intended workstation/network path while actual Bluetooth devices are present. Verify upload pause, reboot/build ID, retained configuration, both telemetry sources and continued OTA reachability after each cycle. Keep physical USB recovery available until accepted.
- [ ] If Bluetooth degrades OTA or normal networking, first investigate scan duty, callback latency, queue pressure and source of reconnects. Do not add a second ESP32 or change backend silently; report an evidence-backed architecture decision if the one-device target fails.
- [ ] Present acceptance report: source/build IDs, local and shed durations, protocol observations, successful/failed OTA paths, diagnostics minima and remaining limits. Stop for owner acceptance. No commit, push, dependency release or wider rollout without a new explicit instruction.

## Completion and honest status

Planning completion means these two reviewed documents exist, not that firmware
or hardware tests passed. Implementation completion requires Tasks 1-7; local
OTA readiness requires Task 8; actual BMS/Victron and shed OTA acceptance require
Task 9. No stage substitutes synthetic fixtures for real-device evidence.
