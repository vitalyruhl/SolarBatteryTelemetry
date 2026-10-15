# MQTT Telemetry Preparation Implementation Plan

> **For agentic workers:** Use superpowers:subagent-driven-development for the scoped implementation and independent review. No automatic commits.

**Goal:** Add working MQTT connectivity, an editable base topic and all requested fixed telemetry/status channel definitions to the current collector, measure the actual image, retain dual-slot OTA and USB-flash COM11 only after the fitting candidate passes review and tests.

**Architecture:** Reuse the existing ConfigManager MQTT module and consumer-owned PubSubClient. A compact, immutable channel catalogue supplies topic suffixes, English labels, types and units to the publisher and runtime view. An application-loop coordinator owns publication/root changes; no device decoder or BMS write is introduced.

**Tech Stack:** Existing espressif32 7.0.1 / Arduino 2.0.17 / nodemcu-32s, reviewed local CM 4.5.0, NimBLE 2.5.1, PubSubClient 2.8, consumer LTO.

**Spec:** Existing ../specs/2026-10-15-ble-integration-design.md, narrowed and amended by the owner's 2026-10-15 request and explicit follow-up: MQTT/channel preparation only, actual device decoding later; default root SolarBatteryTelemetry; BMS control channels reserved but all device access remains read-only. This plan supersedes the earlier aggregate-only MQTT proposal for this preparation stage.

## Global constraints

- Git workflow precedes implementation: CM feature pushed only; SBT integrated/pushed to main and proven merged branches cleaned; new feature/mqtt-telemetry from synchronized main. This is complete at SBT 47369d56bf653bb5d0bd7a92fbbb96f9e38348fe.
- Default root SolarBatteryTelemetry (21 bytes), editable through the existing MQTTBaseTopic setting. Fixed inner topic names are not individually configurable.
- Actual addresses, credentials and OTA auth remain in ignored files. Preserve local CM binding and explicit OTA auth correction. Do not log, commit or upload secrets/evidence artifacts.
- All channel definitions, English labels, state strings and reserved command names must be compiled AND referenced by the production runtime so the flash measurement includes the preparation. Avoid per-channel heap-allocated topic/setting objects and duplicate strings.
- No fabricated sensor values: unknown fields publish unavailable, never zero/false. Charge/discharge activity is distinct from MOS enable status. No command is subscribed to or written to a BMS in this stage.
- No CM implementation changes; preserve pushed CM and pre-existing Serena modification. No new implementation commits, pushes, device decoding, HA changes or live BMS interactions.
- Firmware 0.3.0: compatible MQTT preparation feature. Update current documentation/version references, retain historical plan provenance.
- Keep OTA enabled. CM_ENABLE_LOGGING=0 and CM_ENABLE_VERBOSE_LOGGING=0 already apply. No huge_app/no-OTA workaround. Use actual .bin lengths and actual partition binary; require >=10% reserve in EACH app slot for the final candidate.

## Channel contract

Scalar telemetry under <base>/<source>/<field>, source availability under <base>/bms/availability and <base>/victron/availability. Connection status <base>/status is retained online/offline with LWT. Telemetry payloads are non-retained and sent at most every 5 seconds; publish source availability before telemetry after reconnect. A bounded publisher retries failed admission without marking an incomplete snapshot delivered. No historical backlog.

| Source | Fields and types |
| --- | --- |
| bms | charging bool; discharging bool; state_of_charge_pct real; voltage_v real; current_a real; power_w real; remaining_capacity_ah real; full_capacity_ah real; charge_enabled bool; discharge_enabled bool; temperature_max_c real; cell_voltage_min_v real; cell_voltage_max_v real; cell_voltage_delta_v real; alarm_bits unsigned |
| victron | solar_power_w real; pv_voltage_v real; pv_current_a real; battery_voltage_v real; charge_current_a real; battery_power_w real; load_current_a real; charge_state enum string; error_code unsigned; yield_today_wh real |

