# Unified OpenOCD Fork (`hathach/openocd`) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** One OpenOCD fork at `hathach/openocd` (default branch `tinyusb`) that flashes, debugs and RTT-captures every TinyUSB rig target — RP2040, RP2350 (arm + riscv), all WCH CH32/CH5xx, Analog Devices MAX32, and Espressif — replacing the four separate OpenOCD trees on ci.

**Architecture:** Fork `openocd-org/openocd` master (mainline is 1610 commits ahead of the RPi fork base and now the sole home of RISC-V support). Layer on top: 4 RP2350 TCL configs from the RPi fork, 1 ported max32665 TCL config from the ADI fork, the `wlinke` adapter + `sdi` transport + WCH flash drivers from `hathach/riscv-openocd-wch` (driving CH32 with **mainline's** riscv target if the DTM hypothesis holds), and ESP32-P4 TCL configs adapted from `espressif/openocd-esp32` onto mainline's generic-riscv ESP pattern. ESP32/S2/S3/C3/C6/H2 debug is already in mainline; ESP flash stays with esptool.

**Tech Stack:** OpenOCD (autotools, C), TCL configs, GitHub CLI, TinyUSB HIL rig (`hil_test.py`, `board_lock.py`).

## Global Constraints

- Everything runs **on ci** (this machine *is* the rig — hostname `ci`); no SSH hop needed.
- Repo: `hathach/openocd`, default branch **`tinyusb`**, source clone at `~/app/openocd`, install prefix `$HOME/app/openocd_tinyusb`.
- **One commit per downstream fork** on the `tinyusb` branch: one for raspberrypi/openocd, one for analogdevicesinc/openocd, one for riscv-openocd-wch, one for espressif/openocd-esp32 (plus the initial README commit). Iterate with `git commit --amend` / squash before declaring a task done.
- No `Co-Authored-By: Claude` / `Claude-Session:` trailers in any commit.
- **`~/.local/bin/openocd_wch` (symlink) and `~/app/openocd_wch_new` stay untouched until Task 8's 4/4 WCH boards pass** — it is the rig's only CH32 flasher. Backup exists at `~/.local/bin/openocd_wch.bak-20260727`.
- Hold a board lock for every hardware step: `python3 test/hil/board_lock.py hold <board> --reason "openocd-unified verify"`; release after. **Never stop the actions-runner.**
- WCH RTT: always `rtt polling_interval 1`; **never `reset run` inside an SDI session** (target does not come back).
- `pkill -x openocd` — never `pkill -f` (pattern matches your own shell).
- `libjim-dev` is required to configure mainline; all build deps are already installed on ci (mainline was built here 2026-07-27).
- Back up before replacing `/usr/local/bin/openocd`; the current binary is the RPi-fork build (byte-identical to `~/app/openocd_rpi/src/openocd`).
- Do not modify the TinyUSB checkout at `~/code/tinyusb` except where a task explicitly says so (hil_test.py WCH cfg template, on a `claude/`-prefixed branch). Never `git stash -u` in a TinyUSB worktree.
- OpenOCD resolves its scripts dir relative to the **realpath** of the binary — repoint via symlink into an installed prefix, never a bare copy of the binary.

## Reference: current state (measured 2026-07-27, in `OPENOCD_UNIFIED_FORK_HANDOFF.md`)

| Tree on ci | Repo @ commit | Role |
| --- | --- | --- |
| `~/app/openocd_rpi` | raspberrypi/openocd @ `ebec9504d` (sdk-2.0.0) | rig default (`/usr/local/bin/openocd`) |
| `~/app/openocd_adi` | analogdevicesinc/openocd @ `5fc33af` | max32666fthr (`~/app/openocd_adi/src/openocd`) |
| `~/app/riscv-openocd-wch` | hathach/riscv-openocd-wch @ `ccb04d7` | CH32 flash+RTT (`~/.local/bin/openocd_wch`) |
| `~/app/openocd-mainline` | openocd-org/openocd @ `43441cd83` | candidate build, verified on pico/pico2/max32666fthr |

Rig flasher entries (`test/hil/tinyusb.json`): `openocd` (pico ×3, fruit_jam, stm32h743nucleo, stm32g0b1nucleo), `openocd_adi` (max32666fthr), `openocd_wch` (nanoch32v203, ch32v103r_r1_1v0, ch32v307v_r1_1v0, ch582m_evt), `esptool` (espressif_s3_devkitm, espressif_p4_function_ev).

---

### Task 1: Create `hathach/openocd`, `tinyusb` branch, README

**Files:**
- Create: `~/app/openocd/` (clone), `~/app/openocd/README.md`

**Interfaces:**
- Produces: GitHub repo `hathach/openocd` with default branch `tinyusb`; local clone `~/app/openocd` with remotes `origin` (hathach) and `upstream` (openocd-org). All later tasks commit to this clone's `tinyusb` branch.

- [ ] **Step 1: Fork and clone**

```bash
gh repo fork openocd-org/openocd --clone=false
git clone --recursive https://github.com/hathach/openocd.git ~/app/openocd
cd ~/app/openocd
git remote add upstream https://github.com/openocd-org/openocd.git
git checkout -b tinyusb origin/master
```

- [ ] **Step 2: Verify the clone is at mainline HEAD**

Run: `cd ~/app/openocd && git log --oneline -1`
Expected: `43441cd83 server: add 'services' command to list service information` or newer.

- [ ] **Step 3: Write `README.md`** (new file — GitHub renders it instead of mainline's plain-text `README`, and leaving `README` untouched keeps future rebases conflict-free)

```markdown
# OpenOCD for the TinyUSB test rig

One OpenOCD build that flashes, debugs and RTT-captures every board family on
the [TinyUSB](https://github.com/hathach/tinyusb) hardware-in-the-loop rig, so
the rig does not need four different OpenOCD trees.

This is the `tinyusb` branch, tracking
[openocd-org/openocd](https://github.com/openocd-org/openocd) `master`.
Everything not listed below is unmodified mainline.

## Cherry-picked / ported from

| Source repo | What we took |
| --- | --- |
| [raspberrypi/openocd](https://github.com/raspberrypi/openocd) (`sdk-2.0.0`) | `tcl/target/rp2350-riscv.cfg`, `rp2350-rescue.cfg`, `rp2350-dbgkey-secure.cfg`, `rp2350-dbgkey-nonsecure.cfg`. The RP2040/RP2350 C flash driver is already better in mainline (`rp2xxx.c`). |
| [analogdevicesinc/openocd](https://github.com/analogdevicesinc/openocd) (`release`) | `tcl/target/max32665.cfg` (MAX32665/MAX32666), re-ported onto mainline's `max32xxx_common.cfg`. The fork's QSPI block is dropped — it is guarded by `QSPI_ENABLE`, which this part sets to 0. |
| [hathach/riscv-openocd-wch](https://github.com/hathach/riscv-openocd-wch) (originally [dragonlock2/miscboards](https://github.com/dragonlock2/miscboards) WCH SDK) | `wlinke` adapter driver, `sdi` single-wire transport, and the WCH flash drivers (`wch_riscv`, `wch_arm`) for CH32V/CH32F/CH5xx over WCH-Link/LinkE. |
| [espressif/openocd-esp32](https://github.com/espressif/openocd-esp32) | `tcl/target/esp32p4.cfg` + `tcl/board/esp32p4-builtin.cfg`, adapted to mainline's generic RISC-V ESP pattern. ESP32/S2/S3/C3/C6/H2 debug is already in mainline; ESP flash programming stays with `esptool`. |

## Build

    ./bootstrap
    ./configure --enable-jlink --enable-cmsis-dap --enable-stlink \
                --enable-wlinke --disable-werror
    make -j$(nproc)

`libjim-dev` is required — mainline no longer builds the bundled jimtcl by
default and configure hard-fails without it.
```

- [ ] **Step 4: Commit, push, set default branch**

```bash
cd ~/app/openocd
git add README.md
git commit -m "README: purpose of the tinyusb branch and its downstream sources"
git push -u origin tinyusb
gh repo edit hathach/openocd --default-branch tinyusb \
  --description "OpenOCD for the TinyUSB test rig - one build for RP2040/RP2350, WCH CH32, MAX32 and ESP32 targets"
```

- [ ] **Step 5: Verify default branch**

Run: `gh repo view hathach/openocd --json defaultBranchRef -q .defaultBranchRef.name`
Expected: `tinyusb`

---

### Task 2: Build the fork on ci

**Files:**
- Create: `~/app/openocd_tinyusb/` (install prefix)

**Interfaces:**
- Consumes: `~/app/openocd` clone from Task 1.
- Produces: `~/app/openocd_tinyusb/bin/openocd` (installed binary + scripts at `~/app/openocd_tinyusb/share/openocd/scripts/`). Every later flash/verify step uses this path.

- [ ] **Step 1: Configure and build** (same recipe that already worked for mainline on this box)

```bash
cd ~/app/openocd
./bootstrap
./configure --prefix=$HOME/app/openocd_tinyusb \
  --enable-jlink --enable-cmsis-dap --enable-stlink --disable-werror
make -j$(nproc) && make install
```

- [ ] **Step 2: Verify version and adapters**

Run: `~/app/openocd_tinyusb/bin/openocd --version 2>&1 | head -1`
Expected: `Open On-Chip Debugger 0.12.0+dev-...` with a `-g<sha>` matching `git -C ~/app/openocd rev-parse --short HEAD`.

Run: `~/app/openocd_tinyusb/bin/openocd -c 'adapter list; shutdown' 2>&1 | grep -E 'cmsis-dap|jlink|stlink'`
Expected: all three listed.

*(No commit — build products only.)*

---

### Task 3: Import the 5 TCL configs — one commit per downstream fork

**Files:**
- Create: `~/app/openocd/tcl/target/rp2350-riscv.cfg`, `rp2350-rescue.cfg`, `rp2350-dbgkey-secure.cfg`, `rp2350-dbgkey-nonsecure.cfg`, `max32665.cfg`
- Source of truth: `~/code/tinyusb/openocd-unified-configs/` (the copies already hardware-verified this week; the max32665 port is already written there)

**Interfaces:**
- Consumes: `~/app/openocd` + install prefix from Task 2.
- Produces: `target/rp2350-riscv.cfg` and `target/max32665.cfg` resolvable via `find` in the installed scripts dir — Task 4 flashes with them.

- [ ] **Step 1: Copy the RPi configs and commit (downstream commit #1)**

```bash
cd ~/app/openocd
cp ~/code/tinyusb/openocd-unified-configs/rp2350-riscv.cfg \
   ~/code/tinyusb/openocd-unified-configs/rp2350-rescue.cfg \
   ~/code/tinyusb/openocd-unified-configs/rp2350-dbgkey-secure.cfg \
   ~/code/tinyusb/openocd-unified-configs/rp2350-dbgkey-nonsecure.cfg \
   tcl/target/
git add tcl/target/rp2350-*.cfg
git commit -m "tcl/target: add RP2350 riscv/rescue/dbgkey configs from raspberrypi/openocd

Taken from raspberrypi/openocd branch sdk-2.0.0 @ ebec9504d. These four
configs are the only things that fork has which mainline lacks - the
rp2040/rp2350 C driver was consolidated upstream as rp2xxx.c. All four
use only mainline-present commands (swj_newdap, dap create -adiv6,
target create riscv -ap-num, riscv set_enable_virt2phys).

rp2350-riscv.cfg is what hw/bsp/rp2040/family.cmake requests when
PICO_PLATFORM=rp2350-riscv."
```

- [ ] **Step 2: Copy the ADI config and commit (downstream commit #2)**

```bash
cd ~/app/openocd
cp ~/code/tinyusb/openocd-unified-configs/max32665.cfg tcl/target/
git add tcl/target/max32665.cfg
git commit -m "tcl/target: add max32665 config ported from analogdevicesinc/openocd

Ported from analogdevicesinc/openocd @ 5fc33af onto mainline's
max32xxx_common.cfg (the ADI fork calls the same file max32xxx.cfg).
The fork's QSPI block is dropped: it is guarded by QSPI_ENABLE, which
this part sets to 0, and it needs the ADI-only max32xxx_qspi driver.
Covers MAX32665/MAX32666 (both flash banks). Hardware-verified on
max32666fthr 2026-07-27."
```

- [ ] **Step 3: Install and verify the configs resolve**

```bash
cd ~/app/openocd && make install
~/app/openocd_tinyusb/bin/openocd -c 'puts [find target/max32665.cfg]; puts [find target/rp2350-riscv.cfg]; shutdown'
```
Expected: both paths under `~/app/openocd_tinyusb/share/openocd/scripts/target/` printed; exit without "Can't find".

- [ ] **Step 4: Push**

```bash
cd ~/app/openocd && git push
```

---

### Task 4: Hardware-verify every current-openocd board with the fork binary

**Files:**
- No source changes. Uses `~/code/tinyusb` builds + `test/hil/hil_test.py`.

**Interfaces:**
- Consumes: `~/app/openocd_tinyusb/bin/openocd` with Task 3 configs installed.
- Produces: evidence that the fork can replace `/usr/local/bin/openocd` (Task 5's gate). PATH shim dir `~/app/openocd_tinyusb/shim/` reused by later tasks.

Boards (every `openocd`/`openocd_adi` flasher entry in `tinyusb.json`):
`raspberry_pi_pico`, `raspberry_pi_pico_w`, `raspberry_pi_pico2`, `adafruit_fruit_jam`, `stm32h743nucleo`, `stm32g0b1nucleo`, `max32666fthr`.
Already verified on plain mainline 2026-07-27: pico, pico2, max32666fthr (re-run anyway — the binary changed).

- [ ] **Step 1: Build any missing firmware sets** (repeat per board without `examples/cmake-build-<board>`; `cmake-build-raspberry_pi_pico`, `-stm32g0b1nucleo`, `-max32666fthr` already exist)

```bash
cd ~/code/tinyusb/examples
cmake -B cmake-build-raspberry_pi_pico2 -DBOARD=raspberry_pi_pico2 -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel . \
  && cmake --build cmake-build-raspberry_pi_pico2
```
(Same pattern for `raspberry_pi_pico_w`, `adafruit_fruit_jam`, `stm32h743nucleo`. If a board fails `get_deps`, run `python3 tools/get_deps.py -b <board>` first.)

- [ ] **Step 2: Create the PATH shim** (lets `hil_test.py`'s hardcoded `openocd` resolve to the fork; symlink keeps scripts-dir resolution working because OpenOCD follows the realpath)

```bash
mkdir -p ~/app/openocd_tinyusb/shim
ln -sf ~/app/openocd_tinyusb/bin/openocd ~/app/openocd_tinyusb/shim/openocd
```

- [ ] **Step 3: Smoke-flash one board directly** (fast signal before the full suite)

```bash
python3 ~/code/tinyusb/test/hil/board_lock.py hold raspberry_pi_pico --reason "openocd-unified verify"
~/app/openocd_tinyusb/bin/openocd -c "adapter serial E6614103E72C1D2F" \
  -f interface/cmsis-dap.cfg -f target/rp2040.cfg -c "adapter speed 5000" \
  -c "program /home/hathach/code/tinyusb/examples/cmake-build-raspberry_pi_pico/device/cdc_msc/cdc_msc.elf verify reset exit"
```
Expected: `** Verified OK **` then `** Resetting Target **`. Release the lock after (`board_lock.py release raspberry_pi_pico`).

- [ ] **Step 4: Run the HIL suite for all 7 boards through the shim**

```bash
cd ~/code/tinyusb
PATH=~/app/openocd_tinyusb/shim:$PATH \
python3 test/hil/hil_test.py test/hil/tinyusb.json \
  -b raspberry_pi_pico -b raspberry_pi_pico_w -b raspberry_pi_pico2 \
  -b adafruit_fruit_jam -b stm32h743nucleo -b stm32g0b1nucleo
```
Notes for the executor:
- `hil_test.py` takes the config as a positional arg and `-b` per board; it holds board locks itself (that is the board-lock protocol in CI — do not also hold manual locks around `hil_test.py` runs).
- max32666fthr is **not** in this run: its `flash_openocd_adi()` path uses the hardcoded `OPENCOD_ADI_PATH = ~/app/openocd_adi` (`hil_test.py:408`), which the shim can't intercept. Handle it in Step 4b instead. Do not edit `hil_test.py` for this — the adi path disappears at cutover (Task 10 flips `tinyusb.json`'s flasher entry to plain `openocd` with `-f interface/cmsis-dap.cfg -f target/max32665.cfg`).
- Expected: every board PASS in the report. Any failure: stop, diagnose (consult the `hil` skill), do not proceed to Task 5.

- [ ] **Step 4b: max32666fthr — manual flash with the fork, then tests with `--skip-flash`**

```bash
python3 ~/code/tinyusb/test/hil/board_lock.py hold max32666fthr --reason "openocd-unified verify"
~/app/openocd_tinyusb/bin/openocd -c "adapter serial E6614C311B597D32" \
  -f interface/cmsis-dap.cfg -f target/max32665.cfg \
  -c "program /home/hathach/code/tinyusb/examples/cmake-build-max32666fthr/device/cdc_msc/cdc_msc.elf verify reset exit"
python3 ~/code/tinyusb/test/hil/board_lock.py release max32666fthr
cd ~/code/tinyusb && python3 test/hil/hil_test.py test/hil/tinyusb.json -b max32666fthr -sf
```
Expected: `** Verified OK **` on the flash, then PASS with `-sf` (tests run against the firmware just flashed).

- [ ] **Step 5: RTT smoke on the pico** (mainline RTT was verified 2026-07-27; re-confirm on the fork build — `target-debug` skill has the full flow)

Expected: RTT control block found, events stream, overflow 0.

---

### Task 5: Repoint the rig default `openocd`

**Files:**
- Modify: `/usr/local/bin/openocd` (→ symlink), remove Debian `openocd` package

**Interfaces:**
- Consumes: Task 4 all-green.
- Produces: `which openocd` → fork for every rig user (hil_test.py, skills, CI). Rollback: restore `/usr/local/bin/openocd.rpi-backup-20260727`.

- [ ] **Step 1: Back up and repoint**

```bash
sudo cp -a /usr/local/bin/openocd /usr/local/bin/openocd.rpi-backup-20260727
sudo ln -sf $HOME/app/openocd_tinyusb/bin/openocd /usr/local/bin/openocd
openocd --version 2>&1 | head -1
```
Expected: fork version string (matches Task 2 Step 2).

- [ ] **Step 2: Drop the Debian openocd** (installed 2026-07-27 only to get a jlink-capable OpenOCD; the fork has `--enable-jlink`)

```bash
sudo apt-get remove -y openocd
which -a openocd
```
Expected: only `/usr/local/bin/openocd` remains.

- [ ] **Step 3: Re-verify through the default path (no shim)**

```bash
cd ~/code/tinyusb
python3 test/hil/hil_test.py test/hil/tinyusb.json -b raspberry_pi_pico -b stm32g0b1nucleo -b raspberry_pi_pico2
```
Expected: 3/3 PASS. If CI kicks a workflow mid-way, board locks arbitrate — just wait.

---

### Task 6: WCH part 1 — port the `wlinke` adapter + `sdi` transport (compiles, detects probe)

**Files (all in `~/app/openocd`, sources from `~/app/riscv-openocd-wch` @ `ccb04d7` — this copy already carries the GCC-14 fixes):**
- Create: `src/jtag/drivers/wlinke.c` (2041 lines, copy), `src/jtag/sdi.c` (~130 lines, port), `src/jtag/sdi.h` (if the fork has one — check `ls ~/app/riscv-openocd-wch/src/jtag/sdi*`)
- Modify: `src/transport/transport.h` (new transport id), `src/jtag/interface.h` (add `sdi_ops` to `struct adapter_driver` + `struct sdi_driver` decl), `src/jtag/interfaces.c` (register driver), `src/jtag/drivers/Makefile.am`, `src/jtag/Makefile.am`, `configure.ac` (`--enable-wlinke`)

**Interfaces:**
- Consumes: fork clone + build tree.
- Produces: `openocd -c "adapter driver wlinke"` works; `wlink_*` C exports (`wlink_erase`, `wlink_write`, `wlink_getromram`, `wlink_reset`, `wlink_chip_reset`, `wlink_clean`, `wlink_flash_protect`, …) available for Task 8's flash driver; `sdi` transport selectable. Commit stays **amend-in-progress** — Tasks 6–8 squash into downstream commit #3.

Port notes gathered up front (verified against both trees 2026-07-27):
- Fork wiring to replicate: `configure.ac:117` (adapter list entry `[[wlinke],[WLINKE Programmer],[WLINKE]]`), `:284-286` (`AC_ARG_ENABLE`), `:537`, `:737` (`AM_CONDITIONAL`); `src/jtag/drivers/Makefile.am:189` (`DRIVERFILES += %D%/wlinke.c`); `src/jtag/interfaces.c:154,274` (extern + table entry).
- Mainline transports are now a **fixed bitmask enum** (`src/transport/transport.h:19-25`: `TRANSPORT_JTAG BIT(0)` … `TRANSPORT_SWIM BIT(6)`, plus `TRANSPORT_VALID_MASK`), and `struct transport` selects by `unsigned int id`, not name. Add `#define TRANSPORT_SDI BIT(7)`, extend `TRANSPORT_VALID_MASK`, and port `sdi.c`'s `transport_register` to the id-based struct.
- **SWIM is the exact precedent** — ST's proprietary single-wire transport, wired upstream the same way this needs: `swim_ops` field at `src/jtag/interface.h:363`, its own transport bit, own command namespace. Mirror how `grep -rn swim src/transport/ src/jtag/interface.h src/jtag/swim.c` is structured wherever the fork's 0.11-era pattern no longer matches mainline.
- The fork's `sdi` op is a raw RISC-V DMI transfer: `adapter_driver->sdi_ops->transfer(iIndex, iAddr, iData, iOP, oAddr, oData, oOP)` (`src/jtag/sdi.c:20-22`) — keep that signature; Task 7 builds on it.
- `wlinke.c` includes `"cmsis_dap.h"`, `"hidapi.h"`, `"libusb_helper.h"` and (spuriously) `<windows.h>` — drop/guard the windows include; hidapi + libusb helpers exist in mainline's drivers dir.

- [ ] **Step 1: Copy `wlinke.c` and `sdi.c` in; make the wiring edits above**

- [ ] **Step 2: Reconfigure with wlinke and build**

```bash
cd ~/app/openocd
./configure --prefix=$HOME/app/openocd_tinyusb \
  --enable-jlink --enable-cmsis-dap --enable-stlink --enable-wlinke --disable-werror
make -j$(nproc) && make install
```
Expected: clean build (`--disable-werror` tolerates the fork's warning-dirty code; do fix outright errors).

- [ ] **Step 3: Probe-detection test against real hardware** (nanoch32v203's WCH-LinkE, serial `EBCA8F0670AF`)

```bash
python3 ~/code/tinyusb/test/hil/board_lock.py hold nanoch32v203 --reason "wlinke port bring-up"
~/app/openocd_tinyusb/bin/openocd -c "adapter driver wlinke" \
  -c "adapter serial EBCA8F0670AF" -c "transport select sdi" \
  -c "init" -c "shutdown"
```
Expected: log lines identifying the WCH-Link probe (firmware version print from `wlink_init`), no crash. `init` may complain about missing target — probe identification is the pass signal. Keep the lock held into Task 7 (same board).

- [ ] **Step 4: Snapshot as work-in-progress commit** (will be amended/squashed through Task 8)

```bash
cd ~/app/openocd && git add -A && git commit -m "WIP: wch port (squash into single downstream commit before push)"
```
**Do not push** until Task 8 squashes.

---

### Task 7: WCH part 2 — target spike: mainline `riscv` over wlink DMI

**The hypothesis (from the handoff, sharpened by code reading):** WCH-LinkE's `sdi` op *is* a raw DMI transfer, and mainline's riscv-013 target is just a DMI client. If mainline's riscv target can be fed by wlink DMI transfers, we skip porting `wch_riscv.c`/`wch_riscv-013.c` (~3.5k lines that `#include <target/riscv/...>` 0.11-era internals — the worst possible port surface).

**Files:**
- Modify: `src/jtag/drivers/wlinke.c` (add the DTM bridge), possibly `src/target/riscv/riscv-013.c` shim hooks — decided by Step 1's reading.

**Interfaces:**
- Consumes: Task 6's working adapter (lock on nanoch32v203 still held).
- Produces: a `target create ... riscv` (or, on fallback, `wch_riscv`) config shape that Task 8's flash/RTT/HIL work builds on. Records the decision in the WIP commit message.

- [ ] **Step 1: Read mainline's DMI plumbing before writing anything**

Read `src/target/riscv/riscv-013.c` (the `dmi_op`/`riscv_batch` layer) and `src/target/riscv/riscv.c`'s `riscv dmi_read`/`dmi_write` command handlers (they exist — mainline's `tcl/target/esp32c6.cfg` calls them). Determine the narrowest insertion point, in order of preference:
1. an existing DTM/DMI abstraction the adapter can implement directly (best);
2. a jtag-DTM emulation inside `wlinke.c`: expose `jtag_ops` whose queue executor decodes IR=DTMCS/DMI DR scans into `sdi` transfers (the esp_usb_jtag-style approach, one level up);
3. nothing viable → fallback (Step 4).

- [ ] **Step 2: Implement the chosen bridge; build**

Same build command as Task 6 Step 2.

- [ ] **Step 3: Hypothesis test on nanoch32v203** (write the test cfg to the scratchpad, not the repo)

```tcl
# wch-mainline-riscv-test.cfg
adapter driver wlinke
adapter speed 6000
transport select sdi        ;# or jtag, if Step 1 chose the jtag-DTM emulation
wlink_set_address 0x00000000
sdi newtap ch32 cpu -irlen 5 -expected-id 0x00001
target create ch32.cpu riscv -chain-position ch32.cpu
ch32.cpu configure -work-area-phys 0x20000000 -work-area-size 0x2800 -work-area-backup 1
init
```

Evidence criteria — **all four must hold** to call the hypothesis confirmed:
```
halt                        → "Target halted" with a sane pc
riscv dmi_read 0x11         → plausible dmstatus (nonzero, version field = 2 or 3)
mdw 0x20000000 4            → reads SRAM without error
resume                      → target runs again (LED blink / CDC re-enumerates)
```

- [ ] **Step 4: Decision checkpoint — STOP if the hypothesis fails**

If any criterion fails for reasons that look architectural (wlink protocol can't express raw DMI reads, QingKe deviates from the RISC-V debug spec in ways mainline won't tolerate), **stop and report to the user** with the evidence. The two fallback options, costed:
- (a) Port the fork's full WCH target stack: `src/target/wch_riscv.c` (3033 ln) + `wch_riscv-013.c` + `wch_riscv.h`, plus the fork's core patches (all findable via `grep -rn 'riscvchip\|wlink_' src/` in the fork: `src/flash/nor/tcl.c` 5 hits, `src/target/target.c` 5, `src/server/gdb_server.c` 2). Hard: these files include 0.11-era `target/riscv/*` headers that clash with mainline's current riscv internals.
- (b) Ship the unified fork **without** WCH C support and keep `openocd_wch` as the rig's CH32 flasher indefinitely.
Do not silently pick (a).

---

### Task 8: WCH part 3 — flash drivers, RTT, 4-board HIL green, squash to downstream commit #3

**Files:**
- Create: `src/flash/nor/wchriscv.c` (324 ln, copy), `src/flash/nor/wcharm.c` (897 ln, copy — CH32F ARM parts; self-contained memory-mapped driver, zero wlink deps), `src/jtag/drivers/wlinke.h` (new — prototypes for the `wlink_*` exports; the fork relied on implicit declarations)
- Modify: `src/flash/nor/drivers.c` (extern + table entries, fork pattern at its lines 93-94/170-171), `src/flash/nor/Makefile.am` (fork pattern at lines 78-79)
- Modify (TinyUSB repo, separate branch): `test/hil/hil_test.py` WCH cfg template (~line 381) — only if Task 7 landed on the mainline-riscv target shape

**Interfaces:**
- Consumes: Task 7's confirmed target shape + `wlink_*` exports from Task 6.
- Produces: downstream commit #3 (single squashed commit, pushed); `~/.local/bin/openocd_wch` repointed at the fork; hil_test.py template branch `claude/hil-openocd-unified` in the TinyUSB repo (unpushed — user pushes; "hold pushes" applies to the TinyUSB repo).

- [ ] **Step 1: Copy the flash drivers, add `wlinke.h`, wire `drivers.c`/`Makefile.am`; build**

Keep the flash driver's registered name **`wch_riscv`** — the rig's generated per-probe cfg does `flash bank ... wch_riscv ...` and Task 8 Step 4's template keeps working.
Fork quirk to *not* copy: the fork patched `src/flash/nor/tcl.c` (`handle_flash_protect_check_command`, its line ~414) to call `wlink_softreset()`/`wlnik_protect_check()` for WCH banks. Implement that inside `wchriscv.c`'s own `protect_check` op instead — no core-file patch.
Check the fork's `src/server/gdb_server.c` 2 `wlink_` hits (`grep -n 'riscvchip\|wlink_' ~/app/riscv-openocd-wch/src/server/gdb_server.c`) — port the behavior into the driver/target layer if it matters for our flow (flash + RTT, no gdb needed on the rig for WCH), else document-and-skip in the commit message.

- [ ] **Step 2: Flash test on nanoch32v203** (lock held; cfg = Task 7's test cfg + flash bank line)

```tcl
set _FLASHNAME ch32.flash
flash bank $_FLASHNAME wch_riscv 0x00000000 0 0 0 ch32.cpu
```
```bash
~/app/openocd_tinyusb/bin/openocd -c "adapter serial EBCA8F0670AF" \
  -f wch-mainline-riscv-test.cfg \
  -c "program /home/hathach/code/tinyusb/examples/cmake-build-nanoch32v203-usbfs/device/cdc_msc/cdc_msc.elf verify reset exit"
```
Expected: `** Verified OK **`; board re-enumerates as CDC (`lsusb | grep -i cafe` or dmesg).

- [ ] **Step 3: RTT test on nanoch32v203** (rig rule: `rtt polling_interval 1`, **never `reset run`**)

RTT server start → capture a few seconds → nonzero events. The `target-debug` skill documents the WCH RTT route.

- [ ] **Step 4: Update the rig's WCH flow**

If Task 7 confirmed the mainline-riscv shape, the generated cfg template in `test/hil/hil_test.py` (~line 381: `adapter driver wlinke` … `target create $_TARGETNAME.0 wch_riscv …`) must switch to the Task 7 cfg shape. Do this on a TinyUSB branch:
```bash
cd ~/code/tinyusb && git worktree add .worktrees/claude/hil-openocd-unified -b claude/hil-openocd-unified
# edit test/hil/hil_test.py template in the worktree; commit there; DO NOT push
```
Then repoint the rig's WCH binary (symlink, so scripts resolve):
```bash
ln -sf ~/app/openocd_tinyusb/bin/openocd ~/.local/bin/openocd_wch
```
(Old target `~/app/openocd_wch_new/bin/…` and `~/.local/bin/openocd_wch.bak-20260727` stay as rollback.)

- [ ] **Step 5: HIL green on all four WCH boards** (run from the worktree so the new template is used)

```bash
cd ~/code/tinyusb/.worktrees/claude/hil-openocd-unified
python3 test/hil/hil_test.py test/hil/tinyusb.json \
  -b nanoch32v203 -b ch32v103r_r1_1v0 -b ch32v307v_r1_1v0 -b ch582m_evt
```
Expected: 4/4 PASS. Firmware for missing `cmake-build-<board>` sets: build first (nanoch32v203 sets exist; ch32v103/307/ch582m may need `tools/get_deps.py -b <board>` + the examples build). Known flake: ch32v103r throughput is ~40% flaky historically — retry before blaming the port. If ch582m misbehaves specifically, note it and check `wlinke.c`'s riscvchip dispatch for CH58x.

- [ ] **Step 6: Squash Tasks 6–8 into downstream commit #3 and push**

```bash
cd ~/app/openocd
git reset --soft $(git log --grep='WIP: wch port' --format=%H | tail -1)^
git commit -m "jtag, flash: add WCH-LinkE adapter, sdi transport and CH32 flash drivers

Ported from hathach/riscv-openocd-wch @ ccb04d7 (originally
dragonlock2/miscboards WCH SDK, base openocd 0.11.0):
- src/jtag/drivers/wlinke.c: WCH-Link/LinkE USB adapter (GCC-14 fixes included)
- src/jtag/sdi.c: WCH single-wire debug transport, re-worked onto
  mainline's id-based transport API (TRANSPORT_SDI)
- src/flash/nor/wchriscv.c, wcharm.c: CH32V/CH5xx (wlink protocol) and
  CH32F (memory-mapped) flash drivers
CH32 cores are driven by mainline's riscv target over wlink DMI
transfers; the fork's wch_riscv target stack is not needed.
The fork's core patches (flash/nor/tcl.c protect-check hack) moved into
the wch_riscv flash driver's protect_check op.

Verified on ci rig: nanoch32v203, ch32v103r_r1_1v0, ch32v307v_r1_1v0,
ch582m_evt - flash + verify + HIL suite + RTT (nanoch32v203)."
git push
```
(Amend the target-stack paragraph if the fallback path was taken instead.)
Release the nanoch32v203 lock if still held.

---

### Task 9: Espressif — ESP32-P4 configs, S3 attach verification, downstream commit #4

Mainline already has: `src/target/espressif/` (esp32/s2/s3 xtensa targets + apptrace/semihosting), the `esp_usb_jtag` adapter driver, and builtin cfgs for c2/c3/c6/h2/s3. Missing vs the rig: anything ESP32-P4. Flash stays esptool (rig flashes ESP via `idf.py`/esptool; the espressif fork's flash-stub stack is explicitly out of scope).

**Files:**
- Create: `~/app/openocd/tcl/target/esp32p4.cfg`, `~/app/openocd/tcl/board/esp32p4-builtin.cfg`

**Interfaces:**
- Consumes: install prefix; espressif fork cfgs fetched from GitHub.
- Produces: downstream commit #4; P4 + S3 debug-attach evidence.

- [ ] **Step 1: Verify S3 attach with pure mainline inheritance** (no new files; proves the "espressif support" baseline)

```bash
python3 ~/code/tinyusb/test/hil/board_lock.py hold espressif_s3_devkitm --reason "openocd-unified esp verify"
~/app/openocd_tinyusb/bin/openocd -f board/esp32s3-builtin.cfg -c "init; halt"
```
Expected: both xtensa cores detected over USB-Serial-JTAG (303a:1001), `Target halted`. Then `resume; shutdown`, release lock. Gotchas live in the `esp-target-debug` skill (S3's debug port can be occupied when TinyUSB firmware owns the USB peripheral — use the same recovery steps as that skill).

- [ ] **Step 2: Fetch and adapt the P4 configs (write both files)**

```bash
curl -fsSL https://raw.githubusercontent.com/espressif/openocd-esp32/master/tcl/target/esp32p4.cfg -o /tmp/claude-1000/-home-hathach-code-tinyusb/7dee5f9e-874b-4680-bb09-01a5d13fbd37/scratchpad/esp32p4-espressif.cfg
curl -fsSL https://raw.githubusercontent.com/espressif/openocd-esp32/master/tcl/board/esp32p4-builtin.cfg -o /tmp/claude-1000/-home-hathach-code-tinyusb/7dee5f9e-874b-4680-bb09-01a5d13fbd37/scratchpad/esp32p4-builtin-espressif.cfg
```
Espressif's cfg creates an `esp32p4`-type target (their `esp_riscv` C stack — not in mainline). Rewrite `tcl/target/esp32p4.cfg` following **mainline's own ESP RISC-V pattern** — `tcl/target/esp32c6.cfg` + `esp_common.cfg` (generic `riscv` target create, chip quirks via `riscv dmi_write` with the `_RISCV_*` register constants from `esp_common.cfg`) — carrying over from Espressif's file: `_CPUTAPID`, memory map/workarea, the dual-core SMP topology (P4 is 2× RV32 — model on how mainline handles SMP, and on Espressif's `_ESP_SMP_TARGET`), and the `_ESP_EFUSE_MAC_ADDR_REG` value. `tcl/board/esp32p4-builtin.cfg` = `esp_usb_jtag` adapter + `transport select jtag` + source the target cfg (mirror `board/esp32c6-builtin.cfg`, adjusting `ESP_USB_JTAG_*` ids to Espressif's P4 values).
Also check `src/jtag/drivers/esp_usb_jtag.c` accepts the P4 (VID/PID 303a:1001 is shared; verify any chip-id gating).

- [ ] **Step 3: P4 attach test**

```bash
python3 ~/code/tinyusb/test/hil/board_lock.py hold espressif_p4_function_ev --reason "openocd-unified esp verify"
cd ~/app/openocd && make install
~/app/openocd_tinyusb/bin/openocd -f board/esp32p4-builtin.cfg -c "init; halt"
```
Evidence criteria: both HP cores halt, `mdw 0x4ff00000 4` (P4 HP TCM/SRAM — cross-check the address against Espressif's cfg memory map before running) reads, `resume` works. Known nuance from prior sessions: P4 attach can need the reset-into-attach dance — the `esp-target-debug` skill documents it; an attach that only works with that dance still counts as pass (note it in the commit).
**Decision checkpoint:** if the generic-riscv shape cannot attach P4 for architectural reasons (needs Espressif's C-level `esp_riscv` assist), stop and report — options are cherry-picking their `esp_riscv` stack (large) vs shipping P4 as esptool-flash-only with debug via ESP-IDF's openocd as today. Do not silently pick either.

- [ ] **Step 4: Commit (downstream commit #4) and push**

```bash
cd ~/app/openocd
git add tcl/target/esp32p4.cfg tcl/board/esp32p4-builtin.cfg
git commit -m "tcl: add ESP32-P4 target/board configs adapted from espressif/openocd-esp32

Adapted from espressif/openocd-esp32 master onto mainline's generic
RISC-V ESP pattern (tcl/target/esp32c6.cfg + esp_common.cfg): generic
riscv targets over esp_usb_jtag instead of the fork's esp_riscv C
stack. Flash programming stays with esptool, matching how the rig
flashes all Espressif boards. ESP32/S2/S3/C3/C6/H2 were already
supported by mainline.

Verified on ci rig: espressif_p4_function_ev and espressif_s3_devkitm
attach/halt/resume over built-in USB-Serial-JTAG."
git push
```
Release both ESP board locks.

---

### Task 10: Final sweep, README truth-up, rig config flip

**Files:**
- Modify: `~/app/openocd/README.md` (only if scope shifted in Tasks 7–9)
- Modify (TinyUSB worktree from Task 8): `test/hil/tinyusb.json` — max32666fthr flasher `openocd_adi` → `openocd` with args `-f interface/cmsis-dap.cfg -f target/max32665.cfg` (plain openocd now serves it)

**Interfaces:**
- Consumes: everything green from Tasks 4–9.
- Produces: the finished fork; TinyUSB branch `claude/hil-openocd-unified` with hil_test.py + tinyusb.json changes, committed, **unpushed** (user pushes per standing instruction).

- [ ] **Step 1: Full HIL regression across every openocd-family board**

```bash
cd ~/code/tinyusb/.worktrees/claude/hil-openocd-unified
python3 test/hil/hil_test.py test/hil/tinyusb.json \
  -b raspberry_pi_pico -b raspberry_pi_pico_w -b raspberry_pi_pico2 \
  -b adafruit_fruit_jam -b stm32h743nucleo -b stm32g0b1nucleo -b max32666fthr \
  -b nanoch32v203 -b ch32v103r_r1_1v0 -b ch32v307v_r1_1v0 -b ch582m_evt
```
Expected: 11/11 PASS (ch32v103r throughput may need its usual retries).

- [ ] **Step 2: README truth-up**

Re-read `README.md` against what actually landed (WCH target route, P4 outcome). Fix any row that no longer matches; amend into the README commit or add
`git commit -m "README: reflect verified scope"`. Push.

- [ ] **Step 3: Verify the one-commit-per-fork shape**

Run: `git -C ~/app/openocd log --oneline upstream/master..tinyusb`
Expected: exactly 5 commits (or 6 with a README truth-up): README, RPi configs, ADI config, WCH port, ESP32-P4 configs. If not, interactive-free cleanup: `git rebase --onto` / `reset --soft` re-squash, then `git push --force-with-lease` (fork branch, ours alone — safe).

- [ ] **Step 4: Commit the TinyUSB-side changes in the worktree (do not push)**

```bash
cd ~/code/tinyusb/.worktrees/claude/hil-openocd-unified
git add test/hil/hil_test.py test/hil/tinyusb.json
git commit -m "test(hil): drive WCH boards and max32666fthr through the unified openocd"
```
Leave for the user to push/PR.

- [ ] **Step 5: Leftovers report** (no deletions now)

Write a short status into `OPENOCD_UNIFIED_FORK_HANDOFF.md` (append a "2026-07-XX outcome" section): what was repointed, rollback paths (`/usr/local/bin/openocd.rpi-backup-20260727`, `~/.local/bin/openocd_wch.bak-20260727`), and that `~/app/openocd_rpi`, `~/app/openocd_adi`, `~/app/openocd-mainline`, `~/app/openocd_mainline`, `~/app/openocd_wch_new`, `~/app/riscv-openocd-wch` can be retired **after a week of green CI** — not now.
