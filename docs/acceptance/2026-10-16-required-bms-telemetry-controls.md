# Required BMS telemetry and controls acceptance — 2026-10-16

## Scope and deployment status

Branch: `feature/bms-required-telemetry-controls`, baseline `861068793d3abf064e8d2c1df0888aa1a686d5a2`. Changes remain uncommitted. CM 4.6.0 binding: `554869da7efc98e0c1874e74bf0c78d4bcd1664a`; shared CM sources and production partitions were not changed.

Implementation includes 29 descriptors, minimum NTC temperature, sensor/cell counts, protection descriptions, 5-second source health, guarded charge/discharge MOS requests and a full 60-second MQTT refresh. Between refreshes, numeric deadbands remain 0.1 unit except 1 W. Only current valid measurements are repeated. Retry, source validity and OTA pause remain effective. Network failure can delay delivery beyond a minute.

## Software verification

- Focused JBD decoder, telemetry contract, MQTT mailbox/lifecycle, MOS policy, strict command envelope and status publisher tests pass.
- Refresh tests cover unchanged booleans/alarms, sub-deadband latest readings, receiver restart, long ordinary intervals, failure/retry, pause, uptime wrap and invalid transitions.
- Independent review findings were resolved: reject requests while disabled/unavailable/busy at admission, reserve the command slot during execution, reject malformed/duplicate/trailing JSON fields and escaped NULs, and clear transient results on session changes.
- MOS writes remain installation-disabled. Host/readback results do not establish electrical isolation or qualify live switching.

## Home Assistant

Original configuration preserved in `/config/backups/mqttsensors.before-sbt-20261016.yaml`. Eight sensors and two non-optimistic, gated MQTT switches were added to `/config/mqttsensors.yaml`. Configuration validation succeeded before reload. Existing BMS/Victron cards were expanded, preserving optional existing fields and two-decimal voltage display. PV voltage/current remain explicitly pending VE.Direct hardware. The restored collector, unavailable-source diagnostics and real two-decimal voltage display are verified; see the normal-location follow-up and its network limits below.

## Production OTA failure and diagnosis

Initial feature artifact: program 1,354,341 bytes, static RAM 83,960 bytes, binary 1,360,912 bytes, SHA256 `a7d036a996cc03610c85bd08da8b846e3000047d7a6958a9bc8225c5750905dc`. OTA authentication/upload to `192.168.2.21` completed with exit 0. The device did not return to HTTP; MQTT retained status was offline with no fresh health. An accepted upload is therefore not a successful deployment. No blind OTA retry was made.

COM7 reproduction of that artifact showed WiFi receive-buffer allocation failure and AsyncTCP task-creation failure. Removing one redundant UI metadata entry reduced registrations from 65 to 64, avoiding the shared vector capacity doubling to 128. Before-BLE heap increased from 144,040 to 159,656 bytes, but the longer test still failed WiFi startup and later hit the WiFi-task watchdog. This change alone did not fix startup.

Bounded WiFi-before-BLE startup trials (10 and 30 seconds) did not restore connectivity and were reverted. The 30-second trial already lost heap while BLE had not started. A previously working page-scoped artifact also failed with the same COM7 settings (WiFi init timeout, AsyncTCP task failure, HTTP connection refused), so the new feature alone does not explain this test failure. A separate minimal trial now moves the existing AP-priority setter after `startWebServer()`, matching SIL. CM performs its first connection attempt synchronously: this allows initial automatic AP selection while retaining the configured preference for later reconnects/roaming. The original BLE-before-server order remains unchanged. Independent static review confirmed no diagnostic wait or atomic changes remain. At that diagnostic checkpoint, production recovery and final hardware acceptance were still pending; subsequent results are recorded below. Test-device SIL firmware/settings were backed up and restored after diagnosis. No BMS/Victron physical controls were exercised.


## Minimal startup correction

Candidate SHA256 `bd5b8e37b9a7e0dcbf458fcf1bad282de322c3a05fb1174993b33437c7bf18f8`, program 1,354,253 bytes, static RAM 83,960 bytes. The first COM7 boot with restored diagnostic NVS reached WiFi at 816 ms, RSSI -47 dBm, free heap 40,896 bytes. HTTP runtime and MQTT were available at 25 seconds under isolated root `SILTest/COM7`; controls remained disabled. No WiFi init/AsyncTCP error appeared during this startup. BMS and Victron were out of telemetry reach at the test device, so no fabricated readings were used. The three-minute acceptance then passed: HTTP 18/18; uptime increased from 194,840 to 383,772 ms with no reset or UART fault markers. Independent MQTT receipt showed repeated online/full metadata at 55.36, 115.42 and 176.52 seconds after subscription. The initial test client disconnected because repeated short collection calls never sent its own keepalive; that invalid harness run was corrected and repeated. Malformed, expired and valid-but-installation-disabled requests were rejected as `invalid_envelope`, `expired` and `disabled`. Observed MOS/current values remained JSON null because sources were absent, never synthetic off/zero. Actual unchanged measurement replay is verified by host tests; real BMS/Victron reception on the production device is still pending. Test root: `SILTest/COM7`; no physical switching occurred. Logs: ignored `.local/com7-sbt-acceptance.json` and `.local/com7-sbt-acceptance-uart.log`. COM7 was released for SIL restoration.


