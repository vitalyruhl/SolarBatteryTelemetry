# Cell telemetry and panic evidence implementation plan

> **For agentic workers:** Use the approved conversation requirements and execute with focused red/green host tests, followed by a fresh whole-change review. No commit or deployment is authorized.

**Goal:** Deliver coherent 14-cell telemetry to Home Assistant with bounded, sparse MQTT work and preserve panic evidence without enabling Live UI.

**Architecture:** Preserve the existing scalar topic contract. Carry raw cell millivolts in the completed BMS snapshot and publish one bounded numeric-array packet with a shared sample sequence. Pace ordinary application publications by elapsed time rather than loop iteration count. Retain the bounded historical panic summary so a later subscriber can inspect it, explicitly distinguishing a stored dump from a new crash.

**Tech stack:** C++17, ESP32 Arduino/PlatformIO, existing ConfigManager 4.10.5 freeze, MQTT, Home Assistant.

**Spec:** Owner instructions in this chat on 2026-10-31, refining the six-point investigation. Automatic MOS removal remains a separate conditional plan pending hardware protection evidence. Recovery automation is deferred.

## Global constraints

- Keep `CM_ENABLE_LIVE_UI=0`; no new automatic MOS writes.
- No firmware upload, commit, push, merge, public report, or device-setting change.
- Preserve the exact local CM freeze; do not silently replace transport dependencies.
- Missing/stale data remains unavailable, never a fabricated zero array.
- Retain source availability and freshness semantics, command cancellation, OTA fences and test-profile topic isolation.
- Cell acquisition is one complete checked 03/04 cycle, not a mixture of cells from different cycles.
- Full refresh remains configurable and is spread across small publications; the current device setting of 100 seconds is not changed implicitly.
- Use fixed bounded storage; add no large stack snapshots or unbounded JSON allocations.

## Tasks

- [ ] **1. Checked cell snapshot and bounded array publication.** Extend `src/bms/JbdProtocol.h` with 14 raw millivolt cells and a validity state in the completed snapshot. Add a separate bounded cell publisher rather than extending the existing 32-bit scalar cache past its limit. Test exact 14-cell order, malformed/incomplete frames, stale/unavailable samples, unchanged suppression, failed-publish retry and forced refresh with real protocol/publisher code.
- [ ] **2. Sparse paced MQTT scheduling.** Inspect the existing cursor/budget and preserve it. Add elapsed-time admission for ordinary application telemetry with no catch-up burst, fairness for cell packets and full-refresh work, and immediate OTA fencing. Document the actual bound; do not promise a wall-time bound for the synchronous underlying transport. Test burst limits, rollover, failed admission and refresh completion.
- [ ] **3. Durable historical panic evidence.** Preserve the bounded diagnostic payload as retained historical evidence under the existing root/test-profile rules. Test retained flag, root transition cleanup, disabled/OTA behavior and no fabricated crash timestamp. Do not classify an old dump as a newly observed panic.
- [ ] **4. Documentation, HA entities and validation.** Bump the compatible firmware minor version, update relevant MQTT/BMS docs and add 14 non-optimistic HA cell entities plus source quality metadata. Validate host behavior and USB/OTA builds without uploading. Prepare additive HA configuration; activate only through the owner's authorized HA configuration interface, without fake sensor values.

## Review focus

- No mixed-cycle cell arrays after partial frames, reconnect or timeout.
- No starvation or burst on unchanged scalars, full refresh or retry.
- No recovery of publication during OTA pause.
- No sensitive bytes, stale values presented as live, or production topics from the test profile.
- No additional automatic MOS command or application stack growth hidden by the telemetry change.

## Decisions and evidence

- Native worktree creation cannot address SBT from the Hermes-bound chat. A separate Git worktree is used at `C:/Temp/sbt-cell-diagnostics-20261031` on `feature/cell-telemetry-diagnostics`.
- Existing application scalar publication already has a cursor; reuse it rather than add loop-count timing.
- Raw cell values use 14 x `uint16_t` (28 bytes), not fourteen new generic double-valued channels.
- Production activation and multi-day acceptance remain separate from host/build validation.
