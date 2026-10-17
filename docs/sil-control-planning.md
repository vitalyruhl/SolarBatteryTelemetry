# SIL control planning moved

The control-planning folder belongs to SolarInverterLimiter and has moved to `docs/regelungsplannung/` there. Continue design and implementation planning in that repository; do not maintain a second copy in SBT.

- [Current SIL planning branch](https://github.com/vitalyruhl/SolarInverterLimiter/tree/feature/battery-energy-control/docs/regelungsplannung)
- [Original SBT planning history at the migration baseline](https://github.com/vitalyruhl/SolarBatteryTelemetry/tree/30e64271e92aa9baadbd687714ef412e8be7f78f/docs/regelungsplannung)

Migration date: 2026-10-17. The SIL copy also records the accepted charge-target/PV-ceiling addition and the hardware-determined transient-discharge limitation. This is a documentation move, not a firmware change or completed HITL acceptance.
