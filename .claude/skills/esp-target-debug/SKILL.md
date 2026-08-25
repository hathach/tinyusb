---
name: esp-target-debug
description: Use when debugging TinyUSB firmware on Espressif boards (ESP32-S3/P4 on the rig — dcd_dwc2 examples, idf.py builds) with the chips' built-in USB-Serial-JTAG — attach/halt/backtrace, breakpoints, FreeRTOS task lists, console capture — or when JTAG "could not find or open device", the 303a:1001 port vanishes, or the S3's debug port turns into the TinyUSB device.
---

# esp-target-debug — Espressif built-in USB-JTAG backend

Methodology — intrusiveness ladder, board locks, dual-side capture, diagnosis
standards — lives in `target-debug`; this skill is the Espressif backend: a
different gdb, a different openocd (fork), no probe serial (the debugger IS a
USB device), and a PHY story that decides whether JTAG exists at all.
Built-in USB-Serial-JTAG only; external JTAG is a TODO (no rig adapter).

## The PHY map — decides everything (verified on the rig)

| Board                    | USB-SJ vs TinyUSB OTG                                | JTAG while USB device runs? |
|--------------------------|------------------------------------------------------|-----------------------------|
| espressif_p4_function_ev | separate pins: USB-SJ GPIO24/25 (FS), OTG own HS PHY | **yes — coexist** (verified: 303a:1001 + cafe:4008 simultaneously, gdb attach during live CDC traffic) |
| espressif_s3_devkitm     | ONE shared PHY/port                                  | **no** — the same hub port flips 303a:1001 → cafe:4008 as the app boots; openocd fails `esp_usb_jtag: could not find or open device!` |

Flashing works in ANY PHY state: the rig flashes via the boards' CP2102N UART
bridges (hence `tinyusb.json` esptool uids are CP210x serials, not MACs). The
UART side is also the remote reset: `esptool.py --after hard_reset read_mac`.

### P4 (Function-EV) notes

- The board has **no USB-SJ connector** — GPIO24 (D−, white) / GPIO25 (D+,
  green) / GND are broken out from header J1 to a rig hub port. Swapped
  D+/D− enumerates as `new low-speed USB device` + error -71; correct shows
  `new full-speed`.

### S3 (DevKitM) notes

- Debugging windows: non-USB firmware (`board_test` — attach/halt/symbol
  resolution verified; `usb_new_phy` is absent from the ELF when
  `CFG_TUD/TUH_ENABLED` are 0), bootloader/ROM (always stable), or external
  JTAG (TODO).
- **Keep-alive quirk (verified)**: with app firmware running and nothing
  attached, USB-SJ drops ~4 s after boot (device-side disconnect, then
  half-dead `-71` setup failures until reset). Attach a client inside the
  window — once it survives the window it stays up. Recover via the UART
  reset above.
- PHY mux reference: `RTC_CNTL_RTC_USB_CONF_REG` (0x60008120) bits
  `SW_HW_USB_PHY_SEL`/`SW_USB_PHY_SEL` (TRM 10.56); 0 = eFuse/hardware
  control (default). `esptool.py read_mem/write_mem` peeks and pokes
  registers over plain UART with the chip in download mode.

## Attach

```bash
. $HOME/code/esp-idf/export.sh          # openocd-esp32, riscv32-/xtensa-esp32s3-elf-gdb, esptool
openocd -c 'set ESP_RTOS FreeRTOS' -f board/esp32p4-builtin.cfg \
        -c 'adapter serial <MAC-with-colons>' &        # S3: board/esp32s3-builtin.cfg
riscv32-esp-elf-gdb -batch -ex 'target extended-remote :3333' \
  -ex 'tbreak tud_task_ext' -ex continue -ex bt -ex 'info threads' -ex detach <elf>
# S3 is Xtensa: use xtensa-esp32s3-elf-gdb with the same arguments
```

- `adapter serial` = the chip MAC **with colons** — the USB-SJ device's
  iSerial exactly as `lsusb -v -d 303a:1001` or
  `/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_<MAC>-if00`
  prints it. (The `tinyusb.json` esptool uids are the CP2102N *flasher*
  serials — a different port; never pass those to openocd.)
- `set ESP_RTOS FreeRTOS` must precede the board cfg: with it, `info threads`
  lists every task with name/state/CPU (verified: usbd Running @CPU0, IDLE1
  @CPU1, ...); without it, one bare "Remote target".
- The ELF: `idf.py -B <builddir> -DBOARD=<board> build` under the example
  (CLAUDE.md Espressif notes) — symbolized app backtraces verified
  (`tud_task_ext` ← `usb_device_task` ← `vPortTaskWrapper`).
- **Attach may reset the target** (a boot-fresh FreeRTOS tick observed on a
  minutes-old session). Until pinned down, do NOT trust built-in-JTAG attach
  for post-mortem autopsy of a wedged board (`target-debug`'s
  attach-and-halt-only rule); capture state via console or treat the reset
  as part of the reproduce cycle.
- Halting still stops USB service: the host may drop the DUT during long
  halts; after detach the device may need the UART reset to re-enumerate.

## Scripted-session gotchas (verified)

- xtensa-gdb batch `continue`/`interrupt` is async-flaky — for scripted
  state reads, halt via openocd telnet :4444 first, then attach gdb to the
  stopped target. Interactive sessions are unaffected.
- cpu1 debug-logic examination can fail (`OCD_ID = 00000000`) —
  `-c 'set ESP_ONLYCPU 1'` degrades to cpu0-only debugging.
- ROM-frame backtraces (`0x4004xxxx` on S3, `0x4fc0xxxx` on P4, all `??`)
  mean the core idles in ROM — break in app code (`tbreak tud_task_ext`)
  for symbolized frames.

## Technique mapping (vs the `target-debug` arsenal)

| target-debug technique | Espressif backend |
|------------------------|-------------------|
| GDB autopsy, bp/wp     | same flow via openocd-esp32 :3333; RISC-V triggers (P4) / Xtensa 2 bp + 2 wp (S3) |
| Vector catch           | none — breakpoint the panic handler; `mcause`/`mepc`/`mtval` on P4 |
| SWO / DWT data trace   | none — apptrace over JTAG is the analog (untested: needs CONFIG_APPTRACE + app init) |
| RTT / TU_LOG           | console on **UART0 = the CP2102 flasher tty** by default (verified); USB-SJ console needs sdkconfig `ESP_CONSOLE_USB_SERIAL_JTAG` (untested) |
| FreeRTOS threads       | native — `set ESP_RTOS FreeRTOS` (see Attach) |
| verifybin              | `esptool.py verify_flash` (untested) |

## Rig deltas

- Locks per `hil` skill; reflash-pristine before release applies unchanged.
- One client per USB-SJ: openocd and a terminal on the USB-SJ CDC side
  conflict the same way J-Link clients do.

## TODO — external JTAG (needs hardware)

S3 JTAG pins GPIO39–42 (MTCK/MTDO/MTDI/MTMS) + any adapter openocd-esp32
supports (ESP-Prog/FT2232-class); would give S3 debugging under live USB
traffic. Mind `EFUSE_DIS_PAD_JTAG` / JTAG-source strapping. Unverified.
