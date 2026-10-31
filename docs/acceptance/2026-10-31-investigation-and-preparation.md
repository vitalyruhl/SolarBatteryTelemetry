# Battery incident investigation and prepared changes

Date: 2026-10-31. This record distinguishes production observations, isolated
software verification, and outstanding hardware acceptance.

Subsequent owner authorization completed the bounded test and production rollout;
see [the deployment record](2026-10-31-cell-telemetry-hardware-gate.md). Delivery
boundaries below describe the earlier preparation checkpoint.

## Incident findings

- On 2026-10-29 at 19:02:33 Europe/Berlin, the recorded cell-undervoltage alarm
  was 2 and the physical discharge MOS was off. SIL issued emergency-off request
  `sil-f917e23c-006cc66e`; SBT acknowledged it at 19:02:34. Both MOS paths were
  then off. The alarm changed from 4098 to 4096 at 19:02:40: the undervoltage bit
  cleared while the software MOS lock remained.
- SIL's existing broad hardware-alarm emergency rule requests both MOS off;
  SBT executes that request. This explains why charging stayed disabled after
  the original discharge event. No automatic recovery supervisor was present.
- The owner manually enabled charging on 2026-10-30 at 13:32:46 and discharging
  at 13:32:51. Alarm 0 followed at 13:32:52. The supplied screenshot's PV rise
  matches that manual restoration.
- The existing inverter path capped output using measured MPPT power. With
  MOS off, the measured approximately 5 W and the -130 W correction produced
  0 W. That feedback loop explains the failure to use sunlight in this state.
- Recorded SBT reset reason 4 indicates panic resets. Such resets also occurred
  before the MOS incident. Minute-level heap readings cannot exclude transient
  allocation or stack failure; the precise production panic remains unproven.

## Battery/SOC evidence

Recorded SOC and remaining capacity fell from approximately 27% / 35.31 Ah to
0% / 0 Ah without an SBT restart. The decoder uses the BMS SOC/Ah fields from a
checked complete JBD response pair; missing data do not intentionally publish
numeric zero. At the undervoltage event, minimum/maximum group voltage was
approximately 2.991/3.361 V. That supports investigating the battery/BMS and
capacity configuration, rather than attributing the SOC discontinuity to the
ESP32 reset alone.

The owner's new label data establish 14S40P LG M29 lithium-ion, 50.4 V and 114 Ah
nominal, approximately 5.75 kWh. The BMS-reported 131.25 Ah and SIL's 51.38 V
energy assumption imply approximately 6.74 kWh, about 17.37% higher. See
[battery specifications and limitations](../BATTERY.md).

The 14 new voltage histories measure parallel-group voltages. They can identify
the limiting group but cannot identify one physical cell within a 40P group.
Weak/imbalanced groups, sensing/configuration problems, and BMS estimation still
need to be distinguished. A 31% reserve would reduce planned available energy;
the release threshold must exceed 31%. That would be a provisional operating
choice, not proof or repair of a battery defect. No reserve was changed.

## Prepared software and HA configuration

- SBT 1.5.0: coherent 14-group mV array, sparse cell updates with at least five
  seconds between ordinary changes, fair 100 ms ordinary-publication admission,
  distributed full refresh, retained historical panic evidence, and 30-second
  stack/heap diagnostics. Urgent control and availability remain independent.
- SIL 5.5.0: Automatic, household following, and battery-charge priority. Explicit
  manual household demand is `clamp(household + correction, 0, maximum)`.
  Missing/stale BMS alone is never proof of physical battery absence. Manual
  operation reports uncertainty; charge priority requires fresh SOC.
- Charge priority defaults to zero inverter allocation until 100%, with a
  configurable 95% threshold enabling an optional PV share above that threshold.
  Once full, household following remains active until SOC falls below the
  threshold. Existing explicit OFF and valid directional restrictions apply.
- Twenty HA discovery definitions were created and verified in Recorder: 14
  group voltages, one sample sequence, four memory diagnostics, and the three-mode
  selector. They remain unavailable until supported firmware supplies state.
  See [HA preparation evidence](2026-10-31-ha-entity-preparation.md).

All relevant host checks and USB/OTA compilation passed. A fresh independent
review found and then verified fixes for two new SIL edge cases: missed SOC
hysteresis crossings while output was gated, and unconfirmed mode activation
after a failed persistence readback. The final scoped review reported no
remaining confirmed Important/Critical findings. No hardware acceptance is
implied by these checks.

## AsyncTCP and protection decisions

The installed AsyncTCP-esphome 2.1.4 source reproduced unchecked-allocation
crashes and a queue-refill close deadlock in bounded host harnesses. Equivalent
cases pass with released ESP32Async/AsyncTCP 3.5.0. An isolated unchanged
SBT 1.4.12 / CM 4.10.5 consumer built successfully with the successor for USB
and OTA; CM and the settings webserver remained present. Product dependency
declarations were not changed. This establishes build compatibility, not the
cause or resolution of the production panics. No duplicate public issue, fork,
or PR was created for already-fixed legacy defects. Multi-day hardware testing
has not started.

Manufacturer family documentation describes native BMS voltage/current/thermal
protection. The installed unit's actual thresholds, delays, releases, and sensor
configuration still need a read-only check before executing the conditional
plan to remove automatic SIL/SBT MOS commands. Manual HA commands remain the
intended control boundary. Automatic MOS removal and recovery are not included
in these prepared code changes.

## Delivery boundaries and evidence

Both feature worktrees remain uncommitted and unuploaded:
`C:/Temp/sbt-cell-diagnostics-20261031` and
`C:/Temp/sil-operating-modes-20261031`. The original owning worktrees were
verified clean. Live UI stays disabled. Production uploads, commits, pushes,
merges and releases remain separate owner-authorized operations.

Detailed results are in each worktree's `.local/*implementation-report.md` or
`.local/operating-modes-report.md`, the independent
`C:/Temp/energy-review-20261031.md`, and
`C:/Temp/asynctcp-contribution-20261031-152650/` (source-derived host harnesses,
findings, compatibility report and structured results).
