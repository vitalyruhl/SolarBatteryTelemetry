# MQTT telemetry and guarded BMS commands

The current working firmware adds passive SmartSolar Instant Readout to the 26-channel MQTT contract. Seven direct Victron fields can contain decoded readings; BMS is supplied by the JBD readout adapter. Unsupported PV voltage/current and deferred battery-power placeholders are omitted. Charge/discharge commands are implemented behind a default-disabled installation gate. See [Victron readout](Victron.md) for configuration, field limits and hardware acceptance.

## Settings and ownership

The application persistent MQTT keys are `SBTMQTTEnable`, `SBTMQTTHost`, `SBTMQTTPort`, `SBTMQTTUser`, `SBTMQTTPass`, `SBTMQTTClientId`, `SBTMQTTBaseTopic`, `SBTMQTTPubPer`, and `SBTMQTTListenMs`, under MQTT / MQTT Settings. Each UI setting is constructed once with final defaults. Legacy canonical MQTT keys are copied only when the new destination key is absent; existing new values win and legacy keys are retained. No Config object is reconstructed or copied. Defaults are established before registration/loadAll; existing persisted edits, including an intentionally empty password, win. With real ignored secrets the default is enabled and uses the local broker credentials; placeholder-only builds default disabled. The default base is `SolarBatteryTelemetry`. A deterministic client ID contains all 48 MAC bits, independent of the limiter's identity.

The consumer registers application-owned `MqttUiSettings` for UI/persistence. The public MQTT singleton's canonical settings belong only to the application loop and are not registered with the configuration manager. Separate key namespaces avoid constructor-time collisions under CM settings registration semantics and remain compatible with the local CM comparison builds. The consumer intentionally does not call `MQTTManager::attach`, which would expose those live settings and install client-mutating callbacks. UI callbacks copy only bounded values/revisions into an owned mutex-protected mailbox; no client operation occurs there. After loadAll, initial values enter that mailbox before web-server startup because CM's persisted load does not invoke change callbacks. The application loop applies the latest snapshot, including changes received during OTA. Runtime readers get a synchronized copy, never UI Strings or live client state. This separation does not claim to fix all inherited CM settings-storage concurrency.

Public standalone `begin`, `loop`, `settings`, `setServer`, `setCredentials`, `setClientId`, `setLastWill` and `publishRaw` are used. The existing module still publishes its normal `<base>/System-Info/ESP` and `/WiFi` metadata. The custom retained LWT is `<base>/status=offline`; online is published on connection/root change and on each configured full refresh, with local-admission retry. The existing CM connection callback starts this replay; `publishRaw` does not provide automatic change detection. CM System-Info publication keeps its existing 60-second interval. Publish success means local QoS0 admission, not broker acknowledgment.

## Home Assistant display precision

Display precision is independent of the MQTT send threshold. Configure `suggested_display_precision: 2` on the existing MQTT voltage sensors (BMS pack voltage, minimum/maximum cell voltage, cell spread and Victron battery voltage). If an entity already has a user-selected display precision, set that entity preference to two decimals as well. Keep values numeric; do not turn payloads into formatted strings or round them before deadband comparison.

