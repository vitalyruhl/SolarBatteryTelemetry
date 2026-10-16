# SBT runtime performance diagnosis - 2026-10-16

## Outcome and confidence

**Confirmed:** the production HTTP path can return headers quickly and then truncate the response body. This reproduces missing live cards without a device reboot. BMS layout and metadata both failed during a separate serial request sequence. The full metadata endpoint also returned HTTP 503. The last boot reports a panic reset, but this audit did not capture a new panic or its cause.

**Most relevant CM investigation:** bounded response delivery and its interaction with AsyncTCP ACK timeouts, followed by JSON-generation allocation pressure. **SBT optimization opportunities:** repeated schema data in every runtime response, redundant loop snapshot/status work, and oversized fixed status buffers. None is yet proven to cause the earlier panic. Blanket removal of Arduino Strings is not justified.

No product changes, configuration writes, commands to BMS/Victron, restarts, firmware uploads, commits, or Git mutations were performed by this diagnostic agent. The separate commissioning activity coordinated by the parent is outside this report.

## Inspected state and method

- SBT branch: `feature/bms-required-telemetry-controls`; HEAD `861068793d3abf064e8d2c1df0888aa1a686d5a2`, with pre-existing uncommitted implementation changes.
- Active CM binding: `C:/Daten/_Codding/ESP32/ConfigurationsManager-demand-runtime`, HEAD `554869da7efc98e0c1874e74bf0c78d4bcd1664a`. Its only reported working-tree change was local `.serena/project.yml`.
- Local USB firmware: version 0.3.0, SHA-256 `278d9541b769c999ab3a76986b3953b9f32a05c88d071845fbde714db64086c4`; binary size 1,360,848 bytes. Existing ELF and BIN timestamps: 2026-10-16 13:07:19/20 local. These identify the local artifact; this audit did not read firmware flash back from the running device.
- Target: `192.168.2.21`, real BMS/Victron installation. COM7 / SIL `.126` untouched.
- Existing acceptance evidence was treated as context, not current proof. Fresh evidence is ignored `.local/performance-baseline-20261016.json` and `.local/performance-pages-20261016.json`, with corresponding Python helpers. Evidence files contain private runtime data and must remain local.
- Approximately 100 seconds of passive MQTT subscription, unique client identity, explicit keepalive, serial HTTP requests with a 6-second socket timeout, no command publication. One IAB navigation to `/` occurred during the later portion of this window. Thus the entire window must not be represented as browser-free isolation. A later six-request page sequence had no further browser navigation. Sampling ended by 13:43:58 local, before the parent's separate commissioning window.
- HTTP `ttfb` measures time until response headers were available to urllib, not an on-device CPU timer. Full elapsed time includes response-body delivery.
- Governance: local SBT AGENTS, `.github/AGENTS.md`, control-plane, refactor and docs roles; CM AGENTS, `.github/AGENTS.md`, audit role. SBT `origin/main` has no tracked `.github/AGENTS.md`; parent fetched and confirmed current remote head. SBT audit role is absent, so diagnosis uses the local safe read-only allowance and docs role for this report. Atlas orientation was used; its SBT index was stale and CM index absent, so exact current file reads were used without modifying either index.

## Reproduction and API contract

| Request | Body bytes | Headers | Full body/result |
| --- | ---: | ---: | --- |
| `/live_pages.json` | 318 | 63 ms | 141 ms, valid JSON |
| `/runtime.json` | 3,228 | 79 ms | 766 ms, valid JSON |
| `/runtime.json?groups=["bms"]` (URL encoded) | 1,857 | 63 ms | 750 ms, valid JSON |
| `/runtime.json?groups=["bluetooth","smoke"]` | 335 | 78 ms | 156 ms, valid JSON |
| Combined runtime groups, repeated | 3,225-3,227 | 62-110 ms | Successful samples 656-2,579 ms; one truncated response |
| `/runtime_meta.json` | Not obtained | - | HTTP 503 in 141 ms |
| `/live_layout.json?page=bluetooth` | 1,345 | 188 ms | 485 ms, valid JSON |
| `/runtime_meta.json?page=bluetooth` | 1,790 | 125 ms | 1,890 ms, 11 fields |
| `/live_layout.json?page=bms` | Expected 3,166 | 125 ms | `IncompleteRead(739 bytes read, 2427 more expected)` after 5,656 ms |
| `/runtime_meta.json?page=bms` | Expected 5,319 | 141 ms | `IncompleteRead(512 bytes read, 4807 more expected)` after 5,500 ms |
| `/runtime_meta.json?page=victron` | 2,127 | 110 ms | 1,969 ms, 12 fields |
| `/runtime_meta.json?page=mqtt` | 1,094 | 78 ms | 344 ms, 7 fields |

