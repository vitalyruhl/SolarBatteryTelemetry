# Required BMS telemetry and MOS controls implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Complete Required BMS telemetry and provide two explicitly enabled, verified MOS permission commands through ESP and MQTT, with matching Home Assistant presentation.

**Architecture:** Keep BLE transport and its worker in ConfigurationsManager. Extend the existing SBT decoder, add a portable command state machine, and connect it to bounded application mailboxes. Publish actual measurements and command outcomes separately so numeric deadbands never suppress status changes.

**Tech Stack:** ESP32 Arduino, PlatformIO, ConfigurationsManager 4.6.0, JBD BLE, PubSubClient, ArduinoJson, portable C++17 host tests, Home Assistant YAML.

**Spec:** `docs/superpowers/specs/2026-10-16-required-bms-telemetry-controls.md` (owner approved 2026-10-16).

## Global constraints

- Work on `feature/bms-required-telemetry-controls`; no automatic commit, push or merge.
- Preserve existing channel indices, topics, units and the owner-requested firmware version freeze at `0.3.0`.
- Preserve numeric publication thresholds: `0.1` per real unit except `1 W`. HA voltage display remains two decimal places.
- Add only charge/discharge MOS permission writes, disabled by default. Do not write Victron settings.
- No shared CM source changes; the independent SIL upgrade owns COM7 and its deployment.
- PV voltage/current remain unsupported until VE.Direct hardware exists. Never publish synthetic zero for unsupported or stale data.
- Keep WiFi, credentials, NVS, partitions and production source configuration intact.
- MOS readback confirms permission, not electrical isolation. Live switching qualification is separate from software rollout.

## Review focus

- Invalid or missing NTC samples must not produce a plausible minimum temperature (Task 1).
- An unrelated late BLE response must not complete a newer write request (Task 2).
- A broker reconnect or retained request must never replay previous command intent (Task 3).
- A failed MQTT publish must not advance the status cache or hide a command result (Task 3).
- An HA control must show observed MOS state and remain unavailable while control is disabled or feedback is stale (Task 4).

## Task 1: Complete decoded telemetry and protection names

**Files:** Modify `src/telemetry/TelemetryChannels.h`, `src/bms/JbdProtocol.h`, `src/bms/BmsReadout.h`, `test/host/jbd_protocol_test.cpp`, `test/host/telemetry_contract_test.cpp`.

**Interfaces:** Append channels `bms/temperature_min_c` (real), `bms/cell_count` (unsigned), `bms/temperature_sensor_count` (unsigned), and `bms/protection` (formatted text from the raw bitfield). Keep the existing 25 indices unchanged. Add `FieldType::Protection` and a bounded `formatProtection(uint16_t bits, char* out, std::size_t capacity)` helper shared by MQTT/runtime formatting. Expose a coherent sample sequence/timestamp from the existing `Cycle` snapshot.

- [x] Add failing decoder assertions for zero NTCs, multiple valid sensors, a negative valid temperature and an out-of-range sensor; minimum and maximum use the same validated readings, zero valid readings yields invalid temperatures. Verify counts match the validated basic/cell pair.
- [x] Add formatting assertions for no protections, simultaneous known protections and bits 14/15 preserved as unknown; raw `bms/alarm_bits` remains unchanged.
- [x] Run `tools/test/run-jbd-host-tests.ps1` and the telemetry contract test; confirm failures identify the new behavior.
- [x] Implement the appended fields and bounded protection names. Update runtime serialization for the new text type. Preserve existing frame validation, pairing and freshness.
- [x] Rerun the focused tests and verify the channel count still fits the publisher bitset.

## Task 2: Serialize and verify the two MOS commands

**Files:** Create `src/bms/BmsCommandPolicy.h`, `test/host/bms_command_policy_test.cpp`, `tools/test/run-bms-command-tests.ps1`; modify `src/bms/JbdProtocol.h`, `src/bms/BmsReadout.h` and `test/host/jbd_protocol_test.cpp`.

**Interfaces:** `MosTarget { Charge, Discharge }`; `MosRequest` contains target, absolute desired state, bounded request ID and an application-assigned monotonic deadline. `BmsCommandPolicy` owns at most one pending request and exposes `submit`, `cancel`, `onBasic`, `onWriteResponse`, `onReadback` and `tick` transitions. `BmsReadout` remains the sole owner of BLE calls. A bounded callback mailbox passes requests into its loop. Results expose request ID, phase, reason and both observed MOS flags.

- [x] Add failing tests for both targets and both desired states, preserving the opposite observed MOS bit. Assert the exact `DD 5A E1 02 ... checksum 77` frame and the inverse disable-bit encoding.
- [x] Add tests for pre-read failure, nonzero write status, stale/wrong-register/wrong-query responses, write admission failure, missing acknowledgement, mismatching readback, disconnect, OTA cancellation, expiry and concurrent requests. No case may implicitly retry a write or automatically enable a MOS.
- [x] Implement a bounded write-frame encoder and explicit write response status parsing without relaxing the existing read checks.
- [x] Implement the transaction: accepted request -> fresh basic read -> write acknowledgement -> fresh paired basic/cell readback -> verified or failed result. The same serial query slot serves ordinary telemetry and commands.
- [x] Require control enable, fresh source and a connected worker at admission and again before the write. Cancel intent on configuration changes, source loss, OTA or expiry; generation and query IDs continue to reject unrelated responses.
- [x] Run the command and JBD host tests; verify ordinary read-only cycles still work when controls are disabled.