The scalar key charge_current_a means charger output to the battery, not net battery current. PV voltage/current are reserved unavailable/unsupported because Instant Readout Solar does not supply them. battery_power_w is reserved for later valid voltage*charger-current derivation, not computed from missing values. SoC is the BMS-reported percentage; any future fallback remaining/full calculation needs valid nonzero full capacity and explicit provenance. No fake calculation in this stage.

Use English labels with units; the charge state vocabulary includes off, fault, bulk, absorption, float, storage, equalization, other and unavailable (map verified protocol codes only in the later adapter). Bulk is the main charging stage, not an alarm. Add <base>/bms/control/availability=disabled. Reserve <base>/bms/charge_enabled/set and .../discharge_enabled/set in compiled runtime capability metadata; do not subscribe, echo commands as actual state, or modify hardware. One small capabilities publication exposes schema version, read_only=true, commands_enabled=false, the two reserved command paths and unavailable/unsupported scope. Do not invent Home Assistant command discovery.

Root and buffers: bounded root up to 63 ASCII bytes, reject empty, wildcard +/#, control characters and leading/trailing slash; permit nested nonempty path segments. Entire topic <=128 bytes including separator/suffix, payload <=1024 bytes, MQTT packet buffer 1536. Invalid UI values must not generate malformed publishes or lose the last valid active root; expose rejection visibly. CM currently has no general string validator: do not pretend the backend enforces an input limit. Protect consumer construction/serialization regardless of UI length. Further CM validation is a separate requested audit.

## Review focus

1. Root rename after connection must retire old availability/status and reconnect with new LWT; no old-root publishes/commands after transition.
2. Reconnect must republish complete unavailable/current state even when values did not change; failed sends must retry without an unbounded queue.
3. Missing device data must never appear as valid zero/false; PV unsupported and charger current semantics must remain explicit.
4. Oversize/wildcard roots and near-limit topic/payload strings must fail visibly, not truncate. Retained or ordinary incoming control payloads must have no actuator effect.
5. MQTT outage and OTA overlap: keep OTA servicing priority, document bounded but synchronous TCP/MQTT connection limitations; do not claim nonblocking behavior or fabricate hardware latency results.

## Task 1: Channel catalogue, publication policy and focused tests

**Create:** src/telemetry/TelemetryChannels.h, src/telemetry/TelemetryPublisher.h (additional small .cpp only if useful); test/host/telemetry_contract_test.cpp.
**Modify:** tools/test/run-smoke-host-tests.ps1 to include the actual portable production policy tests.
**Interfaces:** expose Channel metadata with Source, FieldType, suffix, label, unit/support; bounded buildTopic(root,suffix,buffer,capacity); publication coordinator with fakeable transport accepting publish(topic,payload,retain), connected/reconfigure events and tick(now). Avoid Arduino dependencies in the tested policy. Values include explicit validity. No protocol decoder.

- [ ] Write failing tests for complete unique catalogue/type/unit coverage, unknown vs valid bool/zero, root/topic bounds, finite real numbers, reconnect retry and old/new root transition ordering, millis wrap, no control subscription/write and compiled capability references.
- [ ] Implement compact shared definitions and bounded latest-state publisher. Ensure channel/state strings are used by normal builds, not only tests. Runtime getters must not retain temporary buffers.
- [ ] Run focused host tests through the existing verified clang toolchain; record failure then success. No test mirrors of trivial constants alone.

## Task 2: Integrate CM MQTT, defaults, UI and runtime

**Modify:** src/main.cpp, src/settings.h, platformio.ini; ignored local override only to retain dependencies/auth; add small consumer coordinator header if needed. Update readme.md, project.info.md and create docs/MQTT.md.
**Consumes:** Task1 catalogue and coordinator; existing CM MQTTManager public attach/begin/loop/publishRaw or publish/disconnect/setLastWill/onMQTTConnect.

