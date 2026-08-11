# sysview skill v2 — post-mortem autopsy + instrumentation quick wins — Design

Date: 2026-07-25
Branch: claude/add-systemview-debug (builds on the sysview skill, f198be682)

## Goal

Close the gaps between the sysview skill and the SystemView feature set
(UM08027) that matter for USB debugging: crash-context capture (post-mortem
mode) and three small instrumentation recipes (markers, event filtering,
PrintfHost), plus the reporter columns to make them useful without the GUI.

## Context

The sysview skill (`.claude/skills/sysview/`) covers continuous J-Link
recording, FreeRTOS task events, manual ISR wrap, contexts/CPU-load analysis,
and CSV export — hardware-validated on same54_xplained + J-Trace. A gap
analysis against UM08027 found the highest-value uncovered features to be
post-mortem mode (§3.15.3) and the DisableEvents/marker/Printf APIs. The
`--export-terminal` flag exists but has never been exercised with real
`PrintfHost` output.

## Design

### 1. Post-mortem capture path

- **Build**: `sysview.cmake` gains `-DSYSVIEW_POST_MORTEM=1`, defining
  `SEGGER_SYSVIEW_POST_MORTEM_MODE=1` (UM08027 §4.5.2.4).
  `SEGGER_SYSVIEW_SYNC_PERIOD_SHIFT` stays at its target-source default
  (8 = sync every 256 events); add an override only if validation shows
  resync gaps in decoded dumps. The target then
  writes the SysView RTT ring with overwrite — no host reader needed; the
  buffer always holds the most recent scheduling history (~last seconds,
  buffer-size dependent). The existing config's target-side DWT CYCCNT enable
  already satisfies the manual's ENABLE_DWT_CYCCNT requirement (no debugger
  attached while recording).