The combined runtime failure was `IncompleteRead(1024 bytes read, 2203 more expected)` after 5,656 ms, at observation t=52.656 s. These are early end-of-stream failures, not just a client deciding a valid JSON document was too slow.

The returned page keys are lowercase `bluetooth`, `mqtt`, `bms`, `victron`; default is `bluetooth`, and `pageScoped=true`. Two initial requests using `page=BMS` returned expected 404 responses. Those were diagnostic-harness mistakes and are excluded from product findings.

CM `webui/src/runtimeDataClient.mjs:62-98` serializes each page's layout then metadata request and caches successful results in browser memory. `readJson` at lines 4-25 enforces a 5-second timeout through body consumption. Runtime values use a 4-second timeout (`RuntimeDashboard.vue:1591`); failed scoped pages show a 5-second retry (`:2615-2618`). A partial BMS layout prevents the subsequent metadata request and hence card construction. This matches the observed symptom; no wrong group/page contract was found.

The single browser navigation failed with `ERR_CONTENT_LENGTH_MISMATCH`. The tool then blocked its generated `data:` error-page URL, preventing normal rendered-dashboard inspection. The original browser error was visible in the tool result, but successful browser/card acceptance and screenshots were not obtained. The failed tab was an agent-created temporary tab; an explicit cleanup attempt was also blocked by the same error-page URL policy. It was not reloaded.

## Reset, MQTT, WiFi, heap and stack evidence

- Successful runtime samples increased from uptime 400,707 to 490,466 ms. No reset occurred between those samples.
- `smoke.resetReason=4` means `ESP_RST_PANIC` in the installed ESP-IDF `esp_system.h:42-46`. This is evidence of a panic before the observation window, not proof of heap exhaustion, stack overflow, watchdog reset, or which component panicked. No panic backtrace was available.
- Each source produced 20 non-retained health messages in about 100 seconds; all reported available. Sequences advanced BMS 66 to 84 and Victron 117 to 145 including the initial retained samples. Victron health arrived at t=54.578 s, during the failed HTTP transfer. MQTT status observations stayed online.
- Maximum receive gaps were 10.203 s for BMS and 7.047 s for Victron, with two BMS messages collected together after the longer gap. This is evidence of jitter/batching, not a verified hard 5-second service guarantee or a quantified loop stall.
- Fresh non-retained System-Info samples: free heap 33,000 and 29,640 bytes, minimum-ever free heap 17,336, maximum allocatable block 16,372; WiFi RSSI -57 dBm. The initial retained sample was older (uptime 375,868) and is not a contemporaneous baseline. Two fresh points cannot establish a leak or a response-generation heap minimum.
- BLE counters remained queueDrops=1, reconnects=6, lastError=-1003, lateEvents=14. These are historical counters/errors, not fresh failures during this window.
- BLE worker stack high-water free bytes remained 5,824 of an 8,192-byte allocation (`CM src/bluetooth/BluetoothManager.h:55,68,261`). Observed maximum usage is therefore about 2,368 bytes for this boot history. That does not cover every future reconnect/error/OTA/control path.
- Existing ELF static DRAM is 83,960 bytes: `.dram0.data` 26,000 + `.dram0.bss` 57,960. Static object symbol sizes: Bluetooth manager 9,152, telemetry MQTT service 5,504, BMS readout 2,640, Victron readout 1,400 bytes. Task stacks, NimBLE/WiFi internals, JSON allocations and TCP buffers are additional runtime allocations.
- Missing measurements: main-loop, AsyncTCP and NimBLE task high-water marks; per-phase loop maximum/percentiles; metadata allocation failure reason; per-request peak heap/largest block; TCP ACK/retry trace; panic backtrace. AsyncTCP source default stack is 16,384 bytes, but this is not a measured safe reduction budget. Do not infer stack safety from static ELF RAM or a single function's stack frame.

