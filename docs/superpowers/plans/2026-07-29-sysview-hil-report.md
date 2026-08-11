# SystemView HIL Performance Report Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Per-PR performance report from HIL hardware — SystemView captures on sysview-flagged boards, delta vs a base-branch baseline artifact, posted into the existing HIL sticky PR comment with mermaid charts and a legend. Spec: `docs/superpowers/specs/2026-07-29-sysview-hil-report-design.md`.

**Architecture:** Two reporter bug-fixes in `sysview_report.py` (data-quality prerequisites), then a new standalone `test/hil/sysview_ci.py` with a pure `report` subcommand (TDD, fixture JSONs → markdown) and a rig-side `capture` subcommand (reuses `hil_flash.py` / `hil_lock.py`), then workflow wiring: capture+artifact in `build.yml` HIL jobs, an ubuntu compare job, and one append in `pr_comment.yml`'s existing hil-comment job.

**Tech Stack:** Python 3.11 stdlib (`json`, `csv`, `re`, `subprocess`, `unittest`), GitHub Actions YAML, OpenOCD RTT, mermaid `xychart-beta`.

## Global Constraints

- Validity gates, exactly: a capture with `metrics.overflow > 0` withholds **every** duration metric from it (`– ⚠︎ overflow N`); a metric with `n < 50` is withheld (`– ⚠︎ n=<n>`); Δ only when both sides pass; `max` is **never rendered anywhere**.
- Function rows and chart bars sorted by CPU occupancy = `n × p50_us`, computed from the PR side, descending; chart capped at **8 bars**.
- RTT capture always uses `rtt polling_interval 1`; a WCH (`openocd_wch`-flashed) capture session must **never** issue `reset run`.
- Capture is non-blocking in CI: the workflow step uses `continue-on-error: true`; per-board failures land in the JSON `error` field, never as a nonzero exit for the whole step.
- Artifact names: `sysview-<display>` (per HIL rig), `sysview-comment` (rendered markdown). Baseline = the same `sysview-<display>` artifacts from the base branch via `dawidd6/action-download-artifact@v11`.
- Comment header text: `## ⚡ SystemView performance — HIL`. Legend appears exactly once, after the last board section, wording per spec §5.
- Commit messages: imperative mood, **no** `Co-Authored-By`/`Claude-Session` trailers (hathach is sole author).
- Run `pre-commit run --files <changed>` before each commit (it runs from the hook anyway; fix anything it flags).
- Python tests live in `test/hil/`, runnable as `python3 test/hil/<file>.py` (pattern: `test_hil_select.py`). They must not import hardware modules at module scope.

## File Structure

- Modify: `.claude/skills/sysview/scripts/sysview_report.py` (pairing guard, live window, 2 new `--json` fields)
- Modify: `.claude/skills/sysview/SKILL.md` (schema block only)
- Create: `test/hil/test_sysview_report.py` (fixture-driven tests for the reporter fixes)
- Create: `test/hil/sysview_ci.py` (`capture` + `report` subcommands)
- Create: `test/hil/test_sysview_ci.py` (report-generator tests, capture arg/selection tests)
- Modify: `test/hil/tinyusb.json` (add `"sysview"` blocks: `stm32f407disco`, `raspberry_pi_pico`)
- Modify: `.github/workflows/build.yml` (capture step + artifact in HIL matrix job; new `sysview-report` job)
- Modify: `.github/workflows/pr_comment.yml` (hil-comment job appends `sysview-comment`)

---

### Task 1: Reporter fix — discard spliced CALL/RET pairs

`sysview_report.py` currently trusts every `"Returns after X us"` duration. When a record is lost, SystemView's exporter pairs one invocation's CALL with a later invocation's RET (measured: 134 ms "max" on a function whose p99 is 10 µs). Track pairing state per function id and count discards in a new `dropped_pairs` field.

**Files:**
- Modify: `.claude/skills/sysview/scripts/sysview_report.py`
- Create: `test/hil/test_sysview_report.py`

**Interfaces:**
- Produces: `--json` object gains top-level `"dropped_pairs": <int>`. Task 5's wrapper embeds it verbatim; Task 3's generator does not read it (gates use `overflow`/`n`), but it must survive round-trip.

- [ ] **Step 1: Write the failing test**

`test/hil/test_sysview_report.py`. The helper fabricates a minimal SystemView export dir (the two files `sysview_report.py` reads: `contexts.csv`, `events.txt`) and runs the reporter as a subprocess with `--json` — same interface CI uses, no imports of skill code.