## Production recovery path

A single corrected OTA invitation attempt received no response from `192.168.2.21` and exited 1; no corrected image was transmitted. The owner connected the production SBT on COM11. Device identity was confirmed as ESP32-D0WD-V3, MAC `88:57:21:8f:66:4c`, 4 MiB flash. Full-flash backup attempts stalled or failed during serial reads, including with progress output disabled; the cause is not established. The partial backup contains 720,896 individually digest-verified bytes. A fresh 65,536-byte boot/settings backup was read and verified against the device without an intervening reset: SHA256 `39049acbdd9129cb594e673ce6596c2ee15c3031a0e15e182feb2ee0c358b1cb`, ignored `.local/com11-boot-settings-before-recovery-20261016.bin`. An earlier cross-reset digest mismatch was confined to NVS sector `0xb000`; the fresh same-session check passed. This is a boot/settings backup, not a complete flash image. Confirmed layout: NVS `0x9000`/`0x5000`, OTA data `0xe000`/`0x2000`, app0 `0x10000`/`0x1c0000`, app1 `0x1d0000`/`0x1c0000`. USB recovery writes only app0 with the validated artifact and standard OTA selection for app0. Bootloader, partition table and NVS are not write targets.

The owner explicitly confirmed there is no Bluetooth reception at the PC. Local acceptance therefore covers startup, WiFi/HTTP/MQTT, full-refresh metadata, unavailable-source semantics and disabled-command rejection. Real BMS/Victron values and measurement refresh must be checked again at the normal installation location.

The parallel SIL receiver passed its corrected COM7 tests: 19/19 HTTP over 180 seconds, grouped MQTT 9/9, full-runtime MQTT 9/9, no unexpected reset or Guru. SIL's availability vocabulary and JSON string-lifetime bug were fixed. Production SIL remains 4.3.2; new receiver code stays on its uncommitted feature branch. See the SIL repository acceptance document for complete evidence and earlier failed runs.


## COM11 USB recovery and unchanged health regression

USB recovery of the COM7-qualified artifact completed successfully; esptool verified both written ranges. Production SBT returned at `192.168.2.21` with the original MQTT root and installation controls disabled. The initial HTTP probe timed out, but later runtime access succeeded. The 180-second local test passed 18/18 HTTP requests, with uptime 105,473 -> 293,841 ms and no UART fault markers. A brief MQTT disconnect/reconnect occurred near test second 18 while WiFi moved from approximately -86 to -32 dBm; no MCU reboot occurred. Subsequent online refreshes arrived at 78.078 and 138.344 seconds. All three command rejections passed. No device measurements were expected or fabricated.

This local test exposed a separate status regression: `MqttService::health()` scheduled a five-second update, but `StatusPublisher::set()` deduplicated the identical unavailable-source payload. Only initial/reconnect health packets arrived, causing HA's 15-second expiry. A narrowly scoped optional `force` flag now marks the health slot dirty at the already rate-limited call site. Control/result deduplication, retry timing, bounded sending, sample sequence and validity remain unchanged. Focused regression coverage checks ordinary deduplication, forced repetition and retry preservation; the test failed before implementation and passed afterward. Independent static review accepted the change.

Updated candidate: program 1,354,273 bytes, static RAM 83,960 bytes, SHA256 `278d9541b769c999ab3a76986b3953b9f32a05c88d071845fbde714db64086c4`. Build and focused policy/envelope/status suites pass. Final COM11 acceptance passed as detailed below.

The four HA diagnostic entities now depend on collector availability only, so missing Bluetooth does not hide the source reason or installation lock. Measurement and switch availability gates remain unchanged. The updated YAML passed HA configuration validation and was reloaded without restarting HA. The saved example uses the same rules.


## Final SBT local acceptance

