---
name: sysview
description: Use when you need RTOS/scheduler-level timing from real hardware — per-task and per-ISR CPU load, task-switch and ISR enter/exit timeline, ready→run scheduling latency, per-function hot-path timing, FreeRTOS stack high-water or heap tracking, or a post-mortem trace of what ran right before a crash or hang — on a FreeRTOS or bare-metal TinyUSB target, when TU_LOG, GDB or DWT PC-sampling do not answer the question. Covers SEGGER SystemView over a J-Link probe or over any OpenOCD-supported probe via RTT.
---

# sysview — SEGGER SystemView profiling for TinyUSB

Per-context CPU load, ISR durations, ready→run latency, per-function hot-path
timing, FreeRTOS stack high-water, heap totals, app markers.

| Skill          | Answers                                                             |
|----------------|---------------------------------------------------------------------|
| `target-debug` | what the target did (logs, driver state, sampled PCs)               |
| `etm-trace`    | exactly which instructions executed (profile, coverage, history)    |
| **`sysview`**  | **where CPU time goes: task/ISR schedule, load, switch/ISR timing** |

## Requirements

- **Board row in `boards.md`** (same directory) — read it before capturing:
  `JLINK_DEVICE`, any required buffer override, and board caveats live there.
- **A timestamp source.** DWT cores (M3/M4/M7/M33) work as-is; M0/M0+ and RISC-V
  need a ported BSP timer. A family without one refuses the build — at configure
  time where that check exists (`ch32v10x`, TIM2-less `stm32f0`), at link
  otherwise. Both are the intended "port it first" signal, not a regression.
  Ported families and the porting contract: `boards.md`.
- **`lib/SystemView`** — a mandatory `tools/get_deps.py` dependency, so any
  `get_deps.py <family>`/`-b <board>` (or no args) fetches it.
- **A probe.** The transport is RTT — a ring buffer in target RAM — so anything
  that reads memory can carry a trace. Pick by fidelity:

| Route                       | Needs                                | Fidelity                                                    |
|-----------------------------|--------------------------------------|-------------------------------------------------------------|
| Live recorder               | J-Link + SystemView GUI              | full — counts, CPU%, rare events                            |
| `rtt server` → `--from-raw` | OpenOCD with `rtt setup`/`rtt start` | matches the recorder **only with `rtt polling_interval 1`** |
| `dump_image` → `--from-raw` | any probe that reads memory          | last ring-full only, no streaming                           |

  Preference: **OpenOCD streaming for routine/CI work** — headless, no J-Link,
  one command shape for every probe type, the only route `sysview_ci.py` drives
  (the capture is GUI-free; decoding the raw file still runs the GUI, see Host
  setup);
  its cost is drain rate (SEGGER's native stack drains faster, so fast parts
  overflow sooner at the same buffer — and drain depends on the probe's USB
  path, so re-baseline after bus topology changes). **Live recorder for depth**:
  a deep single-board session, a part with no OpenOCD target cfg, or when
  overflow at 65536 says the drain is the bottleneck. **Dump when nothing else
  can work**: post-mortem autopsy of a hang (the halt IS the capture), and all
  WCH parts — their SDI attach is destructive (kills USB on ch32v2/v3, resets
  ch583-class, boards.md), so a live session and a USB workload are mutually
  exclusive there.

- **Hold the board lock** for any route (see the recipe below). Every command
  here runs on the host the probe is attached to — for the ci.lan rig, reaching
  it and picking the right config is the `hil` skill's job.

## Capture: live J-Link route

Complete sequence. `<DEV>` is the board's `JLINK_DEVICE` from `boards.md`.