```python
#!/usr/bin/env python3
"""Tests for sysview_report.py's data-quality guards (spliced pairs, live window)."""
import json, os, subprocess, sys, tempfile, unittest

REPORT = os.path.join(os.path.dirname(__file__), '..', '..',
                      '.claude', 'skills', 'sysview', 'scripts', 'sysview_report.py')

EV_HEADER = ("sequencenum,timestamp,context,event,detail,timestampint,"
             "contextinint,contextint,contextoutint,eventint,eventoffset,eventsize,eventdata\n")
CTX_HEADER = "Name,Type,Activations,CPU Load,Total Run Time,Total Blocked Time,Min Run Time,Avg Run Time,Max Run Time\n"

def ev(seq, ts, event, detail=""):
    return f'{seq},0.0,"ctx","{event}","{detail}",{ts},0x0,0x0,0x0,0,0,0,\n'

INIT = ev(0, 0, "Init", "Cycle Freq.: 1000000, CPU Freq.: 48000000, ID Base: 0x20000000, ID Shift: 0")

def run_report(events_rows, contexts_rows=""):
    d = tempfile.mkdtemp()
    with open(os.path.join(d, 'events.txt'), 'w') as f:
        f.write(EV_HEADER + INIT + "".join(events_rows))
    with open(os.path.join(d, 'contexts.csv'), 'w') as f:
        f.write(CTX_HEADER + contexts_rows)
    r = subprocess.run([sys.executable, REPORT, d, '--json'],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    return json.loads(r.stdout)

class SplicedPairs(unittest.TestCase):
    def test_clean_pairs_kept(self):
        rows = []
        t = 1000
        for i in range(60):                       # 60 clean call/ret pairs, 10 us each
            rows.append(ev(len(rows)+1, t, "Function #512"))
            rows.append(ev(len(rows)+1, t+10, "Function #512", "Returns after 10.000 us"))
            t += 1000
        j = run_report(rows)
        fn = {f['name']: f for f in j['functions']}
        self.assertEqual(fn['tud_task']['n'], 60)
        self.assertEqual(j['dropped_pairs'], 0)

    def test_ret_after_overflow_dropped(self):
        rows = [ev(1, 1000, "Function #512")]                       # call
        rows.append(ev(2, 1500, "*** Overflow ***"))                # loss marker
        rows.append(ev(3, 135000, "Function #512", "Returns after 134000.000 us"))  # spliced ret
        j = run_report(rows)
        self.assertEqual(j.get('functions', []), [])                # no bogus 134 ms sample
        self.assertEqual(j['dropped_pairs'], 2)                     # invalidated call + orphan ret

    def test_double_call_drops_first(self):
        rows = [ev(1, 1000, "Function #512"),                       # call, ret lost
                ev(2, 2000, "Function #512"),                       # next call
                ev(3, 2010, "Function #512", "Returns after 10.000 us")]
        j = run_report(rows)
        fn = {f['name']: f for f in j['functions']}
        self.assertEqual(fn['tud_task']['n'], 1)                    # only the clean pair
        self.assertEqual(j['dropped_pairs'], 1)

if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run it, confirm it fails**

Run: `python3 test/hil/test_sysview_report.py -v`
Expected: FAIL — `dropped_pairs` KeyError (field doesn't exist), and the 134 ms sample appears in `functions`.

*(If the fixture instead fails on CSV column names: adjust `CTX_HEADER`/`EV_HEADER` to whatever `sysview_report.py` actually reads — check its `csv.DictReader` usage — then re-run. The fixture serves the reporter, not vice versa.)*

- [ ] **Step 3: Implement the pairing guard**

In `sysview_report.py`'s event loop (the `for row in csv.DictReader(...)` block): add before the loop `open_calls = {}` and `dropped_pairs = 0`. Change the two branches:

```python
        elif event == "*** Overflow ***":
            overflow += 1
            # any in-flight CALL may have lost its RET (or vice versa) across the
            # gap -- the exporter would splice it with a later invocation
            dropped_pairs += len(open_calls)
            open_calls.clear()
        elif event.startswith("Function #"):
            fm = func_re.match(event)
            if fm:
                fid = int(fm.group(1)) - TU_SV_EVENT_BASE
                dm = returns_re.search(detail)
                if dm is None:                       # a CALL
                    if open_calls.pop(fid, None):    # previous call never returned
                        dropped_pairs += 1
                    open_calls[fid] = True
                elif open_calls.pop(fid, None) is None:
                    dropped_pairs += 1               # RET without its CALL: spliced
                else:
                    func_durs.setdefault(fid, []).append(
                        float(dm.group(1)) * (1e-6 if dm.group(2) == "us" else 1e-3))
```

Add `"dropped_pairs": dropped_pairs` to the `--json` output object and a `dropped_pairs: N` line to the text output (next to the overflow line).

- [ ] **Step 4: Run tests, confirm pass**

Run: `python3 test/hil/test_sysview_report.py -v` → 3 passing.

- [ ] **Step 5: Commit**

```bash
git add .claude/skills/sysview/scripts/sysview_report.py test/hil/test_sysview_report.py
git commit -m "sysview: discard spliced CALL/RET pairs instead of reporting bogus durations"
```

---

### Task 2: Reporter fix — statistics over the live window only

The export contains boot-time records still in the RTT ring; a quiet capture's stats average over dead air (measured: 111 s span, 98 s idle). Split at the **last inter-event gap > 2 s**: everything after it is the live window. Percentile samples (ISR / ready→run / functions / markers) are filtered to the live window — exact. Context `cpu_pct` is rescaled as `total_ms / live_window_ms` — the numerator still includes boot activity (contexts.csv is whole-recording), a small overstatement that is identical on both sides of a delta; documented in the code comment.

**Files:**
- Modify: `.claude/skills/sysview/scripts/sysview_report.py`
- Modify: `.claude/skills/sysview/SKILL.md` (add `dropped_pairs` + `live_window_s` to the `--json` schema block)
- Test: extend `test/hil/test_sysview_report.py`

**Interfaces:**
- Produces: `--json` gains `"live_window_s": <float>`; `cpu_pct` semantics change to live-window share. Task 5 embeds; Task 3 renders `live_window_s` into the section header line.

- [ ] **Step 1: Write the failing tests** (append to `test_sysview_report.py`)

```python
class LiveWindow(unittest.TestCase):
    def _mixed_rows(self):
        rows = []
        t = 1000
        for i in range(55):                        # stale boot burst at 0-1s
            rows.append(ev(len(rows)+1, t, "ISR Enter", "Runs for 100.000 us")); t += 15
        t = 99_000_000                             # 98 s hole, then live window
        for i in range(60):
            rows.append(ev(len(rows)+1, t, "ISR Enter", "Runs for 5.000 us")); t += 100_000
        return rows

    def test_stale_samples_excluded(self):
        j = run_report(self._mixed_rows())
        isr = j['isr'][0]
        self.assertEqual(isr['n'], 60)             # only live-window samples
        self.assertEqual(isr['p50_us'], 5.0)       # not polluted by the 100 us boot ISRs

    def test_live_window_reported(self):
        j = run_report(self._mixed_rows())
        self.assertAlmostEqual(j['live_window_s'], 5.9, delta=0.2)

    def test_no_gap_means_full_span(self):
        rows = [ev(1, 1000, "ISR Enter", "Runs for 5.000 us"),
                ev(2, 500_000, "ISR Enter", "Runs for 5.000 us")]
        j = run_report(rows)
        self.assertAlmostEqual(j['live_window_s'], 0.5, delta=0.01)
