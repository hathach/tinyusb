#!/usr/bin/env python3
"""Summarize a sysview_record.py output directory into profiling numbers.

From contexts.csv: per-context (task/ISR) CPU load, activation count,
min/avg/max run time, and total blocked time. From events.txt (if
present): overflow count (nonzero = data loss — raise SYSVIEW_BUFFER_SIZE),
per-ISR duration percentiles, per-task ready->run latency percentiles,
FreeRTOS task stack high-water bytes (from periodic "Stack Info" events,
CFG_TUD_SYSVIEW's tusb_sysview_stack_report()), heap alloc/free totals
(from "Allocate Memory"/"Free Memory" events, traceMALLOC/traceFREE ->
tusb_sysview_heap_alloc/free — only present on dynamic-allocation builds),
per-function timing percentiles (TUD/TUH_SYSVIEW_CALL/RET call sites, see
src/common/tusb_sysview.h) and marker timing percentiles (SEGGER_SYSVIEW_
MarkStart/MarkStop pairs, app-inserted).

--json prints one JSON object to stdout instead of the text tables (every
numeric field a real number, not a formatted string) — for CI/PR-comment
consumption, e.g. `sysview_report.py <dir> --json | jq .contexts`.
"""
import argparse
import csv
import json
import re
import sys
from pathlib import Path

# Function-timing event ids are recorded at TU_SV_EVENT_BASE (512) + id
# (src/common/tusb_sysview.h) instead of a registered SEGGER_SYSVIEW_MODULE
# (module registration greys out Save/Export on the SystemView 4.10b Linux
# host, bench-confirmed with 5 module configurations). SystemView therefore renders both the CALL
# and its matching RecordEndCall as event name "Function #<512+id>" (verified
# on real hardware: build/record/inspect events.txt, not guessed) — this
# table maps the numeric id back to the C function name for the report.
TU_SV_EVENT_BASE = 512
TU_SV_FUNC_NAMES = {
    0: "tud_task",
    1: "usbd_edpt_xfer",
    2: "dcd_edpt_xfer",
    3: "tud_cdc_write_flush",
    4: "tud_cdc_read",
    5: "mscd_xfer_cb",
    6: "tuh_task",
    7: "hcd_edpt_xfer",
}
# W13: the workload-window anchor below keys off this table instead of a hardcoded "Function
# #516" literal, so test_sysview_report.py's check that TU_SV_FUNC_NAMES matches the header's
# enum order also protects this derivation -- inserting/reordering an id shifts both together.
_WORKLOAD_ANCHOR_EVENT = "Function #" + str(
    TU_SV_EVENT_BASE + next(fid for fid, name in TU_SV_FUNC_NAMES.items() if name == "tud_cdc_read"))


def parse_time_s(s):
    """SystemView CSV writes '0.008 165 075 s' / '0.034 725 ms'."""
    m = re.match(r"([\d. ]+)\s*(s|ms|us)", s.strip())
    if not m:
        return 0.0
    v = float(m.group(1).replace(" ", ""))
    return v * {"s": 1.0, "ms": 1e-3, "us": 1e-6}[m.group(2)]


def parse_ts_int(v):
    """events.txt's timestampint column, defensively -- some rows carry an
    unparsable/empty value; treat those as unknown rather than crashing."""
    try:
        return int(v)
    except (TypeError, ValueError):
        return None


def pct(sorted_vals, p):
    """Conventional nearest-rank percentile: index (n-1)*p/100, not n*p/100 -- the latter lands
    one slot too high (e.g. pct([10, 100], 50) would report the MAX as the p50 median) and
    biases every low-n p50 upward in general."""
    if not sorted_vals:
        return 0.0
    n = len(sorted_vals)
    return sorted_vals[min(n - 1, int((n - 1) * p / 100))]


def fmt_us(sec):
    return f"{sec * 1e6:.1f}"


def num_us(sec):
    """Same value fmt_us() prints, as a real float for --json."""
    return round(sec * 1e6, 1)


def duration_table(durs_by_key, name_of=lambda k: str(k)):
    """durs_by_key: key -> [duration_s, ...]. Returns rows sorted by key, each
    {name, n, p50_us, p99_us, max_us} — shared by ISR/ready-run/function/marker
    tables, which are all "sorted duration list per named thing"."""
    rows = []
    for key, vals in sorted(durs_by_key.items()):
        vals.sort()
        rows.append({
            "name": name_of(key),
            "n": len(vals),
            "p50_us": num_us(pct(vals, 50)),
            "p99_us": num_us(pct(vals, 99)),
            "max_us": num_us(vals[-1]),
        })
    return rows


