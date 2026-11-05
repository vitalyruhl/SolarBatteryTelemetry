# Independent correctness review: SBT 1.5.0 and SIL 5.5.0

Date: 2026-10-31. Review scope: uncommitted tracked changes and new source/test files in `C:/Temp/sbt-cell-diagnostics-20261031` and `C:/Temp/sil-operating-modes-20261031`. No product source, governance, branch, settings, device, service, broker or Home Assistant state was changed by the reviewer. The reviewer did not delegate.

## Final assessment

No remaining substantiated Important/Critical correctness finding was identified in the inspected final source. Two Important SIL defects found during this review were reproduced or directly demonstrated by the existing regression boundary, reported to the coordinator, corrected by the implementation owner, and re-reviewed. This assessment does not establish hardware safety, MQTT stability, crash root cause or multi-day behavior.

## Findings addressed during review

### Important: Output gates hid reserve and full-charge latch transitions

Original location: `src/control/OperatingModes.h`, the early OFF/transport/meter/fault returns preceding reserve updates (original lines 51-63) and full-charge updates (original lines 70-73). Current corrected location: `C:/Temp/sil-operating-modes-20261031/src/control/OperatingModes.h:51-60`.

Reproduction against the original production header, compiled directly with clang++:

1. Select household_follow with valid meters/BMS, household 900 W, correction -50 W, maximum 900 W, reserve OFF 2%, reserve ON 10%, SOC 60%. Output is 850 W and the reserve latch opens.
2. Disable the inverter while fresh SOC drops to 1%. Output stops, but the original policy returns before clearing reserve permission.
3. Enable the inverter with fresh SOC 5%. Original output is incorrectly 850 W, even though reserve should remain blocked until SOC reaches 10%.
4. Similarly, charge_priority at SOC 100% latches household following. If meters become unavailable while fresh SOC falls to 90%, and return when SOC has recovered only to 96%, the original policy still outputs 850 W with full_household_follow instead of prioritizing charging back to 100%.

Observed original reproduction:

```
reserve start=850
off low=0
reenabled below release=850 reason=household_follow
full=850
missing meters below threshold=0
meters recover below full=850 reason=full_household_follow
```

This was introduced by the new policy: the existing automatic CurrentControl updates its reserve latch before OFF/meter gates. Recommended correction was to advance valid source-derived latches before output gates while retaining zero-output precedence. The final implementation does that at lines 51-60. The new production-policy regression assertions in `test/host/operating_modes_test.cpp:66-75` exercise both sequences and now pass. The isolated reproduction executable was removed after use.

### Important: Failed persistence readback could activate an unconfirmed mode after reboot

Original locations: `src/control/ModeProfile.h` write/readback sequence and `src/control/ModeSettingsRuntime.h` active-profile write. Current corrected locations: `C:/Temp/sil-operating-modes-20261031/src/control/ModeSettingsRuntime.h:79-82` and `C:/Temp/sil-operating-modes-20261031/src/control/ModeProfile.h:28-40`.

Original reproduction: start with confirmed household_follow; request charge_priority; let active-profile putBytes succeed but fail the following readback. Runtime correctly retained household_follow and reported save_failed, but reboot loaded the already-overwritten charge_priority record. The original runtime host test explicitly asserted that behavior. It violated the requested failure-preserves-old behavior across reboot and could activate a mode that had never been acknowledged.

The final code writes to `SILModes/staged`, completely reads and validates that record, then atomically replaces the active `profile`. Boot reads only `profile`. A complete successful active NVS write is now the activation commit acknowledgement; there is no subsequent readback whose failure can misclassify a committed change. Failure tests cover staged write/readback, corrupt readback, short write, namespace open and active promotion, including old-mode restoration on reboot. The final adapter test passes. This adds two bounded writes for an actual change and zero writes for a duplicate; documentation was synchronized.

## Other inspected contracts

SBT: checked 03/04 cell pairing, malformed/partial-cycle preservation, 14-group array bounds and stale validity; bounded serializer, sequence/age/acquisition metadata, last-successful-array delta cache and failure retry; global ordinary one-attempt/100 ms scheduler, rotating producer fairness, scalar cursor interaction and full refresh; urgent control path and OTA admission fence; retained historical panic publication, root retirement and broker ownership; fixed-storage runtime diagnostics and additive HA sensor availability. No additional substantiated Important/Critical bug found.

SIL: checked measured inverter plus signed grid demand, exactly-once correction before clamping, explicit inverter/transport OFF, stale BMS distinction, reserve and physical MOS feedback, charge threshold/share/full latch, directional discharge-alarm mapping, mode persistence/confirmation, deferred UI/MQTT path, expiring command parser, subscription/root handling, output application and HA confirmed select. The latest directional mapping `source.alarmRestricted && !source.softwareMosLockOnly` is intentional: a charge-only restriction is not automatically a discharge restriction.

Existing automatic MOS emergency-OFF behavior remains present and documented. No new recovery supervisor or MOS-policy removal was part of this review. The physical consequence of actual MOS topology/direct PV operation remains hardware acceptance, not a validated software conclusion.

## Reviewer-executed validation

- Existing SBT `.pio/host-tests/cells.exe`: passed; 104 ordinary attempts / 11000 ms, first cells 100 ms, last initial scalar 3700 ms, last refreshed scalar 8478 ms.
- Isolated production-policy reproduction above: confirmed original latch defects before the fix.
- Final SIL `.local/operating_modes_test.exe`: passed.
- Final SIL `.local/mode_settings_runtime_test.exe`: passed.
- Final SIL `.local/operating_modes_application_test.exe`: passed.
- Both worktrees `git diff --check`: no errors; SBT emitted existing CRLF normalization notices.

During implementation, the new SIL RED test executables were also observed failing at the intended latch and reboot-preserves-old assertions. Those were intermediate RED results; the final rerun above was green.

The reviewer did not repeat full firmware builds; the implementation owners/coordinator own USB/OTA build evidence. ProjectAtlas session briefs reported missing indexes for both isolated worktrees and explicitly permitted filesystem fallback; no index was created during this read-only review. No live UI, hardware, broker publication, service operation, upload, commit, push, merge or release was performed. Long-term stability and the historical SBT panic cause remain unproven.

## Final bounded root-guard follow-up

The coordinator requested a final review of the last SIL root-guard clarification only, after the preceding assessment. Inspected `src/main.cpp:709`, `1198-1203`, `1625-1633`, `src/control/ModeSettingsRuntime.h:110-113`, and `test/host/mode_settings_runtime_test.cpp:38-46`.

The command callback now compares the configured root against the root configuration snapshot even when the configured value is empty/fallback. The main loop repeats the mismatch check immediately before draining queued operating-mode requests. The root snapshot is updated before the effective-root early return, covering configuration changes whose resolved topic root is unchanged. Discarding affects only expiring MQTT requests, retaining independent UI requests with expiresAt=0. The existing disconnect/root-change discard path uses that same distinction.

No additional Important/Critical defect was identified in this bounded follow-up. The final adapter executable was rerun by the reviewer and passed, including expired/invalid-clock/discarded MQTT commands and preservation of an independent UI request. No broader tests or firmware builds were repeated by the reviewer; the implementation owner/coordinator owns final incremental USB/OTA build completion.