## Task 3: Connect bounded MQTT requests and independent status publication

**Files:** Create `src/telemetry/BmsCommandEnvelope.h`; modify `src/telemetry/MqttService.h`, `src/telemetry/TelemetryPublisher.h`, `src/telemetry/TelemetryChannels.h`, `src/main.cpp`, and the affected host tests.

**Interfaces:** Subscribe only to the two existing command suffixes under the current MQTT root. JSON request fields are `state` (boolean), `request_id` (1-32 ASCII letters/digits/hyphen/underscore), `session` (current random 128-bit token as 32 hex characters) and `expires_at` (UTC epoch seconds). Maximum envelope size is 256 bytes; expiry must be strictly future and no more than 30 seconds ahead. Generate a new session at each MQTT connection/root change. Reject duplicate IDs within that session using a bounded cache; once full, rotate the session instead of evicting IDs that could replay. Publish session/readiness via `bms/control`, outcomes via `bms/control/result`, and source health via `bms/health` and `victron/health`.

- [x] Add failing envelope tests for missing/wrong types, length limits, unknown target, old session, duplicate ID, expired/far-future expiry and unavailable wall clock. Reject without producing a BLE command.
- [x] Add publisher tests for status changes below numeric deadbands, disconnect/reconnect, root retirement and failed publish retry. Source health contains reason, availability, sample sequence and age; publish on transitions and at a bounded 5-second cadence.
- [x] Implement strict envelope parsing and replay policy. Queue only validated absolute requests; callbacks never call BLE. Disconnect/root changes cancel queued remote intent and rotate the token.
- [x] Extend capability metadata to distinguish supported commands, installation enablement and current readiness. Keep the compatibility `bms/control/availability` topic accurate. Command results and health bypass numeric deadbands.
- [x] Wire application callbacks in `src/main.cpp`, including OTA cancellation. Verify no generic register write or Victron command subscription exists.
- [x] Run affected MQTT lifecycle, mailbox, telemetry and command-policy host tests.

## Task 4: ESP and Home Assistant presentation

**Files:** Modify `src/bms/BmsReadout.h`, `src/telemetry/MqttService.h`, `docs/BMS.md`, `docs/MQTT.md`, `docs/telemetry-value-inventory.md`, `readme.md`; add `docs/home-assistant-bms-controls.yaml` as a reusable example. Update the existing HA configuration/card after preserving its prior contents.

**Interfaces:** Persist only the explicit `BMS control enabled` configuration flag, default false; never persist desired MOS state. Runtime controls use separate absolute enable/disable actions and the same application mailbox. Display observed state plus pending/result/reason independently. HA non-optimistic MQTT switches use existing MOS feedback topics, current control availability and templated envelopes with session, unique request ID and 30-second expiry.

- [x] Verify existing CM runtime action routes and document their local-network access boundary. Keep installation control disabled during rollout; do not add a separate authentication system or change shared CM in this task.
- [x] Register the additional measurements, protection text, source health and command feedback in the ESP runtime. Add four explicitly labelled absolute actions, with unavailable/disabled feedback instead of optimistic state changes.
- [x] Extend HA without duplicating remaining capacity, charging, discharging or spread sensors. Add minimum NTC temperature, counts, protection descriptions, source health, actual MOS states and the two unavailable-until-enabled controls.
- [x] Preserve two-decimal voltage configuration and existing Victron fields; mark PV voltage/current pending hardware. Real rendered values require normal-location reception.
- [x] Validate and reload HA configuration; verify unavailable sources, visible diagnostic reasons and locked switches. Malformed/expired/disabled commands are rejected without BLE writes.
- [x] Verify real rendered measurements and observed MOS feedback at the normal installation; switches remain locked.
- [x] Update the existing inventory with implementation status and SIL-required classification; document protocol limits, message schema and installation gate.

## Task 5: Review, build and authorized rollout

**Files:** Existing implementation/tests/docs above; add acceptance evidence under `docs/acceptance/2026-10-16-required-bms-telemetry-controls.md`.

- [x] Review the final diff and run `git diff --check`. Request one independent review of protocol, cancellation/replay behavior, callback ownership and actual-state reporting; resolve findings.
- [x] Run affected host tests and `pio run -e usb`. Record exact CM revision, build size, firmware hash and available partition headroom. Do not change partitions to make it fit.
- [x] Attempt authorized OTA, verify the failed return, then recover through owner-provided COM11 with preserved NVS/partitions and verified flash digest. See acceptance evidence; do not classify the initial OTA upload as deployment success.
- [x] Verify three minutes of increasing uptime, HTTP 18/18, independent 60-second MQTT refresh, five-second health and locked-command rejection on COM11. No Bluetooth reception exists at the PC.
- [x] Verify repeated real BMS/Victron samples, unchanged-minute replay and rendered HA values at the normal installation. Record 17/18 HTTP and a recovered three-second BMS interruption separately; do not claim uninterrupted network stability.
- [x] Record verified software/deployment results separately from outstanding live MOS switching qualification. No claim that readback or host tests prove current interruption.
- [x] Leave changes on the feature branch for owner review. No automatic final commit or publication.

## Owner-approved extension during implementation

Add a fixed 60-second full MQTT refresh so Home Assistant recovers unchanged values after its own restart without restarting SBT. Preserve numeric deadbands between refreshes, source validity, bounded sends, retry and OTA pause. Focused host coverage includes timing, normal intervals longer than a minute, sub-deadband latest values, unchanged boolean/alarm values, null transitions, failed admission, pause and uptime wrap.