```

- [ ] **Step 2: Run, confirm fail** — `python3 test/hil/test_sysview_report.py -v` (n=115, no `live_window_s`).

- [ ] **Step 3: Implement**

Restructure the event loop to two passes: `rows = list(csv.DictReader(...))` first; parse the timestamp frequency from the Init row's detail (`re.search(r"Cycle Freq\.: (\d+)", ...)`, default 1_000_000 if absent); compute `ts = [int(r["timestampint"]) for r in rows]`; find the last index `i` where `ts[i+1]-ts[i] > 2*freq`; `live_start = ts[i+1]` (or `ts[0]` when no such gap). `live_window_s = (ts[-1]-live_start)/freq`. In the existing sample-collection branches, `continue` for rows with timestamp `< live_start` **except** the overflow counter (count overflow over the whole stream — a pre-window overflow still voids trust). Rescale each context row: `cpu_pct = total_ms / (live_window_s*1000) * 100` when `live_window_s > 0`. Emit `"live_window_s": round(live_window_s, 2)` in `--json` and in the text header. Update the SKILL.md `--json` schema block with both new fields.

- [ ] **Step 4: Run all reporter tests** — 6 passing.

- [ ] **Step 5: Commit**

```bash
git add .claude/skills/sysview/scripts/sysview_report.py .claude/skills/sysview/SKILL.md test/hil/test_sysview_report.py
git commit -m "sysview: compute statistics over the live window, report live_window_s"
```

---

### Task 3: `sysview_ci.py report` — pure markdown generator (TDD)

**Files:**
- Create: `test/hil/sysview_ci.py` (this task: shared helpers + `report`; Task 5 adds `capture`)
- Create: `test/hil/test_sysview_ci.py`

**Interfaces:**
- Consumes: per-board JSON files named `sysview-<board>.json`, schema per spec §3 (`board, commit, example, workload, duration_s, capture{route,poll_ms,n_events,live_window_s}, metrics{...sysview_report --json...}, error`).
- Produces: `report(base_dir, pr_dir) -> str` (empty string = post nothing) and CLI `sysview_ci.py report BASE_DIR PR_DIR -o OUT.md`. Task 6 calls the CLI.

- [ ] **Step 1: Write the failing tests**

`test/hil/test_sysview_ci.py` — fixtures built by helpers, properties asserted (not golden files):

```python
#!/usr/bin/env python3
import json, os, sys, tempfile, unittest
sys.path.insert(0, os.path.dirname(__file__))
import sysview_ci

def metrics(overflow=0, funcs=None, isr=None, contexts=None, stack=None):
    return {"contexts": contexts or [], "isr": isr or [], "ready_run": [],
            "functions": funcs or [], "markers": [], "stack": stack or [],
            "heap": None, "overflow": overflow, "dropped_pairs": 0,
            "live_window_s": 14.2}

def board_json(board="stm32f407disco", err=None, **mk):
    return {"board": board, "commit": "abc1234", "example": "device/cdc_msc",
            "workload": "cdc_burst", "duration_s": 15,
            "capture": {"route": "openocd-rtt", "poll_ms": 1,
                        "n_events": 100000, "live_window_s": 14.2},
            "metrics": None if err else metrics(**mk), "error": err}

def write_set(d, *objs):
    os.makedirs(d, exist_ok=True)
    for o in objs:
        with open(os.path.join(d, f"sysview-{o['board']}.json"), 'w') as f:
            json.dump(o, f)

F = lambda name, n, p50: {"name": name, "n": n, "p50_us": p50, "p99_us": p50*2, "max_us": p50*99}
I = lambda name, n, p50: {"name": name, "n": n, "p50_us": p50, "p99_us": p50+3, "max_us": p50*9}

