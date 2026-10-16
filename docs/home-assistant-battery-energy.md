# Home Assistant battery energy

## Purpose and source

The October 16, 2026 owner-authorized HA setup derives separate cumulative battery charging and discharging energy from the existing `sensor.sbt_bms_power_w` entity. Its MQTT source is `SolarBatteryTelemetry/bms/power_w`. Positive values mean charging; negative values mean discharging. These are battery-side DC measurements, not inverter AC energy or total PV production.

Two Template helpers split the signed source into nonnegative directional power:

- `sensor.sbt_battery_charging_power`: charging power, W.
- `sensor.sbt_battery_discharging_power`: discharging power, W.

Both require numeric source data and available BMS/collector sources. Missing data must remain unavailable rather than becoming zero power. No additional ESP telemetry or physical device command is needed.

## Cumulative energy

Each directional power feeds a Home Assistant Integral helper with method `left`, unit prefix `k`, integration time `h`, six decimal places, and maximum sub-interval `00:01:00`. The resulting unit is kWh. The sub-interval updates totals even during unchanged power; it does not change the MQTT publication interval.

The counters accumulate from their creation, without importing or inventing historical energy. HA restores Integral totals across restarts. Missing observations, HA downtime, sample timing and telemetry deadbands limit accuracy; unobserved energy is not recovered. These counters estimate transferred energy, not remaining battery capacity.

References: [HA Integral helper](https://www.home-assistant.io/integrations/integration/) and [Energy sensor requirements](https://www.home-assistant.io/docs/energy/faq/#troubleshooting-missing-entities).

## Deployment scope

The helpers are created through HA UI. No YAML configuration file, firmware, BMS protection threshold, MOS permission or Victron setting is changed. No HA restart is required. Device energy-management behavior is outside this setup.


## Exact Template expressions

Charging state:

```jinja
{% set p = states('sensor.sbt_bms_power_w') %}
{{ [p | float, 0] | max if is_number(p) else none }}
```

Discharging state:

```jinja
{% set p = states('sensor.sbt_bms_power_w') %}
{{ [-(p | float), 0] | max if is_number(p) else none }}
```

Availability for both:

```jinja
{{ is_number(states('sensor.sbt_bms_power_w')) and is_state('binary_sensor.sbt_bms_availability', 'on') and is_state('binary_sensor.sbt_collector_availability', 'on') }}
```

## Saved Energy dashboard mapping

A new `SBT Battery` entry was saved and reopened at Settings > Energy > Electricity. No existing battery entry was present; unrelated grid, solar and device entries were preserved.

| Dashboard field | Entity | Class / state class |
| --- | --- | --- |
| Energy discharged from battery | `sensor.sbt_battery_energy_discharged` | energy / total |
| Energy charged into battery | `sensor.sbt_battery_energy_charged` | energy / total |

The corresponding Integral sources are `sensor.sbt_battery_discharging_power` and `sensor.sbt_battery_charging_power`. Both sources use device class power, state class measurement, and W. Optional battery power, SOC and usable capacity fields were left empty; this request only covers the two energy counters.

## Validation and limits

All four helper creation dialogs succeeded. Source unavailability was naturally observed during setup; the Template preview reported unavailable rather than fabricated zero. The saved battery dialog was reopened to verify both assignments. Developer Tools confirmed energy classes/units and the correct directional source attributes. One observed sample was charged 0.014366 kWh, discharged 0.000000 kWh, charging power 667.3239 W and discharging power 0 W. Values are observations, not setpoints. Positive charge accumulation was observed; a physical discharge interval and restart persistence were not deliberately exercised.

HA initially displayed its normal statistics initialization notice (up to five minutes). This setup does not claim historical statistics are immediately populated. Screenshot evidence remains local in the project mirror: `ha-battery-energy-mapping-20261016.jpg`.

No firmware or product code changed, so no firmware build/test was run. No commit or push was performed. This document records a HA deployment, not a new firmware release; no firmware version/changelog change is needed.


Independent parent review confirmed exactly four new entities in Developer Tools, with charged energy increasing further to 0.025326 kWh, discharge 0.000000 kWh, and the expected source/class/unit attributes. A fresh navigation to the Energy configuration showed the saved `SBT Battery` entry with no battery-specific statistics warning remaining. HA's general notice says newly configured data can take up to two hours to appear in the Energy dashboard. Existing unavailable-device warnings belonged to unrelated Tasmota entities and were not modified.


## Display precision and overview cleanup (October 16, 2026)

Both energy entities now use Home Assistant entity display precision 2. This changes formatting only; Integral calculation precision remains six decimal places, with existing totals and Energy assignments preserved. The entity list verified charged 0.15 kWh and discharged 0.01 kWh after saving. Values are observations, not fixed expected totals.

The overview Battery / BMS card no longer lists Data available, Cell count or Temperature sensor count. Protection remains because it describes active BMS protection causes. Actual charge/discharge MOS feedback moved to the renamed BMS MOS card, directly below the respective Allow switch. The switches were not operated and the existing control gate remains disabled.

The Victron SmartSolar overview card no longer lists Data available, Load current or Source health. The MPPT 150/35 has no physical load output; this field is not inverter or household consumption. Source health is a freshness/transport diagnostic. Removing these rows does not delete the underlying telemetry or availability handling. No firmware was changed.

The future automation-only MOS permission and strategy-card requirements are tracked in SolarInverterLimiter/docs/todo.md. The owner selected documentation only; implementation and deployment remain future work.

The built-in Energy distribution overview was checked separately and still displayed battery statistics as 0 kWh. Its aggregate statistics formatting is separate from entity display precision; this change does not claim to force trailing zeros in that diagram.
