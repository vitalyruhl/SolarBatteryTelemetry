# Victron Instant Readout

The collector decodes the Solar Charger advertisement record (`0x01`) used by the
owner's SmartSolar MPPT 150/35. It receives passive BLE advertisements through the
existing ConfigManager worker. It does not pair, connect to GATT, or send charger
commands. The adapter uses the random static BLE address type required by these
Victron advertisements.

## Settings

Settings > Bluetooth > Victron exposes the Instant Readout MAC and the encryption
key. The MAC accepts 12 hexadecimal characters or six hexadecimal pairs separated
by colons; uppercase and lowercase are accepted. The worker receives canonical
lowercase colon notation. The key is
exactly 32 hexadecimal characters (16 bytes), not the Bluetooth pairing PIN.
An empty field disables readout. Malformed edits are rejected; invalid persisted
configuration disables readout. The key is a password setting and is omitted
from runtime telemetry and logs.

`VICTRON_READOUT_MAC` and `VICTRON_READOUT_KEY` from the ignored secrets header
provide defaults only when the corresponding NVS settings do not exist.
Persisted edits, including empty values, remain authoritative. Configuration
callbacks only update bounded mailboxes; the application loop reconfigures the
existing BLE worker and invalidates old samples. No second BLE stack or scanner
is started. Missing configuration pauses this collector's BLE work.

## Measurements and freshness

The implemented fields use the existing MQTT channel units:

| Channel | Unit |
| --- | --- |
| `victron/solar_power_w` | W |
| `victron/battery_voltage_v` | V |
| `victron/charge_current_a` | A |
| `victron/load_current_a` | A |
| `victron/charge_state` | Existing named state enumeration |
| `victron/error_code` | Numeric charger error code |
| `victron/yield_today_wh` | Wh |

The protocol's unavailable sentinels become null, including load current when
unsupported by the device. PV voltage/current remain unsupported; battery power
remains reserved for later derivation. Charger output current is not net battery
current. Unknown charge states map to `other`; state 255 is unavailable.

A reading becomes unavailable 30 seconds after its last new data counter. This
is an explicit telemetry-display policy, not a control or safety threshold.
Duplicates, truncated records, and unsupported records do not refresh samples.
Timestamp subtraction handles `millis()` rollover. Configuration changes clear
samples immediately; OTA/configuration pause suppresses availability. Disabling
Bluetooth stops reception; an existing sample expires within the same 30-second
window. Reception has no application timeout that triggers a connection or write.

A key-check mismatch clears availability and stops decoding until configuration
changes. AES-CTR has no authentication tag: matching the first key byte does not
prove the entire key is correct or authenticate the sender. Confirm actual
readings against VictronConnect during physical acceptance.

## Verification and evidence

`tools/test/run-victron-host-tests.ps1` covers the decoder, an independently
computed AES-CTR fixture using Windows CNG, strict configuration validation,
unavailable fields, signed current, state mapping, truncated messages, duplicate
counters, stale data, timer rollover, and key mismatch handling. The fixture is
synthetic, with a public test key, and contains no captured device data.
Firmware builds validate the production mbedTLS integration. Actual RF reception,
key compatibility, coexistence with WiFi/OTA, and agreement with VictronConnect
remain device-acceptance checks; a build does not establish those results.

Protocol sources:

- [Victron Extra Manufacturer Data specification](https://communityarchive.victronenergy.com/storage/attachments/extra-manufacturer-data-2022-12-14.pdf)
- [Victron BLE advertisement reference implementation](https://github.com/Fabian-Schmidt/esphome-victron_ble/blob/main/components/victron_ble/victron_ble.cpp)
- [VictronBLE observer address-type requirements](https://gitea.sh3d.com.au/Sh3d/VictronBLE/src/branch/main/samples/observer)