- **Dump**: new script `scripts/sysview_dump.py` — a separate unit from the
  live recorder (nothing in common with Xvfb/GUI choreography). Flow: resolve
  probe (shared helper pattern), attach **without reset or reflash** (evidence
  preservation, same rule as target-debug's fault autopsy), halt, read the
  `_SEGGER_RTT.aUp[1]` descriptor (pBuffer/SizeOfBuffer/WrOff/RdOff) via
  JLinkExe, dump the buffer memory, and linearize oldest→newest. **Split
  point to be determined on hardware**: ring logic says `[WrOff..end) +
  [0..WrOff)`, but the manual's literal text (§3.15.3) ends the second chunk
  at `RdOff - 1` — with no host reader these may or may not coincide.
  Implementation decodes both candidates; the one yielding a sane timeline
  wins and gets recorded in the SKILL. Write `capture.SVDat` into `--out`.
- **Decode**: unchanged — `sysview_record.py --from-raw <capture.SVDat>`
  already parses a raw stream via SystemView `-load` and exports CSV.
- **Routing**: one cross-ref line in target-debug's vector-catch/fault-autopsy
  section: a post-mortem sysview build answers "what was the system doing
  right before the fault/wedge".
- **Caveats documented in SKILL.md**: post-mortem and live recording are
  mutually exclusive build modes (the live GUI recorder cannot drain an
  overwrite-mode ring); halting for the dump kills USB service (host URB
  timeouts), which is acceptable post-crash.

### 2. Instrumentation quick wins (SKILL.md optional edits 3–5)

- **Markers**: `SEGGER_SYSVIEW_MarkStart(id)/MarkStop(id)` bracketing one code
  path (e.g. a transfer, an enumeration phase), with `NameMarker` for the
  label.
- **Event filtering**: `SEGGER_SYSVIEW_DisableEvents(<mask>)` after `Conf()`
  as the documented overflow fix — keeps task/ISR events, drops the highest-
  rate classes. The exact mask combination is determined empirically during
  implementation on hardware and recorded in the SKILL (not guessed here).
- **PrintfHost**: `SEGGER_SYSVIEW_PrintfHost()` for log lines correlated with
  the timeline; exercised in validation so `--export-terminal` is proven.

### 3. Reporter additions (`sysview_report.py`)

- Surface `Total Blocked Time` per context (column already present in
  contexts.csv, currently dropped).
- Marker-pair durations from events.txt: per marker id, n/p50/p99/max — same
  table shape as the existing ISR/ready→run tables.

## Error handling

- `sysview_dump.py`: refuses to run if the RTT magic ("SEGGER RTT") is absent
  at the given/ELF-derived address (wrong ELF or corrupted RAM); reports
  whether the buffer had wrapped (WrOff vs sync coverage); resumes the core
  only if it halted it and `--resume` is passed (default: leave halted — the
  user is mid-autopsy).
- Decode of a wrapped ring starting mid-packet is expected to produce leading
  garbage until the first sync — the SYNC_PERIOD_SHIFT packets exist for
  this; the SKILL documents "leading events before the first sync are
  unreliable".

## Validation (same54_xplained + jtrace, dogfood pattern)

1. Post-mortem: flash `-DSYSVIEW_POST_MORTEM=1` build, run CDC bulk traffic,
   then run `sysview_dump.py` mid-load — its halt IS the simulated crash —
   and decode via `--from-raw`. Expect plausible contexts and the traffic
   window's tail present in the timeline.
2. Quick wins, deterministic overflow A/B at `-DSYSVIEW_BUFFER_SIZE=4096`
   (the size that reliably overflowed in v1 under CDC bulk load with API
   tracing): baseline run shows nonzero overflow; same run with the
   validated DisableEvents mask shows overflow 0. Same instrumented build
   carries a marker pair + PrintfHost — expect the marker table populated
   and terminal.csv containing the Printf lines.
3. Both scripts pass `python3 -m py_compile`; pre-commit clean; instrumentation
   edits reverted and pristine firmware reflashed afterward (skill's own
   rule).

## Scope revision (2026-07-25, user-directed — supersedes parts of the above)

- **SystemView becomes a first-class optional dependency**:
  `tools/get_deps.py` entry `lib/SystemView` →
  github.com/SEGGERMicro/SystemView pinned at the V4.12.0 tag commit
  (92ca7a810c5765ba64911919acd511c61b6b083f). `sysview.cmake` consumes it;
  the ~/.cache ad-hoc clone goes away.
- **Leveled instrumentation** (2026-07-25 refinement): not a boolean —
  `CFG_TUD_SYSVIEW` and `CFG_TUH_SYSVIEW` are levels 0–4 (0 = off), like
  `CFG_TUSB_DEBUG`. Site macros `TUD_SYSVIEW_CALL/RET(level, id)` (and
  `TUH_`) expand to nothing when the configured level is below the site's
  level, via the same token-paste dispatch as `TU_LOG(n, …)`. Category
  levels are macro-configurable with defaults: **USB ISR = 1, usbd/usbh
  functions = 2, dcd/hcd API = 3, class-driver API = 4**. dcd/hcd
  instrumentation wraps the call sites in usbd.c/usbh.c (the port boundary),
  never the portable drivers themselves. Build: `-DSYSVIEW=<level>` sets
  both sides (ON = 4).
- **Instrumentation moves in-tree as first-class analysis**:
  `SEGGER_SYSVIEW_Config_TinyUSB.c` becomes `src/common/tusb_sysview.c/.h`
  behind a `CFG_TUSB_SYSVIEW` option (default 0; empty macros, zero code/size
  when off — proven with a code-size compare). Adds `TU_SYSVIEW_*`
  enter/exit macros placed in usbd/usbh/dcd/hcd/class-driver hot paths
  (SEGGER module + RecordVoid/RecordEndCall convention) → **per-function
  timing of the USB stack with no trace hardware** — the easy alternative to
  etm-trace for hot-function hunting.
- **Metrics focus** (all machine-readable): USB task + ISR timing (have),
  per-function stack-path timing (new), FreeRTOS **task stack** high-water
  (INCLUDE_* defines + shim include via guarded blocks in family
  FreeRTOSConfig.h — in-tree now, the zero-edit constraint no longer
  applies; validation families first, sweep later), **heap** events via a
  traceMALLOC/traceFREE → SEGGER_SYSVIEW_HeapAlloc/HeapFree mapping (the
  V4.12 shim lacks one — verified). Heap events exist only when
  `configSUPPORT_DYNAMIC_ALLOCATION=1`; validation families are fully
  static, so the reporter handles zero-heap gracefully and the mapping is
  validated with a temporary dynamic-alloc build.
- **Reportable everything**: `sysview_report.py --json` emitting
  contexts/ISR/latency/functions/stack/heap/overflow — the future hook for
  posting a PR comment from HIL runs (the posting itself is NOT wired now).
- **OpenOCD / non-SEGGER probes** (question resolved): the SystemView GUI's
  live recorder is J-Link-only (SEGGER-official; community TCP bridges are
  experimental). Supported OpenOCD routes here: (a) raw channel-1 capture
  via OpenOCD `rtt server` → file → `--from-raw` decode — works because the
  target self-starts recording in `Conf()`; (b) the post-mortem dump —
  probe-agnostic memory reads. OpenOCD RTT is polled (drop risk under
  burst) — the overflow gate detects loss. Validation task on
  stm32h743nucleo (rig, ST-Link/OpenOCD).

## Out of scope

Each waits for a real pull, per the harness promotion-criteria philosophy:
UART recorder (non-SEGGER-probe boards), multicore (rp2350), data plot
(RegisterData/SampleData), single-shot recording, GUI trigger modes,
HIL/PR-comment posting automation.

## Rejected alternatives

- Folding the dump into `sysview_record.py` as a `--post-mortem` flag: one
  entry point, but it grows an already-long script and tangles two unrelated
  workflows (live capture vs halted autopsy).
- Automating the GUI's Target → Read Recorded Data: more headless dialog
  choreography — the most fragile part of v1.
