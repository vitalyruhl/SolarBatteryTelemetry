# Telemetry value inventory

Verified: 2026-10-16. Source baseline: `142fe8a60ef29e02cf38ae57e335e871e5d517f9`.

This inventory compares protocol capabilities with the current SolarBatteryTelemetry implementation for the Victron SmartSolar MPPT 150/35 and the JBD/SP15S020 14-series-cell BMS. It is a reference for future extensions, not approval to implement them or change device settings.

## Evidence and interpretation

- The accepted baseline defined 25 telemetry channels: 15 BMS and 10 Victron fields. The approved October 16 feature appends four BMS fields (29 total); historical values below remain observations from the earlier baseline.
- A read-only ESP runtime response on 2026-10-16 reported both sources available, all 15 BMS fields populated, six Victron fields populated, and four Victron fields null.
- The snapshot below is historical, not a live dashboard or a calibration result. Values from the two sources are not guaranteed to have identical acquisition times.
- Web fields were verified against the source-defined page metadata; no new rendered-browser acceptance test was performed.
- All valid implemented channels feed the MQTT publisher. Independent broker receipt was not verified in this inventory check. Placeholder fields do not provide measurements.
- "Received" includes fields contained in an already-requested protocol response, even when the application discards them.
- Optional protocol capabilities are not proof that this exact device/firmware implements every register.

Implementation references: [channel definitions](../src/telemetry/TelemetryChannels.h), [BMS decoder](../src/bms/JbdProtocol.h), [BMS transport](../src/bms/BmsReadout.h), [Victron decoder](../src/victron/VictronDecoder.h), [Victron runtime provider](../src/victron/VictronReadout.h), and [web field registration](../src/telemetry/MqttService.h).

## MQTT send thresholds and display