```bash
# 1. deps + lock (the lock holder runs until you release it)
python3 tools/get_deps.py -b <board>
python3 test/hil/helper/hil_lock.py hold <board> --reason "sysview" &

# 2. build instrumented + flash (-DJLINK_OPTION pins THIS probe by nickname;
#    without it the -jlink target grabs whichever J-Link enumerates first)
cd examples/device/cdc_msc_freertos
cmake -B build-sv -DBOARD=<board> -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DSYSVIEW=4 -DJLINK_OPTION="-USB jtrace" .
cmake --build build-sv
ninja -C build-sv cdc_msc_freertos-jlink
cd -

# 3. record (all flags: sysview_record.py --help), driving the load you care about
python3 .claude/skills/sysview/scripts/sysview_record.py \
  --device <DEV> --probe jtrace \
  --elf examples/device/cdc_msc_freertos/build-sv/cdc_msc_freertos.elf \
  --duration-ms 8000 --out /tmp/sysview-out \
  --traffic-cmd "…drive the failing/loaded case…"

# 4. report (add --json for one machine-readable object instead of tables)
python3 .claude/skills/sysview/scripts/sysview_report.py /tmp/sysview-out

# 5. ALWAYS: restore pristine firmware, then release
cmake -B build-clean -DBOARD=<board> -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DJLINK_OPTION="-USB jtrace" examples/device/cdc_msc_freertos
cmake --build build-clean && ninja -C build-clean cdc_msc_freertos-jlink
python3 test/hil/helper/hil_lock.py release <board>
```

- **Nothing may live under `/tmp/sv-*`** — SystemView deletes that pattern at
  every startup. The recorder rejects such an `--out`, but the guard does **not**
  cover build dirs: a `cmake -B /tmp/sv-hw-<board>` tree was erased mid-session.
- `--duration-ms` is how long the recorder *waits*, **not how much trace you
  get** (6000 measured 12-14 s wide). If you need an exact window, read
  `live_window_s` back out of `--json` and adjust.
- **Class-driver functions only fire under load** — an idle capture leaves
  `mscd_xfer_cb`/`tud_cdc_read` empty. Drive `--traffic-cmd` at the DUT's stable
  node (`/dev/serial/by-id/usb-TinyUSB_*-if00`), never a volatile
  `/dev/ttyACM<n>`. A ready-made CDC load is `_workload_cdc_burst()` in
  `test/hil/sysview_ci.py`; the equivalent one-liner:
  `python3 -c "import serial,time; p=serial.Serial('<node>',115200,timeout=0.2,write_timeout=2); [ (p.write(b'x'*64), p.read(64), time.sleep(0.002)) for _ in range(4000) ]"`
  **Only read back if the example echoes.** `cdc_msc` echoes; the dual examples do
  not call `tud_cdc_read()`, so a read-based workload blocks its full timeout per
  iteration, outlives the recording window, and can hang the wrapper — drive
  write-only there, and always set `write_timeout`.
- `--no-events` skips the large `events.txt` (needed only for the percentile
  tables); `--export-terminal` adds `SEGGER_SYSVIEW_PrintfHost` output.
  `recording.SVDat` opens in a desktop SystemView for the visual timeline.

## Capture: OpenOCD route (no J-Link)

Same build, minus `-DJLINK_OPTION` — but pin the flash by probe serial the same
way (`-DOPENOCD_SERIAL=<uid>`; `<uid>` is `flasher.uid` in
`test/hil/tinyusb.json`). Standing up the RTT transport is the **`rtt`** skill's
job — `tools/rtt.py` wraps the whole session (control-block address from the
ELF, `rtt polling_interval 1`, teardown) so none of it is hand-assembled here:

```bash
# flash first; the capture session then reboots the target itself
openocd <board's -f/-c args> -c 'adapter serial <uid>' \
  -c "init; halt; program build-sv/cdc_msc_freertos.elf verify; reset; exit"

python3 tools/rtt.py --backend openocd --probe <uid> \
  --cfg "<board's -f interface/... -f target/... args>" \
  --elf build-sv/cdc_msc_freertos.elf --channel 1 --seconds 25 \
  --reset-before-attach > capture.SVDat &
# ...drive the workload while it records, then decode:
python3 .claude/skills/sysview/scripts/sysview_record.py \
  --from-raw capture.SVDat --out /tmp/sysview-openocd
python3 .claude/skills/sysview/scripts/sysview_report.py /tmp/sysview-openocd
```

