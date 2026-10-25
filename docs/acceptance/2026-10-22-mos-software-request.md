# Controlled MOS observation, 2026-10-22

Owner-authorized test on installed SBT 1.4.1. Scope: temporarily inhibit the SIL inverter, verify output near zero, block both MOS permissions, then allow charging while keeping discharge blocked, and restore the previous SIL enable state. No protection thresholds or automatic command retries.

The SIL Inverter Enabled value was changed from true to false in RAM. Requested output was 0 W and observed AC output 2 W (standby). Exactly three MOS requests were sent:

| UTC observation | Request | Software request | 1.4.1 result | Actual flags | Current |
| --- | --- | --- | --- | --- | --- |
| 10:02:18.793 | ui-1 | Charge off, discharge on | verified: ok | off/on | 0 A |
| 10:02:43.874 | ui-2 | Both off | verified: ok | off/off | 0 A |
| 10:03:07.515 | ui-3 | Charge on, discharge off | verified: ok | on/on after completion | +12.53 A |

The ui-3 absolute E1 frame was `DD 5A E1 02 00 02 FF 1B 77`. Firmware 1.4.1 required three fresh matching basic/cell pairs spanning at least two seconds before reporting verified. After completion, actual flags changed to on/on during charging without another discharge request. Alarm bits were 4096 throughout these observations. This demonstrates that actual FET flags cannot be used as persistent software permission feedback or as the opposite permission for a later absolute E1 write.

The previous SIL Inverter Enabled=true state was restored and verified. Last MOS request was charge on/discharge off. Final observed battery values: 21% SOC, 51.14 V, +9.16 A, +468.44 W, sample sequence 584. SIL requested/calculated output remained 0 W, observed AC 2 W, source_alarm/forecast_charge. These are bounded observations, not proof of persistent electrical isolation.

