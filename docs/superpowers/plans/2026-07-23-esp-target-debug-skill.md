# esp-target-debug Skill Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create `.claude/skills/esp-target-debug/SKILL.md` (Espressif built-in USB-Serial-JTAG debug backend) with every recipe verified on the rig's P4, the S3 PHY boundary verified both ways, plus pointer edits in `target-debug` and the `target-debugger` agent.

**Architecture:** Per spec `docs/superpowers/specs/2026-07-23-esp-target-debug-design.md`. Verification-first: hardware gates 1–6 run before the skill text lands, so only proven content ships unmarked. One lock session per board.

**Tech Stack:** ESP-IDF at `$HOME/code/esp-idf` (`export.sh` → `openocd-esp32`, `riscv32-esp-elf-gdb`, `xtensa-esp32s3-elf-gdb`, `esptool.py`), rig boards `espressif_p4_function_ev` (uid 6055F9F98715), `espressif_s3_devkitm` (uid 84F703C084E4).

## Global Constraints

- Worktree `/home/hathach/code/tinyusb/.claude/worktrees/improve-debug-skill-agent`, branch `claude/improve-debug-skill-agent`.
- Board-lock discipline per `hil` skill; reflash pristine firmware before release; evidence (command + output snippet) in commit message bodies.
- Formatting: aligned table columns, skill-name-only cross-references.
- Unverified content ships tagged `(untested)` or not at all.
- Espressif anything requires `. $HOME/code/esp-idf/export.sh` in that shell first.

---

### Task 1: P4 recon + coexistence gate (spec gates 1)

- [x] **Step 1: Environment + firmware recon**

```bash
ls $HOME/code/esp-idf/export.sh && source $HOME/code/esp-idf/export.sh && which openocd riscv32-esp-elf-gdb
ls /home/hathach/code/tinyusb/examples/cmake-build-espressif_p4_function_ev 2>/dev/null || echo "no prebuilt"
lsusb -d 303a:1001   # USB-SJ devices present
```
If no prebuilt firmware: build `device/cdc_msc_freertos` for the P4 (`idf.py -DBOARD=espressif_p4_function_ev build` in that example, per CLAUDE.md), else use the prebuilt binary. Identify the ELF path for gdb symbolization.

- [x] **Step 2: Lock P4, ensure known firmware, confirm DUT traffic**

```bash
python3 test/hil/board_lock.py hold espressif_p4_function_ev --reason "esp-target-debug verify: coexistence"
# flash known build (esptool/idf.py flash -p <port-by-uid>), settle, then confirm enumeration:
lsusb | grep -i cafe   # TinyUSB VID on the DUT port
# generate traffic: echo > /dev/ttyACM<N> of the cdc, or timeout 5s cat
```

- [x] **Step 3: Attach openocd over USB-SJ while the device runs**

```bash
openocd -f board/esp32p4-builtin.cfg -c 'adapter serial 60:55:F9:F9:87:15' &   # gdb :3333 — USB-SJ iSerial = MAC with colons
riscv32-esp-elf-gdb -batch -ex 'target extended-remote :3333' -ex 'monitor halt' \
  -ex bt -ex 'monitor resume' <p4 elf>
```
Expected: backtrace with symbols; after resume the CDC device still answers (re-run the traffic check). Record: does the DUT drop off the bus during halt (host URB timeouts — expected per target-debug) and does it recover on resume without re-enumeration?

- [x] **Step 4: Release-or-continue checkpoint** — keep the lock for Task 2 (same session). No commit yet; evidence to `/tmp/esp_evidence.txt`.

### Task 2: P4 budget, watchpoint, threads, console (spec gates 2–4)

- [x] **Step 1: Breakpoint/watchpoint budget** — RISC-V trigger count: in gdb `monitor riscv info` or set watchpoints until rejection; verify a hardware watchpoint on a TinyUSB variable (e.g. `watch -l` on a usbd counter) reports and hits.
- [x] **Step 2: FreeRTOS threads** — `info threads` after halt; expect ESP-IDF tasks incl. the USB task; note whether it works at attach or needs run→stop (mirror the ARM finding).
- [x] **Step 3: Console during traffic** — OUTCOME: stock builds route the console to UART0 (the CP2102 flasher tty — boot log captured there); the USB-SJ CDC carries no log without sdkconfig `ESP_CONSOLE_USB_SERIAL_JTAG`, which stays (untested) in the skill.
- [x] **Step 4: Reflash pristine, release P4 lock.** Evidence appended to `/tmp/esp_evidence.txt`.

### Task 3: P4 apptrace spike — GATED (spec gate 5)

Budget 30 min. `openocd -c 'esp apptrace start ...'` against a firmware built with apptrace enabled? Stock HIL firmware has no apptrace init — if a code change would be required, that's the gate answer: land apptrace as `(untested — needs CONFIG_APPTRACE + firmware init)` with the recipe sketch. Only a working capture lands unmarked.

### Task 4: S3 boundary (spec gate 6)

- [x] **Step 1: Lock S3, flash `board_test`** (no TinyUSB → PHY free). Attach `openocd -f board/esp32s3-builtin.cfg -c 'adapter serial 84F703C084E4'` + `xtensa-esp32s3-elf-gdb`: halt + bt works.
- [x] **Step 2: Flash a USB device example** — record the exact failure: does 303a:1001 vanish from lsusb (PHY switched), does openocd fail to attach or die mid-session? Capture verbatim error.
- [x] **Step 3: Reflash pristine (a USB example — that is the CI-expected state), release.**

### Task 5: Write the skill + integration edits + commit

- [x] **Step 1: Write `.claude/skills/esp-target-debug/SKILL.md`** per spec section order (role/defer, PHY map with verified boundary symptoms, toolchain+attach with the real commands from Tasks 1–4, technique mapping table with verified annotations, rig deltas, external-JTAG TODO). Aligned tables.
- [x] **Step 2: `target-debug` pointer** (2 lines, after probe-mapping bullets) + `target-debugger` agent table row.
- [x] **Step 3: pre-commit, single commit** with evidence summary from `/tmp/esp_evidence.txt`.
- [x] **Step 4: Retrieval sanity** — one fresh-subagent scenario: "debug a TinyUSB hang on the rig's P4" routes to esp-target-debug (not JLink recipes); "same on S3 while cdc_msc runs" routes to the PHY boundary + external-JTAG TODO.