- **`--channel 1` selects the "SysView" up-buffer; `--reset-before-attach` is
  required, not tidiness**: the Init record carrying the timestamp frequency is
  emitted once at boot, so a mid-flight attach can capture a stream with no sync
  preamble that decodes to zero events (measured: 238 KB of undecodable bytes
  without the flag; with it, h743 metrics identical to the J-Link golden capture
  — ISR p50 4.3 µs on both routes). **Exception: never reset WCH parts — under
  SDI the target does not come back** (dump route only, `boards.md`); on SAMD5x
  the in-session reset holds the core, so skip the flag there and rely on the
  flash's own reset still holding the boot preamble in the ring (short window —
  metro caveat, `boards.md`).
- Durations are transport-independent (p50/p99 match within ~1%); only sample
  count changes. OpenOCD's drain is slower than SEGGER's stack (polling-loss
  numbers: `rtt` skill), so fast parts overflow sooner at the same buffer — use
  J-Link when you need counts, CPU%, or statistics on rare events.
- Server won't come up, drops output, "control block not found", probe/transport
  quirks → the `rtt` skill's transport matrix and common mistakes.
  `test/hil/sysview_ci.py` runs this same `rtt server` route for the rig
  campaign with its own openocd invocation (same setup, polling interval and
  channel; it predates `tools/rtt.py`).

## Capture: post-mortem (crashed/wedged target)

Answers "what ran right before this hung". Build with post-mortem mode **in
addition to** a level, flash it, then reproduce the failure under it — it only
captures what happens after flashing, so a board already halted under a
different image cannot be autopsied this way.

```bash
cmake -B build-pm -DBOARD=<board> -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DSYSVIEW=4 -DSYSVIEW_POST_MORTEM=1 .
cmake --build build-pm && ninja -C build-pm cdc_msc_freertos-jlink
# ... reproduce the hang, then dump WITHOUT resetting (reset destroys evidence) ...
python3 .claude/skills/sysview/scripts/sysview_dump.py \
  --device <DEV> --probe jtrace --elf build-pm/cdc_msc_freertos.elf \
  --out /tmp/sysview-pm
python3 .claude/skills/sysview/scripts/sysview_record.py \
  --from-raw /tmp/sysview-pm/capture.SVDat --out /tmp/sysview-pm-decoded
```