class Report(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.base, self.pr = os.path.join(self.tmp,'b'), os.path.join(self.tmp,'p')

    def go(self, base_objs, pr_objs):
        write_set(self.base, *base_objs); write_set(self.pr, *pr_objs)
        return sysview_ci.report(self.base, self.pr)

    def test_empty_intersection_returns_empty(self):
        self.assertEqual(self.go([board_json(board="a")], [board_json(board="b")]), "")
        self.assertEqual(sysview_ci.report(self.base, os.path.join(self.tmp,'nope')), "")

    def test_delta_and_occupancy_order(self):
        b = board_json(funcs=[F("dcd_edpt_xfer",200,5.9), F("tud_task",22000,7.4)],
                       isr=[I("ISR 83",24000,6.7)])
        p = board_json(funcs=[F("dcd_edpt_xfer",200,6.1), F("tud_task",22000,7.4)],
                       isr=[I("ISR 83",24000,6.8)])
        md = self.go([b],[p])
        self.assertIn("## ⚡ SystemView performance — HIL", md)
        self.assertIn("+3.4%", md)                                   # dcd 5.9 -> 6.1
        self.assertLess(md.index("tud_task"), md.index("dcd_edpt_xfer"))  # occupancy order
        self.assertIn("```mermaid", md)
        self.assertEqual(md.count("Legend"), 1)
        self.assertNotIn("max", md.split("Legend")[0])               # max never rendered

    def test_overflow_gates_all_durations(self):
        p = board_json(overflow=3, funcs=[F("tud_task",22000,7.4)])
        md = self.go([board_json(funcs=[F("tud_task",22000,7.4)])],[p])
        self.assertIn("⚠︎ overflow 3", md)
        self.assertNotIn("+", md.split("tud_task")[1].split("\n")[0])  # no delta on gated row

    def test_low_n_gates_metric(self):
        p = board_json(funcs=[F("mscd_xfer_cb",9,13.0)])
        md = self.go([board_json(funcs=[F("mscd_xfer_cb",9,13.0)])],[p])
        self.assertIn("⚠︎ n=9", md)

    def test_capture_failed_board(self):
        md = self.go([board_json()],[board_json(err="flash failed: rc=1")])
        self.assertIn("capture failed: flash failed: rc=1", md)

    def test_missing_baseline_absolute(self):
        md = sysview_ci.report(os.path.join(self.tmp,'nobase'), self.pr) or \
             self.go([], [board_json(funcs=[F("tud_task",22000,7.4)])])
        self.assertIn("new", md)                                     # Δ column shows new
        self.assertIn("7.4", md)

    def test_chart_capped_at_8(self):
        funcs=[F(f"fn{i}", 1000-i, 5.0+i) for i in range(10)]
        md = self.go([board_json(funcs=funcs)],[board_json(funcs=funcs)])
        chart = md.split("```mermaid")[1].split("```")[0]
        self.assertLessEqual(len(chart.split("x-axis")[1].split("]")[0].split(",")), 8)

if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run, confirm fail** — `python3 test/hil/test_sysview_ci.py -v` (ImportError: no sysview_ci).

- [ ] **Step 3: Implement `report` in `test/hil/sysview_ci.py`**

```python
#!/usr/bin/env python3
"""SystemView CI: per-board capture on the HIL rig + PR performance report.

report: pure -- two directories of sysview-<board>.json in, markdown out.
capture (Task 5): flash SYSVIEW build, drive workload, RTT-capture, decode.
Spec: docs/superpowers/specs/2026-07-29-sysview-hil-report-design.md
"""
import argparse, glob, json, os, re, sys

GATE_MIN_N = 50
CHART_MAX_BARS = 8
HEADER = "## ⚡ SystemView performance — HIL"
LEGEND = ("<sub>**Legend** — **p50/p99**: median / 99th-percentile duration over all calls "
          "in the capture window (µs; p50 = typical cost, p99 = tail latency). **Δ**: "
          "change vs base branch; **−** is faster/better. **pt**: percentage points. "
          "**CPU load**: context's share of the live capture window. **stack high-water**: "
          "peak bytes of stack used. Function rows/bars are ordered by CPU occupancy "
          "(calls × p50) in the PR capture, hottest first. **– ⚠︎**: metric withheld — RTT "
          "ring overflowed (`overflow N`) or too few samples (`n<50`); withheld beats wrong. "
          "`max` is never shown: under overflow it splices two invocations into one bogus "
          "duration. Capture: OpenOCD RTT @1 ms poll — p50/p99 match the J-Link recorder "
          "within ~1%.</sub>")

def load_set(d):
    out = {}
    for p in glob.glob(os.path.join(d, "sysview-*.json")):
        try:
            j = json.load(open(p))
            out[j["board"]] = j
        except (OSError, ValueError, KeyError):
            continue
    return out

def gate(side, name_metrics):
    """None if usable, else the withhold reason string."""
    if side is None:
        return "new"
    if side["metrics"] is None:
        return "failed"
    if side["metrics"].get("overflow", 0) > 0:
        return f"overflow {side['metrics']['overflow']}"
    if name_metrics is not None and name_metrics.get("n", 0) < GATE_MIN_N:
        return f"n={name_metrics.get('n', 0)}"
    return None

def by_name(mlist):
    return {m["name"]: m for m in (mlist or [])}

def fmt_delta(b, p):
    if b is None or p is None or b == 0:
        return "new" if b is None else "—"
    d = (p - b) / b * 100
    if abs(d) < 1.0:
        return "—"
    mark = " ✅" if d < 0 else ""
    bold = ("**", "**") if abs(d) >= 5 else ("", "")
    return f"{bold[0]}{d:+.1f}%{bold[1]}{mark}"

def metric_cell(side_json, m, fmt):
    reason = gate(side_json, m)
    if reason in (None,):
        return fmt(m), None
    if reason == "new":
        return None, "new"
    if reason == "failed":
        return None, "failed"
    return f"– ⚠︎ {reason}", reason

def row(label, bmap, pmap, name, base_j, pr_j, key="p50_us", unit=" µs"):
    bm, pm = bmap.get(name), pmap.get(name)
    if pm is None and bm is None:
        return None
    fmt = lambda m: f"{m[key]:.1f}{unit}" if m else "—"
    bcell, bgate = metric_cell(base_j, bm, fmt) if bm else ("—", None)
    pcell, pgate = metric_cell(pr_j, pm, fmt) if pm else ("—", None)
    if bgate or pgate:
        delta = "—"
        bcell, pcell = bcell or "—", pcell or "—"
    else:
        delta = fmt_delta(bm and bm[key], pm and pm[key])
    return f"| {label} | {bcell} | {pcell} | {delta} |"

def board_section(name, base_j, pr_j):
    lines = [f"### {name}", ""]
    if pr_j.get("error"):
        return "\n".join(lines + [f"capture failed: {pr_j['error']}", ""])
    pm_all = pr_j["metrics"]
    bm_all = (base_j or {}).get("metrics") or {}
    lines += ["| metric | base | PR | Δ |", "|---|---:|---:|---:|"]
    bisr, pisr = by_name(bm_all.get("isr")), by_name(pm_all.get("isr"))
    for iname, pm in pisr.items():
        bm = bisr.get(iname)
        preason = gate(pr_j, pm)
        breason = "new" if bm is None else gate(base_j, bm)
        def _cell(m, reason):
            if reason is None:
                return f"{m['p50_us']:.1f} / {m['p99_us']:.1f} µs"
            return "—" if reason == "new" else f"– ⚠︎ {reason}"
        if preason or (breason not in (None, "new")):
            delta = "—"
        elif breason == "new":
            delta = "new"
        else:
            delta = (f"{fmt_delta(bm['p50_us'], pm['p50_us'])} / "
                     f"{fmt_delta(bm['p99_us'], pm['p99_us'])}")
        lines.append(f"| {iname} p50 / p99 | {_cell(bm, breason)} | "
                     f"{_cell(pm, preason)} | {delta} |")
    bfn, pfn = by_name(bm_all.get("functions")), by_name(pm_all.get("functions"))
    order = sorted(pfn, key=lambda k: pfn[k]["n"] * pfn[k]["p50_us"], reverse=True)
    for fname in order:
        r = row(f"`{fname}` p50", bfn, pfn, fname, base_j, pr_j)
        if r: lines.append(r)
    bctx, pctx = by_name(bm_all.get("contexts")), by_name(pm_all.get("contexts"))
    for cname, c in pctx.items():
        if cname.lower() in ("usbd", "usbh"):
            b = bctx.get(cname)
            d = "new" if base_j is None else (
                f"{c['cpu_pct']-b['cpu_pct']:+.1f} pt" if b and abs(c['cpu_pct']-b['cpu_pct']) >= 0.1 else "—")
            lines.append(f"| CPU load ({cname} ctx) | "
                         f"{b['cpu_pct']:.1f} % | {c['cpu_pct']:.1f} % | {d} |" if b else
                         f"| CPU load ({cname} ctx) | — | {c['cpu_pct']:.1f} % | {d} |")
    bst, pst = by_name(bm_all.get("stack")), by_name(pm_all.get("stack"))
    for sname, sm in pst.items():
        b = bst.get(sname)
        d = "—" if (b and b["bytes_used"] == sm["bytes_used"]) else ("new" if not b else
            f"{sm['bytes_used']-b['bytes_used']:+d} B")
        lines.append(f"| `{sname}` stack high-water | "
                     f"{b['bytes_used'] if b else '—'} B | {sm['bytes_used']} B | {d} |")
    # chart: occupancy order, capped, only ungated
    if order and gate(pr_j, None) is None:
        chart = order[:CHART_MAX_BARS]
        short = [re.sub(r'^(tud_|tuh_|dcd_|hcd_)', '', c) for c in chart]
        pv = [f"{pfn[c]['p50_us']:.1f}" for c in chart]
        bv = [f"{bfn[c]['p50_us']:.1f}" if c in bfn else "0" for c in chart]
        ymax = max(float(v) for v in pv + bv) * 1.3
        lines += ["", "```mermaid", "xychart-beta",
                  '    title "hot functions p50 µs (base vs PR)"',
                  f"    x-axis [{', '.join(short)}]",
                  f'    y-axis "µs" 0 --> {ymax:.0f}',
                  f"    bar [{', '.join(bv)}]",
                  f"    bar [{', '.join(pv)}]", "```"]
    return "\n".join(lines) + "\n"

