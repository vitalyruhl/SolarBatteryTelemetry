# SIL/SBT/HA integration planning

The [single current integration plan](../../SolarInverterLimiter/docs/superpowers/plans/2026-10-20-sil-sbt-ha-integration.md) and [integration specification](../../SolarInverterLimiter/docs/control-planning/2026-10-20-integration-spec.md) are maintained in the adjacent SolarInverterLimiter repository. Do not maintain a second implementation queue in SBT. These relative links assume the existing sibling checkout layout.

The 2026-10-20 checkpoint consolidates documentation and local commits, then stops. Integration implementation starts later. The canonical plan contains the latest owner approvals for isolated test-ESP builds/uploads, Settings/Live checks and at least five minutes of observation per consumer; older COM7 instructions in historical evidence do not redefine that authorization. No production upload, MOS operation or other device action is performed by this documentation checkpoint.

SBT owns collector telemetry, source validity and the MQTT producer. SIL owns the receiver and control policy; HA owns the approved presentation and forecast publication work described in the canonical plan. The earlier SBT plans have been moved to `docs/acceptance/` with explicit historical headers, retaining their original findings and acceptance limits.

## Historical migration

- [SIL control-planning baseline](../../SolarInverterLimiter/docs/control-planning/README.md)
- [Original SBT planning history at the migration baseline](https://github.com/vitalyruhl/SolarBatteryTelemetry/tree/30e64271e92aa9baadbd687714ef412e8be7f78f/docs/regelungsplannung)

The original control-planning migration was performed on 2026-10-17. Its accepted charge-target/PV-ceiling addition and hardware-determined transient-discharge limitation remain evidence, not a claim of completed firmware integration or HITL acceptance.
