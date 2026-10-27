# SBT 1.4.12 HTTP OTA and BLE shutdown acceptance

The firmware uses immutable CM 4.10.5. Its production BIN SHA256 is
`037eccd7ae13cc1e8d7b69b4bb6e768ecbe2b46e0af361acac1e6796caf1cf5a`, and the exact
ELF SHA256 is `ed82ee8b8b7f6428826a92278597f1b2b7003da7ec4c99af034f0efb2e4e0dee`.
The consumed 193-file CM source manifest SHA256 is
`70eceafa310481762f4f89ae06daf1d21f9aa72d46c0575b4a09ea7ed0c9b568`.
All event times below are UTC.

## Reproduced fault and correction

The old HTTP upload path called `ESP.restart()` from an AsyncTCP disconnect
callback. An isolated same-image OTA with an initialized BLE controller caused a
task watchdog naming `async_tcp`. Deferring the restart to the application loop
removed that callback dependency, but did not complete the repair: the next real
BLE-active test still starved `IDLE0`, with the WiFi task running. The exact same
binary and upload without an initialized BLE controller restarted normally.

CM now queues the HTTP reboot once and polls an optional preparation callback
from the application loop. SBT requests terminal BLE shutdown there. Only the
existing BLE worker cancels operations and calls `NimBLEDevice::deinit(false)`.
It requires both a stopped host and native controller `IDLE` before calling
`deinit(true)` to delete objects. Generation fences reject late work and prevent
reinitialization. Failure or a ten-second preparation deadline blocks the
automatic restart; the watchdog remains enabled. The ArduinoOTA transport was
not changed by this HTTP-specific repair.

The tests cover the actual worker and HTTP methods, false stop/cleanup results,
host/controller mismatches, unavailable workers, duplicate requests, late
callbacks, wraparound, feature-disabled builds and setup callback preservation.
Normal production firmware contains neither an intentional panic trigger nor
the compile-time shutdown phase markers.

## Bounded hardware evidence

The isolated normal test profile used passive Victron reception with native
MIN_MODEM, requested power saving false and no native apply/read errors. BMS
control remained blocked and MQTT stayed under the test namespace.

Same-image OTA was accepted on 2026-10-26 at 23:50:18.278287. Actual phase markers
confirmed stop success, stopped host and native `IDLE`, object cleanup, ACK and
only then the main-loop restart. The stop and confirmation took about 16 ms.
The reboot produced SDK reset reason 3 and continued for more than two minutes
without a watchdog or new panic. HTTP confirmed the intended profile and all 33
settings, with only the deliberate test-peer clearing after reboot.

An earlier fixture bootloop was a separate owner upload error: an application-only
USB upload retained SIL's smaller partition table. Saved flash bytes proved this;
the frozen SBT bootloader, partition table and OTA selection repaired it before
the actual BLE test. Firmware sources were unchanged by that repair.

## Production installation and diagnostic limits

The first production upload was accepted at 2026-10-26 23:54:53.736541. Its outgoing
1.4.9 firmware still used the old shutdown path, so the resulting reason-6 reboot
does not establish a failure of the newly installed code. Fresh identity and HTTP
confirmed 1.4.12 and 33 unchanged settings.

A separate deliberate same-image validation exercised the new code with the
actual production BLE connections. It was accepted at 2026-10-27 00:00:19.909563;
fresh telemetry confirmed reset reason 3, restored BMS/Victron samples and another
complete unchanged-settings reconciliation.

Boot diagnostics read the stored flash dump once. The bounded SDK/ELF resolver
publishes only a strict abort caller PC and core, without raw RAM, register lists
or free text. This resolved the reproduced watchdog despite its damaged
backtrace. The stored dump has no crash timestamp; publishing a historical dump
after reboot is not evidence of a new panic. The earlier spontaneous runtime
panic remains unassigned. Chip temperature was not measured, and these tests do
not establish long-term crash freedom or a thermal cause.


## Final production observation

The complete 900-second window on 2026-10-27 from 00:00:32 through 00:15:32 passed. SIL 5.4.7 supplied 294 runtime samples with boot count seven and all nine transport counters zero. SBT supplied 15 ESP samples with reset reason three and monotonically increasing uptime. Neither device reported an offline transition or another reboot; BMS and Victron health remained available. Both plan types remained valid with 144 slots while SIL requalified the restarted sources. The observed source fence was about 101 seconds; unchanged arrays were replaced by fresh qualified captures before the original retention deadline. This bounded result does not establish long-term crash freedom.
