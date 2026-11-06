# ESP32-C3 BLE recovery investigation, 2026-11-06

Status: replacement C3 installed; bounded desk and real-source gates passed.

## Observed production state

The existing collector at 192.168.2.21 reports SBT 1.5.1. Its live Bluetooth
setting was false; BMS and Victron MQTT health were unavailable with sequence
zero. The earlier 1.5.1 experiment deliberately persisted BLE disabled and
enabled it only in RAM. Rebooting therefore does not restore readout.
No production firmware or setting was changed during candidate preparation.

The stored panic reports ELF prefix `47ed3c8002bcad9e`, task `tiT`, cause 28.
The exact archived ELF was found in the project-local cleanup archive and its
complete SHA256 verified as
`47ed3c8002bcad9e76d784bf7f39a52a37c7c4176067bb80a56dbe0bf6495eee`.
The decoded stack is `udp_input` -> `ip4_input` -> `ethernet_input` ->
`tcpip_thread_handle_msg`. This is a UDP receive crash, distinct from the older
Bluetooth-controller assertion. The UDP PCB access is invalid; the concrete
origin of its corruption or lifetime failure is not dynamically proven.

The frozen CM calls Arduino 2.0.17 `configTzTime` from the WiFi lifecycle and
`ESP_TIMER_TASK`. Arduino uses raw SNTP operations outside the TCP/IP task.
The unchanged function fails the host SDK-affinity regression. SBT now redirects
that one legacy header entrypoint to a serialized adapter using `esp_sntp_*`.
The SDK marshals those operations to TCP/IP. Production crash causality remains
a hypothesis; the test establishes the unsafe API boundary and its correction.

## C3 test constraints

The owner replaced the previous test board with COM10. Esptool identified
ESP32-C3 revision 0.4 with 4 MB embedded flash and USB Serial/JTAG.

- Environment: `testesp_c3`, firmware candidate 1.5.2.
- Native C3 controller; never link the classic ESP32 replacement archive.
- Real configured read peers; DHCP; MQTT root `SBTTest/C3` and unique client.
- BMS MOS and gate commands blocked regardless of persisted, UI or MQTT values.
- No publication into production telemetry or Home Assistant discovery.
- The owner confirmed ESP32-C3 Super Mini V1601. The selected custom board
  profile supplies ordinary GPIO8, active-low, to CM's board-aware default LED
  constructor. It does not use the generic DevKit virtual RGB pin.
- Do not make this profile a production collector by changing settings; its
  namespace and control restrictions are compile-time policy.

The build derives the two exact `std::min<uint32_t>` corrections needed by
RISC-V plus the reviewed CM LED helper backport from the unchanged CM 4.10.5
freeze. The two derived helper files match the canonical CM 4.11.0 candidate
after line-ending normalization. No whole-library upgrade is imported. All
other package bytes and the complete file set remain checked. Classic builds
retain strict original package bytes.

CM's explicit pin/level constructor stays compatible; the new default
constructor uses board metadata. Constructors perform no GPIO/time calls.
First runtime use preloads the inactive latch before enabling output.

## Validation

- New profile regression failed before implementation, then passed.
- Existing smoke, MQTT contract/lifecycle and namespace checks passed.
- Actual BMS Readout field-profile test rejects control-gate and MOS requests.
- Thirteen focused CM derivation tests passed, including all helper anchors,
  CRLF preservation, interrupted generation and tampering rejection.
- NTP regression reproduces the raw SDK-affinity failure and exercises 200
  concurrent timer/WiFi-like requests through the corrected adapter.
- The first C3 upload failed its start gate with repeated load-access panics.
  The exact C3 ELF decodes the path through `xTaskCheckForTimeOut`,
  `xQueueSemaphoreTake`, `rmt_write_items`, and `neopixelWrite` during global LED
  initialization. The DevKit variant's `LED_BUILTIN` is a virtual RGB pin,
  invoking the RMT driver before a current FreeRTOS task exists. A temporary
  LED-disabled candidate passed 310.277 seconds with 202 MQTT messages,
  10 successful HTTP checks, no panic markers and no observed reset. The failed image and UART log are
  preserved locally; this was a test-only failure.
- The owner subsequently requested board-aware CM LED support. The canonical
  CM 4.11.0 candidate passed three helper host profiles and a USB build; it is
  uncommitted. The exact LED-enabled C3 image completed 310.073 seconds with
  223 MQTT messages, no serial panic/Guru markers, no observed reset and no
  current-image stored panic. Ten HTTP checks succeeded after one startup
  timeout. The owner confirmed that the physical status LED blinks.
- Actual BMS/Victron acceptance requires moving the board to the shed.

No long-term stability or hardware-defect conclusion follows from a desk test.

## Owner-authorized replacement after resume

The owner requested the same IP and MQTT identity for the C3 and confirmed the
original collector was already disconnected from power. The `c3` environment
runs normal SBT behavior with the same C3 hardware corrections; `testesp_c3`
continues to enforce its separate restrictions.

All 33 settings were compared with the last production backup, saved on the
C3 and verified again after booting the normal firmware at 192.168.2.21.
The BMS/Victron identities match the backup; the Victron, WiFi, MQTT and OTA
secrets match the existing local production inputs without being printed.
The previous control policy, MQTT client/root, 2-second publication interval,
100-second full refresh, 90-second BMS timeout and 5-minute WiFi recovery
timeout are retained. BLE is intentionally persisted enabled, correcting the
former BLE-off-after-reboot behavior. Private comparison evidence stays local.

The replacement image is under .pio/bt-controller-1.5.1/c3/. Its ELF SHA256 is
`5a282e926fc147880883d9d629dae8a0af8f3b457e21bba3ff7cd347e3c5a69d`;
the BIN SHA256 is
`a65c78d7d97fe04ec1641e18b1863441369dceddffe747f3951f6130cf91f9d2`.
The exact normal replacement image passed 310.244 seconds, 207 MQTT messages,
zero serial panic/Guru markers, no observed reset, no matching stored panic,
and ten successful HTTP checks. MQTT independently identified the C3 hardware
MAC. All 33 saved settings, including secrets checked privately, survived boot.
No firmware was uploaded to the old SBT or SIL during this replacement stage.

## Real-source acceptance

After the owner moved and powered the C3 at the installation site, both real
sources became available. The read-only MQTT observer completed 311.125
continuous healthy seconds within 387.798 total seconds, capturing 701 messages.
BMS sequence advanced from 1 to 54 and Victron from 1 to 105. Final sample ages
were 1152 ms and 454 ms respectively; the BMS cell payload contained all 14
series-group voltages. Runtime increased monotonically across the observed
samples, and no stored panic matching the exact replacement ELF was reported.

The final snapshot reported BMS 51.46 V, SOC 47%, current 16.8 A, power about
863 W and alarm bits 0; Victron reported 931 W solar power and 52.41 V. These
are simultaneous-observation bounds, not independently calibrated measurements.
The NTP/UDP historical crash trigger and long-term reliability remain unproven.
Private raw evidence is .local/c3-20261106-field.json. No sensor values were
fabricated or republished by the observer.
