# SBT 1.5.1 controller candidate

Status: owner-authorized production experiment installed; long-term repaired behavior remains unproven.

## Production experiment

The owner explicitly superseded the proposed six-hour bench gate and requested production deployment immediately. The completed COM8 initial gate observed300.016seconds, one initial boot,54scan starts,14live MQTT runtime reports, no panic, no unexpected reboot and a maximum runtime-message gap of30seconds. Actual BMS/Victron coverage was unavailable on the bench.

An authenticated HTTP rollback on COM8 from1.5.1 to the exact old isolated1.5.0 image succeeded, including orderly BLE shutdown and version readback. An earlier unpaced HTTP attempt and a later paced forward attempt failed with Windows10053; do not describe HTTP transport as generally reliable. The fixture remains at1.5.0 after that separate forward failure.

Production192.168.2.21 was confirmed at1.5.0 before writing. All33config values were backed up. Existing CM save/apply APIs were source-reviewed: persist BluetoothEnable=false, upload, confirm thatfalse survives boot, then applytrue in RAM only. The one-time readout marker preservedfalse. Actual post-boot readback confirmed thatonly this setting changed; all other32values matched.

Regular ArduinoOTA installed the exact production1.5.1 image SHA256eb25748b97a78971d778a526852bfc5f43a82a01643a01523b367fb9f4ee1625. Uploader exit0 and live version1.5.1 were verified. BLE was subsequently appliedtrue without saving. Avoid Save all during this experiment: it would persist the temporarytrue value. A subsequent reboot deliberately leaves BLE off and OTA available for rollback. Live remains disabled; no MOS policy or SIL setting was changed.

The requested300-second production observation captured626MQTT messages and12live runtime samples. Uptime increased monotonically from210473to480481ms. BMS sequence advanced1to53, both sources were available, and52cell-array messages were observed; the final14-group array was56ms old. Only the historical1.5.0 panic prefixe6d3835c829768f9 was seen, not the current production ELF prefix47ed3c8002bcad9e. No reboot or new matching panic was observed in this bounded interval. Last default-heap free/largest/minimum values were48280/42996/11264bytes, with2012bytes loop-stack minimum reserve; these are not pre-crash evidence or long-run leak proof.

The original production1.5.0 binary and source checkpoint remain preserved. No claim that all historical panic mechanisms are fixed is made. The six-hour bench run was not performed by explicit owner direction.

## Evidence and scope

Production1.5.0 crashed in btController at llm_util.c:215. The exact ELF identifies controller0f0c5a2; its linked archive is byte-identical to the original IDF4.4.7 archive. Espressif identifies the same assertion in issue13516 and provides an IDF4.4.7-specific replacement in [this official release](https://github.com/espressif/esp-idf/issues/13747#issuecomment-2139078325).

Use the targeted4.4.7(0b40e99) release, not the larger generic4.4-branch e314148 candidate. Independent static comparison found no removed existing global definitions or changed sizes of shared global objects. New timer dependencies already exist in the installed ROM linker script. That supports testing; it is not an ABI or runtime proof.

Only the controller archive, version and isolated-test coverage change. CM4.10.5, NimBLE2.5.1, AsyncTCP2.1.4, settings schema, MOS policy and Live-disabled behavior remain unchanged.

## Reproducible build

Run `python tools/bt_controller.py --prepare` once. This verifies the complete official ZIP and exact member hashes before writing only below `.local/bt-controller/0b40e99/`.

- ZIP SHA256:621c70f029f522328907a5815ae43107b19b85725cd3e46a7e07e4a9e535c63f.
- Controller SHA256:3d890636991328a7a0aec387c876104372a1dabcd4920cec061268ecfe327b33.
- Expected untouched SDK archive SHA256:981595473bda6953657f7504a6711d522fc112654759dd65bd9f9a07145b7078.

`post:tools/bt_controller.py` rejects other chips/framework baselines and wrong archives. It prepends the local library path without replacing the shared SDK. Post-link verification checks every mapped controller object, the embedded controller identity and the unchanged original archive, then writes controller-provenance.json beside the ELF. Build all profiles with `pio run -e usb -e ota -e testesp`.

## Extended test

Use only the owner-confirmed COM8 fixture. The full SBT test profile retains DHCP, the SBTTest/COM8 namespace and unconditional command isolation. Real peer settings stay empty. A clearly marked passive-only advertisement filter enables the normal CM scan scheduler without configuring a GATT connection or decoding fabricated measurements. The fixture macro fails compilation outside the isolated profile.

The initially proposed six-hour gate was cancelled by the owner's later instruction to deploy production now. Preserve that distinction from the completed five-minute bench gate. Record serial output and periodic progress before declaring any later extended gate result.

The desk cannot reach the BMS/Victron in the shed. This test exercises passive reception and Wi-Fi/coexistence, not the actual BMS connection-state trigger, device telemetry or battery control. Production testing must retain this qualification even if the bench gate passes.

## Recovery

The unchanged baseline is main db714531b11ac0ad5ddeed09fd1de8f87f93f2cd. A verified complete Git bundle and the exact1.5.0 OTA bin/ELF/map, frozen CM and private build inputs are preserved locally at C:/Temp/sbt-controller-fix-recovery-20261031. Private inputs remain outside Git.

The original OTA binary, not a new rebuild, is the preferred firmware rollback. Candidate builds never replace the global SDK, so the baseline remains rebuildable. Keep baseline binary hashes, current production settings and panic evidence before any production write. Check the actual device identity and version first.

A source branch cannot recover an ESP that loses OTA before startup completes. Before a production experiment, establish an OTA-reachable BLE-disabled startup/recovery procedure with existing settings, or arrange physical USB recovery. Do not claim guaranteed remote recovery from an early boot failure. Do not erase NVS or the stored crash partition as a rollback shortcut.

No main merge, release or branch cleanup is included in this separate candidate experiment.