The collector does not publish Home Assistant discovery configuration. Update the existing HA sensor configuration instead of adding duplicate entities. See the [official MQTT sensor documentation](https://www.home-assistant.io/integrations/sensor.mqtt/#suggested_display_precision). On 2026-10-16, the five existing voltage entities were changed through the HA entity settings to two decimal places. The live dashboard then showed BMS pack 48.52 V, cell minimum 3.43 V, maximum 3.48 V, spread 0.04 V, and Victron battery 49.01 V. Changing only HA display precision restored the decimals, confirming a presentation issue. These are historical observations, not synchronized calibration readings.

## Fixed channels

All 26 descriptors, labels, units, type/support names, state vocabulary and reserved command names are referenced by the production runtime and serializer. There are no per-field editable topics or allocated setting objects. Telemetry is non-retained except for the Victron error and daily-yield scalars described in the 1.4.0 display-history section. The publish interval limits ordinary value updates to at most once per 5 seconds (or the configured interval when longer); it is not a heartbeat. Each coherent sample compares fields against the last successful local admission. Real-valued measurements publish when their absolute change reaches 0.1 in the channel unit, except W channels which require 1 W. This applies to V (including cell minimum, maximum and spread), A, Ah, C, %, and Wh; future real-valued kW channels would use 0.1 kW. Small changes accumulate against the last sent value, not the previous sample. Boolean, enum, alarm and error changes retain exact change detection. Initial values, validity changes, reconnect replay and source restoration bypass the numeric deadband. Payloads retain their existing significant-digit precision; this policy does not round measurements. Between full refreshes, unchanged samples and absent sources emit no telemetry messages. Reconnect or root changes replay metadata and all currently valid fields; source availability changes publish availability/capabilities promptly, and restoration replays that source's valid fields even when unchanged from its previous connection. At most four normal publications are attempted per policy tick, with a finite cursor and a 250 ms admission retry. A pending old-root retirement can add three sends before that normal budget; a root transition or configuration disconnect retires at most two advertised roots (six sends). CM's independent automatic online/System-Info messages are outside this preparation budget. A failed item does not advance its field cache or mark the snapshot delivered. Already accepted fields keep their cache across partial failures; retries wait at least 250 ms, including when a source transition replaces the pending snapshot. The fixed cache holds one `Value` per channel and a bitmask, using scalar serialization for equality and validity transitions, plus numeric deadbands for valid real measurements. There is no historical sample backlog in the live publisher. The separate display-history publisher may add one MQTT admission per loop and keeps only two last-valid fields.

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
| bms | temperature_1_c | real | C; NTC 1 (Top), first JBD sensor |
| bms | temperature_2_c | real | C; NTC 2 (Bottom), second JBD sensor |

Topics are `<base>/<source>/<leaf>`. Both `<base>/bms/availability` and `<base>/victron/availability` publish their retained `available`/`unavailable` state before scalar data. BMS availability follows complete JBD basic/cell cycles; Victron availability follows the readout freshness check. Availability and capabilities are sent once per connection/root and on source availability changes. `<base>/bms/control/availability` is retained `disabled`, `unavailable`, or `available`, reflecting installation enablement and current readiness. Capability metadata keeps schema_version=1 and the compatibility `reserved_commands` paths; `read_only` and `commands_enabled` now reflect the installation gate. Command readiness is separate from supported commands and enablement. No Victron writes or Home Assistant discovery are implemented. Bulk means the main charging stage, not an alarm.

## Guarded command contract

Subscribe to `<base>/bms/charge_enabled/set`, `<base>/bms/discharge_enabled/set` and `<base>/bms/permissions/set`. Send a non-retained QoS0 JSON envelope, never a toggle or bare boolean:

```json
{"state":false,"request_id":"sil_stop_123","session":"CURRENT_32_HEX_CHARACTER_TOKEN","expires_at":1792170000}
```

Single-target commands require a known acknowledged software-request baseline or the peer-bound explicit manual baseline described in the 1.4.4 section below. Joint initialization remains available via `bms/permissions/set`, with exactly five fields:

```json
{"charge_enabled":true,"discharge_enabled":false,"request_id":"manual_pair_123","session":"CURRENT_32_HEX_CHARACTER_TOKEN","expires_at":1792170000}
```

The pair request uses the same gate, replay ledger, freshness and expiry constraints. Both permission fields must be booleans; missing, extra or duplicate fields are rejected. No joint request is sent automatically.

The example expiry is illustrative: use current UTC plus at most 30 seconds. `state` must be boolean; `request_id` is 1-32 ASCII letters, digits, hyphens or underscores. Maximum payload is 256 bytes. The current random session comes from retained `<base>/bms/control`; reconnect/root changes rotate it and cancel outstanding intent. UTC must be synchronized. Expired, old-session, duplicate, malformed and unsupported requests are rejected. The 16-entry replay ledger rotates the session when exhausted rather than forgetting accepted IDs. Session tokens prevent obsolete command replay; they are not secrets or broker authentication.

`bms/control` contains enabled, ready, pending, session, reason, permissions_known, nullable requested_charge_enabled/requested_discharge_enabled, and mos_readback (unknown/unavailable/matching/different). The requested values are the last acknowledged software request in this collector session, not actual conducting FET flags. They become null after collector restart, peer reconfiguration and after an uncertain transmitted write. Ordinary same-peer BLE reconnection preserves the last acknowledged intent; source freshness still interlocks all new commands. A new explicit joint request or a new manual single-target request carrying the peer-bound historical ACK pair can re-establish the baseline; no request is replayed automatically. Readiness additionally requires fresh BMS data, installation enablement, an active command subscription, a valid clock and no OTA pause. `bms/control/result` is non-retained and contains request_id, phase, reason, actual charge_enabled/discharge_enabled observed_current_a and session. Unknown observations are null. `acknowledged: ok` confirms the software write ACK, not physical isolation; actual MOS feedback can differ during charging. Ordinary MOS scalar topics remain actual feedback; never optimistically set them from a requested state. See [BMS transaction](BMS.md#guarded-mos-permissions).

`bms/health` and `victron/health` are retained status objects with available, reason, sample_sequence and sample_age_ms. They refresh at a 5-second cadence and on transitions, independently of numeric deadbands. A controller should require collector/source availability, a recently received health heartbeat, and an advancing sequence after reconnect before using retained data as fresh. A 15-second receiver deadline is appropriate for this cadence. Sequence values reset on collector reboot and wrap; compare advancement rather than global ordering. Measurement timestamps describe protocol receipt, not synchronized acquisition across devices.

A separate bounded status publisher caches at most one latest object per status topic, publishes one item per loop and retries failed local admission after 250 ms. It replays status after connection. Old-root health/control records may remain retained; consumers must always combine them with that root's collector and source availability. Command results are informational and cannot actuate anything.

[Home Assistant additions](home-assistant-bms-controls.yaml) provide additional diagnostics, two non-optimistic request switches and four explicit joint request buttons. Merge their lists into existing YAML; do not duplicate existing BMS/Victron entities. The controls remain unavailable while the installation gate is disabled. Existing voltage entities retain two-decimal display precision.


Values have explicit validity. Real values must be finite; unsigned values must be integral and within uint32 range; invalid booleans/enums are null. Unsupported and deferred fields serialize as null even if accidentally given a value, but initial unknown fields are not published. While a source stays available, a previously published valid field becoming invalid sends `null` once; valid zero remains `0` (or `false` for boolean fields). Source loss publishes availability/capabilities without a burst of null field messages. The JBD and Victron adapters establish source availability from validated, fresh protocol data.

Home Assistant consumers should combine collector `<base>/status` and source `<base>/<source>/availability` for availability/freshness. Do not use a short `expire_after` based on the publish interval: unchanged values intentionally stay silent while the source is still fresh. Collector reconnect replays valid values; a newly subscribing consumer receives all currently valid values in the next full refresh because telemetry itself is non-retained. `SBTMQTTRefresh` (Settings > MQTT, Full refresh interval) controls this period from 5 to 3600 seconds, default 60, independently of the delta publish interval. A live change applies the new period against the last refresh without reconnecting; source changes/reconnects still publish immediately. Invalid UI values are rejected and out-of-range persisted values are bounded before use. The effective interval is available as `telemetry.full_refresh_interval_s` in runtime JSON.

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
### Configurable full MQTT refresh

The collector republishes source/control availability, capabilities and all currently valid measurements, including unchanged values and differences below the deadband. `SBTMQTTRefresh` sets this full-refresh period to 5-3600 seconds, default 60 (the original fixed period requested on October 16). This timer is independent of the normal publication interval. Between refreshes the 0.1-unit/1-W deadbands still apply. Unsupported fields remain unavailable; no fabricated values are emitted. Admission retry, bounded cursor work and OTA pause remain in force. A restarted Home Assistant recovers unchanged values within approximately the configured full-refresh period while the collector, sources and broker are available. Network failures or OTA pauses can extend that interval. This refresh does not replace the 5-second source-health heartbeat.


Firmware 1.1.0 intentionally removes five optional channels from the schema:
bms/cell_count, bms/temperature_sensor_count, victron/pv_voltage_v,
victron/pv_current_a and victron/battery_power_w. No separate device queries were
needed for counts; mandatory JBD response validation is retained. Existing
remaining topic names, command envelopes, health/status publication, freshness,
retained cleanup and guarded MOS semantics remain. Consumers must tolerate the
removed optional fields; SIL already represents unseen fields as unavailable.
GUI-only MQTT root/OTA/validation fields are hidden without removing those
settings or internal functions.

## 1.2.0 source timing and positioned temperatures

The application copies each source snapshot after BLE event dispatch, then captures
`millis()` separately for its health generation. `sample_age_ms` uses modulo uint32
subtraction, including real rollover; unavailable sources retain `null`. This avoids
the previous T/T+1 ordering defect without clamping genuinely stale samples or
changing sequence advancement, acquisition deadlines or guarded MOS readiness.
The collector's 30-second source availability is an acquisition diagnostic. A SIL
consumer's configurable transport/display deadline is independent and must not
relax physical protection or command-validity limits. Silence is not an alarm.

The appended non-retained Celsius topics `bms/temperature_1_c` (**NTC 1 (Top)**) and
`bms/temperature_2_c` (**NTC 2 (Bottom)**) use the first two original JBD sensor slots.
They are not sorted. No second sensor means no substituted value; zero sensors or
an unavailable source produces no invented temperatures. An invalid NTC still
rejects the basic/cell cycle under the existing JBD validation policy. Existing
`temperature_min_c` / `temperature_max_c` keep their previous meaning across all
validated sensors. The publisher has 26 descriptors and retains its 32-bit mask,
fixed message bounds, delta thresholds and periodic full refresh (60 seconds by default).

Focused checks: `tools/test/run-health-age-tests.ps1`,
`tools/test/run-jbd-host-tests.ps1`, `tools/test/run-smoke-host-tests.ps1` and
`tools/test/run-runtime-metadata-tests.ps1`. Synthetic fixtures exercise production
logic; they do not establish physical sensor positions, BLE coverage or overnight
stability. Production firmware deployment remains owner-operated.

## 1.3.0 BMS availability tolerance and configurable refresh

`BmsDataTimeout` controls BMS telemetry validity from its last complete cycle
(1-9999 seconds, default 60, previously fixed at 30 seconds). Transient BLE/query
failures no longer erase that cycle immediately. During recovery `/bms/health`
reports `available: true`, `reason: "cached"`, and the unchanged sample sequence
with its increasing age. Once the configured timeout expires, source availability
changes to `unavailable`; no old sample is relabeled as newly measured. Runtime
JSON includes the effective `bms.data_timeout_s`. Explicit source reconfiguration
or OTA/pause invalidates immediately. Victron freshness is unchanged.

MOS command readiness remains independent: disconnect/error cancels intent and
blocks commands immediately, even while telemetry is retained in memory. Only a
new complete valid cycle on a healthy connection can restore readiness, and the
existing 30-second command freshness cap still applies. MQTT commands remain
non-retained. Measurement retain flags are unchanged; `SBTMQTTRefresh` controls
periodic resend, not the lifetime of a source measurement.


## 1.4.0 Victron display history

The existing numeric `victron/error_code` and `victron/yield_today_wh` topics now use MQTT RETAIN. Other measurement topics remain non-retained. Collector `/status` and source `/availability` continue to gate live consumers, including SIL; missing BLE data never becomes a fabricated valid 0 W measurement. Scalar null transitions remain explicit; a last-valid display cache is separate.

Two additional retained topics support Home Assistant display history:

| Topic suffix | Payload example |
| --- | --- |
| `victron/last_valid/error_code` | `{"value":0,"received_at":1792600000}` |
| `victron/last_valid/yield_today_wh` | `{"value":1230,"received_at":1792600000}` |

`received_at` is the UTC Unix reception time in seconds, estimated from the synchronized clock minus sample age. It is `null` when the clock was not valid at acquisition. Only a new, valid field acquisition updates that field's value and time. Invalid fields, source loss, repeated application snapshots and reconnect replay never re-date history or replace it with zero. Each field publishes at most once per five seconds during normal acquisition; reconnect/root changes replay pending history without changing its time. OTA pause applies to this path as to other MQTT publications. History lives in RAM and at the broker; a reboot with no new BLE reading leaves the broker's prior retained history intact. No NVS measurement writes are added.

Display consumers may show this history independently of live BLE availability, but must identify it as last received and expose its reception time. A value with an unknown reception date must not be labelled as today's yield; compare a valid reception timestamp with the Home Assistant local date before displaying it as today's value. An older daily reading may remain visible with its original date. Before any valid acquisition, no history value is fabricated.

The Home Assistant power display may use 0 W plus an explicit missing-data indicator when the raw PV measurement is unavailable. That display fallback must not replace the raw measurement, feed SIL, or be used for energy integration.

### Focused validation for 1.4.0

The retained-scalar regression failed on the previous non-retained publisher. The history regression failed on the empty interface, and the busy-stream regression exposed starvation before the per-field cadence/fair cursor correction. All four affected host programs now pass: telemetry contract, Victron history, status publisher and MQTT lifecycle. They cover loss/reconnect, per-field invalid values, null clock, original reception time, admission retry, monotonic rollover, busy-stream fairness and unchanged raw source gating.

The normal USB-profile build passed with the selected local CM 4.8.0 checkout: 1,389,477 flash bytes of 1,835,008 and 81,336 static RAM bytes. The existing NimBLE LTO type-mismatch warning remains. This is local software validation; the candidate has not been uploaded and new retained-topic delivery on the device remains pending.

### HA control gate availability (2026-10-22)

Do not expose the diagnostic reason `unavailable` as the state of `sensor.sbt_bms_control`: HA treats that reserved word as entity unavailability and `has_value()` then also hides the still-enabled master gate. The example maps this diagnostic state to `not_ready`, while retaining the original JSON attributes and separate collector-status availability. A real collector offline event still makes the entity unavailable. Home keeps the existing individual Allow switches; explicit joint commissioning actions are not shown on its main MOS card.


## Historical 1.4.4 manual restart recovery (superseded by 1.4.5)

The following records the prior model; it is not the current HA control contract.

`bms/permissions/acknowledged` is a retained historical display baseline containing
`peer`, `charge_enabled` and `discharge_enabled`. Only an E1-acknowledged collector
pair can produce it. It is output-only: SBT never subscribes to it, restores a
pending request from it or sends MOS commands on boot. Reboot still starts with
no pending request and an unknown collector ledger. A peer change prevents use
of an old device's retained pair.

The existing four-field charge/discharge command remains supported. HA may add
exactly three fields to a new explicit single-target command:
`baseline_charge_enabled` (boolean), `baseline_discharge_enabled` (boolean), and
`peer` (canonical BMS MAC). All seven fields must fit the existing 256-byte bound.
The current session, expiry, control gate, fresh source and mailbox checks remain.
The peer must match the currently configured BMS. When the collector ledger is
known it preserves its current opposite bit, ignoring the possibly older supplied
baseline. Otherwise the explicitly supplied pair is used only for this manual
request. Only a valid E1 ACK establishes a new collector baseline and updates the
retained display pair. Failure/uncertain writes do not update the historical pair;
retry requires a new manual request. SIL continues to consume fresh `bms/control`,
not this historical topic.

`bms/control/result` remains non-retained and includes the MQTT `session`.
Each new connection publishes `idle: no_request` for the new session, suppressing
old transaction replay. HA invalidates a result from a different session.
`SBTMQTTRefresh` (5-3600 s, default 60 s) now refreshes valid health, control,
current-session result and known acknowledged-pair slots as well as measurements
and availability. Updates remain immediate; the periodic resend repairs missed
state delivery while connected. Unknown/unsupported measurements are not invented.
During a fresh collector boot, the retained historical pair stays in the broker
until another command is actually acknowledged.


## 1.4.5 Actual MOS status and common persistent gate

The current [BMS contract](BMS.md#guarded-mos-permissions) supersedes the historical ACK-pair display model. `bms/charge_enabled` and `bms/discharge_enabled` are retained actual readback and drive HA switches. `bms/permissions/acknowledged` is retired as a control/display authority; its historical retained broker value is never subscribed to or replayed. There is no startup initialization command.

Single-direction commands use only `state`, `request_id`, `session`, `expires_at`; the existing strict current-session/30-second/replay and 256-byte envelope checks remain. `bms/control_enabled/set` uses the same envelope to update and save the existing ESP `BmsControl` setting, including while BMS control readiness is unavailable. It does not write a MOS register. OFF immediately fences queued BLE writes; ON takes effect only after successful saving. Last desired gate changes coalesce for 150 ms.

`bms/emergency_off/set` accepts only `state:false` and writes BothOff only with the common gate on, fresh readiness and current hard-fault bits 0-11, rechecked in BasicInfo immediately before E1. SIL has no economic/SOC MOS-control path and no automatic enable/retry. Gate OFF has no emergency exception.

Changed scalar data starts publication on the next bounded cycle without waiting for the delta interval. An in-progress snapshot completes so continuously changing early channels cannot starve later MOS channels. Existing deadbands remain; complete refresh (default 60 seconds, configurable 5-3600) republishes valid status, health, scalar measurements and current-session result. Status never generates a command. Results are non-retained; each collector/MQTT session starts with `idle: no_request`.

The [MQTT include](home-assistant-bms-controls.yaml) and separate [template gate include](home-assistant-bms-gate.yaml) retain existing entity identifiers. HA is non-optimistic and never retries a request in response to status. Actual-state switches remain visible while the common gate is off or control is busy; the backend rejects all disabled, stale, concurrent or debounced commands. A requested value may revert to actual readback; the operator can explicitly retry after the transaction ends.
