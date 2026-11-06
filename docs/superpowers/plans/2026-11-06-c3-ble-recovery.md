# ESP32-C3 BLE recovery test plan

Goal: prepare the owner-connected ESP32-C3 on COM10 as a read-only SBT test device for a subsequent shed comparison.

The production SBT 1.5.1 currently has Bluetooth disabled after its deliberate BLE-off recovery boot. Its stored current-image panic is in task `tiT`; the exact cause needs matching-ELF evidence. Do not equate this with the older Bluetooth-controller assertion.

- [x] Add and verify an isolated field-test profile: `SBTTest/C3`, actual configured read peers, DHCP, unconditional MOS/gate command denial.
- [x] Select the native C3 board/framework controller without the classic-ESP32-only controller override. Preserve classic profiles.
- [x] Compile relevant host isolation tests, the C3 firmware, and the required classic USB profile; bump the candidate patch version to 1.5.2.
- [x] Identify and upload only COM10; observe serial and MQTT for at least five minutes, recording reset/panic evidence and HTTP reachability.
- [x] Report bench bounds and request the owner's physical move to the shed for actual BMS/Victron acceptance.

## Owner-authorized replacement scope after resume

The owner now requests that the C3 replace the original collector with the same
IP and MQTT identity. The owner confirmed the original SBT is disconnected from
power. This supersedes the earlier test-namespace-only constraint for the C3.

- [x] Compare all settings with the last production backup; validate real peer
  identities and local secret inputs without exposing them.
- [x] Persist the old IP 192.168.2.21, production MQTT identity and existing
  control policy. Persist BLE enabled to restore readout after reboot.
- [x] Build/upload only COM10 using normal `c3` behavior; repeat the five-minute
  panic/reset observation on the exact replacement image.
- [x] Have the owner move the board into the shed, disconnect the phone BMS
  client, and validate both real sources before claiming connection recovery.

Result: the replacement C3 passed 311.125 seconds of continuous real-source
availability, advancing BMS/Victron sequences, 14-cell readout and no observed
current-image panic or reset. This is a bounded acceptance, not long-term proof.

The replacement stage did not authorize commits, pushes, merges, uploads to the old SBT or SIL, or synthetic production telemetry. The replacement C3 retains the previous SBT policy.

## Owner-authorized integration

After the separately authorized SIL production deployment passed its bounded gate,
the owner authorized committing and pushing the reviewed SBT 1.5.2 changes,
integrating them into main through a pull request, and removing only branches
verified as integrated. This supersedes the earlier Git restriction. No release
publication or additional device operation is included.