- [ ] Register standard MQTT settings before loadAll. Seed local broker/user/password/base defaults before load so persisted user edits, including an intentional empty password, win. Use unique client identity, not the limiter's client ID. Keep default root SolarBatteryTelemetry.
- [ ] Use the same baseline groups as SIL, but fixed telemetry leaf topics rather than one editable setting per field. Expose concise runtime field/status view using the catalogue, including actual unavailable values and preparation scope.
- [ ] Install consumer change callbacks after final attach for a loop handoff. Do not invoke PubSubClient from an AsyncWeb callback. Root transition marks old base/source status unavailable/offline while connected, disconnects, sets new LWT and reconnects; onMQTTConnect requests full publication. Preserve existing system metadata conventions.
- [ ] Ensure local lib_deps overrides include PubSubClient 2.8 for BOTH environments rather than dropping the new tracked dependency. Keep settings/UI/WebUI source coherent and unchanged CM source hashes.
- [ ] Add -DMQTT_SOCKET_TIMEOUT=1 globally for PubSubClient constructor default and use a bounded retry cadence. CM's private WiFiClient still permits ~3s TCP timeout and hostname DNS delay; explicitly document this inherited limitation. Skip new MQTT attempts/publication during OTA Starting/success latch; resume on failure through the application loop. No uncontrolled extra client task.
- [ ] Run focused host/precompile tests, inspect diff/secret exclusion and version mirrors. No workstation broker writes; the final authorized firmware may connect/publish its preparation channels.

## Task 3: Build, size comparison and partition gate

**Modify if needed:** partitions/dual-ota-1792k.csv and platformio.ini; docs/MQTT.md acceptance section.

- [ ] Build full USB/OTA candidate with all catalogue strings and MQTT runtime enabled. Keep existing BLE dependency for a representative combined image. Preserve logs and exact ELF/bin/hash/source evidence locally.
- [ ] Explain the 488KB scaffold versus 1.3MB initialized candidate using existing evidence and available ELF symbols. Do not attribute the entire delta to NimBLE or claim CM logging is still enabled.
- [ ] If default slots fail fit/headroom, verify filesystem consumers before reducing SPIFFS. Preferred 4MB candidate: NVS 0x9000/0x5000, otadata 0xe000/0x2000, app0 0x10000/0x1c0000, app1 0x1d0000/0x1c0000, SPIFFS 0x390000/0x60000, coredump 0x3f0000/0x10000. Retain NVS and coredump ranges, two aligned slots and 4MB boundary. This requires USB partition provisioning, never OTA-only migration.
- [ ] Rebuild affected environments after an actual partition change, parse final partition binary and compare actual BIN lengths to BOTH slots with >=10% reserve. Review known NimBLE LTO warning against final ELF/callers. Stop and report if a safe candidate does not fit; do not drop OTA or silently disable requested channels.
- [ ] Independent scoped review of catalogue correctness, root changes, validity, bounds, OTA ownership and partition arithmetic. Fix concrete blockers and rerun only affected checks.

## Task 4: Authorized COM11 USB installation

- [ ] Controller verifies final exact source/firmware/partition hashes and chip/flash on COM11. Warn about the older .124 unit if still powered. No automatic full-chip erase.
- [ ] Flash the verified full bootstrap bundle and application using exact built offsets; do not use PlatformIO nobuild upload alone because it omits bootstrap images on this platform.
- [ ] Confirm all four flash hashes and final reset. Report firmware size, slots/reserve, prepared topics and explicit missing-device limits. No additional commits/push, uncontrolled soak or BMS operations. Serial monitoring requires its separate existing authorization rule; do not assume flashing authorizes a broad acceptance session.

## Implementation ruling: isolate editable settings from the network owner

Live source inspection shows MQTTManager reads its Config fields during synchronous
client operations, while standard UI setters assign those fields before callbacks.
Callback handoff alone therefore cannot protect that boundary. Use a separate
public MQTTManager::Settings instance for the existing UI/persistence keys and
standard groups, then copy bounded validated desired values through a synchronized
latest-value mailbox into the unregistered manager settings owned only by loop().
Load the initial UI snapshot before starting the web server. No duplicate stored
keys, custom password mechanism, private-member access or CM change is permitted.
Do not call attach() on the network-owner settings if it would register them.
This supersedes the attach/callback wording in Task 2 only; preserve the requested
UI behavior, connection lifecycle and all publication/OTA/size gates. Use existing
core system diagnostics if standalone CM MQTT SystemInfo binding is not publicly
available. Test configuration revision replacement across pause and reconfiguration.