The [Overkill register map](https://gitlab.com/Overkill-Solar-LLC/overkill-solar-bms-tools/-/blob/master/JBD_REGISTER_MAP.md) distinguishes actual conducting FET bits in basic information from E1 software disable bits. Protection bit 12 indicates a software lock in either direction; it cannot initialize both permissions.

## Corrective implementation

Version 1.4.2 separates acknowledged collector requests from actual FET telemetry. A RAM-only ACK ledger preserves the opposite software request; unknown startup/link state requires one explicit joint request. Success is called acknowledged, not verified physical isolation. Firmware 1.4.2 was built and deployed once through authenticated ArduinoOTA. Sender exit code 0; browser title and heading confirmed SolarBatteryTelemetry 1.4.2. Firmware binary: 1,399,712 bytes, SHA256 `5150A09F362AF50EACED8305DC4A3DEFAF60F95BE11CD3C6019008949D0878EE`. USB build passed; static RAM 81,360/327,680 bytes, flash 1,393,141/1,835,008 bytes. Existing ConfigManager ArduinoJson deprecation and NimBLE LTO type warnings remained.

Focused validation passed: MOS policy and actual-FET independence regressions, strict MQTT envelopes, status publication, runtime metadata/action registration, all 23 readout scenarios, and the telemetry contract. Independent integration review identified gate-off during a pending pre-read unnecessarily invalidating the baseline. The corrected fence applies only to pending E1 writes; its regression failed before the fix and passed afterward, including completion of the outstanding read. Whitespace validation passed.

Passive post-OTA checks observed advancing uptime/sample sequence, MQTT online, fresh BMS/Victron data, `permissions_known=false`, both requested values null, `mos_readback=unknown`, no pending request and an empty request ID. An intermittent BLE timeout/queue drop occurred and polling recovered; this bounded observation is not a claim of long-run BLE stability.

Home Assistant configuration was backed up to `/config/mqttsensors.yaml.bak-mos142-20261022`, updated only for the two request switches and four joint buttons, validated with `ha core check`, then activated by reloading manually configured MQTT entities. Only the BMS MOS dashboard stack was edited through the UI. Live UI showed both individual switches unavailable while the request was unknown, all four joint actions present, and independent actual feedback (charge on/discharge off) received again after the non-retained telemetry refresh. No joint action or MOS command was sent during this rollout. Screenshot is saved in the local task workspace as `mos-ha-1.4.2.png`.

The firmware correction is implemented and deployed. A new live switch transaction with 1.4.2, including permission preservation across different actual FET states, remains untested on the device; the owner must explicitly initialize both permissions before using individual switches. No automatic commit or publication was performed.

## Follow-up: HA usability and same-peer reconnects, 1.4.3

The owner reported the upper HA gate intermittently appeared off and asked to keep only the original Allow switches on Home. The raw control sensor used diagnostic reason `unavailable` as its state. HA `has_value()` excludes that reserved state, so the template master gate became unavailable even though the collector continued reporting enabled=true. The sensor template now maps that diagnostic to `not_ready`; genuine collector offline availability remains intact. HA configuration validation and focused Jinja online/offline/readiness cases passed, then manually configured MQTT entities were reloaded. A backup exists at `/config/mqttsensors.yaml.bak-mos-gate-20261022`. The main MOS card no longer contains the extra pair actions, and uses a static explanation to distinguish unknown from off.

Passive observation before the correction covered 126 snapshots from 13:00:54 to 13:03:57 Europe/Berlin, with gate continuously enabled, idle commands, known=false and stable actual charge-on/discharge-off. Uptime at 13:07:35 implied the last boot at 12:22:19, matching the preceding OTA; subsequent BLE queue/drop increments did not indicate reboots. This interval did not reproduce a true gate-off event. The reserved diagnostic/availability mapping is independently reproducible from the templates and firmware status producer.

SBT 1.4.3 preserves the last acknowledged collector intent across ordinary same-peer BLE reconnect/generation changes. Fresh source qualification still interlocks new commands. Collector boot, peer reconfiguration and uncertain transmitted writes remain unknown; no NVS state or automatic MOS replay was added. All 23 real-readout host scenarios pass, including disconnect/fresh-pair requalification, opposite-mask preservation, peer change and uncertain E1 timeout. USB build passed: 1,399,712-byte binary, SHA256 `0B6B1CA2060111774D2FCC67667790780C19D610AC5DC3D73051CBCB480C0D0C`. Existing dependency warnings remained.

The frozen 1.4.3 artifact was transferred once through authenticated ArduinoOTA (sender exit 0). Browser title/heading confirmed 1.4.3, MQTT online, increasing uptime and source samples. The owner separately authorized one initialization request charge-on/discharge-off. Exactly one actual UI action was admitted as ui-1 and completed `acknowledged: ok`; control status reported known=true, requested charge=true/discharge=false, no pending command. The HA Home card visibly showed the original charging/discharging switches enabled with those requested values, and separate actual MOS feedback. No additional single-switch hardware test or automated command replay was executed. A selector attempt before the successful native UI action matched no element and sent no command.

Screenshot: local task workspace `mos-ha-restored-1.4.3.png`. These are bounded checks, not proof of long-run BLE or HTTP stability. An intermittent HTTP timeout remains observable and is separate from this control-state correction. No automatic commit or publication.

Working branch for the 1.4.3 follow-up: `feature/move-sil-planning`, preserving all pre-existing dirty work. Applicable local-only `.github/AGENTS.md` and refactor/docs governance were used; no commit or push. Follow-up source scope: `BmsReadout.h`, its behavioral host test and the firmware version; accompanying BMS/MQTT/HA example documentation. Live HA changes are limited to the MQTT control-status template and the main MOS dashboard stack.


## Follow-up: collector restart and MQTT display baseline, 1.4.4

Owner-approved behavior: boot has no outstanding MOS request; no automatic replay.
HA retains only the last confirmed pair for its original Allow switches. A new
manual click carries that pair and its BMS identity; the collector prefers a
currently known ACK pair and otherwise uses the explicit peer-matched baseline.
The result is neutralized for a new MQTT session and refreshed with the existing
configurable default-60-second cadence. Result messages remain non-retained;
control and historical ACK state are retained. NVS and protection thresholds are
unchanged. The separate panic reported by 1.4.3 is not diagnosed by this change.

Targeted policy, envelope, status publisher, real Readout and HA template checks
pass. Readout cases cover wrong/right peer, unknown startup, actual-FET mismatch,
freshness, reconnect and gate interlocks. Policy cases cover stale supplied pairs
after an intervening ACK and uncertain writes. HA templates cover all four pairs,
both directions, malformed/missing history, peer/session changes and the 256-byte
command limit. USB build passed. Owner explicitly authorized OTA after validation;
production rollout evidence is recorded below after completion.


1.4.4 rollout completed after owner authorization: exact 1,400,928-byte artifact,
SHA256 `663C40C5AA299614A3391765FD3991B217CFDB4D0CC88BCDC7D3711BDCF2301F`.
Native ArduinoOTA sender exited 0; installed `/appinfo` reported 1.4.4. Eighteen
post-boot observations showed increasing uptime, no pending/identified request,
unknown collector permissions and restored MQTT/readiness. All 172 compared
configuration leaves were unchanged. No MOS request was sent for acceptance.

HA configuration changed only the two existing MOS switches, the result template
and one diagnostic historical-ACK sensor; `ha core check` passed and manually
configured MQTT entities were reloaded. The existing documented `ui-1` ACK
(`charge=true`, `discharge=false`) was migrated once to the new retained historical
topic for the unchanged configured peer. This was a status publication, not a
command or a new BMS confirmation. Original HA configuration was backed up as
`/config/mqttsensors.yaml.bak-sbt144-20261022`.

An independent broker subscription received `idle: no_request` with RETAIN=0.
The final Home card visibly showed that neutral result, both original controls
available, charge-on/discharge-off, and separate actual FET feedback. The obsolete
unknown-request explanation was replaced through the dashboard editor; no extra
permission buttons were added. Screenshot in the local task workspace:
`mos-ha-restart-1.4.4.png`. Local evidence: `.local/mos-restart-144-20261022/`.
UI loading/reload transients recovered on publication. A briefly cached BLE sample
also recovered; no long-run crash/BLE stability claim is made. Actual manual
switch writes after this OTA were deliberately not exercised; their restart path
is covered by host tests. No commit or push.

Final integration check: the SBT publisher-to-SIL receiver contract passed with
26 supported values, 3 unavailable fields and exact availability states. The
1.4.4 source remains on `feature/move-sil-planning`, preserving prior dirty work.
Changed follow-up paths: `src/bms/BmsCommandPolicy.h`, `BmsReadout.h`,
`src/telemetry/BmsCommandEnvelope.h`, `StatusPublisher.h`, `MqttService.h`,
`src/settings.h`, focused host tests/runner, HA example YAML and BMS/MQTT/readme
notes. No dependency, NVS, threshold or automatic command changes. Local-only
`.github/AGENTS.md` and refactor/docs roles applied; the remote main ref does not
contain those local governance files. No branch switch, commit or push.

## 1.4.5 actual-state contract acceptance

The owner subsequently selected actual MOS readback for HA Allow controls, replacing the earlier acknowledged-request display model. SBT 1.4.5 and SIL 5.1.3 were built, installed by OTA and their installed versions verified. HA now uses retained actual MOS scalar states; the common persistent ESP gate is changed through non-retained, session-bound MQTT commands. Gate OFF also fences buffered BLE commands and has no emergency exception. The settings gate was toggled OFF/ON in HA and restored; no MOS hardware switching was performed during this acceptance. Relevant host tests and HA configuration checks passed. Detailed immutable hashes, snapshots, broker observation, backups and UI proof are retained locally. The current contract is in [BMS.md](../BMS.md#guarded-mos-permissions).

The accepted live snapshot showed actual discharge OFF while SIL discharge intent remained available, with charging still possible. A later valid forecast selected forecast_hold and 0 W; a positive expensive-period command with discharge OFF was verified in the actual PID/send-path test harness. This does not prove electrical isolation or resolve the separate historical SBT panic/HTTP delivery issue.

## 1.4.6 Settings-only Registry rollout

The owner authorized the published PlatformIO ConfigManager 4.9.0 dependency, disabled Live UI, retained Settings UI, a short isolated COM8 test and production OTA. Version 1.4.6 pins the published package exactly and treats disabled Live metadata registration as an intentional no-op, preserving BLE/MQTT startup. Consumer-local embedded assets were generated by the package's official Settings-only generator and their feature/provenance proof was checked. No unpublished CM 4.9.1 source or bounded-transport change was included. Focused host regressions and USB/OTA/testesp builds passed; build logs and detailed evidence remain local under `.local/settings-only-20261025/` and the task workspace `sil-hardening-20261025/`. No commit or push was made.

The isolated COM8 check ran for 180.493 seconds from 2026-10-25T15:37:29Z, with one intentional boot and no critical serial markers. Three fresh ESP samples reached uptime 2m43s. Final appinfo/capabilities/config requests completed in 0.095/0.028/0.131 seconds and verified test app 1.4.6, Live=false, Settings=true, OTA inactive and all 32 settings. Test root `SBTTest/COM8` and client ID `SBTTest-COM8-collector` were exact. Only five MQTT connection settings were loaded from the private source; every other exposed key stayed unchanged. BLE initialized with empty peers and BMS control remained hard-blocked, so this test did not qualify physical BLE reception or control. Test firmware SHA256: `B9E6EFA475D17690764388D0769D836DF72FE6C1726C21C221B2B5CF105FB3E0`.

Production accepted the frozen OTA artifact at 2026-10-25T15:42:38.192918Z. Firmware SHA256: `D8B3E3BAF4BEE8E7B8DEED82964B42A09B5323052CB46E945C033E31CFF2AA56`; matching ELF SHA256: `E5D8D17E59C06DE209AAFA6017206A3217C897B30F07C63FCFE5FD91FC615D42`. A fresh post-boot identity check confirmed the intended MAC/IP, installed 1.4.6, Live=false, Settings=true and OTA inactive. All 32 exposed settings matched the preflight snapshot; masked credential equality cannot prove underlying secret equality. No settings write or BMS control command was part of production acceptance.

A fresh production ESP message at 2026-10-25T15:45:54Z showed uptime 3m10s, free heap 64,252 bytes, minimum heap 25,884 bytes and largest allocation 47,092 bytes. A separate passive subscription at 15:46:37Z–15:47:42Z observed advancing real BMS/Victron health sequences with fresh sample ages, confirming resumed acquisition rather than retained status alone. COM8 was restored from the complete prior SIL flash backup, SHA256 `93205144A9704B543D83893D464E845A09D295026FF25B00C47875E22C2D3270`, with esptool hash verification; subsequent SIL serial observation opened without an additional reset.

These bounded checks establish the exact rollout and resumed data acquisition. They do not resolve the historical panic/HTTP continuity failure, prove long-run stability or qualify physical MOS writes. Published CM 4.9.0 still contains synchronous MQTT paths; the separate CM transport investigation remains open.
