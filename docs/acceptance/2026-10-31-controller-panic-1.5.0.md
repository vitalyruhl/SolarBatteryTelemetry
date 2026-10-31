# SBT 1.5.0 production panic analysis

Date: 2026-10-31. Scope: read-only analysis of an already captured production panic and its exact archived ELF. No firmware, configuration, device, dependency, build, upload, or Git changes were performed. This report is the only authored output.

## Conclusion

This event is a deliberate assertion in the ESP32 Bluetooth controller task, reached while the controller processes a received PDU. It is not the previously hypothesized application floating-point formatter, lwIP semaphore, or AsyncTCP null-allocation failure path.

The exact call site identifies the proprietary controller source marker **`llm_util.c:215`**. Static inspection of the matching binary shows a four-bit value read from the controller RX descriptor region entering a dispatch that handles values 0 through 6 and asserts for values 7 through 15. An unexpected advertising-PDU type is therefore the leading explanation of the immediate assertion condition. The actual runtime value, packet, transmitter, and reason that this value reached this dispatch remain unproven. Descriptor corruption is still an alternative. No production fix is established.

This evidence identifies one 1.5.0 crash. It does not prove that every earlier 1.4.x reset had the same cause or that the cell telemetry change introduced the controller problem.

## Artifact and capture identity

- Checkout: `C:\Temp\sbt-cell-diagnostics-20261031`, detached merged commit `ec24f08feab8fef5da236047c4f02976b6710bb2`.
- Matching ELF: `.pio\cell-telemetry-1.5.0\ota\firmware.elf`, 12,148,428 bytes.
- Independently computed full ELF SHA256: `e6d3835c829768f9993470052ec14fd6c07113c5691c6ae8967dd5df8ed0066e`.
- Stored crash ELF prefix: `e6d3835c829768f9`, matching the exact ELF above. The payload explicitly reports a 16-hex-character prefix, not a complete SHA256. It is not an older 1.4.x firmware prefix.
- Captures: `C:\Temp\energy-rollout-20261031\during-ha-repair.json` (`captured_utc=2026-10-31T11:44:00.460122+00:00`) and `C:\Temp\energy-rollout-20261031\sbt-runtime-during-ha.json` (`captured_utc=2026-10-31T11:51:55.825954+00:00`).
- Both contain the same retained `SolarBatteryTelemetry/Diagnostics/Panic` payload. Re-observing this retained payload is not evidence of another crash between captures.
- Dump validation: `dump_present=true`, `dump_valid=true`, `bounds_valid=true`; image/check/summary errors all zero; image address 4,128,768 (`0x3f0000`), size 25,700 bytes.
- Crash task: `btController`; PC `0x40090c90`; exception cause 0; nine frames; `bt_truncated=false`, `bt_corrupted=false`.
- Runtime environment supplied for the deployed artifact: Arduino-ESP32 2.0.17 / IDF `v4.4.7-dirty`, classic ESP32-D0WD-V3, NimBLE-Arduino 2.5.1, unchanged CM 4.10.5 local freeze, AsyncTCP 2.1.4, Live UI disabled. No upgrade was performed during this analysis.

## Exact symbolization

Tool: `C:\Users\Vital\.platformio\packages\toolchain-xtensa-esp32\bin\xtensa-esp32-elf-addr2line.exe`, with `-e <exact ELF> -a -f -C -i`. Symbolization completed successfully.

| Captured address | Matching ELF symbol | Source availability |
| --- | --- | --- |
| `0x40090c90` | `r_assert` | Build DWARF: `components/bt/controller/esp32/bt.c:1902` |
| `0x40085b1b` | `r_assert_param` | Controller binary symbol; no source line |
| `0x401543b1` | `r_llm_pdu_defer` | Controller binary symbol; assertion marker recovered below |
| `0x40150f8f` | `r_lld_pdu_check` | Controller binary symbol; no source line |
| `0x4014df52` | `r_lld_evt_deffered_elt_handler` | Controller binary symbol; no source line |
| `0x40019d11` | unresolved ROM address | No exact function attribution from this ELF |
| `0x40055b4d` | unresolved ROM address | No exact function attribution from this ELF |
| `0x401428fb` | `r_rw_schedule` | Controller binary symbol; no source line |
| `0x40142f7e` | `btdm_controller_task` | Controller binary symbol; no source line |