## Mechanisms and targeted opportunities

### 1. CM response delivery is the first investigation target

`src/web/BoundedResponse.h:61,83-110` caps unacknowledged data at 512 bytes per connection. Headers and body are admitted separately; response completion explicitly closes the connection. Both runtime JSON and the root asset use this sender (`WebServer.cpp:1128`, `:944`; `StaticGzipResponse.h:6-12`). The ELF gzip UI asset is 60,407 bytes, requiring at least 118 body admission windows. This can amplify delayed ACK, radio scheduling and packet-loss latency despite fast generation/headers.

The installed AsyncTCP defines `ASYNC_MAX_ACK_TIME=5000` (`AsyncTCP.h:58`), checks it in `AsyncTCP.cpp:997-1005`, and the web request timeout callback closes the client (`WebRequest.cpp:213-217`). The observed ~5.5-second truncations are consistent with that path. **The ACK-timeout explanation remains unproven** without trace/counters; RF loss, scheduling pressure, sender bookkeeping or another close path may be involved. HTTP headers alone do not prove complete JSON generation either, but the repeated body truncation is established.

Proposed validation: instrument aggregate per-request sent/acked bytes, add/send failures, close reason and duration; capture one root load plus one BMS page without parallel polling. Then compare 512 versus a bounded MSS-sized/two-segment window, one change at a time. Keep a global outstanding-memory budget and slow-client/cancel tests. Expected benefit: fewer ACK-gated rounds; risk: larger TCP heap demand. Raising timeouts alone would mask the failure and prolong occupied buffers.

### 2. CM metadata builds allocate repeatedly and retain output during slow transfer

`RuntimeManager.cpp:148-159,337-416` constructs a JSON document per field to measure it, reserves the complete output String, then constructs another per field to emit it, while holding the registry mutex. Selected-page requests call this path directly (`ConfigManagerLayout.cpp:559-562`). Full metadata uses a weak cache (`RuntimeManager.h:278-281`, `RuntimeManager.cpp:425-474`): once the last response releases its payload, a later request rebuilds it. The weak lifetime saves resident heap and must not simply become a permanent ~10 KB cache on this device.

The installed ArduinoJson is **7.4.3**. `StaticJsonDocument<1536>` inherits dynamic `JsonDocument` (`ArduinoJson/compatibility.hpp:62-70`); it is not a 1,536-byte fixed stack pool. Recreating it per field is heap churn. Reuse a bounded scratch allocator/document across the size and emit passes, or use a checked serializer that preserves escaping and schema semantics. Cache immutable normalized placement keys/order rather than copying full JSON responses indefinitely. `ConfigManagerLayout.cpp:572-707` also rebuilds sorted pointer lists and placement JSON fragments, with duplicate scans and a second sizing/emission pass.

Expected benefit: fewer allocator operations, shorter mutex hold and lower transient allocation peaks; exact savings need allocation counting. The current fast header times make this a secondary latency explanation, while the observed full-metadata 503 warrants instrumentation. That 503 does not identify which allocation/build step failed. CM logging is disabled by SBT build flags, so existing failure log statements do not provide that missing evidence.

### 3. SBT repeats invariant schema data in every values response

`src/bms/BmsReadout.h:48-52` and `src/victron/VictronReadout.h:64-70` emit `types` and `support` on every read. Removing those objects in an offline copy reduces the captured full runtime JSON from 3,228 to 1,535 bytes: **1,693 bytes / 52.4%** are repeated schema descriptions. This is a measured wire-size opportunity, not an implemented change.

Propose an explicit slim values request for the GUI, with schema loaded once and the existing full contract preserved for other consumers. Verify all UI consumers before changing it. Expected benefit: about half the full-response wire bytes for this sample, fewer JSON nodes and ~4 instead of ~7 body windows at 512 bytes. Group filtering already works: CM selects providers before calling them (`RuntimeManager.cpp:296-329`); there is no basis for claiming all providers run on every selected request.

### 4. SBT loop performs avoidable work even with unchanged data

