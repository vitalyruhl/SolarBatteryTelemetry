# Home Assistant entity preparation

Date: 2026-10-31. The owner authorized creating cell-voltage and diagnostic
entities and the three-position SIL operating-mode selector in Home Assistant.

## Applied configuration

Twenty additive MQTT discovery definitions were published with retained QoS 1
configuration messages. Every publication received PUBACK and a fresh broker
subscription verified the exact retained configuration. No measurement,
availability, MOS, inverter, or operating-mode command was published.

| Entities | Count | Definition source |
| --- | --- | --- |
| `sensor.sbt_bms_cell_01_voltage` through `sensor.sbt_bms_cell_14_voltage` | 14 | `docs/home-assistant-bms-cells.yaml` |
| `sensor.sbt_bms_cells_sequence` | 1 | `docs/home-assistant-bms-cells.yaml` |
| `sensor.sbt_loop_stack_min_free_bytes`, `sensor.sbt_heap_free_bytes`, `sensor.sbt_heap_largest_free_block_bytes`, `sensor.sbt_heap_min_free_bytes` | 4 | `docs/home-assistant-runtime-diagnostics.yaml` |
| `select.solar_limiter_operating_mode` | 1 | SIL `docs/home-assistant-operating-modes.yaml` |

Read-only queries through the authenticated HA SQL interface verified that all
20 entities had states in Recorder. Their current states were `unavailable`,
as expected before the new firmware supplies its telemetry and supported mode
state. This proves entity registration and Recorder inclusion; it is not a
test of hardware measurements or mode changes.

The selector labels are `Automatik`, `Hausverbrauch folger`, and
`Batterie-lade-Vorrang`. It uses confirmed state (`optimistic: false`), requires
both SIL online and a supported mode state, and sends non-retained JSON commands
with a 30-second expiry when a user later operates it.

## Maintenance and recovery

The source YAML files are reference/manual-install alternatives. Do not install
them a second time while the commissioned discovery definitions remain active.
Discovery configuration is under
`homeassistant/{sensor|select}/energy_review_20261031/{unique_id}/config` and is
marked with origin `Owner-authorized energy entity setup`.

The exact manifest, previous definitions, publication-topic list, and bounded
commissioning helper are retained in
`C:/Temp/ha-energy-entities-20261031`. The helper refuses foreign-origin
configuration and publishes only the whitelisted discovery definitions. Any
future rollback should restore those exact previous definitions, rather than
delete unrelated discovery topics or entities.

No HA restart, dashboard replacement, firmware upload, or device setting change
was performed. Live UI remains disabled. The 14 readings are voltages of the
14 series groups of the 14S40P pack; see [battery label data](../BATTERY.md).