def report(base_dir, pr_dir):
    pr = load_set(pr_dir)
    if not pr:
        return ""
    base = load_set(base_dir)
    boards = sorted(pr)
    any_pr = next(iter(pr.values()))
    head = (f"*{any_pr['example']} `SYSVIEW=4`, workload `{any_pr['workload']}` "
            f"{any_pr['duration_s']} s, OpenOCD rtt @1 ms · "
            f"base `{next(iter(base.values()))['commit'] if base else '(none)'}` → "
            f"PR `{any_pr['commit']}`*")
    parts = [HEADER, "", head, ""]
    for b in boards:
        parts.append(board_section(b, base.get(b), pr[b]))
    parts.append(LEGEND)
    return "\n".join(parts) + "\n"

def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    rp = sub.add_parser("report")
    rp.add_argument("base_dir"); rp.add_argument("pr_dir")
    rp.add_argument("-o", "--out", default="sysview_report.md")
    args = ap.parse_args()
    if args.cmd == "report":
        md = report(args.base_dir, args.pr_dir)
        with open(args.out, "w") as f:
            f.write(md)
        print(f"{'empty (no captures)' if not md else args.out}")

if __name__ == "__main__":
    main()
```

- [ ] **Step 4: Run, iterate until green** — `python3 test/hil/test_sysview_ci.py -v`. Adjust test/implementation mismatches by fixing the *implementation* unless the test contradicts the spec.

- [ ] **Step 5: Eyeball one render** — `python3 - <<'EOF'` (build the delta fixture, print `report()`), paste output into any markdown previewer; check the mermaid block parses (mermaid.live).

- [ ] **Step 6: Commit**

```bash
git add test/hil/sysview_ci.py test/hil/test_sysview_ci.py
git commit -m "hil: sysview report generator - gated deltas, occupancy-sorted chart, legend"
```

---

### Task 4: Flag two boards in `tinyusb.json`

**Files:** Modify: `test/hil/tinyusb.json`

- [ ] **Step 1: Add the blocks** — to `stm32f407disco` (jlink-flashed, so `ocd_args` required) and `raspberry_pi_pico` (openocd-flashed, flasher args reused):

```json
"sysview": {"example": "device/cdc_msc", "workload": "cdc_burst", "duration_s": 15,
            "ocd_args": "-f interface/jlink.cfg -c \"transport select swd\" -f target/stm32f4x.cfg"}