`src/main.cpp:147-164` refreshes both adapters, copies snapshots, updates all channel values, then runs MQTT every loop. `BmsReadout.h:22-28,138` and `VictronReadout.h:33-38,89` rebuild/copy views each pass. `MqttService.h:47-51` (called at lines 147 and 168 within `loop`, which starts at line 143) formats the same control JSON twice per loop, takes the control mutex twice, and compares/copies a slot even when control state is unchanged. Source health already has a 5-second/change guard (`MqttService.h:38-45`), and telemetry publication is bounded to four cursor steps (`TelemetryPublisher.h:98-100`); the 60-second full refresh is not an unbounded burst in a single loop.

Propose generation/dirty flags for immutable snapshots and control status, while explicitly scheduling freshness expiry, availability transitions and pending command deadlines. Expected benefit: fewer snapshot copies, mutex acquisitions, string scans and snprintf calls. No measured CPU percentage is available. Preserve immediate transitions and safety timing; do not broadly slow the main loop or move blocking BLE work into it. BLE radio operations already run on the separate worker, with a 10 ms yield; the inspected UI callbacks only copy/enqueue state.

### 5. Target fixed budgets after instrumentation

- `StatusPublisher.h:13` reserves four 512-byte payload slots (2,048 payload bytes). Current producing buffers are 256 bytes for each health message and 384 for each control/result message. Matching capacities would save **768 bytes** of static payload storage, subject to a producer-size contract/test.
- `TelemetryPublisher.h:116` uses two 512-byte scratch arrays for every channel, although most scalar payloads require much less. Specializing protection formatting versus numeric fields can reduce peak stack scratch by roughly 1 KB on common paths; actual task high-water must be measured first. Shared global scratch would introduce concurrency/reentrancy hazards.
- A BLE worker reduction from 8,192 to 6,144 bytes would nominally recover **2,048 heap bytes**, leaving about 3,776 free relative to this boot's measured maximum. This is only a candidate after reconnect, lost-source, OTA and allowed control-path stress on a separate authorized test device. Do not shrink AsyncTCP/main/NimBLE stacks without their high-water measurements.
- Metadata registration currently totals 64 fields (11 transport, 7 MQTT, 4 availability/reason, 9 control status, 29 channels, 4 buttons). `RuntimeManager.cpp:219` uses an unreserved vector; an additional field can grow its capacity. Avoid designing around a magic 64-field ceiling: quantify `sizeof(RuntimeFieldMeta)` and vector capacity, then reserve exact expected count or change storage deliberately. Short strings use SSO in the installed Arduino String (`WString.h:305-323`), so replacing every String with heap-owning alternatives or large char arrays can regress memory.
- CM System-Info builds both JSON documents/payloads in one scope (`MQTTManager.h:1693-1725`) once per 60 seconds in the observed run. Reusing one scratch document and releasing the ESP payload before building WiFi can lower its temporary peak. This is lower priority than HTTP delivery. SBT telemetry uses `publishRaw`, already avoiding CM's payload-preview String path.

## Proposed approval plan and acceptance evidence

1. Add low-cost diagnostics first: per-phase loop max/percentiles, task high-water marks, heap/largest-block before and after JSON generation, response completion/close counters, and persistent panic information where supported. Avoid emitting full configuration or credentials.
2. Address the confirmed truncated HTTP delivery in CM, testing its transport invariant independently of metadata generation. Measure one cold root navigation and each page, plus a slow/cancelled client; require exact Content-Length completion, no silent truncation, recovery after failure, and stable MQTT/BLE progress.
3. Address JSON churn and the slim SBT GUI values path separately. Compare body sizes, generation time, minimum/largest heap blocks, and existing JSON/UI contract tests. Preserve legacy clients and invalid/stale values.
4. Apply small SBT dirty-state and fixed-buffer changes with focused host tests, then compare loop timing. Only thereafter consider measured task-stack reductions.
5. Revalidate on authorized hardware over idle, page navigation, MQTT refresh/reconnect, BLE reconnect and OTA paths. No production control exercise is needed to diagnose HTTP. A later separately authorized control test must not be presented as performance proof.

This audit changed only this report and ignored local diagnostic evidence/helpers. No firmware rebuild or product test suite was run because product source was unchanged. Documentation validation: current references and observed values checked; explicit whitespace validation passed for the new untracked report (Git diff alone does not inspect untracked files). The performance issue is reproduced and localized, not fixed; the earlier panic remains unexplained.
