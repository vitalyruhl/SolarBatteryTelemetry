# SolarBatteryTelemetry

Preparation scaffold for a dedicated ESP32 that will read a battery BMS and a
Victron solar charger and publish telemetry via MQTT for Home Assistant.

**Status: preparation only.** The firmware compiles and links ConfigManager,
sets application metadata, prints a serial status, and idles. It does not start
WiFi, MQTT, Bluetooth, a web server, OTA, or GPIO activity. Device adapters,
runtime settings, MQTT schema/discovery, and the MQTT activity LED are pending.

## Preparation baseline

- PlatformIO / Arduino / `nodemcu-32s` (provisional board from the limiter).
- ESP32 Configuration Manager `4.2.2`, matching the limiter baseline.
- No display, wired sensors, relay outputs, or RS485 driver in this project.
- Only the built-in LED is planned as a hardware output, pulsing on successful
  MQTT publication. Pin and polarity must be confirmed for the actual board.
- SolarInverterLimiter remains responsible for inverter control.

Read [project.ifo.md](project.ifo.md) for decisions, context, and open questions.

## Local configuration

`src/secret/secrets.h` contains local credentials copied from the limiter.
Only the application name and MQTT root were changed to `SolarBatteryTelemetry`.
The inherited static IP is not a new allocation. Before flashing, the owner must
select a free address and update `src/secret/secrets.h` and
`platformio.local.ini`. Credentials and local configuration are ignored by Git.

On a fresh clone, copy `src/secret/secrets.example.h` to `src/secret/secrets.h`
and create `platformio.local.ini` (an empty file is sufficient for USB builds).
The example contains placeholders only. This scaffold does not apply network
settings at runtime yet. ConfigManager UI and credential/default provisioning
will be connected during the approved implementation phase.

## Local validation

```powershell
pio run -e usb
pio run -e ota
pio run -e usb -t compiledb
```

These commands build firmware or generate analysis data; they do not upload it.
No device upload or serial monitor session is part of this preparation.

## Local agent setup

Governance (`AGENTS.md`, `.github/`), Serena (`.serena/`), ProjectAtlas
(`.projectatlas/`), editor settings, compilation databases, and local notes are
intentionally untracked. They exist only on this PC and are not restored by a
Git clone. Use `.local/tooling.md` on this PC for exact tool commands and
`.local/governance-provenance.md` for template provenance.

This repository has no remote. The initial checkpoint is a local preparation
commit; implementation begins only after owner review and further device data.