```
```json
"sysview": {"example": "device/cdc_msc", "workload": "cdc_burst", "duration_s": 15}
```

- [ ] **Step 2: Validate** — `python3 -c "import json; json.load(open('test/hil/tinyusb.json'))"` and `python3 test/hil/test_hil_select.py` (selection must be unaffected by the new key).

- [ ] **Step 3: Commit** — `git commit -m "hil: flag stm32f407disco and raspberry_pi_pico for sysview capture"`

---

### Task 5: `sysview_ci.py capture`

**Files:** Modify: `test/hil/sysview_ci.py`, `test/hil/test_sysview_ci.py`

**Interfaces:**
- Consumes: `hil_flash.flash_<name>(board, firmware)` (firmware = extension-less path), `hil_flash.find_firmware(variant, example)`, `hil_lock.acquire_board_lock` (**check its exact call convention in `hil_test.py` first** — `grep -n acquire_board_lock test/hil/hil_test.py` — and mirror it), skill scripts `sysview_record.py --from-raw` / `sysview_report.py --json`, `rtt_cb_from_elf` imported from `sysview_record.py`.
- Produces: `sysview-<board>.json` files per spec §3 in `--out` dir. Exit code 0 unless *zero* boards were even attempted due to bad args.

- [ ] **Step 1: Write failing tests for the pure parts** (append to `test_sysview_ci.py`)

```python
class CaptureSelection(unittest.TestCase):
    CFG = {"boards": [
        {"name": "a", "uid": "U1", "flasher": {"name": "openocd", "uid": "P1",
         "args": "-f interface/x.cfg -f target/y.cfg"},
         "sysview": {"example": "device/cdc_msc", "workload": "cdc_burst", "duration_s": 15}},
        {"name": "b", "uid": "U2", "flasher": {"name": "jlink", "uid": "P2", "args": "-device X"},
         "sysview": {"example": "device/cdc_msc", "workload": "idle", "duration_s": 10,
                     "ocd_args": "-f interface/jlink.cfg -f target/z.cfg"}},
        {"name": "c", "uid": "U3", "flasher": {"name": "openocd", "uid": "P3", "args": ""}}]}

    def test_flagged_only(self):
        self.assertEqual([b["name"] for b in sysview_ci.select_boards(self.CFG, [])], ["a", "b"])

    def test_intersection_with_board_args(self):
        self.assertEqual([b["name"] for b in sysview_ci.select_boards(self.CFG, ["b", "c"])], ["b"])

    def test_ocd_args_resolution(self):
        a, b = sysview_ci.select_boards(self.CFG, [])
        self.assertIn("target/y.cfg", sysview_ci.capture_ocd_args(a))   # falls back to flasher
        self.assertIn("target/z.cfg", sysview_ci.capture_ocd_args(b))   # explicit override
        with self.assertRaises(ValueError):                              # jlink flasher, no override
            sysview_ci.capture_ocd_args({"flasher": {"name": "jlink", "args": "-device X"},
                                         "sysview": {}})

    def test_wrapper_error_shape(self):
        j = sysview_ci.board_result(self.CFG["boards"][0], "abc1234", error="flash failed: rc=1")
        self.assertEqual(j["error"], "flash failed: rc=1"); self.assertIsNone(j["metrics"])
```

- [ ] **Step 2: Run, confirm fail.**

- [ ] **Step 3: Implement.** Add to `sysview_ci.py` (below `report`, above `main`), with `capture` kept import-safe (hardware imports inside functions):

```python
# ---------------------------------------------------------------- capture
OPENOCD_FAMILY = ("openocd", "openocd_wch", "openocd_adi")

def select_boards(cfg, board_args):
    picked = [b for b in cfg["boards"] if "sysview" in b]
    if board_args:
        picked = [b for b in picked if b["name"] in set(board_args)]
    return picked

def capture_ocd_args(board):
    args = board["sysview"].get("ocd_args") or (
        board["flasher"]["args"] if board["flasher"]["name"] in OPENOCD_FAMILY else None)
    if not args:
        raise ValueError(f"{board.get('name')}: non-openocd flasher and no sysview.ocd_args")
    return [a.strip('"') for a in re.findall(r'"[^"]*"|\S+', args)]

def board_result(board, commit, metrics=None, capture_info=None, error=None):
    sv = board["sysview"]
    return {"board": board["name"], "commit": commit,
            "example": sv["example"], "workload": sv["workload"],
            "duration_s": sv["duration_s"],
            "capture": capture_info or {}, "metrics": metrics, "error": error}