The ring is an overwrite buffer holding only the most recent events — a 16 KB
ring under steady CDC bulk traffic covered a ~20-25 ms tail. Size
`SYSVIEW_BUFFER_SIZE` for the window you need — you get the last tens of ms at
bulk rates, never minutes. `--resume` lets the core carry on; by default it stays
halted (you're mid-autopsy). When done, step 5 of the live recipe applies
unchanged: restore pristine firmware, then release the lock.

## Build options

`-DSYSVIEW=` accepts `1..4` or `ON` (= 4); anything else is a configure error.
Recording starts itself at boot — no source edits, no `Conf()` call, no ISR wrap.
**CMake only**: `make BOARD=<x> SYSVIEW=4` stops with an error rather than
building an uninstrumented ELF.

| Level | Adds                                                                    |
|-------|-------------------------------------------------------------------------|
| 1     | USB interrupt enter/exit                                                |
| 2     | + `usbd`/`usbh` core (`tud_task`, `tuh_task`, `usbd_edpt_xfer`)         |
| 3     | + `dcd`/`hcd` API (`dcd_edpt_xfer`, `hcd_edpt_xfer`)                    |
| 4     | + class drivers (`tud_cdc_read`, `tud_cdc_write_flush`, `mscd_xfer_cb`) |

- **Level 1 records the ISR from whichever entry the port actually uses**, so ISR
  coverage is per-port, not universal: the `tud_int_handler`/`tuh_int_handler`
  macros on BSPs that call them, and the driver's own handler on ports that
  install one directly (`dcd_rp2040.c`/`hcd_rp2040.c` — rp2040 never reaches the
  macros). A port doing neither records no ISR spans at all: `ch32v20x`'s FSDEV
  port 0 tail-calls `dcd_int_handler` from naked asm, so its ISR table stays empty
  at every level. Check `boards.md` before concluding a board has no interrupts.
- `SYSVIEW_BUFFER_SIZE`: `-D` override wins; else RAM-rich families default to
  65536 via `SYSVIEW_BUFFER_SIZE_DEFAULT` in their `family.cmake` (imxrt, stm32h7,
  stm32f4/f7, rp2040, samd5x_e5x — the dual-role dogfood's measured-safe value);
  everything else falls back to 4096 and overrides per board (`boards.md`). `SYSVIEW_RAM_BASE` (default
  `0x20000000`) is set per **family** and only matters for named-object ids; the
  six lpc families override it themselves (see `boards.md`).
- Off by default and **verified byte-identical** to a build with no SYSVIEW code:
  every call site compiles away below its level.
- **Not everything is instrumented**: `usbd_edpt_xfer_fifo`/`dcd_edpt_xfer_fifo`
  (audio/UAC2 streaming) and the two control-transfer `hcd_edpt_xfer` sites in
  enumeration are unwrapped, so function timing under-reports those paths.

## Reading results

- **CPU load** is share of the capture window (`live_window_s` in `--json` — first
  event to last, nothing trimmed). `Idle` ≈ headroom.
- **ISR duration** is exact per enter→exit. USB IRQ time ≫ its instruction count
  = stalled on the peripheral (slave-mode FIFO at wire pace).
- **ready→run** is scheduling latency — the number behind throughput jitter. Big
  p99 vs p50 = a higher-priority task or long ISR preempting.
- **function** (level ≥ 2) is per-invocation wall time for the 8 built-in call
  sites; SystemView computes CALL→RET itself. The table is sorted by name and has
  no total-time column — **to rank hot functions, sort by `n × p50_us`**
  (CPU occupancy), which is what the CI report does:
  `sysview_report.py <dir> --json | jq -r '.functions|sort_by(-(.n*.p50_us))[]|"\(.n*.p50_us/1000|floor)ms \(.name)"'`
- **marker** (`SEGGER_SYSVIEW_MarkStart/MarkStop`, app-inserted) times a span you
  bracket yourself — a temporary source touch, revert when done.
- **stack** high-water round-robins **one task per call**, so a short capture
  surfaces only 1-2 of ~6 tasks. **heap** needs `configSUPPORT_DYNAMIC_ALLOCATION=1`
  in the app's FreeRTOSConfig.h — none of the families wired for the CPU-load
  table (`nrf`, `samd5x_e5x`, `stm32f4`, `stm32f7`, `stm32h7`) enable that today,
  so `heap` reads `null` on every stock TinyUSB example (static-allocation-first);
  the plumbing is proven and ready for an app that opts in.
- **`max` is never trustworthy when overflow is nonzero** — a dropped record makes
  the decoder pair one invocation's CALL with a later RET, producing impossible
  outliers (134 ms on a 10 µs function). Quote p50; treat `max` as a lead.
- **`p99` needs `n > 100` to mean anything.** The percentile index lands on the
  last sample at or below n=100, so p99 *is* the max there — including any spliced
  outlier. The PR report withholds it below that threshold (`– ⚠︎ n=<n>`) while
  still showing p50, which only needs n ≥ 50. A withheld p99 on a clean capture
  means "drive more traffic or capture longer", not "something is wrong".
- **overflow** climbing into double digits means events were dropped and load
  stats undercount: raise `SYSVIEW_BUFFER_SIZE` or shorten the window. `overflow`
  in `--json` is the real count over the capture — one number, and what gates a
  report row.

### `--json` schema

```
{
  "contexts": [{name, activations, cpu_pct, total_ms, blocked_ms, min_us, avg_us, max_us}],
  "isr" | "ready_run" | "functions" | "markers": [{name, n, p50_us, p99_us, max_us}],
  "stack":  [{name, bytes_used}],
  "heap":   {allocs, frees, net_bytes} | null,
  "overflow": N, "dropped_pairs": N,
  "live_window_s": F | null, "warnings": [str, ...],
  "workload_window_s": F | null, "workload_anchor": "cli"|"markers"|"cdc-read-span"|null,
  "contexts_workload": [{name, busy_ms, cpu_pct_workload}]
}
```
`live_window_s` is the capture itself, first event to last -- nothing is trimmed, so an idle
stretch stays in it. It is `null` (and `contexts[].cpu_pct` too) when events.txt has no `Init`
record: without a timestamp frequency the decode is untrustworthy, so the figure is withheld
rather than derived from a guess; see `warnings`. `cpu_pct` is SystemView's own CPU Load column
as recorded -- the reporter no longer recomputes or clamps it.
`cpu_pct_workload` is busy time summed from the export's scheduling events over the workload
window (anchor priority: `--window T0:T1`, an app's Start/Stop Marker pair, the span of
`tud_cdc_read` events). On a bare-metal build only ISR shares appear -- there are no Task
Run/System Idle events without an RTOS, so main-loop time is unattributable and the shares do
not sum to 100. `null`/empty when no anchor exists (e.g. the workload never ran).

## Warnings

- **Never register a `SEGGER_SYSVIEW_MODULE`.** SystemView 4.10b (Linux) greys
  out Save Recording / Export Data as soon as any module is registered —
  bench-proven with 5 configurations. TinyUSB uses a fixed event base
  (`TU_SV_EVENT_BASE` = 512) and maps ids to names host-side instead.
- **`SEGGER_SYSVIEW_LOCK` masks interrupts.** A burst of SystemView calls inside
  the USB path dropped a device off the bus on real hardware — this is why the
  stack reporter is round-robin. Keep any new periodic call site cheap.
- **Default FreeRTOS stacks are marginal at `SYSVIEW=4`** — enough to overflow
  `cdc_msc_freertos`'s 1024-byte task stacks during enumeration. The example
  already bumps them under `CFG_TUD_SYSVIEW`; check stack sizes if you add a new
  instrumented example.
- **Reported CPU% is only as good as the window.** The export mixes the boot
  window you actually drove. The reporter does not trim: it reports the capture
  first event to last, idle included, so check `live_window_s` and the workload
  before quoting a percentage.
- **Restore pristine firmware before releasing the lock** so the next CI/HIL run
  doesn't inherit an instrumented image, and revert marker/Printf source touches.
- Never commit recordings — `events.txt` can exceed 30 MB.

## Host setup and licensing

```bash
sudo apt-get install -y xvfb xdotool imagemagick
curl -sL -o /tmp/sv.deb https://www.segger.com/downloads/systemview/systemview_linux_deb64
sudo apt-get install -y /tmp/sv.deb        # provides /usr/bin/systemview
```
`JLinkExe` is a separate install (segger.com/jlink); the OpenOCD route needs no
`JLinkExe`, but decoding its raw capture (`--from-raw`) launches the GUI under
Xvfb, so the packages above apply to every route.

`sysview_record.py` drives the GUI under a private Xvfb over its single-instance
socket (`localhost:19050`): it starts from a fresh ini, kills stale Xvfb servers,
and dismisses modals before every export — all three were real sources of flaky
runs. On failure it writes `debug.png` into `--out`.

**Licensing affects only the GUI recorder** — the target sources in
`lib/SystemView` are 1-clause BSD, so nothing shipped in a built image carries a
condition. Without a registered key the GUI shows a dialog on every launch and
the script clicks "Continue under SFL", which **asserts non-commercial or
educational use on the operator's behalf**. Commercial use needs a CUL. The
OpenOCD capture itself never launches the GUI, but `--from-raw` decoding does, so
the condition applies wherever a capture is decoded. Registering a free key stops
the dialog appearing (ci.lan has one, node-locked, expiring 2027-01-29).

## Per-board notes

Every validated board has a row in `boards.md` (same directory) — `JLINK_DEVICE`,
timestamp source and measured rate accuracy, capture route, buffer override —
plus a caveat entry where it has one. **Read a board's row and caveat before
capturing on it**; a new validation adds both, following that file's "Adding a
board" ladder.

## References

- SystemView User Guide (UM08027; command line §3.14, target integration ch.4,
  FreeRTOS §4.7.5, module registration §6.2/§7.5):
  <https://www.segger.com/downloads/systemview> — local copy in the calibre
  library (`read-doc` skill).
- Target sources: <https://github.com/SEGGERMicro/SystemView> (pinned in
  `tools/get_deps.py` as `lib/SystemView`).
- Build wiring: `hw/bsp/family_support.cmake` (`if (SYSVIEW)`); leveled macros:
  `src/common/tusb_sysview.h`/`.c`.