def print_duration_table(label, rows, width=12):
    print(f"\n{label:<{width}} {'n':>7} {'p50_us':>8} {'p99_us':>8} {'max_us':>8}")
    for r in rows:
        print(f"{r['name']:<{width}} {r['n']:>7} {r['p50_us']:>8.1f} "
              f"{r['p99_us']:>8.1f} {r['max_us']:>8.1f}")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("outdir", help="directory produced by sysview_record.py")
    ap.add_argument("--json", action="store_true",
                    help="print one JSON object instead of text tables")
    ap.add_argument("--window", metavar="T0:T1",
                    help="explicit workload window, seconds from the first event -- overrides "
                         "the marker/CDC-span anchors for cpu_pct_workload")
    args = ap.parse_args()
    out = Path(args.outdir)

    ctx_csv = out / "contexts.csv"
    if not ctx_csv.exists():
        sys.exit(f"error: {ctx_csv} not found")

    if not args.json:
        print(f"{'context':<12} {'act':>7} {'cpu%':>7} {'total_ms':>10} {'blk_ms':>8} "
              f"{'min_us':>8} {'avg_us':>8} {'max_us':>8}")
    contexts = []
    for row in csv.DictReader(open(ctx_csv)):
        act = int(row["Activations"] or 0)
        total = parse_time_s(row["Total Run Time"])
        if act == 0 and total == 0:
            continue
        avg = total / act if act else 0.0
        blk = parse_time_s(row["Total Blocked Time"])
        cpu_str = row["CPU Load"].strip()
        if not args.json:
            print(f"{row['Name']:<12} {act:>7} {cpu_str:>7} {total * 1e3:>10.3f} "
                  f"{blk * 1e3:>8.3f} "
                  f"{fmt_us(parse_time_s(row['Min Run Time'])):>8} {fmt_us(avg):>8} "
                  f"{fmt_us(parse_time_s(row['Max Run Time'])):>8}")
        contexts.append({
            "name": row["Name"],
            "activations": act,
            "cpu_pct": float(cpu_str.rstrip("%").strip() or 0.0),
            "total_ms": round(total * 1e3, 3),
            "blocked_ms": round(blk * 1e3, 3),
            "min_us": num_us(parse_time_s(row["Min Run Time"])),
            "avg_us": num_us(avg),
            "max_us": num_us(parse_time_s(row["Max Run Time"])),
        })

    ev = out / "events.txt"
    if not ev.exists():
        if args.json:
            print(json.dumps({
                "contexts": contexts, "isr": [], "ready_run": [], "functions": [],
                "markers": [], "stack": [], "heap": None, "overflow": 0,
                "dropped_pairs": 0, "live_window_s": None,
                "workload_window_s": None, "workload_anchor": None, "contexts_workload": [],
                "warnings": [],
            }, indent=1))
        else:
            print("\n(no events.txt — re-run without --no-events for percentiles/latency)")
        return

    overflow = 0       # real count over the capture -- what gate() consumes
    isr_runs = {}     # context -> [duration_s]  from ISR Enter "Runs for X us"
    ready_lat = {}    # task -> [latency_s]      from Task Ready "name, runs after X us"
    stack_used = {}   # task -> bytes_used, last "Stack Info" wins (high-water only grows)
    heap_last = None  # last "Allocate Memory" / "Free Memory" match (running totals)
    func_durs = {}    # func id (0-based, see TU_SV_FUNC_NAMES) -> [duration_s]
    marker_durs = {}  # marker id -> [duration_s]
    open_calls = {}   # func id -> [True, ...] stack, one entry per CALL awaiting its RET
                       # (a list, not a bool: nesting -- CALL,CALL,RET,RET on the same id, e.g.
                       # a preempted task -- must pair LIFO, innermost CALL to innermost RET)
    dropped_pairs = 0 # count of spliced CALL/RET pairs discarded due to data loss
    # Scheduling reconstruction for cpu_pct_workload: the export's transition events (Task Run /
    # System Idle switch the running task; ISR Enter/Exit nest on top) let per-context busy time
    # be SUMMED from raw events over any window -- no rescaling of SystemView's whole-recording
    # totals. Segments: (t_start, t_end, context). Runner state: ISR stack over current task.
    sched_segments = []
    sched_task = None     # current running task/idle context, from Task Run / System Idle
    sched_isrs = []       # nested ISR contexts, innermost last
    sched_last_ts = None  # timestamp of the previous transition
    wl_first = None       # workload window anchors: span of tud_cdc_read (Function #516) events
    wl_last = None
    mark_first = None     # marker-pair anchors (Start/Stop Marker), preferred over the CDC span
    mark_last = None
    runs_re = re.compile(r"Runs for ([\d.]+) (us|ms)")
    ready_re = re.compile(r"runs after ([\d.]+) (us|ms)")
    # "<task> (0x<id>): <size> @ 0x<base>, <used> Bytes used" — tusb_sysview_stack_report()'s
    # periodic reports; the one-shot report at task-create time has no "Bytes used" suffix
    # (StackUsage not yet meaningful) and is intentionally not matched here.
    stack_re = re.compile(r"^(.+) \(0x[0-9A-Fa-f]+\): \d+ @ 0x[0-9A-Fa-f]+, (\d+) Bytes used$")
    # SystemView's own running totals, common to "Allocate Memory" and "Free Memory" details:
    # "... -- <used> used, <free> free, <pct>% full -- <N> allocations, <M> frees, difference <D>"
    heap_re = re.compile(r"(\d+) used, \d+ free, [\d.]+% full -- (\d+) allocations, (\d+) frees, difference")
    # Un-registered module CALL/RET pair (tusb_sysview.h _TU_SV_RECORD/_TU_SV_END): SystemView
    # renders BOTH the call and its matching RecordEndCall under the same event name
    # "Function #<512+id>" (verified on hardware, see TU_SV_FUNC_NAMES above) — the RecordEndCall
    # row's detail already carries the computed duration ("Returns after X us"), so no manual
    # timestamp pairing is needed the way ISR/ready-latency above do it.
    func_re = re.compile(r"^Function #(\d+)$")
    returns_re = re.compile(r"Returns after ([\d.]+) (us|ms)")
    # SEGGER_SYSVIEW_MarkStart(id)/MarkStop(id) (app-inserted, see Step 5): SystemView decodes
    # these natively (no module registration needed, unlike the Function-id events above) as
    # "Start Marker 0x<id>" / "Stop Marker 0x<id>", and — verified on hardware — the Stop row's
    # own detail text already carries the computed duration ("Ran for X us, pass #N"), the same
    # shape as the Function RecordEndCall rows, so no manual pairing is needed here either.
    mark_stop_re = re.compile(r"^Stop Marker 0x([0-9A-Fa-f]+)$")
    ran_re = re.compile(r"Ran for ([\d.]+) (us|ms)")
    freq_re = re.compile(r"Cycle Freq\.: (\d+)")

    # The export contains boot-time records still sitting in the RTT ring (a quiet capture can
    # be mostly dead air -- measured: 111 s span, 98 s idle). live_window_s below is just the
    # capture's own first-event-to-last span -- no gap heuristic trims it, and every percentile
    # sample (ISR/ready-run/function/marker) is drawn from the whole stream, not "the window";
    # see the "No gap heuristic" comment further down for why.
    rows = list(csv.DictReader(open(ev, errors="replace")))
    warnings = []
    freq = None
    init_row = next((r for r in rows if r.get("event") == "Init"), None)
    if init_row:
        fm = freq_re.search(init_row.get("detail", ""))
        if fm:
            freq = int(fm.group(1))
    if freq is None:
        # Don't silently guess: on DWT cores the real frequency is CPU cycles, not
        # microseconds (tusb_sysview_init() passes tusb_sysview_cpu_freq()), so a wrong
        # guess collapses live_window_s by orders of magnitude while still looking
        # plausible. Every existing capture has an Init row; a missing one means an
        # older/malformed export -- withhold rather than trust a made-up time base.
        warnings.append("no Init record in events.txt -- missing timestamp frequency, "
                         "live_window_s/cpu_pct withheld")
    ts_all = [parse_ts_int(r.get("timestampint")) for r in rows]
    valid_ts = [t for t in ts_all if t is not None]
    # No gap heuristic: the reported window is the capture, first event to last. An idle
    # stretch inside a capture is a real observation about the target, not something to trim --
    # trimming it silently changed which data the percentiles and CPU% described. The only thing
    # ever worth excluding is pre-attach ring content, and the capture path already avoids that
    # by draining from a reset target rather than reading whatever accumulated earlier.
    live_start = valid_ts[0] if valid_ts else 0
    live_window_s = ((valid_ts[-1] - live_start) / freq) if (valid_ts and freq) else None
    backwards = next((i for i in range(len(valid_ts) - 1) if valid_ts[i + 1] < valid_ts[i]), None)
    # W5: also gates workload_window_s/workload_anchor/contexts_workload further below -- those
    # are computed from this same valid_ts/freq, so a corrupt clock taints them exactly as much
    # as it taints live_window_s, not just the metric it happens to be computed next to.
    clock_corrupt = live_window_s is not None and (live_window_s <= 0 or backwards is not None)
    if clock_corrupt:
        # Timestamps went non-monotonic -- e.g. a 32-bit DWT CYCCNT wraps every ~25.6 s at
        # 168 MHz, comfortably inside a 15 s capture plus boot dead-air. This monotonicity check
        # is the only defense against that: the window itself is just first-event-to-last (no
        # gap heuristic, see above), so nothing else here would ever notice a wrapped counter on
        # its own. A negative/zero span is not a smaller-but-valid window, it is "the clock
        # lied" -- treat it exactly like no time base at all (gate() already withholds every
        # metric once live_window_s is None) rather than publish a negative "live -N s" heading
        # with whole-recording CPU% silently relabelled as live-window share.
        warnings.append(f"timestamps not monotonic (counter wrap?) or zero span "
                         f"({live_window_s:.3f}s) -- treating as no time base")
        live_window_s = None

    for idx, row in enumerate(rows):
        event = row.get("event", "")
        detail = row.get("detail", "")
        _ts = ts_all[idx]
        if _ts is not None:
            _trans = None
            if event == "Task Run" or event == "System Idle":
                _trans = ("task", row.get("context", ""))
            elif event == "ISR Enter":
                _trans = ("isr_in", row.get("context", ""))
            elif event == "ISR Exit":
                _trans = ("isr_out", None)
            if _trans is not None:
                _runner = sched_isrs[-1] if sched_isrs else sched_task
                if _runner is not None and sched_last_ts is not None and _ts > sched_last_ts:
                    sched_segments.append((sched_last_ts, _ts, _runner))
                kind, ctx = _trans
                if kind == "task":
                    sched_task = ctx
                elif kind == "isr_in":
                    sched_isrs.append(ctx)
                elif kind == "isr_out" and sched_isrs:
                    sched_isrs.pop()
                sched_last_ts = _ts
            if event == _WORKLOAD_ANCHOR_EVENT:      # tud_cdc_read CALL or RET: the throughput test
                wl_first = _ts if wl_first is None else wl_first
                wl_last = _ts
            elif event.startswith("Start Marker ") or event.startswith("Stop Marker "):
                mark_first = _ts if mark_first is None else mark_first
                mark_last = _ts
        if "*** OVERFLOW ***" in detail:
            # SystemView's second loss marker: an exit/switch whose return context was lost to
            # ring overflow renders as "Returns to *** OVERFLOW ***" in the detail text, WITHOUT
            # a companion "*** Overflow ***" event row. A dual-role dogfood measured 96-99% of
            # ISR exits carrying this form while the explicit rows numbered 0-1 -- counting only
            # the rows understated real loss by two orders of magnitude.
            overflow += 1
        if event == "*** Overflow ***":
            overflow += 1
            # any in-flight CALL may have lost its RET (or vice versa) across the
            # gap -- the exporter would splice it with a later invocation
            dropped_pairs += sum(len(v) for v in open_calls.values())
            open_calls.clear()
        elif event == "ISR Enter":
            m = runs_re.search(detail)
            if m:
                isr_runs.setdefault(row["context"], []).append(
                    float(m.group(1)) * (1e-6 if m.group(2) == "us" else 1e-3))
        elif event == "Task Ready":
            m = ready_re.search(detail)
            if m:
                task = detail.split(",")[0]
                ready_lat.setdefault(task, []).append(
                    float(m.group(1)) * (1e-6 if m.group(2) == "us" else 1e-3))
        elif event == "Stack Info":
            m = stack_re.match(detail)
            if m:
                stack_used[m.group(1)] = int(m.group(2))
        elif event in ("Allocate Memory", "Free Memory"):
            m = heap_re.search(detail)
            if m:
                heap_last = m
        elif event.startswith("Function #"):
            # Pairing bookkeeping runs for EVERY row: a CALL must land in open_calls, or its RET
            # finds nothing to pop and gets wrongly counted as a spliced orphan -- systematically
            # losing the first invocation of every instrumented function. There is no live-window
            # filter applied here or anywhere below: every completed CALL/RET pair becomes a
            # duration sample regardless of where in the capture it falls (see the "No gap
            # heuristic" comment above -- the window is the capture).
            fm = func_re.match(event)
            if fm:
                fid = int(fm.group(1)) - TU_SV_EVENT_BASE
                dm = returns_re.search(detail)
                # A return is "Returns after <N> us" only when SystemView could annotate the
                # duration; overwhelmingly it emits a bare "Returns" (measured: 99.5% on
                # stm32f407disco, 99.2% on raspberry_pi_pico). Keying CALL-vs-RET off the
                # duration regex therefore misread almost every return as a call, which both
                # inflated dropped_pairs to nonsense (108923 against 706 paired) and left the
                # p50/p99 columns computed from the surviving ~0.5% subsample.
                if "Returns" not in detail:          # a CALL
                    open_calls.setdefault(fid, []).append(_ts)
                else:                                 # a RET
                    stack = open_calls.get(fid)
                    if not stack:
                        dropped_pairs += 1           # RET without its CALL: spliced
                    else:
                        call_ts = stack.pop()        # LIFO: pairs with the innermost open CALL
                        if dm:                        # SystemView's own number, when given
                            func_durs.setdefault(fid, []).append(
                                float(dm.group(1)) * (1e-6 if dm.group(2) == "us" else 1e-3))
                        elif freq and call_ts is not None and _ts is not None:
                            # Same quantity from the same capture: the recorded timestamps
                            # reproduce SystemView's annotated durations to 167.998 vs 168.000
                            # ticks/us on stm32f407disco (264 annotated returns cross-checked).
                            func_durs.setdefault(fid, []).append((_ts - call_ts) / freq)
        elif event.startswith("Stop Marker "):
            m = mark_stop_re.match(event)
            dm = ran_re.search(detail)
            if m and dm:
                marker_id = int(m.group(1), 16)
                marker_durs.setdefault(marker_id, []).append(
                    float(dm.group(1)) * (1e-6 if dm.group(2) == "us" else 1e-3))

    # Any CALL still open at end of stream never got a matching RET (recording simply ended
    # mid-call) -- count it as a lost pair rather than silently dropping it uncounted.
    dropped_pairs += sum(len(v) for v in open_calls.values())

    # Close the final scheduling segment: whoever was running at the last transition kept the
    # CPU until at least the last observed event -- beyond that is unobserved, so accounting
    # stops there rather than extrapolating.
    if sched_last_ts is not None and valid_ts and valid_ts[-1] > sched_last_ts:
        _runner = sched_isrs[-1] if sched_isrs else sched_task
        if _runner is not None:
            sched_segments.append((sched_last_ts, valid_ts[-1], _runner))

    # cpu_pct_workload: busy share per context over the workload window. Anchor priority:
    # explicit --window, then an app's own Start/Stop Marker pair (the target declaring "the
    # test runs here"), then the span of tud_cdc_read events (the CI throughput workload's
    # definitional footprint). Busy time is summed from the scheduling segments above and
    # clipped to the window -- by construction it cannot exceed the window.
    workload_anchor = None
    w0 = w1 = None
    if args.window and valid_ts and freq:
        try:
            t0_s, t1_s = (float(x) for x in args.window.split(":", 1))
        except ValueError:
            sys.exit(f"error: --window expects T0:T1 in seconds, got {args.window!r}")
        # W6: the two fallback anchors below both carry their own last>first check
        # (mark_last > mark_first / wl_last > wl_first) -- this explicit path had none, so
        # e.g. --window 5:2 silently produced a negative workload_window_s.
        if t1_s <= t0_s:
            sys.exit("error: --window end must be after start")
        w0 = valid_ts[0] + int(t0_s * freq)
        w1 = valid_ts[0] + int(t1_s * freq)
        workload_anchor = "cli"
    elif mark_first is not None and mark_last is not None and mark_last > mark_first:
        w0, w1, workload_anchor = mark_first, mark_last, "markers"
    elif wl_first is not None and wl_last is not None and wl_last > wl_first:
        w0, w1, workload_anchor = wl_first, wl_last, "cdc-read-span"
    workload_window_s = None
    contexts_workload = []
    if w0 is not None and freq:
        workload_window_s = (w1 - w0) / freq
        busy = {}
        for seg0, seg1, ctx in sched_segments:
            lo, hi = max(seg0, w0), min(seg1, w1)
            if hi > lo:
                busy[ctx] = busy.get(ctx, 0) + (hi - lo)
        span = w1 - w0
        contexts_workload = [
            {"name": ctx, "busy_ms": round(t / freq * 1e3, 3),
             "cpu_pct_workload": round(t / span * 100, 2)}
            for ctx, t in sorted(busy.items(), key=lambda kv: -kv[1])
        ]

    # W5: a corrupt clock (clock_corrupt, set above) taints these the same way it taints
    # live_window_s -- they are computed from the very same valid_ts/freq. Withhold outright
    # rather than publish numbers derived from timestamps already known to be wrong (repro: a
    # wrapped-timestamp capture showed cpu_pct_workload 100.0 right alongside the "no time base"
    # warning that should have been the tell).
    if clock_corrupt:
        workload_window_s = None
        workload_anchor = None
        contexts_workload = []

    # cpu_pct is SystemView's own CPU Load column, reported as recorded. It used to be
    # recomputed against the window and clamped to 100 -- a derived number wearing the same
    # name as the measured one. If no Init record fixed the timestamp frequency, the decode
    # itself is untrustworthy, so withhold rather than publish a figure derived from a guess.
    if freq is None:
        for c in contexts:
            c["cpu_pct"] = None

    isr_rows = duration_table(isr_runs)
    ready_rows = duration_table(ready_lat)
    func_rows = duration_table(func_durs, lambda fid: TU_SV_FUNC_NAMES.get(fid, f"id{fid}"))
    marker_rows = duration_table(marker_durs, lambda mid: f"marker{mid}")
    stack_rows = [{"name": task, "bytes_used": used} for task, used in stack_used.items()]
    if heap_last:
        net_bytes, allocs, frees = (int(x) for x in heap_last.groups())
        heap = {"allocs": allocs, "frees": frees, "net_bytes": net_bytes}
    else:
        heap = None

    if args.json:
        print(json.dumps({
            "contexts": contexts,
            "isr": isr_rows,
            "ready_run": ready_rows,
            "functions": func_rows,
            "markers": marker_rows,
            "stack": stack_rows,
            "heap": heap,
            "overflow": overflow,
            "dropped_pairs": dropped_pairs,
            "workload_window_s": round(workload_window_s, 2) if workload_window_s is not None else None,
            "workload_anchor": workload_anchor,
            "contexts_workload": contexts_workload,
            "live_window_s": round(live_window_s, 2) if live_window_s is not None else None,
            "warnings": warnings,
        }, indent=1))
        return

    print(f"\nlive_window_s: {live_window_s:.2f}" if live_window_s is not None
          else "\nlive_window_s: n/a (no time base)")
    if workload_window_s is not None:
        print(f"workload window ({workload_anchor}): {workload_window_s:.2f} s")
        for c in contexts_workload:
            print(f"  {c['name']:<14} {c['cpu_pct_workload']:>6.2f} %  busy {c['busy_ms']:.3f} ms")
    print(f"overflow events: {overflow}"
          + (" — DATA LOST: raise SYSVIEW_BUFFER_SIZE or lighten tracing" if overflow else ""))
    print(f"dropped_pairs: {dropped_pairs}")
    for w in warnings:
        print(f"warning: {w}")
    if isr_rows:
        print_duration_table("ISR duration", isr_rows)
    if ready_rows:
        print_duration_table("ready->run", ready_rows)

    if stack_rows:
        print(f"\n{'stack high-water':<20} {'bytes_used':>10}")
        for r in stack_rows:
            print(f"{r['name']:<20} {r['bytes_used']:>10}")

    if heap:
        print(f"\nheap: allocs={heap['allocs']} frees={heap['frees']} net_bytes={heap['net_bytes']}")
    else:
        print("\nheap: no events (static allocation build)")

    if func_rows:
        print_duration_table("function", func_rows, width=20)
    if marker_rows:
        print_duration_table("marker", marker_rows, width=20)


if __name__ == "__main__":
    main()