The working-tree publisher uses the owner-selected absolute deadband: 0.1 in each real-valued channel unit, except 1 W for channels expressed in W. This includes all BMS cell voltages and cell spread. Comparison uses the last successfully sent value; payload precision is preserved. Status, alarm and validity transitions remain exempt. Home Assistant voltage display should use two decimal places independently of these thresholds. The owner subsequently installed and tested the deadband baseline by OTA. Separately, all five existing HA voltage entities were set to two decimal places and verified on the live dashboard on 2026-10-16. See [MQTT policy and display configuration](MQTT.md#home-assistant-display-precision).

## Basis for the future SIL assessment

The added `SIL need / purpose` column is a requirements assessment, not an implementation claim. It uses [the recorded energy-management intent](../project.info.md#energy-management-intent-for-later-work): prioritize PV charging during cheap periods, preserve a battery reserve, cover actual household demand during expensive periods, and release available inverter capacity when the battery is full and sunshine continues. The reported inverter limit is 900 W; the approximate 600 W household idle load is not a fixed dispatch target. Actual prices must be used, not a fixed evening time window. Grid charging hardware is unconfirmed.

The owner additionally requested independent charge/discharge MOS blocking on 2026-10-16. The owner subsequently approved the guarded design and implementation plan; the installation gate stays disabled and no live MOS switching is authorized during rollout. The analysis also consulted the sibling SolarInverterLimiter `readme.md` and `docs/todo.md`: signed grid feedback and price inputs are documented, while asymmetric setpoint ramp limiting is still planned. These are documentation observations, not fresh SIL code or hardware acceptance.

- **Required**: an input or behavior needed by this assessment of the stated functions; exact policies and thresholds remain to be designed. **Required (owner)** identifies the explicit MOS-control request.
- **Conditional**: needed only for the stated extension or selected architecture; not a blanket requirement to add another interface.
- **Optional**: useful for diagnosis/reporting, not necessary for initial dispatch.
- **Service only**: commissioning or maintenance, not an automatic SIL write.
- **Not needed**: outside the stated initial SIL function set.

No new battery limits, thresholds, MQTT topics or component ownership are specified here. The approved feature implements only guarded charge/discharge MOS permissions; other write operations remain unimplemented. SOC/capacity confidence, battery chemistry, sensor placement and wiring must be resolved before defining battery-dependent operating limits.

## BMS: implemented values

All rows have a BMS web field and MQTT channel. MQTT paths are relative to the configured base topic, normally `SolarBatteryTelemetry`.

| Value | Acquisition | MQTT suffix | Snapshot on 2026-10-16 | SIL need / purpose |
| --- | --- | --- | --- | --- |
| Battery voltage | Direct | `bms/voltage_v` | 47.82 V | Required - battery plausibility and operating limits; not a standalone SOC estimate |
| Net battery current | Direct; positive means charging | `bms/current_a` | +1.61 A | Required - detect net charge/discharge and verify requested blocking |
| Battery power | Voltage multiplied by signed current | `bms/power_w` | +76.9902 W | Required - quantify battery contribution and check energy flows |
| State of charge | BMS-reported | `bms/state_of_charge_pct` | 1% | Required - reserve/full decisions; validate accuracy first |
| Remaining capacity | Direct | `bms/remaining_capacity_ah` | 1.96 Ah | Conditional - usable-energy planning; validate reported capacity |
| Configured full capacity | Direct; not measured usable capacity | `bms/full_capacity_ah` | 131.25 Ah | Conditional - capacity model; not proof of usable energy |
| Charging activity | Current greater than zero | `bms/charging` | true | Optional - convenient derived status; signed current already covers this |
| Discharging activity | Current less than zero | `bms/discharging` | false | Optional - convenient derived status; signed current already covers this |
| Charge MOSFET permission | Direct status bit | `bms/charge_enabled` | true | Required - observe charge permission after control requests |
| Discharge MOSFET permission | Direct status bit | `bms/discharge_enabled` | true | Required - observe discharge permission after control requests |
| Maximum temperature | Maximum of validated NTC readings | `bms/temperature_max_c` | 17.4 C | Required - high-temperature operating gate; limits remain unspecified |
| Minimum cell voltage | Minimum across all 14 cells | `bms/cell_voltage_min_v` | 3.290 V | Required - discharge-limit plausibility beyond SOC |
| Maximum cell voltage | Maximum across all 14 cells | `bms/cell_voltage_max_v` | 3.433 V | Required - charge-limit plausibility beyond SOC |
| Cell voltage spread | Maximum minus minimum | `bms/cell_voltage_delta_v` | 0.143 V / 143 mV | Conditional - imbalance policy; threshold and response remain open |
| Protection/alarm bits | Raw bitfield, not individual messages | `bms/alarm_bits` | 0 | Required - inhibit inappropriate actions on BMS faults |

MOSFET permission and actual charging/discharging activity are separate concepts. SOC and capacity accuracy have not been independently established.

## BMS: unused data and possible extensions

The regular cycle reads basic information (`0x03`) and cell information (`0x04`). The following table distinguishes additional decoding from additional requests. No new MQTT suffixes are assigned by this document.

| Value | Availability | Current processing | Dedicated web/MQTT output | SIL need / purpose |
| --- | --- | --- | --- | --- |
| Cell voltages 1-14 | Already received in `0x04` | Reduced to minimum, maximum and spread | No | Optional - diagnosis; minimum/maximum already support basic cell checks |
| Individual temperatures | Already received in `0x03` | Reduced to highest temperature | No | Conditional - identify sensor placement and derive relevant cold/hot limits |
| Minimum relevant battery temperature | Derivable from received NTC readings once sensor placement is known | Minimum of all validated NTCs; placement relevance still unverified | `bms/temperature_min_c` | Required - cold-temperature gate; limits depend on confirmed battery specification |
| Lowest-voltage cell number | Derivable from received cells | Not calculated | No | Optional - diagnosis |
| Highest-voltage cell number | Derivable from received cells | Not calculated | No | Optional - diagnosis |
| Average cell voltage | Derivable from received cells | Not calculated | No | Optional - diagnosis; does not replace cell extremes |
| Cycle count | Included in `0x03` | Ignored | No | Optional - aging/reporting, not initial dispatch |
| Manufacture date | Included in `0x03` | Ignored | No | Optional - inventory |
| BMS software version | Included in `0x03` | Ignored | No | Conditional - qualify compatible write behavior before enabling controls |
| Balancing state and affected cells | Bitmask in `0x03` | Ignored | No | Optional - explain cell behavior; no balancing control needed for tariff dispatch |
| Cell count | Included in `0x03` | Validated as 14 | `bms/cell_count` | Required - validate expected pack layout; already checked internally |
| Temperature sensor count | Included in `0x03` | Controls temperature parsing | `bms/temperature_sensor_count` | Required - detect incomplete temperature coverage; already parsed internally |
| Individual protection messages | Already received as alarm bits | Raw bits plus named active flags | `bms/protection` | Required - distinguish charge/discharge blocking causes and report them |
| Model/hardware identifier | Separate `0x05` request | Not requested | No | Conditional - qualify commands; can also use verified commissioning records |
| Historical protection-event counters | Separate `0xAA` request | Not requested | No | Optional - diagnosis; not a substitute for active faults |

Protection bits distinguish cell/pack overvoltage and undervoltage, charge/discharge temperature limits, overcurrent, short circuit, and additional controller faults. Optional requests require device-specific verification. A complete configuration-register inventory for this exact BMS is not established here.

Protocol reference: [JBD BLE implementation](https://github.com/syssi/esphome-jbd-bms/blob/main/components/jbd_bms_ble/jbd_bms_ble.cpp). Local behavior: [BMS readout documentation](BMS.md).

## Victron: current passive BLE readout

The Solar Charger advertisement record (`0x01`) contains seven documented measurement/status fields. The application additionally reserves three currently unimplemented channels. All ten rows have web fields.

| Value | Availability through current BLE record | Implementation | MQTT suffix | Snapshot on 2026-10-16 | SIL need / purpose |
| --- | --- | --- | --- | --- | --- |
| PV power | Direct | Decoded | `victron/solar_power_w` | 96 W | Required - identify current solar contribution; not uncurtailed solar potential |
| Battery voltage at charger | Direct | Decoded | `victron/battery_voltage_v` | 48.00 V | Conditional - charger/battery plausibility and derived charger power |
| Charger output current | Direct | Decoded | `victron/charge_current_a` | 2.0 A | Required - charge-flow observation and coordinated charge-stop verification |
| Charge state | Direct | Mapped to named state | `victron/charge_state` | `bulk` | Required - charging context; float alone does not prove usable SOC |
| Charger error code | Direct | Decoded | `victron/error_code` | 0 | Required - charger fault/availability decision |
| Yield today | Direct | Converted to Wh | `victron/yield_today_wh` | 120 Wh | Optional - reporting; not required for immediate dispatch |
| Load output current | Protocol field; unavailable on this device | Decoder exists; unavailable sentinel becomes null | `victron/load_current_a` | null | Not needed - no physical load output on this model |
| PV voltage | Absent from this BLE record | Unsupported placeholder | `victron/pv_voltage_v` | null | Optional - PV diagnosis; no extra interface needed just for initial dispatch |
| PV current | Absent from this BLE record | Unsupported placeholder | `victron/pv_current_a` | null | Optional - PV diagnosis; PV power is already available |
| Battery-side charger power | Derivable from voltage and output current | Deferred placeholder; calculation absent | `victron/battery_power_w` | null; approximately 96 W if calculated | Conditional - more useful DC flow accounting; derive from existing readings |

The MPPT 150/35 has no physical load output. Its load-current field must not be interpreted as household or inverter consumption. Charger output current is not net battery current; loads on the same DC bus can account for differences.

References: [Victron advertisement specification, page 3](https://communityarchive.victronenergy.com/storage/attachments/extra-manufacturer-data-2022-12-14.pdf), [MPPT 150/35 device features](https://www.victronenergy.com/media/pg/Manual_SmartSolar_MPPT_150-35__150-45/en/features.html), and [local Victron documentation](Victron.md).

## Victron: capabilities requiring another access method

VE.Direct documents the following additional capabilities. None is currently queried through VE.Direct by this collector. Hardware/firmware support and external sensor requirements must be checked before implementation. Active Bluetooth/GATT is not assumed to be an equivalent documented replacement for VE.Direct.

| Additional value or group | Access method / condition | Queried or displayed by current collector | SIL need / purpose |
| --- | --- | --- | --- |
| PV voltage | VE.Direct TEXT/HEX | No | Optional - PV diagnosis; no extra interface needed just for initial dispatch |
| PV current | VE.Direct HEX; model-dependent | No | Optional - PV diagnosis; PV power is already available |
| Internal controller temperature | VE.Direct HEX | No | Optional - charger diagnosis; not battery temperature |
| Battery temperature | HEX with a suitable external temperature source | No | Conditional - additional source; does not replace relevant BMS sensor coverage |
| Total yield | VE.Direct | No | Optional - reporting |
| Yield yesterday | VE.Direct | No | Optional - reporting |
| Maximum PV power today/yesterday | VE.Direct | No | Optional - reporting; not a forecast |
| MPPT operating mode and shutdown reason | VE.Direct; firmware-dependent | No | Optional - improve diagnosis and control explanations |
| Firmware version and serial number | VE.Direct | No | Conditional - qualify external control compatibility before writes |
| Daily history and previous errors | VE.Direct HEX | No | Optional - diagnosis and later forecasting inputs |
| Charging parameters, such as absorption/float voltage and current limit | VE.Direct HEX; supported registers only | No | Conditional - approved limits are required commissioning evidence; VictronConnect/records suffice, no new interface or HEX polling required |

References: [VE.Direct TEXT specification](https://www.victronenergy.com/upload/documents/VE.Direct-Protocol-3.34.pdf) and [VE.Direct HEX specification](https://www.victronenergy.com/upload/documents/BlueSolar-HEX-protocol.pdf). This is an extension inventory, not a device-verified exhaustive configuration-register catalog.

## Write capabilities

Checked against the linked references on 2026-10-16. These tables describe potential extensions. No live device-control writes were performed or tested. Only the two guarded BMS MOS controls are implemented, with installation enablement still disabled. Protocol support does not establish compatibility with the installed firmware or suitability of a setting for the battery.

### Access methods

| Device / access | Reading | Device commands/settings | Current collector | SIL need / purpose |
| --- | --- | --- | --- | --- |
| Victron passive Instant Readout | Selected measurements | No return channel | Implemented, read-only | Required - existing source for initial solar telemetry |
| VictronConnect over Bluetooth | Yes | Supported app settings | Separate app, not ESP functionality | Service only - commissioning; not an automated SIL control path |
| Victron VE.Direct HEX | Yes | Documented write commands | Not connected/implemented | Conditional - candidate for charger stop/limits; no cable decision approved |
| Victron active Bluetooth from custom ESP code | Requires separate protocol investigation | Not verified for this project | Not implemented | Conditional - alternative requiring protocol feasibility; not assumed available |
| JBD over active BLE/GATT | Yes | Protocol-family write operations exist; device verification pending | `0x03`/`0x04` telemetry plus guarded `0xE1` MOS commands | Required - existing telemetry path plus owner-requested MOS controls |

VictronConnect access is described in the [device configuration manual](https://www.victronenergy.com/media/pg/Manual_SmartSolar_MPPT_150-35__150-45/en/configuration-and-settings.html). BLE transport writes used to request BMS measurements are not BMS configuration/control writes.

### Victron commands and settings

All rows require a separate VE.Direct implementation; hardware/firmware capabilities remain to be checked. None has an ESP control, command handler or accepted device-write test.

| Command / setting | HEX reference | ESP implemented | SIL need / purpose |
| --- | --- | --- | --- |
| Charger on/off | `0x0200`; remote-control prerequisite `0x0202` | No | Conditional - candidate coordinated charge-stop path; required mechanism remains open |
| Maximum charging current setting | `0xEDF0` | No | Service only - approved static limit; not a frequent tariff-control write |
| Dynamic current limit | `0x2015` with network mode `0x200E` | No | Conditional - smooth charge limiting/curtailment if later design needs it |
| Remote voltage target | `0x2001` with appropriate network mode | No | Not needed - initial tariff dispatch does not require changing charge voltage |
| Absorption / float voltage | `0xEDF7` / `0xEDF6` | No | Service only - battery-specific commissioning |
| Absorption time limit | `0xEDFB` | No | Service only - battery-specific commissioning |
| Battery preset / system voltage | `0xEDF1` / `0xEDEF` | No | Service only - battery-specific commissioning |
| Temperature compensation | `0xEDF2` | No | Service only - chemistry-specific commissioning |
| Equalisation settings | `0xEDFD`, `0xEDF4`, `0xEDE4`, `0xEDE3` | No | Not needed - not part of SIL dispatch; chemistry remains unconfirmed |
| Start/stop manual equalisation | `0x2004`, capability-dependent | No | Not needed - not part of SIL dispatch |
| Restart | Protocol command `6`; intentionally no response | No | Conditional - high-priority recovery/service candidate raised by owner; automated recovery policy remains open |
| Virtual load output / TX mode | `0xED9E`, mode 4; supported cable and external input required | No | Conditional - candidate external load/inverter permission signal; target compatibility unverified |
| Virtual load switching policy | VictronConnect load settings; exact HEX control/readback support requires verification on this model | No | Conditional - voltage-based fallback or external permission; does not implement tariff planning by itself |
| Charger remote on/off input (RX) | Device manual; dedicated remote on/off cable | No | Conditional - alternative controlled charge-stop path; not an output and not a reboot |
| Restore defaults / clear history | `0x0004` / `0x1030` | No | Not needed - exclude from automatic energy management |

Persistent battery settings must not be rewritten continuously. Dynamic limits require periodic renewal and appropriate timeout behavior. Equalisation support is not a recommendation for this battery.

Source: [Victron HEX specification](https://www.victronenergy.com/upload/documents/BlueSolar-HEX-protocol.pdf), especially device control, battery settings, remote control and history sections.

### Restart and auxiliary switching: model-specific assessment

A restart is distinct from restoring factory settings and from disabling charging. HEX command `6` sends no reply: future recovery logic must observe communication returning and recheck operational state. A missing command acknowledgment is expected, not justification for blind retries. A software restart requires a responsive command path and is not a guaranteed recovery from total lockup. The owner highlighted restart as important on 2026-10-16; automatic triggers/retry limits are not defined. Source: [HEX protocol](https://www.victronenergy.com/upload/documents/BlueSolar-HEX-protocol.pdf).

The 150/35 has a virtual load output via VE.Direct TX, not a physical power-load output. Its 5 V logic signal needs the specified TX output cable and a compatible external input/interface. A suitable inverter remote input could be a use case, but the installed inverter's input is unverified. RX remote on/off instead controls the charger. Source: [TX/RX settings](https://www.victronenergy.com/media/pg/Manual_SmartSolar_MPPT_150-35__150-45/en/configuration-and-settings.html).

TX mode 4 replaces unsolicited VE.Direct TEXT transmission; valid HEX requests temporarily resume communication. Qualify the resulting output behavior before combining polling and external switching. Generic load-register applicability to this virtual output remains unverified. None of these additions is available through the current passive BLE receiver.

### BMS operational commands

References describe the JBD family. Every row is untested on the installed SP15S020 and absent from the ESP command handling.

| Command | Evidence / reference | ESP implemented | SIL need / purpose |
| --- | --- | --- | --- |
| Enable/disable charging permission | MOS control `0xE1`; [switch definition](https://github.com/syssi/esphome-jbd-bms/blob/main/components/jbd_bms_ble/switch/__init__.py) | Implemented, default-disabled; live qualification pending | Required (owner) - charge MOS blocking and controlled re-enable; coordinated sequence needed |
| Enable/disable discharging permission | MOS control `0xE1`; same switch definition | Implemented, default-disabled; live qualification pending | Required (owner) - discharge MOS blocking and controlled re-enable; inverter coordination needed |
| Force SOC reset/reassessment | `0x0A`, operation `0x0100`; [button definition](https://github.com/syssi/esphome-jbd-bms/blob/main/components/jbd_bms_ble/button/__init__.py) | No | Service only - validate SOC separately; never automatically reset for tariff dispatch |
| Select automatic balancing | `0x0A`, operation `0x0700`; same button definition | No | Service only - no tariff-dependent balancing requirement |
| Clear alarm | `0x0A`, operation `0x0400`; same button definition | No | Service only - no automatic fault clearing/re-enable |
| Set remaining capacity | `0xE0`; register map below | No | Service only - calibration; not an energy-management actuator |
| Manual balance control | `0xE2`; special mode in register map, not a generic on/off switch | No | Not needed - no manual balancing in automatic SIL operation |

Balancing paths must not be conflated: the reference also lists an EEPROM balancer-enable bit, but its BLE switch configuration is commented out. Treat automatic balancing, manual balance control and stored balancing policy as different operations requiring device-specific verification. Alarm clearing is not evidence that the underlying condition has disappeared; SOC reset is not a battery-capacity measurement.

The two existing `bms/charge_enabled/set` and `bms/discharge_enabled/set` paths now accept bounded session/expiry-protected absolute requests when the installation gate and runtime readiness permit. Readback checks both MOS flags and reports observed net current. At rollout the gate remains disabled, so commands are rejected. See [MQTT documentation](MQTT.md#guarded-command-contract).

### BMS persistent settings and service operations

Selected writable groups from the [Overkill Solar JBD register map](https://gitlab.com/Overkill-Solar-LLC/overkill-solar-bms-tools/-/blob/master/JBD_REGISTER_MAP.md). Each row is unverified on this device and unimplemented in the ESP. EEPROM operations use a separate factory-mode workflow; this table is not an execution procedure.

| Setting / operation | Scope | ESP implemented | SIL need / purpose |
| --- | --- | --- | --- |
| Nominal and cycle capacity | Capacity configuration | No | Service only - verify capacity model before energy planning |
| SOC voltage reference points | Voltage-based capacity estimation | No | Service only - chemistry-specific SOC configuration |
| Cell/pack voltage protection | Thresholds, recovery and delays | No | Service only - verify autonomous BMS protection; do not alter for tariff dispatch |
| Charge/discharge temperature protection | Thresholds and recovery | No | Service only - verify protection; do not alter for tariff dispatch |
| Overcurrent / short-circuit protection | Limits and timing | No | Service only - verify protection; not a throttling mechanism |
| Balancing policy | Enable, charging-only mode, start voltage and difference | No | Service only - battery-specific commissioning |
| Cell count and sensor configuration | Pack topology | No | Service only - establish correct topology; not a runtime SIL write |
| Calibration | Cell voltage, NTC and current | No | Service only - resolve measurement accuracy before relying on it |
| Enter/exit factory mode; save settings | Configuration lifecycle | No | Not needed - outside automatic SIL operation |

The documented save-and-exit operation can also clear historical error counters. Calibration and protection parameters are service settings, not ordinary telemetry controls. This is a selected inventory, not an exhaustive device-verified register map.

## Availability and diagnostics

| Information | Current handling | SIL need / purpose |
| --- | --- | --- |
| BMS/Victron source availability | Web field and retained MQTT availability topic | Required - reject unavailable inputs; combine with collector connection state |
| BMS/Victron status reason | Web/runtime field; MQTT capabilities contain a combined source summary | Required - distinguish stale, disconnected, paused and faulty sources |
| Victron product ID | Stored internally; no dedicated web/MQTT field | Conditional - qualify supported charger behavior |
| Victron BLE signal strength | Stored internally; no dedicated source web/MQTT field | Optional - installation diagnosis |
| Victron receive time | Stored internally for freshness; no dedicated web/MQTT field | Required internally - freshness; consumer stale-data contract still needs design |
| Victron advertisement counter | Used internally to reject duplicate samples; no dedicated web/MQTT field | Required internally - duplicate/freshness detection; SIL does not need raw counter |

BMS cycles are approximately five seconds apart after completion. Both adapters use a 30-second freshness limit; invalidation can happen earlier on disconnection, configuration changes or pauses as documented by each adapter. MQTT telemetry is change-driven, not a periodic heartbeat. See [MQTT behavior](MQTT.md) for publication and availability semantics.

## SIL inputs and control requirements outside the device tables

These requirements are identified by the assessment; the table does not create new interfaces or assert existing implementations. A later design must decide whether SIL, Home Assistant or another component owns each decision.

| Input / behavior | Existing evidence or gap | SIL need / purpose |
| --- | --- | --- |
| Signed grid import/export and input age | SIL README documents `E320.Power_in`; current hardware behavior not rechecked | Required - follow actual household demand and avoid unintended export |
| Inverter actual output and command feedback | Existing RS485 control is documented; feedback semantics need verification | Required - observe response and distinguish an allowed limit from actual generation |
| Inverter power setpoint and verified zero-output behavior | 900 W maximum / 100 W minimum owner-reported; negative-price zero setting documented, actual zero behavior unresolved | Required - preserve battery during cheap/low-PV periods without assuming a MOS cut is normal regulation |
| Current electricity price, negative-price flag and validity | SIL README documents price topics | Required - tariff-dependent mode selection and explicit priority of negative-price behavior |
| Future tariff slots and usable-energy estimate | Not established by current price input alone | Conditional - schedule energy for expensive future periods rather than only react to current price |
| PV/load forecasts and conversion losses | Later design in project intent | Conditional - predictive optimization; not required for the first reactive controller |
| Reserve target, full-battery criterion and restart hysteresis | Policy/thresholds not specified | Required - stable reserve/full decisions based on validated measurements |
| Allowed charge/discharge envelope | Battery specifications, temperatures and BMS protections not fully verified | Required - independently constrain economic decisions; preserve autonomous BMS protection |
| Collector/source availability and communication-loss behavior | Source freshness exists; end-to-end SIL behavior is not designed | Required - define stale-data handling; do not treat unchanged MQTT values as stale solely because they are silent |
| MOS command result and state readback | Only observed permission fields and reserved command paths exist | Required - observe both MOS states and current; a transport acknowledgment alone does not prove the requested electrical result |
| Command ownership, expiry, restart behavior and deliberate re-enable | Implemented session, expiry, duplicate rejection and actual readback; installation qualification pending | Required - define who may control, reject obsolete/duplicate commands, and avoid unintentional re-enable after reconnect |
| Coordinated charge/discharge stopping | Exact BMS port topology and behavior with charger/inverter connected remain unverified | Required - qualify stopping the relevant power source/load before routine MOS blocking; exact sequence and timing remain open |
| Smooth inverter ramping | SIL TODO requests separate rise/fall rates | Required for the already-planned SIL extension - coordinate with reserve/fault/zero handling; do not let an ordinary ramp delay a required stop |
| PV curtailment at full battery | Current PV power may already be curtailed; no additional energy sink confirmed | Conditional - charger limiting if selected; increasing inverter allowance cannot create household demand |

### Control boundary for later implementation

Normal economic dispatch should use the inverter power setpoint. The requested MOS controls are discrete permission/blocking operations, not a fast power-regulation actuator. Before making either MOS path writable, qualify hardware compatibility, readback, the opposite MOS state, communication loss and restart/re-enable behavior. No fixed shutdown sequence or unconditional "both MOS off" fallback is approved here.

A controllable charger stop or current-limit path may be needed for coordinated charge blocking. The actual mechanism remains open; this does not mandate VE.Direct wiring or claim that passive BLE can send commands. Verify the installation-specific behavior first. The [Victron installation guidance](https://www.victronenergy.com/media/pg/Manual_SmartSolar_MPPT_150-35__150-45/en/installation.html) is a hardware reference, not proof that a software MOS command is equivalent to safely disconnecting the battery.

Most first-stage dispatch inputs already exist. The feature fills the software gaps for minimum NTC temperature, decoded protection causes, source freshness and guarded MOS commands. A validated SOC/usable-capacity basis, sensor placement and live installation qualification remain open. Direct Victron write support is conditional; PV voltage/current, history, cycle count, balancing control and configuration/calibration writes are not prerequisites for initial price-based dispatch.

## Maintaining this inventory

When an extension is implemented, update its acquisition, decoding, web and MQTT status separately and cite the implementation and validation evidence. Keep protocol support distinct from acceptance on the actual device. Preserve the dated snapshot as historical evidence or replace it with an explicitly dated observation; never present it as live data.

The closest extensions using existing data are individual cell voltages, individual temperatures, balancing, cycle count and per-cell diagnostics beyond the new protection messages. Battery-side Victron power can also be derived from existing values. These are candidates, not approved implementation tasks.

## Approved October 16 implementation status

| Function | Current feature | Required for SIL | Remaining qualification |
| --- | --- | --- | --- |
| Minimum NTC, cell count, NTC count, protection descriptions | ESP and MQTT implemented; HA additions prepared | Required | Battery chemistry, sensor placement and operating limits |
| Source freshness and reason | Independent 5-second health objects with receipt sequence/age | Required | SIL mocked reception/reconnect/fault checks passed; real-source reception pending at the normal installation |
| Charge/discharge MOS permissions | Serialized E1 write with fresh pre-read, opposite-MOS preservation and paired readback; gate disabled | Required (owner) | Live compatibility and coordinated charger/inverter stopping |
| Remaining capacity, charging/discharging activity, cell spread | Existing fields preserved | Optional additions requested by owner | Capacity accuracy remains unverified |
| Victron PV power, output current, charge state and error | Existing passive BLE fields preserved | Required | No direct charger-control path in this scope |
| PV voltage/current | Still unsupported without VE.Direct hardware | Optional additions requested by owner | Isolated UART connection and separate approved implementation |

The software additions do not define a new SIL regulation algorithm, battery limits or automatic re-enable policy. A verified command result is protocol feedback, not proof of electrical isolation. No Victron sending is included.

### Fixed 60-second MQTT refresh

At the owner's request on October 16, every 60 seconds the collector republishes source/control availability, capabilities and all currently valid measurements, including unchanged values and differences below the deadband. This timer is independent of the normal publication interval. Between refreshes the 0.1-unit/1-W deadbands still apply. Unsupported fields remain unavailable; no fabricated values are emitted. Admission retry, bounded cursor work and OTA pause remain in force. A restarted Home Assistant recovers unchanged values within approximately one minute while the collector, sources and broker are available. Network failures or OTA pauses can extend that interval. This refresh does not replace the 5-second source-health heartbeat.
