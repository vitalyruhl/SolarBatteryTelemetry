> **Historical evidence; superseded on 2026-10-20.** This document preserves a previous plan and its original execution/acceptance record. Its tasks, checkboxes, version targets, device instructions and permissions are historical, not current execution authority.
>
> The [single current SIL/SBT/HA integration plan](../../../SolarInverterLimiter/docs/superpowers/plans/2026-10-20-sil-sbt-ha-integration.md) owns all subsequent work and current owner approvals, including the test-ESP build/upload, Settings/Live checks and at least five minutes of observation for each consumer. Earlier COM7 restrictions or approvals below do not override that current scope. Production uploads are not authorized. This consolidation performs documentation work only; implementation and testing start in a later step.

---

# JBD BMS Readout Implementation Plan

Approved scope: user requested integration on an extra branch. Resume the existing BLE integration plan, task 5, using the shipped CM worker and MQTT contract. Keep firmware version 0.3.0 during the current debugging cycle as previously requested. No automatic commits or pushes.

- [x] Implement and host-test bounded JBD framing, checksum, signed quantities, 14-cell validation, basic/cell cycle freshness, disconnect and fragment handling.
- [x] Register one BMS MAC setting; local secret default identifies the owner-provided Powerwall. Never publish the actual peer address or credentials.
- [x] Route CM events to both adapters, poll basic/cell registers serially every five seconds, preserve passive Victron reception and OTA pause. Only DD A5 read commands 03 and 04 are permitted. No authentication or configuration writes.
- [x] Reuse existing BMS runtime/MQTT channels; correct capabilities and documentation. Add transport clear-peer APIs only if required for disabling a source without stopping the other.
- [x] Run focused host tests, review, USB-environment compilation; use authorized network OTA only after successful build and reachable-device preflight. Verify fresh BMS and Victron readings plus UI recovery.

Ruling: defer optional hardware-ID register 05; it is not needed for measurements and would consume another transaction. Device model remains the documented, unverified identification.
Ruling: implement protocol facts independently from the JBD reference (syssi/esphome-jbd-bms); do not import control/authentication implementation.

Verification: synthetic JBD host tests, existing Victron/MQTT/smoke tests and CM BLE transport tests passed. Final USB build passed: flash 1,342,481 bytes, static RAM 80,360 bytes. Binary SHA-256: 0aa819dee046d247e736d2ab40127fb868da6df5a22457226f91a1f9a7eee7ad; ELF SHA-256: e590ee7658eb73e5d155afb5f45720e91b2e23ba834cf4c7ecc6c6b77f2e4e02.

Independent review identified and verified fixes for recovery after CM write failure (-1003) and first-time Bluetooth enablement for a BMS-only installation. The device returned both BMS and Victron data after OTA; sender final acknowledgment was missing and is documented, not treated as a confirmed protocol-level upload success.

Ruling: retain CM 4.6.0 as the existing unpublished development version; the compatible transport APIs join that unreleased scope. The SBT 0.3.0 owner-requested debugging version stays unchanged.
