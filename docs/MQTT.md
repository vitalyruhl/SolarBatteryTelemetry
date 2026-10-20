# MQTT telemetry and guarded BMS commands

The current working firmware adds passive SmartSolar Instant Readout to the 24-channel MQTT contract. Seven direct Victron fields can contain decoded readings; BMS is supplied by the JBD readout adapter. Unsupported PV voltage/current and deferred battery-power placeholders are omitted. Charge/discharge commands are implemented behind a default-disabled installation gate. See [Victron readout](Victron.md) for configuration, field limits and hardware acceptance.

## Settings and ownership

The application persistent MQTT keys are `SBTMQTTEnable`, `SBTMQTTHost`, `SBTMQTTPort`, `SBTMQTTUser`, `SBTMQTTPass`, `SBTMQTTClientId`, `SBTMQTTBaseTopic`, `SBTMQTTPubPer`, and `SBTMQTTListenMs`, under MQTT / MQTT Settings. Each UI setting is constructed once with final defaults. Legacy canonical MQTT keys are copied only when the new destination key is absent; existing new values win and legacy keys are retained. No Config object is reconstructed or copied. Defaults are established before registration/loadAll; existing persisted edits, including an intentionally empty password, win. With real ignored secrets the default is enabled and uses the local broker credentials; placeholder-only builds default disabled. The default base is `SolarBatteryTelemetry`. A deterministic client ID contains all 48 MAC bits, independent of the limiter's identity.

The consumer registers application-owned `MqttUiSettings` for UI/persistence. The public MQTT singleton's canonical settings belong only to the application loop and are not registered with the configuration manager. Separate key namespaces avoid constructor-time collisions under CM settings registration semantics and remain compatible with the local CM comparison builds. The consumer intentionally does not call `MQTTManager::attach`, which would expose those live settings and install client-mutating callbacks. UI callbacks copy only bounded values/revisions into an owned mutex-protected mailbox; no client operation occurs there. After loadAll, initial values enter that mailbox before web-server startup because CM's persisted load does not invoke change callbacks. The application loop applies the latest snapshot, including changes received during OTA. Runtime readers get a synchronized copy, never UI Strings or live client state. This separation does not claim to fix all inherited CM settings-storage concurrency.

Public standalone `begin`, `loop`, `settings`, `setServer`, `setCredentials`, `setClientId`, `setLastWill` and `publishRaw` are used. The existing module still publishes its normal `<base>/System-Info/ESP` and `/WiFi` metadata. The custom retained LWT is `<base>/status=offline`; online is published on connection/root change and on the full 60-second refresh, with local-admission retry. The existing CM connection callback starts this replay; `publishRaw` does not provide automatic change detection. CM System-Info publication keeps its existing 60-second interval. Publish success means local QoS0 admission, not broker acknowledgment.

## Home Assistant display precision

Display precision is independent of the MQTT send threshold. Configure `suggested_display_precision: 2` on the existing MQTT voltage sensors (BMS pack voltage, minimum/maximum cell voltage, cell spread and Victron battery voltage). If an entity already has a user-selected display precision, set that entity preference to two decimals as well. Keep values numeric; do not turn payloads into formatted strings or round them before deadband comparison.

