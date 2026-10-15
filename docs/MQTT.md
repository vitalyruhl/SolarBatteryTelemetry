# MQTT preparation

Firmware 0.3.0 prepares MQTT connectivity and a fixed scalar channel contract. Device adapters are absent: BMS and Victron source availability remains `unavailable`, every scalar is JSON `null`, and the runtime view says unavailable. No readings, SOC fallback calculation, charge/discharge inference or battery-side power calculation are invented. Bluetooth still has no peer/profile and remains Disabled/Idle without radio initialization.

## Settings and ownership

The standard persistent MQTT keys are `MQTTEnable`, `MQTTHost`, `MQTTPort`, `MQTTUser`, `MQTTPass`, `MQTTClientId`, `MQTTBaseTopic`, `MQTTPubPer`, and `MQTTListenMs`, under MQTT / MQTT Settings. Defaults are seeded before registration/loadAll; existing persisted edits, including an intentionally empty password, win. With real ignored secrets the default is enabled and uses the local broker credentials; placeholder-only builds default disabled. The default base is `SolarBatteryTelemetry`. A deterministic client ID contains all 48 MAC bits, independent of the limiter's identity.

The consumer registers one standard CM Settings object for UI/persistence. A separate, unregistered instance of the public MQTT singleton's settings belongs only to the application loop. The consumer intentionally does not call `MQTTManager::attach`, which would expose those live settings and install client-mutating callbacks. UI callbacks copy only bounded values/revisions into an owned mutex-protected mailbox; no client operation occurs there. After loadAll, initial values enter that mailbox before web-server startup because CM's persisted load does not invoke change callbacks. The application loop applies the latest snapshot, including changes received during OTA. Runtime readers get a synchronized copy, never UI Strings or live client state. This separation does not claim to fix all inherited CM settings-storage concurrency.

Public standalone `begin`, `loop`, `settings`, `setServer`, `setCredentials`, `setClientId`, `setLastWill` and `publishRaw` are used. The existing module still publishes its normal `<base>/System-Info/ESP` and `/WiFi` metadata. The custom retained LWT is `<base>/status=offline`; online is published on connection and retried in each full prepared snapshot. Publish success means local QoS0 admission, not broker acknowledgment.

## Fixed channels

All 25 descriptors, labels, units, type/support names, state vocabulary and reserved command names are referenced by the production runtime and serializer. There are no per-field editable topics or allocated setting objects. Telemetry is non-retained. Each complete snapshot is sent at most once per 5 seconds (or the configured publish interval when longer); reconnect forces a fresh complete snapshot. At most four normal publications are attempted per policy tick, with a finite cursor and a 250 ms admission retry. A pending old-root retirement can add three sends before that normal budget; a root transition or configuration disconnect retires at most two advertised roots (six sends). CM's independent automatic online/System-Info messages are outside this preparation budget. A failed item does not mark the snapshot delivered. There is no historical sample backlog.

| Source | Leaf | Type | Unit / meaning |
| --- | --- | --- | --- |
| bms | charging | bool | Actual charging activity; unknown |
| bms | discharging | bool | Actual discharging activity; unknown |
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
| bms | alarm_bits | unsigned | Bitfield, unknown |
| victron | solar_power_w | real | W |
| victron | pv_voltage_v | real | V; unsupported by selected Instant Readout Solar record |
| victron | pv_current_a | real | A; unsupported by selected Instant Readout Solar record |
| victron | battery_voltage_v | real | V |
| victron | charge_current_a | real | A, charger output to battery; not net battery current |
| victron | battery_power_w | real | W; reserved for later valid voltage/current derivation |
| victron | load_current_a | real | A |
| victron | charge_state | enum string | off, fault, bulk, absorption, float, storage, equalization, other, unavailable |
| victron | error_code | unsigned | Unknown |
| victron | yield_today_wh | real | Wh |

Topics are `<base>/<source>/<leaf>`. Both `<base>/bms/availability` and `<base>/victron/availability` publish retained `unavailable` before scalar data. `<base>/bms/control/availability` is retained `disabled`. The retained `<base>/capabilities` JSON contains schema_version=1, read_only=true, commands_enabled=false, source reason adapter_missing, unsupported PV fields, deferred battery power, charge-state vocabulary and full reserved `<base>/bms/charge_enabled/set` and `<base>/bms/discharge_enabled/set` paths. They are metadata only: no subscriptions, message handlers, device writes or Home Assistant command discovery exist. Bulk means the main charging stage, not an alarm. Retained incoming commands cannot actuate anything in this firmware.

Values have explicit validity. Real values must be finite; unsigned values must be integral and within uint32 range; invalid booleans/enums are null. Unsupported and deferred fields remain null even if accidentally given a value. The future adapter must verify protocol mappings and freshness before it can establish source availability; the generic prepared publisher is not a protocol decoder.

## Bounds, root migration and OTA