The `r_assert` function starts at `0x40090c90`; its body is an Xtensa `entry a1,32` followed by an intentional `ill` instruction at `0x40090c93`. Consequently, exception cause 0 is consistent with the controller intentionally invoking its assertion trap; it is not evidence here of an accidental jump into arbitrary code. The recorded PC points to the function entry, while the actual trap instruction is three bytes later.

The public [ESP-IDF v4.4.7 controller adapter](https://github.com/espressif/esp-idf/blob/v4.4.7/components/bt/controller/esp32/bt.c#L1778-L1780) independently shows the same intentional illegal-instruction implementation. Its source line number differs from this dirty Arduino SDK build; exact local ELF instructions are the controlling evidence. The public function signature also confirms the assertion argument order. Internal controller functions remain distributed as a proprietary binary; their original C conditions and variable names are unavailable in this checkout.

## Recovered assertion condition

`xtensa-esp32-elf-objdump.exe -d -C <exact ELF>` and a read-only ELF literal-pool decoder establish:

1. `r_llm_pdu_defer` starts at `0x40154204`.
2. At `0x401542dd` through `0x401542e8`, the code calculates `12 * rx_index`, adds base `0x3ffb094c`, and reads a 16-bit word from that controller descriptor region.
3. At `0x401542eb`, `extui a11,a11,0,4` extracts its low four bits.
4. At `0x401542ee`, `bltui a11,7,...` handles values below 7; otherwise the next jump goes directly to assertion setup at `0x401543a4`.
5. The below-7 dispatch covers all remaining values: mask `0x57` handles 0,1,2,4,6; separate branches handle 3 and 5. Its nominal fall-through assertion is therefore unreachable for a valid four-bit value below 7 under intact control flow.
6. Assertion setup loads the literal `llm_util.c` from ELF address `0x3f4230dd`, sets line argument 215 (`movi a13,215` at `0x401543a7`), and invokes the assertion callback at `0x401543b1`.
7. The captured second frame is the corresponding `r_assert_param` call to `r_assert` at `0x40085b1b`.

Thus, assuming intact code/control flow, the immediate rejected descriptor value was in the range 7..15. The saved JSON summary does not expose the native assertion parameters, so an exact value of 7 is not established.

The installed NimBLE vocabulary defines advertising types 0..6 and `BLE_ADV_PDU_TYPE_ADV_EXT_IND (7)` at `.pio\cell-telemetry-1.5.0\libdeps\ota\NimBLE-Arduino\src\nimble\nimble\controller\include\controller\ble_ll.h:384-392`. This supports the advertising-type interpretation; it does not make NimBLE's controller implementation the active ESP32 controller binary. A Bluetooth 5 extended advertisement is a plausible trigger, not a captured fact. Values 8..15, malformed received data, or damaged descriptor state cannot be distinguished from this summary alone.

## Application/CM boundary

The frozen CM starts a passive scan: `.local\cm-ble-reboot-4.10.5-final-package\src\bluetooth\BluetoothManager.h:351-355` initializes NimBLE, installs scan callbacks, calls `setActiveScan(false)`, and disables retained scan results. Its worker starts bounded scanning at line 370 with `start(1000,false,true)`.

The crashing frames are below those application callbacks, inside controller receive/dispatch processing. No SBT parser, MOS command handler, JSON serializer, floating-point formatter, allocator, lwIP, or AsyncTCP function appears in this captured chain. Filtering an unwanted MAC only in the SBT/CM advertisement callback occurs too late to prevent a controller assertion that happens before host report delivery. This does not rule out earlier corruption or resource pressure originating elsewhere; it rules out presenting those hypotheses as the demonstrated immediate fault site.

There is no evidence here that a MOS action or an OTA shutdown was in progress. The adjacent functions around `btdm_controller_task` include normal coexistence and scan housekeeping; their proximity is not proof that Wi-Fi coexistence caused this event.

## Resource and timing limits

The first capture's retained ESP status reports uptime 1,510,108 ms, reset reason 4, free heap 61,540, minimum free heap 21,676, and largest allocation 45,044. The later capture reports uptime 1,930,116 ms retained, followed by 1,990,119 ms live, still reset reason 4 and a 45,044-byte largest block. These samples are consistent with recovery after the separately established approximately 11:18-11:19 UTC reboot; they are not measurements at the assertion instant. Capture-level timestamps and retained values cannot establish an exact crash timestamp.

The later `Diagnostics/Runtime` sample at uptime 1,980,709 ms reports loop stack minimum free 2,020 bytes, heap free 48,688, largest block 45,044, and heap minimum 8,844. Its producer samples `heap_caps_get_info(MALLOC_CAP_DEFAULT)` and the current app task's stack (`src\telemetry\MqttService.h:194-196`). The loop-task high-water mark does not measure `btController` stack reserve. Heap metrics with different capability masks and sampling phases must not be equated. None of these post-reboot values proves or excludes pre-crash transient OOM, heap corruption, or controller stack exhaustion.

The earlier reported minimum of 1,868 belongs to another observation/boot history and is not a captured allocation failure associated with this controller assertion. The previous Newlib `_dtoa_r` OOM candidate remains a separate possible failure mechanism; this backtrace does not select it.

`abort_status="context"` is expected: this is the controller's `r_assert` trap, not the `panic_abort` context recognized by the existing abort-message resolver. Null `abort_caller_pc` is not a failed identification of the controller assertion above.

## Focused next evidence and repair proposals

1. **Preserve before changing anything.** Keep this exact ELF, both captures, and the existing dump. With separately authorized read-only dump acquisition or a future narrowly scoped diagnostic build, recover the assertion's native parameters and controller registers/descriptor context. Parameter 1 distinguishes type 7 from another rejected value. Capture controller UART assertion output if available without resetting production. Do not erase the stored dump or trigger a deliberate production panic to collect more evidence.
2. **Reproduce the descriptor/PDU hypothesis on an isolated original ESP32.** Use the same SDK/controller binary and passive scan configuration, with a controlled BLE environment and packet capture. Compare legacy-only traffic with explicitly generated extended advertisements, and separate passive scanning from the BMS GATT connection. Capture assertion parameters, controller-task stack reserve, capability-specific allocation failures, and RF packets. No production BLE toggles or MOS writes are implied. Failure to reproduce is not proof of safety.
3. **Choose a controller-focused remedy only after that result.** If an unsupported/malformed PDU reliably triggers this exact assertion, investigate an Espressif-supported controller/SDK correction and validate it as a separate dependency migration. A host-only NimBLE or AsyncTCP update does not by itself prove replacement or correction of this proprietary controller path. No exact upstream fixed release for `llm_util.c:215` was verified in this analysis. Do not patch out the assertion or assume a software MAC filter protects the earlier controller receive path. If instead dump/allocator evidence shows descriptor corruption, investigate that producer before selecting a radio upgrade.

## Validation performed

Independently hashed the exact ELF, symbolized every recorded address, inspected the relevant controller instructions and literal pools, checked the two existing capture files, and inspected only the named passive-scan and diagnostics producer boundaries. Applicable root/role governance files in the detached checkout were byte-identical to those already read in the canonical SBT checkout. No tests or builds were run because no software change was made; no long-run stability or repaired behavior is claimed.