The collector does not publish Home Assistant discovery configuration. Update the existing HA sensor configuration instead of adding duplicate entities. See the [official MQTT sensor documentation](https://www.home-assistant.io/integrations/sensor.mqtt/#suggested_display_precision). On 2026-10-16, the five existing voltage entities were changed through the HA entity settings to two decimal places. The live dashboard then showed BMS pack 48.52 V, cell minimum 3.43 V, maximum 3.48 V, spread 0.04 V, and Victron battery 49.01 V. Changing only HA display precision restored the decimals, confirming a presentation issue. These are historical observations, not synchronized calibration readings.

## Fixed channels

All 24 descriptors, labels, units, type/support names, state vocabulary and reserved command names are referenced by the production runtime and serializer. There are no per-field editable topics or allocated setting objects. Telemetry is non-retained. The publish interval limits ordinary value updates to at most once per 5 seconds (or the configured interval when longer); it is not a heartbeat. Each coherent sample compares fields against the last successful local admission. Real-valued measurements publish when their absolute change reaches 0.1 in the channel unit, except W channels which require 1 W. This applies to V (including cell minimum, maximum and spread), A, Ah, C, %, and Wh; future real-valued kW channels would use 0.1 kW. Small changes accumulate against the last sent value, not the previous sample. Boolean, enum, alarm and error changes retain exact change detection. Initial values, validity changes, reconnect replay and source restoration bypass the numeric deadband. Payloads retain their existing significant-digit precision; this policy does not round measurements. Between full refreshes, unchanged samples and absent sources emit no telemetry messages. Reconnect or root changes replay metadata and all currently valid fields; source availability changes publish availability/capabilities promptly, and restoration replays that source's valid fields even when unchanged from its previous connection. At most four normal publications are attempted per policy tick, with a finite cursor and a 250 ms admission retry. A pending old-root retirement can add three sends before that normal budget; a root transition or configuration disconnect retires at most two advertised roots (six sends). CM's independent automatic online/System-Info messages are outside this preparation budget. A failed item does not advance its field cache or mark the snapshot delivered. Already accepted fields keep their cache across partial failures; retries wait at least 250 ms, including when a source transition replaces the pending snapshot. The fixed cache holds one `Value` per channel and a bitmask, using scalar serialization for equality and validity transitions, plus numeric deadbands for valid real measurements. There is no historical sample backlog.

| Source | Leaf | Type | Unit / meaning |
| --- | --- | --- | --- |
| bms | charging | bool | Actual charging activity from positive current |
| bms | discharging | bool | Actual discharging activity from negative current |
| bms | state_of_charge_pct | real | BMS-reported % |
| bms | voltage_v | real | V |
| bms | current_a | real | A, net battery current |
| bms | power_w | real | W |
| bms | remaining_capacity_ah | real | Ah |
| bms | full_capacity_ah | real | Ah |
| bms | charge_enabled | bool | Observed charge MOS permission, not activity |
| bms | discharge_enabled | bool | Observed discharge MOS permission, not activity |
| bms | temperature_max_c | real | C |
| bms | cell_voltage_min_v | real | V |
| bms | cell_voltage_max_v | real | V |
| bms | cell_voltage_delta_v | real | V |
| bms | alarm_bits | unsigned | Raw BMS protection bitfield |
| victron | solar_power_w | real | W |
| victron | battery_voltage_v | real | V |
| victron | charge_current_a | real | A, charger output to battery; not net battery current |
| victron | load_current_a | real | A |
| victron | charge_state | enum string | off, fault, bulk, absorption, float, storage, equalization, other, unavailable |
| victron | error_code | unsigned | Unknown |
| victron | yield_today_wh | real | Wh |
| bms | temperature_min_c | real | C; minimum of validated NTCs |
| bms | protection | text | Named active protection bits, or none |

Topics are `<base>/<source>/<leaf>`. Both `<base>/bms/availability` and `<base>/victron/availability` publish their retained `available`/`unavailable` state before scalar data. BMS availability follows complete JBD basic/cell cycles; Victron availability follows the readout freshness check. Availability and capabilities are sent once per connection/root and on source availability changes. `<base>/bms/control/availability` is retained `disabled`, `unavailable`, or `available`, reflecting installation enablement and current readiness. Capability metadata keeps schema_version=1 and the compatibility `reserved_commands` paths; `read_only` and `commands_enabled` now reflect the installation gate. Command readiness is separate from supported commands and enablement. No Victron writes or Home Assistant discovery are implemented. Bulk means the main charging stage, not an alarm.

## Guarded command contract

Subscribe only to `<base>/bms/charge_enabled/set` and `<base>/bms/discharge_enabled/set`. Send a non-retained QoS0 JSON envelope, never a toggle or bare boolean:

```json
{"state":false,"request_id":"sil_stop_123","session":"CURRENT_32_HEX_CHARACTER_TOKEN","expires_at":1792170000}
```

The example expiry is illustrative: use current UTC plus at most 30 seconds. `state` must be boolean; `request_id` is 1-32 ASCII letters, digits, hyphens or underscores. Maximum payload is 256 bytes. The current random session comes from retained `<base>/bms/control`; reconnect/root changes rotate it and cancel outstanding intent. UTC must be synchronized. Expired, old-session, duplicate, malformed and unsupported requests are rejected. The 16-entry replay ledger rotates the session when exhausted rather than forgetting accepted IDs. Session tokens prevent obsolete command replay; they are not secrets or broker authentication.

`bms/control` contains enabled, ready, pending, session and reason. Readiness additionally requires fresh BMS data, installation enablement, an active command subscription, a valid clock and no OTA pause. `bms/control/result` is non-retained and contains request_id, phase, reason, actual charge_enabled/discharge_enabled and observed_current_a. Unknown observations are null. Readback confirms reported permission only, not physical current interruption. Ordinary MOS scalar topics remain actual feedback; never optimistically set them from a requested state. See [BMS transaction](BMS.md#guarded-mos-permissions).

`bms/health` and `victron/health` are retained status objects with available, reason, sample_sequence and sample_age_ms. They refresh at a 5-second cadence and on transitions, independently of numeric deadbands. A controller should require collector/source availability, a recently received health heartbeat, and an advancing sequence after reconnect before using retained data as fresh. A 15-second receiver deadline is appropriate for this cadence. Sequence values reset on collector reboot and wrap; compare advancement rather than global ordering. Measurement timestamps describe protocol receipt, not synchronized acquisition across devices.

A separate bounded status publisher caches at most one latest object per status topic, publishes one item per loop and retries failed local admission after 250 ms. It replays status after connection. Old-root health/control records may remain retained; consumers must always combine them with that root's collector and source availability. Command results are informational and cannot actuate anything.

[Home Assistant additions](home-assistant-bms-controls.yaml) provide eight additional sensors and two non-optimistic MQTT switches. Merge their lists into existing YAML; do not duplicate existing BMS/Victron entities. The controls remain unavailable while the installation gate is disabled. Existing voltage entities retain two-decimal display precision.


Values have explicit validity. Real values must be finite; unsigned values must be integral and within uint32 range; invalid booleans/enums are null. Unsupported and deferred fields serialize as null even if accidentally given a value, but initial unknown fields are not published. While a source stays available, a previously published valid field becoming invalid sends `null` once; valid zero remains `0` (or `false` for boolean fields). Source loss publishes availability/capabilities without a burst of null field messages. The JBD and Victron adapters establish source availability from validated, fresh protocol data.

Home Assistant consumers should combine collector `<base>/status` and source `<base>/<source>/availability` for availability/freshness. Do not use a short `expire_after` based on the publish interval: unchanged values intentionally stay silent while the source is still fresh. Collector reconnect replays valid values; a newly subscribing consumer receives all currently valid values in the next fixed 60-second refresh because telemetry itself is non-retained.

## Bounds, root migration and OTA

The operational root limit is 63 ASCII bytes. Reject empty values, `+`, `#`, control/non-ASCII/space characters, quotes/backslashes, leading/trailing slash and empty segments. Nested prefixes such as `shed/collector` are accepted. The consumer protects topic construction (128 bytes), prepared payloads (1024 bytes) and a 1536-byte MQTT packet buffer. Broker/user/password copies are bounded to 128 bytes, client IDs to 64 bytes, ports to 1..65535. Publish period is clamped to 5..3600 seconds; listen period to 0..1000 ms. Invalid values leave the last valid active value in use and retain rejection in raw runtime diagnostics. These are consumer operational checks: CM's generic UI/backend/persistence does not enforce a String storage bound, and a callback cannot veto storage. No silent truncation occurs in application-owned topic paths.

A root change retires old retained source availability/status, disconnects, replaces the LWT and reconnects. The publisher separately retains one advertised-root cleanup obligation and the latest desired root. Failed retirement is retried; a disconnected rename can connect under its selected root to retire the previous one, while subsequent desired changes wait in the bounded latest slot. Never-connected intermediate roots create no cleanup history. While cleanup is pending, the active root view can lag the saved desired root. After a transition no ordinary channel publishes use the previous root; only its explicit retirement does. Every accepted online publication restores the current advertisement obligation, including after delayed same-root cleanup. Before a configuration disconnect, both an older pending root and the current advertised root are retired when local sends succeed. Failed same-endpoint retirement remains pending without blocking disable indefinitely. If host/port changes and old-endpoint retirement fails, the old obligation is explicitly abandoned as unconfirmed, never sent to or falsely discharged on the new broker. The Connectivity page keeps a previous-broker-cleanup-unconfirmed warning for the rest of the boot. Graceful retirement is best effort local QoS0; an unreachable old broker cannot be proven cleaned when broker configuration changes. Downstream consumers should require collector `/status` and source `/availability`, not assume a collector LWT changes two source topics.

The existing OTA observer pauses MQTT attempts and preparation publications at Starting and keeps the success latch paused until reboot. The loop rechecks the pause after the publication phase before invoking CM's network phase, including when Async OTA starts during that publication phase. Failure/abort resumes from the application loop. BLE's independent OTA pause remains composed with its other pause reasons. `ConfigManager.handleClient()` is serviced before MQTT each loop. `MQTT_SOCKET_TIMEOUT=1` is applied globally to PubSubClient 2.8, retries occur at 10-second cadence, but MQTT/TCP is still synchronous: the private Arduino WiFiClient can block about 3 seconds for TCP plus up to 1 second for MQTT, and DNS may add time. A pause cannot cancel an already-running synchronous call. No low-latency, coexistence, outage or physical OTA result is claimed by host tests.

If OTA was disabled at boot, enabling it in the UI on the same WiFi connection requires a restart before ArduinoOTA testing, due to inherited CoreWiFiServices initialization behavior.

## Build and acceptance

Tracked consumer dependencies are NimBLE-Arduino 2.5.1 and PubSubClient 2.8. The selected ignored absolute override now uses the adjacent CM 4.7.0 development checkout. SBT requires CM >=4.7.0 for compact runtime descriptors; see the [migration handoff](acceptance/2026-10-18-compact-runtime-metadata-evidence.md). This local binding does not publish a CM dependency release. Both local environment dependency lists must retain all three dependencies; do not replace the tracked MQTT dependency when configuring the override. Commands build only:

```powershell
./tools/test/run-bms-command-tests.ps1
./tools/test/run-jbd-host-tests.ps1
pio run -e usb -e ota
```

The 0.2.0 actual binary was 1,298,272 bytes, leaving only 12,448 bytes in the old 1,310,720-byte slots, already below the new 10% reserve requirement. The full MQTT/BLE build determines the final candidate size. The default-slot MQTT candidate exceeded capacity, so the approved consumer table now uses two 0x1c0000-byte app slots with 0x60000 SPIFFS, unchanged NVS/otadata/coredump extents, and a 4 MB boundary. No application filesystem use was found in collector/CM source; the UI is embedded as WEB_HTML_GZ. A changed partition table requires full USB bootstrap/partition provisioning. An app-only OTA upload cannot perform that migration. Physical flash contents are not inspected or promised preserved by source analysis.

The user's 488,393-byte scaffold result has no proven equivalent exact source/flags here. Prior same-round CM probes show WiFi-dependent features differ by about 553 KB, OTA by about 53 KB and MQTT by about 39 KB; these are configuration comparisons, not additive ownership totals. Embedded UI, WiFi/network, web/OTA, libc and BLE all contribute to the initialized image; logging was already off. Do not attribute the whole size difference to NimBLE.

Consumer central/observer role removal and LTO remain in use. The known upstream NimBLE `is_ble_hs_resolv_enabled` declaration/definition mismatch warning is preserved. Previously examined exact ELFs omitted the incompatible checker/caller path; that is an exact-image assessment, not a warning-free or future universal compatibility guarantee. Final candidate sizes, hashes, partition parsing and exact-ELF warning assessment are recorded in local acceptance evidence and the implementation report. Workstation broker writes, hardware flashing and full physical tests are separate gates.

## Installed candidate, 2026-10-15

The final 0.3.0 USB/OTA images are each 1,328,448 bytes, leaving 506,560 bytes (27.61%) per 1,835,008-byte OTA slot. Focused tests, independent review, exact ELF/BIN identity and binary partition checks passed. COM11 received the full bootstrap, partition and application bundle with every upload hash verified. NVS was not erased. Live broker delivery, a full OTA transfer and BLE-device acceptance remain pending. See [the exact acceptance record](acceptance/2026-10-15-mqtt-preparation-0.3.0.md).

## Settings repair in the current working tree

The existing `OTAPass` object is retained. The local OTA password is provisioned only if its NVS key is absent; an explicitly saved empty password is preserved. Initialization failures stop startup before web/radio services. Local upload authentication remains in ignored `platformio.local.ini`; firmware and upload passwords must match after a user changes the saved password. The local CM 4.5 checkout and the selected CM 4.6 comparison checkout both include the WiFi/Bluetooth coexistence repair that enables minimum modem sleep while both radios are active.

See [JBD BMS readout](BMS.md) for configuration, freshness and protocol limits. BMS support is `jbd_readout`; capabilities report both source availabilities independently.

## Deadband validation (2026-10-16)

The focused telemetry contract test failed against the previous publisher when a sub-threshold change was sent, then passed after the change. Coverage includes every supported real channel, exact thresholds in both directions, accumulated changes, full payload precision, validity transitions, failed admissions, reconnect replay and discrete status fields. The MQTT lifecycle regression test also passed. The USB build succeeded (1,342,881 bytes flash; 80,360 bytes static RAM). Firmware remains 0.3.0 under the existing owner-requested debugging version freeze. This build has not been uploaded; live MQTT deadband acceptance is pending.
### Fixed 60-second MQTT refresh

At the owner's request on October 16, every 60 seconds the collector republishes source/control availability, capabilities and all currently valid measurements, including unchanged values and differences below the deadband. This timer is independent of the normal publication interval. Between refreshes the 0.1-unit/1-W deadbands still apply. Unsupported fields remain unavailable; no fabricated values are emitted. Admission retry, bounded cursor work and OTA pause remain in force. A restarted Home Assistant recovers unchanged values within approximately one minute while the collector, sources and broker are available. Network failures or OTA pauses can extend that interval. This refresh does not replace the 5-second source-health heartbeat.


Firmware 1.1.0 intentionally removes five optional channels from the schema:
bms/cell_count, bms/temperature_sensor_count, victron/pv_voltage_v,
victron/pv_current_a and victron/battery_power_w. No separate device queries were
needed for counts; mandatory JBD response validation is retained. Existing
remaining topic names, command envelopes, health/status publication, freshness,
retained cleanup and guarded MOS semantics remain. Consumers must tolerate the
removed optional fields; SIL already represents unseen fields as unavailable.
GUI-only MQTT root/OTA/validation fields are hidden without removing those
settings or internal functions.