Final binary SHA256 `278d9541b769c999ab3a76986b3953b9f32a05c88d071845fbde714db64086c4`, 1,360,848 bytes. USB app0 write completed and its flash digest was verified. NVS, partition table and bootloader were not written. Three minutes on production SBT/COM11 passed 18/18 HTTP requests; uptime increased from 233,194 to 421,833 ms without reset or UART fault markers. MQTT online refresh receipts were 16.641, 76.641 and 136.657 seconds after subscription. BMS and Victron health each arrived 37 times; maximum observed gaps were 5.422 and 5.750 seconds. Both sources remained unavailable, as expected at the PC. No physical MOS write was issued.

Two test-harness issues are explicitly separated from firmware results. The first final run could not save UART text using the Windows default encoding and was repeated after selecting UTF-8. The next complete stability/health run passed its HTTP, uptime, UART and health assertions, then its command expectation failed because it reused a request ID from the previous run; firmware correctly returned `duplicate`. Only the command checks were repeated with fresh unique IDs, and all passed (`invalid_envelope`, `expired`, `disabled`). Future harness runs generate unique IDs. Evidence: ignored `.local/production-sbt-acceptance.json`, `.local/production-sbt-acceptance-uart.log`, `.local/production-sbt-final-command-checks.json`, and `.local/com11-health-heartbeat-write.log`.

Home Assistant now visibly shows collector connected, measurement sources unavailable, BMS reason `disconnected_or_timeout`, Victron reason `waiting_for_advertisement`, control status `disabled`, and disabled MOS switches. The local USB acceptance is complete and COM11 is released. At this USB-only checkpoint, real reception, two-decimal rendered voltages and unchanged-measurement replay still required the subsequent normal-location follow-up below. The 60-second data replay and precision are covered by focused host/configuration checks; absence of radio reception is not a failed local source test. Changes remain uncommitted on the feature branch.


## Normal-location follow-up with real devices

The owner confirmed returning SBT to its normal installation. Initial observation included offline/boot/reconnect and is not counted as a continuous acceptance window. A subsequent 180-second read-only observation received real data from both devices without synthetic MQTT publications or physical commands. Evidence: ignored `.local/production-sbt-real-source-stability.json` and its summary log.

- HTTP succeeded 17/18 times; one four-second request timed out near second 35. Successful samples had monotonically increasing uptime 98,825 -> 268,911 ms, with no observed ESP restart. This is not an uninterrupted HTTP stability pass; the timeout cause is unresolved.
- BMS health arrived 37 times after subscription and MPPT health 35 times. BMS sample sequence advanced 18 -> 48 and MPPT 31 -> 86. BMS availability briefly changed to unavailable near 110.437 seconds, then recovered at 113.500 seconds. The source-health reasons correctly reported disconnected/timeout and waiting for response. MPPT stayed available in the observed health events. Automatic recovery was verified; continuous BMS availability was not.
- Unchanged cell count `14`, NTC count `2`, charge MOS `true` and discharge MOS `true` were independently received again at 35.656, 92.781 and 152.406 seconds, plus the expected restoration snapshot at 113.500 seconds. The complete-minute resend is verified with physical source data, not only host tests.
- HA visibly displayed pack voltage with two decimals, cell minimum/maximum with two decimals, spread `0.04 V`, the added counts/minimum temperature/protection, remaining capacity/activity, MPPT power/current/state/error, fresh source reasons and locked command switches. At one captured point the pack was `50.49 V`, cells `3.58 V` / `3.62 V`, and spread `0.04 V`. These are observed examples, not setpoints.
- The installation control remained false in every successful runtime sample. No actual MOS switching was performed or qualified.

Requested telemetry, display and refresh behavior is verified. Occasional HTTP/radio interruptions remain a documented installation/network limitation; no unproven CM/network-policy change was added to conceal it. PV voltage/current still require VE.Direct hardware. The feature branch remains uncommitted for review.


## Owner-authorized MOS commissioning - partial result

The owner authorized a live test and confirmed independent SBT power and access to the original BMS app for manual recovery. The performance auditor completed its live sampling before this separate test. Firmware was unchanged; no Victron setting or command was sent.

The first harness attempt used the key hint `BmsControl` rather than CM's hashed storage key. `/config/save` rejected it with HTTP 400; no MOS command was issued and both MOS remained enabled. The source-confirmed storage key is `7B8D02D5007C`. This is a harness correction, not a firmware defect.