The operational root limit is 63 ASCII bytes. Reject empty values, `+`, `#`, control/non-ASCII/space characters, quotes/backslashes, leading/trailing slash and empty segments. Nested prefixes such as `shed/collector` are accepted. The consumer protects topic construction (128 bytes), prepared payloads (1024 bytes) and a 1536-byte MQTT packet buffer. Broker/user/password copies are bounded to 128 bytes, client IDs to 64 bytes, ports to 1..65535. Publish period is clamped to 5..3600 seconds; listen period to 0..1000 ms. Invalid values leave the last valid active value in use and show rejection on the Telemetry page. These are consumer operational checks: CM's generic UI/backend/persistence does not enforce a String storage bound, and a callback cannot veto storage. No silent truncation occurs in application-owned topic paths.

A root change retires old retained source availability/status, disconnects, replaces the LWT and reconnects. The publisher separately retains one advertised-root cleanup obligation and the latest desired root. Failed retirement is retried; a disconnected rename can connect under its selected root to retire the previous one, while subsequent desired changes wait in the bounded latest slot. Never-connected intermediate roots create no cleanup history. While cleanup is pending, the active root view can lag the saved desired root. After a transition no ordinary channel publishes use the previous root; only its explicit retirement does. Every accepted online publication restores the current advertisement obligation, including after delayed same-root cleanup. Before a configuration disconnect, both an older pending root and the current advertised root are retired when local sends succeed. Failed same-endpoint retirement remains pending without blocking disable indefinitely. If host/port changes and old-endpoint retirement fails, the old obligation is explicitly abandoned as unconfirmed, never sent to or falsely discharged on the new broker. The Telemetry page keeps a previous-broker-cleanup-unconfirmed warning for the rest of the boot. Graceful retirement is best effort local QoS0; an unreachable old broker cannot be proven cleaned when broker configuration changes. Downstream consumers should require collector `/status` and source `/availability`, not assume a collector LWT changes two source topics.

The existing OTA observer pauses MQTT attempts and preparation publications at Starting and keeps the success latch paused until reboot. The loop rechecks the pause after the publication phase before invoking CM's network phase, including when Async OTA starts during that publication phase. Failure/abort resumes from the application loop. BLE's independent OTA pause remains composed with its other pause reasons. `ConfigManager.handleClient()` is serviced before MQTT each loop. `MQTT_SOCKET_TIMEOUT=1` is applied globally to PubSubClient 2.8, retries occur at 10-second cadence, but MQTT/TCP is still synchronous: the private Arduino WiFiClient can block about 3 seconds for TCP plus up to 1 second for MQTT, and DNS may add time. A pause cannot cancel an already-running synchronous call. No low-latency, coexistence, outage or physical OTA result is claimed by host tests.

If OTA was disabled at boot, enabling it in the UI on the same WiFi connection requires a restart before ArduinoOTA testing, due to inherited CoreWiFiServices initialization behavior.

## Build and acceptance

Tracked consumer dependencies are NimBLE-Arduino 2.5.1 and PubSubClient 2.8. The reviewed local CM 4.5.0 remains an ignored absolute override and is unpublished. Both local environment dependency lists must retain all three dependencies; do not replace the tracked MQTT dependency when configuring the override. Commands build only:

```powershell
./tools/test/run-smoke-host-tests.ps1
pio run -e usb -e ota
```

The 0.2.0 actual binary was 1,298,272 bytes, leaving only 12,448 bytes in the old 1,310,720-byte slots, already below the new 10% reserve requirement. The full MQTT/BLE build determines the final candidate size. The default-slot MQTT candidate exceeded capacity, so the approved consumer table now uses two 0x1c0000-byte app slots with 0x60000 SPIFFS, unchanged NVS/otadata/coredump extents, and a 4 MB boundary. No application filesystem use was found in collector/CM source; the UI is embedded as WEB_HTML_GZ. A changed partition table requires full USB bootstrap/partition provisioning. An app-only OTA upload cannot perform that migration. Physical flash contents are not inspected or promised preserved by source analysis.

The user's 488,393-byte scaffold result has no proven equivalent exact source/flags here. Prior same-round CM probes show WiFi-dependent features differ by about 553 KB, OTA by about 53 KB and MQTT by about 39 KB; these are configuration comparisons, not additive ownership totals. Embedded UI, WiFi/network, web/OTA, libc and BLE all contribute to the initialized image; logging was already off. Do not attribute the whole size difference to NimBLE.

Consumer central/observer role removal and LTO remain in use. The known upstream NimBLE `is_ble_hs_resolv_enabled` declaration/definition mismatch warning is preserved. Previously examined exact ELFs omitted the incompatible checker/caller path; that is an exact-image assessment, not a warning-free or future universal compatibility guarantee. Final candidate sizes, hashes, partition parsing and exact-ELF warning assessment are recorded in local acceptance evidence and the implementation report. Workstation broker writes, hardware flashing and full physical tests are separate gates.

## Installed candidate, 2026-10-15

The final 0.3.0 USB/OTA images are each 1,328,448 bytes, leaving 506,560 bytes (27.61%) per 1,835,008-byte OTA slot. Focused tests, independent review, exact ELF/BIN identity and binary partition checks passed. COM11 received the full bootstrap, partition and application bundle with every upload hash verified. NVS was not erased. Live broker delivery, a full OTA transfer and BLE-device acceptance remain pending. See [the exact acceptance record](acceptance/2026-10-15-mqtt-preparation-0.3.0.md).
