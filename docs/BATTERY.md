# Installed battery: label data and diagnostic meaning

Recorded on 2026-10-31 from specifications supplied by the owner. These are
owner-reported label data, not measurements or a verified BMS parameter dump.

## Reported specifications

| Item | Reported value | Meaning |
| --- | --- | --- |
| Nominal voltage | 50.4 V | Battery nominal voltage |
| Rated capacity | 114 Ah | Battery label capacity |
| Configuration | 14S40P | 14 series groups, each containing 40 parallel cells |
| Cell type | LG M29 | Lithium-ion; this pack is not LiFePO4 |
| BMS current rating | 60 A | Maximum continuous current according to the label |
| Internal resistance | Ri: 30 mΩ | Label value; measurement conditions and present resistance are unknown |
| Date marking | 11.19 | Probably November 2019; manufacturing date is not independently confirmed |

## Values derived from those specifications

| Item | Calculation | Result |
| --- | --- | --- |
| Physical cell count | 14 × 40 | 560 |
| Nominal energy | 50.4 V × 114 Ah | 5,745.6 Wh, approximately 5.75 kWh |
| Maximum charge voltage reported by the owner | 14 × 4.2 V | 58.8 V |
| DC power at nominal voltage and the BMS label current | 50.4 V × 60 A | 3,024 W, approximately 3.02 kW |
| Current per parallel cell at 60 A, assuming equal sharing | 60 A / 40 | 1.5 A |

These calculations do not establish the pack's present usable capacity, actual
current sharing, thermal limits, charger configuration, or permitted inverter
output. The reported maximum charge voltage is not an instruction to change any
charger or BMS setting.

## Consequences for the SOC investigation

The BMS supplies 14 series-group voltages. A Home Assistant entity named
`SBT BMS Cell 01 Voltage` represents the voltage of one 40P group, not a separate
measurement of each of its 40 physical cells. The same interpretation applies to
all 14 voltage entities.

Indexed group history can identify which series group reaches undervoltage first
and whether its voltage recovers when load is removed. It cannot by itself
identify a defective physical cell inside that parallel group or distinguish
group degradation from a sensing/wiring problem. The observed SOC drop remains
an investigation finding, not proof of a particular failed cell.

The observed BMS full-capacity value of 131.25 Ah differs from the label's
114 Ah. A BMS-reported configuration or estimate must not silently replace the
physical battery rating in documentation. Conversely, the label alone does not
prove that rewriting the BMS capacity would correct its SOC estimate.

The existing SIL energy calculation uses 51.38 V and the reported 131.25 Ah,
which gives 6,743.625 Wh before reserve and conversion losses. This is about
17.37% above the label-derived 5,745.6 Wh. That mismatch should be included in
future capacity and reserve calibration; no runtime value is changed by this
documentation update. Present usable energy may be lower still because of age,
imbalance, load, temperature, or an early undervoltage cutoff.

## Protection verification remains separate

The observed JBD/LLT BMS identity and manufacturer's product-family documentation
support native voltage, current, and temperature protection. Actual thresholds,
release conditions, delays, and enabled temperature sensors on this installed
unit still require a read-only parameter check. Neither a 60 A label nor two
temperature readings proves that every desired protection is configured.

See [BMS readout and acceptance](BMS.md). The conditional removal plan is in the
SolarInverterLimiter operating-modes worktree at
`docs/superpowers/plans/2026-10-31-manual-only-mos-policy.md`.