The subsequent test temporarily enabled the installation control gate and sent exactly one non-retained QoS0 charge-disable request, with a unique ID and 20-second expiry. Baseline had both MOS enabled, zero protection flags, 50.24 V pack voltage and +1.29 A battery current. Immediately before the write, current was +1.78 A and solar power 456 W. The command returned `verified` with charge false and discharge true. Its immediate observed current remained +1.87 A, so acknowledgement alone did not prove current interruption. After a further six-second observation, battery current was 0 A and charge remained false. The BMS reported only bit `0x1000` (`software_mos_lock`), consistent with the commanded software disable. The harness initially treated every nonzero protection bit as an abort condition and therefore stopped before automatic restoration; future commissioning must distinguish this expected state from independent protection faults.

At that point, the charger-side voltage rose from about 50.59 V to 57.44 V and later 58.75 V, while the BMS pack stayed about 50.22-50.23 V with cells 3.553-3.595 V. Victron error code remained zero. These are different electrical measurement points, not an unexplained jump of the battery voltage. The configured Victron absorption value is historically documented as 58.80 V, but the battery chemistry and approved maximum remained unconfirmed.

An attempted restoration action was rejected by automatic approval review before execution because charger-side 58.75 V required battery/voltage-limit qualification. No charge-enable command was sent. The user was informed immediately and asked for battery chemistry/approved maximum and, as a safer recovery option, temporary charger disable through VictronConnect. The last verified state at this checkpoint is charge MOS disabled, discharge MOS enabled, command gate disabled, current 0 A, and only software lock bit set. The discharge-MOS test has not been run. The full test is incomplete; restoration must be verified and appended below before claiming completion.

Private evidence: `.local/mos-live-test-gate-rejected-20261016.json`, `.local/mos-live-test-20261016.json`, and `.local/mos-preflight-20261016.json` (latest read-only follow-up). The latter is a fresh follow-up rather than the original preflight, whose baseline is retained in the live-test record. No retained command, automatic command retry, firmware change, commit or push occurred.


### Recovery clarification from owner-supplied screenshot

The owner reported no current VictronConnect access and referred to `Screenshot_20261015_094016_Overkill Solar.jpg`. Direct visual inspection confirms 14 cells and `Cell Full Voltage` 4150 mV under `Capacity Configuration` (14 x 4.15 V = 58.10 V). The image does not display a chemistry label or the cell/pack overvoltage trigger/release values: the `Protections` section is cut off below its header. This capacity reference is not sufficient evidence of the battery manufacturer's maximum permitted charge voltage. `Screenshot_20261015_093808_Overkill Solar.jpg` shows device identity/status, without that missing limit.

Fresh read-only follow-up: uptime 1,090,029 ms, pack 50.22 V, cell range 3.553-3.595 V, current 0 A, charge MOS false, discharge MOS true, command gate false, software lock 0x1000 only. Victron reported 58.67 V and no error. No additional MOS command or setting write was issued during this clarification. Battery-limit qualification or a safe charger-disabled recovery path remains unresolved.


### Charge MOS restoration completed

After the earlier review rejection, the owner explicitly instructed restoration and confirmed the BMS is correctly configured, also reporting prior successful on/off tests through Overkill Solar. The subsequent reviewed action was permitted. Fresh pre-read confirmed charge false, discharge true, only software lock 0x1000, pack 50.22 V and cells 3.553-3.595 V. A single new, non-retained MQTT charge-enable command returned `verified`, preserving discharge true. Subsequent complete samples confirmed both MOS enabled and protection bits zero. The installation control gate was then disabled and independently read back false. No BMS protection threshold or Victron setting was changed. Evidence: ignored `.local/mos-charge-restore-20261016.json`.

This completes restoration of the original MOS states. Battery current was 0 A in the bounded post-restoration observation, so resumed positive charging is not claimed. Discharge switching remains untested; it was not resumed after the owner's instruction to restore charging.

## Owner-authorized integration checkpoint

On October 16 the owner requested commit, push, main integration and cleanup of
confirmed merged branches. The discharge MOS test is deferred until approximately
19:00 Europe/Berlin so charging can continue during the cheaper period. No MOS
command, firmware upload or device setting change is part of this integration.
The installation gate remains disabled; discharge switching is not qualified.

Fresh pre-integration validation passed the JBD protocol, BMS command policy,
command envelope, status publisher, telemetry contract, MQTT mailbox/lifecycle,
smoke/dependency tests and all eight real-CM settings-registration scenarios.
The USB build passed with 1,354,273 bytes program and 83,960 bytes static RAM.
The existing owner-requested debugging version freeze at 0.3.0 is retained.
No tracked GitHub Actions workflow exists; local checks provide the software gate.
Known HTTP truncation/timeouts and the historical panic remain unresolved and
are handed off to ConfigurationsManager's diagnostic TODO. These findings are
not represented as fixed by this integration.