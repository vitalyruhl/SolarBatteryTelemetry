# MOS request state implementation plan

> For agentic workers: execute the owner-approved design in this existing dirty branch; preserve unrelated work. No automatic commits or hardware commands.

Goal: distinguish acknowledged software requests from actual conducting FET states and preserve the opposite software lock.

Architecture: RAM-only E1 ACK ledger in BmsCommandPolicy, nullable provider/MQTT request fields, unchanged actual scalar sensors, explicit full-pair initialization in ESP and HA. Never seed software permission from actual flags or global protection bits.

Design approval: owner confirmed the exact separation, opposite preservation, joint initialization, unknown restart state and command-admission gate semantics on 2026-10-22.

- [x] Add failing policy tests for unknown singles, explicit pairs, opposite preservation and uncertain writes. Implement ledger and acknowledged result.
- [x] Extend strict MQTT envelope with a five-field full-pair command; test type, replay and expiry constraints.
- [x] Add runtime controls and diagnostic separation; test real readout/queue/ACK and disconnect paths.
- [x] Update HA request-state mapping and joint actions; preserve actual observations and gate. Validate configuration before reload; never press a MOS action automatically.
- [x] Review integration, run focused host tests and USB firmware build, record artifact identity.
- [x] Deploy the frozen artifact under the existing owner OTA authorization; activate validated HA changes and verify boot/passive unknown state. No new MOS transaction during rollout.

Review focus: ACK loss; link generation changes; gate cancellation before/after transmission; actual flags differing from request; initialization buttons usable while singles are unknown. No independent app permission readback or protection-threshold changes.