```

Then the hardware path (single function per concern, all subprocess-based, mirroring the session-proven harness):

```python
def _workload_cdc_burst(node, duration_s):
    import serial, time
    s = serial.Serial(node, 115200, timeout=0.02)
    end = time.monotonic() + duration_s
    while time.monotonic() < end:
        t = time.monotonic()
        while time.monotonic() - t < 0.30 and time.monotonic() < end:
            try:
                s.write(b"x" * 64); s.read(64)
            except Exception:
                return
        time.sleep(min(1.0, max(0, end - time.monotonic())))
    s.close()

WORKLOADS = {"cdc_burst": _workload_cdc_burst,
             "idle": lambda node, duration_s: __import__("time").sleep(duration_s)}

def capture_one(board, commit, out_dir, repo_root):
    """Build, flash, RTT-capture, decode; returns the wrapper dict. Never raises."""
    import subprocess, time, socket, signal
    sv, name = board["sysview"], board["name"]
    scripts = os.path.join(repo_root, ".claude", "skills", "sysview", "scripts")
    sys.path.insert(0, scripts)
    from sysview_record import rtt_cb_from_elf
    import hil_flash
    try:
        ocd_args = capture_ocd_args(board)
        exdir = os.path.join(repo_root, "examples", sv["example"])
        bdir = os.path.join(repo_root, "examples", f"cmake-build-sysview-{name}")
        buf = [f"-DSYSVIEW_BUFFER_SIZE={sv['buffer']}"] if "buffer" in sv else []
        for cmd in ([ "cmake", "-B", bdir, f"-DBOARD={name}", "-G", "Ninja",
                      "-DCMAKE_BUILD_TYPE=MinSizeRel", "-DSYSVIEW=4", *buf, "." ],
                    [ "cmake", "--build", bdir ]):
            r = subprocess.run(cmd, cwd=exdir, capture_output=True, text=True, timeout=1800)
            if r.returncode:
                return board_result(board, commit, error=f"build failed: {r.stderr[-300:]}")
        base = os.path.basename(sv["example"])
        fw = os.path.join(bdir, base)                       # extension-less base path
        flash = getattr(hil_flash, f"flash_{board['flasher']['name']}")
        r = flash(board, fw)
        if r.returncode:
            return board_result(board, commit, error=f"flash failed: rc={r.returncode}")
        node = f"/dev/serial/by-id/usb-TinyUSB_TinyUSB_Device_{board['uid']}-if00"
        for _ in range(25):
            if os.path.exists(node): break
            time.sleep(1)
        cb = int(rtt_cb_from_elf(fw + ".elf"), 16)
        is_wch = board["flasher"]["name"] == "openocd_wch"
        port = 19500
        ocd = ["openocd", "-c", "tcl_port disabled", "-c", "gdb_port disabled",
               "-c", "telnet_port disabled",
               "-c", f"adapter serial {board['flasher']['uid']}"] + ocd_args + \
              ["-c", "init"] + ([] if is_wch else ["-c", "reset run", "-c", "sleep 2000"]) + \
              ["-c", f'rtt setup {cb} 0x1000 "SEGGER RTT"',
               "-c", "rtt polling_interval 1", "-c", "rtt start",
               "-c", f"rtt server start {port} 1"]
        raw = os.path.join(out_dir, f"{name}-capture.SVDat")
        p = subprocess.Popen(ocd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
        try:
            for _ in range(80):
                time.sleep(0.2)
                try:
                    socket.create_connection(("localhost", port), 0.3).close(); break
                except OSError:
                    if p.poll() is not None:
                        return board_result(board, commit,
                                            error=f"openocd: {p.stderr.read()[-200:]}")
            else:
                return board_result(board, commit, error="rtt server never listened")
            with open(raw, "wb") as f:
                nc = subprocess.Popen(["nc", "localhost", str(port)], stdout=f)
                try:
                    WORKLOADS[sv["workload"]](node, sv["duration_s"])
                finally:
                    nc.send_signal(signal.SIGINT)
                    try: nc.wait(5)
                    except subprocess.TimeoutExpired: nc.kill()
        finally:
            p.send_signal(signal.SIGINT)
            try: p.wait(8)
            except subprocess.TimeoutExpired: p.kill()
        dec = os.path.join(out_dir, f"{name}-decoded")
        for cmd in ([sys.executable, os.path.join(scripts, "sysview_record.py"),
                     "--from-raw", raw, "--out", dec],):
            r = subprocess.run(cmd, capture_output=True, text=True, timeout=900)
        r = subprocess.run([sys.executable, os.path.join(scripts, "sysview_report.py"),
                            dec, "--json"], capture_output=True, text=True, timeout=600)
        if r.returncode:
            return board_result(board, commit, error=f"decode failed: {r.stderr[-200:]}")
        m = json.loads(r.stdout)
        info = {"route": "openocd-rtt", "poll_ms": 1,
                "n_events": None, "live_window_s": m.get("live_window_s")}
        return board_result(board, commit, metrics=m, capture_info=info)
    except Exception as e:
        return board_result(board, commit, error=f"{type(e).__name__}: {e}")
    finally:
        _reflash_pristine(board, repo_root)

def _reflash_pristine(board, repo_root):
    import hil_flash, subprocess
    try:
        fw = hil_flash.find_firmware(board["name"], board["sysview"]["example"])
        if fw:
            getattr(hil_flash, f"flash_{board['flasher']['name']}")(board, str(fw))
    except Exception:
        pass                                    # pristine reflash is best-effort
```

`capture` CLI in `main()`: `capture CONFIG_JSON [-b BOARD]... [--out DIR]`; loads config, `select_boards`, per board: `with acquire_board_lock(name, reason="sysview capture"):` (convention verified per Interfaces) around `capture_one`, writes `sysview-<board>.json`, prints one status line per board, exits 0.

- [ ] **Step 4: Run tests** — `python3 test/hil/test_sysview_ci.py -v` (all report + selection tests green).

- [ ] **Step 5: Commit** — `git commit -m "hil: sysview capture - flash SYSVIEW build, drive workload, RTT-capture, decode"`

---

### Task 6: `build.yml` wiring

**Files:** Modify: `.github/workflows/build.yml`

- [ ] **Step 1: Capture step + artifact in the `hil-tinyusb` matrix job**, immediately after the `hil_test.py` step, passing the **identical selection-args expansion** that step uses (copy its `${{ matrix.test_args }} ... $RERUN_ARGS`-style expression, converting to `-b` form only if hil_select emits bare `-b` args — inspect the select step output format and mirror):

```yaml
      - name: SystemView capture
        if: always()
        continue-on-error: true
        run: |
          python3 test/hil/sysview_ci.py capture ${{ env.HIL_JSON }} \
            $SYSVIEW_BOARD_ARGS --out sysview-out
      - name: Upload SystemView captures
        if: always()
        continue-on-error: true
        uses: actions/upload-artifact@v7
        with:
          name: sysview-${{ matrix.display }}
          path: sysview-out/sysview-*.json
          if-no-files-found: ignore
```

where `SYSVIEW_BOARD_ARGS` is derived in the same step from the test step's board selection (empty on push = all flagged boards).

- [ ] **Step 2: `sysview-report` job** (ubuntu, `needs: hil-tinyusb`, `if: always() && github.event_name == 'pull_request'`): checkout; `download-artifact` pattern `sysview-*` → `pr-sysview/` (merge-multiple); `dawidd6/action-download-artifact@v11` with `workflow: build.yml`, `branch: ${{ github.base_ref }}`, `name: sysview-.*`, `name_is_regexp: true`, `path: base-sysview`, `continue-on-error: true`; flatten both dirs; `python3 test/hil/sysview_ci.py report base-sysview pr-sysview -o sysview_report.md`; upload artifact `sysview-comment` (`if-no-files-found: ignore`, skip upload when the file is empty).

- [ ] **Step 3: Validate YAML** — `python3 -c "import yaml; yaml.safe_load(open('.github/workflows/build.yml'))"` (PyYAML available on the dev box; if not, `actionlint` or a py `ruamel` fallback).

- [ ] **Step 4: Commit** — `git commit -m "ci: sysview capture in HIL jobs, compare job producing sysview-comment"`

---

### Task 7: `pr_comment.yml` — append to the HIL sticky comment

**Files:** Modify: `.github/workflows/pr_comment.yml`

- [ ] **Step 1:** In the `hil-comment` job, after the `hil-report-*` download, add a `sysview-comment` download (`continue-on-error: true`, same run-id/token pattern). In the combine step, after the rig loop, append:

```bash
          if [ -s sysview-comment/sysview_report.md ]; then
            echo >> hil_combined.md
            cat sysview-comment/sysview_report.md >> hil_combined.md
          fi
```

The existing zero-width-space @-mention neutralization runs after combining, so it covers the sysview markdown automatically — keep the append **before** that step.

- [ ] **Step 2: Validate YAML** (as Task 6 Step 3). Commit — `git commit -m "ci: append sysview performance report to the HIL sticky comment"`

---

### Task 8: Live verification on ci.lan + mermaid render check

No code. Verifies the hardware path end-to-end before the workflow ever runs it.

- [ ] **Step 1:** Sync the worktree to the rig mirror (`rsync` `test/hil/`, `src/`, `hw/bsp/`, `.claude/skills/sysview/` → `hathach@ci.lan:sysview-v2/`, never touching dep symlinks).
- [ ] **Step 2:** On ci.lan (toolchains + `~/.local/bin` on PATH): `python3 test/hil/sysview_ci.py capture test/hil/tinyusb.json -b stm32f407disco -b raspberry_pi_pico --out /tmp/sv-ci-test` — expect two `sysview-*.json` with `error: null`, `metrics.overflow == 0`, `live_window_s ≈ 14–15`.
- [ ] **Step 3:** `report` with the same dir as both base and PR (`-o /tmp/r.md`) — every Δ must be `—`; then against a copy with one hand-edited p50 — that Δ and only that Δ appears.
- [ ] **Step 4:** Paste `/tmp/r.md` into a scratch GitHub PR comment (or gist) — confirm the mermaid chart renders and the legend reads correctly. Delete the scratch comment.
- [ ] **Step 5:** Confirm both boards are back on pristine firmware (`hil_test.py -b <board>` smoke or check enumeration), release any locks, remove `/tmp/sv-ci-test`.
- [ ] **Step 6:** Commit any fixes found, message prefixed `hil: sysview capture fixes from rig verification -`.

---

## Execution notes

- Tasks 1→2→3 are strictly ordered (reporter fields feed fixtures). Task 4 is independent after 3. Task 5 depends on 3+4. Tasks 6–7 depend on 5. Task 8 last.
- The rig is shared with live CI: Task 8 must hold board locks and strictly follow the one-instance rule for hardware access.
- If `acquire_board_lock`'s convention differs from the plan's `with` usage, adapt the call site — do not modify `hil_lock.py`.
