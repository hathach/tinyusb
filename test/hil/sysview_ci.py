#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""SystemView CI: per-board capture on the HIL rig + PR performance report.

report: pure -- two directories of sysview-<board>.json in, markdown out.
capture: flash a SYSVIEW build, drive the workload, RTT-capture, decode. Capture and decode
run agentrc's rtt.py and target-debug sysview scripts from lib/agentrc at the get_deps.py pin
(agentrc's skills/target-debug/sysview.md is their manual).
"""
import argparse, glob, html, json, os, re, signal, sys

SCRIPTS = "skills/target-debug/scripts/"
# lib/SystemView takes the first free up-buffer; 0 is the RTT terminal
SYSVIEW_CHANNEL = 1
# absolute event id -> name for sysview_report.py --event-map; test_hil_sysview_ci checks it
# against tusb_debug.h's TU_SV_ID_* enum
EVENT_MAP = os.path.join(os.path.dirname(os.path.abspath(__file__)), "helper", "sysview_events.json")
WORKLOAD_EVENT_NAME = "tud_cdc_read"

def workload_event():
    """The workload window's anchor: the span of tud_cdc_read activity, the throughput test's
    own footprint."""
    with open(EVENT_MAP) as f:
        return next(int(i) for i, n in json.load(f).items() if n == WORKLOAD_EVENT_NAME)

GATE_MIN_N = 50
# A tail percentile needs roughly n >= 1/(1-p) samples to estimate anything: 100 for p99.
P99_MIN_N = 100
CHART_MAX_BARS = 8
HEADER = "## ⚡ SystemView performance — HIL"
LEGEND = ("<sub>**Legend** — **p50/p99**: median / 99th-percentile duration over all calls "
          "in the capture window (µs; p50 = typical cost, p99 = tail latency). **Δ**: "
          "change vs base branch; **−** is faster/better; **+∞%** marks a jump from an "
          "(effectively) zero base -- not literally infinite, just too big to express as a "
          "normal percentage; **gone** marks a row base had that PR does not (work that used to "
          "run no longer does, or was renamed -- investigate before merging). **pt**: "
          "percentage points. **CPU load, workload win**: context's busy share summed from the raw scheduling events over the workload window (the span of `tud_cdc_read` activity -- the throughput test's own footprint); falls back to the whole-recording share, labelled as such. "
          "**stack high-water**: peak bytes of stack used. Function rows/bars are ordered by "
          "CPU occupancy (calls × p50) in the PR capture, hottest first; base-only rows sort "
          "after them. **– ⚠︎**: metric withheld — RTT ring overflowed (`overflow N`), no "
          "timestamp frequency (`no time base`), or too few samples (`n<50` for p50; p99 needs "
          "`n>100` for the tail estimate to be statistically meaningful, so it can show `n=<n>` "
          "even on a row whose p50 is shown); withheld beats wrong. Durations read "
          "**p50 / p99 / max**, all as captured — spliced CALL/RET pairs are discarded "
          "upstream and counted in `dropped_pairs`. Capture: OpenOCD RTT "
          "@1 ms poll — p50/p99 match the J-Link recorder within ~1%.</sub>")

def load_set(d):
    out = {}
    # recursive: a base download puts each artifact in its own subdirectory
    for p in glob.glob(os.path.join(d, "**", "sysview-*.json"), recursive=True):
        try:
            with open(p) as f:
                j = json.load(f)
            out[j["board"]] = j
        except (OSError, ValueError, KeyError):
            continue
    return out

def gate(side, name_metrics):
    """None if usable, else the withhold reason string."""
    if side is None:
        return "new"
    if not side.get("metrics"):
        # a malformed capture JSON must not abort the whole report
        return "failed"
    if (side.get("capture") or {}).get("workload_ok") is False:
        return "workload"
    if side["metrics"].get("live_window_s") is None:
        return "no time base"  # sysview_report.py couldn't establish a timestamp frequency
    overflow = side["metrics"].get("overflow", 0)
    if overflow is None:
        return "loss unknown"  # sysview_report.py had no events to count it from
    if overflow > 0:
        return f"overflow {overflow}"
    if name_metrics is not None and name_metrics.get("n", 0) < GATE_MIN_N:
        return f"n={name_metrics.get('n', 0)}"
    return None

def gate_p99(side, name_metrics):
    """Like gate(), but withholds p99 whenever n <= P99_MIN_N even though p50 (n >= GATE_MIN_N)
    is shown."""
    reason = gate(side, name_metrics)
    if reason is not None:
        return reason
    n = name_metrics.get("n", 0) if name_metrics else 0
    if n <= P99_MIN_N:
        return f"n={n}"
    return None

def by_name(mlist):
    return {m["name"]: m for m in (mlist or [])}

def fmt_delta(b, p):
    if b is None and p is None:
        return "—"
    if b is None:
        return "new"
    if p is None:
        # base had this row, PR does not -- a real disappearance, not "no change" (the
        # bare "—" a normal near-zero delta would show).
        return "**gone**"
    if b == 0:
        # a base that rounds to 0 has no ratio; a nonzero PR against it is still a jump
        return "—" if p == 0 else "**+∞%**"
    d = (p - b) / b * 100
    if abs(d) < 1.0:
        return "—"
    mark = " ✅" if d < 0 else ""
    bold = ("**", "**") if abs(d) >= 5 else ("", "")
    return f"{bold[0]}{d:+.1f}%{bold[1]}{mark}"

def metric_cell(side_json, m, fmt):
    reason = gate(side_json, m)
    if reason is None:
        return fmt(m), None
    if reason in ("new", "failed"):
        return None, reason
    return f"– ⚠︎ {reason}", reason

def missing_reason(side_j, key, absent="gone"):
    """Why a row the other side has is absent here: `absent` ("gone" for the PR, "new" for the
    base) only from a usable capture that measured the table (sysview_report.py nulls a table it
    cannot support)."""
    reason = gate(side_j, None)
    if reason:
        return reason
    return "not measured" if ((side_j or {}).get("metrics") or {}).get(key) is None else absent

def row(label, bmap, pmap, name, base_j, pr_j, table, key="p50_us", unit=" µs"):
    bm, pm = bmap.get(name), pmap.get(name)
    if pm is None and bm is None:
        return None
    fmt = lambda m: f"{m[key]:.1f}{unit}" if m else "—"
    def cell(side_j, m, absent):
        if m:
            return metric_cell(side_j, m, fmt)
        reason = missing_reason(side_j, table, absent)
        return ("—", None) if reason == absent else (f"– ⚠︎ {reason}", reason)
    bcell, bgate = cell(base_j, bm, "new")
    pcell, pgate = cell(pr_j, pm, "gone")
    if bgate or pgate:
        delta = "—"
        bcell, pcell = bcell or "—", pcell or "—"
    else:
        delta = fmt_delta(bm and bm[key], pm and pm[key])
    return f"| {label} | {bcell} | {pcell} | {delta} |"

def gated_pair(base_j, pr_j, b, p, fmt, delta_fn, table):
    """Render (base_cell, pr_cell, delta) for a base/PR value pair that has no per-sample "n"
    of its own (contexts' cpu_pct, stack's bytes_used) -- gated through gate() the same as every
    duration metric, passing name_metrics=None so gate()'s n<GATE_MIN_N check no-ops.
    Mirrors row()/metric_cell()'s Δ precedence: a gate reason on either side wins over "new",
    which wins over an actual delta.

    p is None: this name is base-only (union iteration in board_section() found no PR-side
    entry at all) -- gate(pr_j, None) can't tell that apart from "PR side is fine, this metric
    just wasn't asked for", so it must be special-cased here rather than left to fall through."""
    if p is None:
        breason = gate(base_j, None)
        pr_missing = missing_reason(pr_j, table)
        if pr_missing != "gone":
            return (fmt(b) if breason is None else f"– ⚠︎ {breason}"), f"– ⚠︎ {pr_missing}", "—"
        if breason is None:
            return fmt(b), "—", "**gone**"
        return f"– ⚠︎ {breason}", "—", "—"  # base itself unusable -- can't confirm "gone" either
    preason = gate(pr_j, None)
    breason = missing_reason(base_j, table, "new") if b is None else gate(base_j, None)
    pcell = fmt(p) if preason is None else ("—" if preason == "new" else f"– ⚠︎ {preason}")
    if breason is None:
        bcell = fmt(b)
    elif breason == "new":
        bcell = "—"
    else:
        bcell = f"– ⚠︎ {breason}"
    if preason or (breason not in (None, "new")):
        delta = "—"
    elif breason == "new":
        delta = "new"
    else:
        delta = delta_fn(b, p)
    return bcell, pcell, delta

def board_section(name, base_j, pr_j, base_dropped=None):
    pm_all = pr_j.get("metrics") or {}
    live_window_s = pm_all.get("live_window_s", (pr_j.get("capture") or {}).get("live_window_s"))
    heading = f"### {name}" + (f" — live {live_window_s:.1f} s" if live_window_s is not None else "")
    if base_dropped:
        # The report header states the PR-side config only, so a dropped baseline has to say
        # here why every row reads "new" -- otherwise it looks like base never captured.
        heading += f" — base dropped, capture config changed ({', '.join(base_dropped)})"
    lines = [heading, ""]
    if pr_j.get("error"):
        # one inert line: build and tool output must not add headings, fences or HTML
        err = html.escape(re.sub(r"\s+", " ", str(pr_j["error"])).strip()).replace("`", "&#96;")
        return "\n".join(lines + [f"capture failed: {err}", ""])
    if not pm_all:
        # a malformed capture JSON must not abort the whole report
        return "\n".join(lines + ["capture failed: missing metrics", ""])
    bm_all = (base_j or {}).get("metrics") or {}
    # reported, not gated: e.g. the decoder's timing warnings (agentrc sysview.md)
    park_error = (pr_j.get("capture") or {}).get("park_error")
    warnings = (pm_all.get("warnings") or []) + ([park_error] if park_error else [])
    warnings = [f"> ⚠︎ {w}" for w in warnings]
    lines += warnings + [""] if warnings else []  # a blank line ends the quote before the table
    lines += ["| metric | base | PR | Δ |", "|---|---:|---:|---:|"]
    bisr, pisr = by_name(bm_all.get("isr")), by_name(pm_all.get("isr"))
    # both sides' rows: one only base has (work that no longer runs) still renders
    for iname in sorted(set(pisr) | set(bisr)):
        pm, bm = pisr.get(iname), bisr.get(iname)
        # p50 and p99 are gated independently -- p99 needs n>P99_MIN_N (see its comment)
        # for the tail estimate to be meaningful, a stricter bar than p50's GATE_MIN_N, so
        # p99 can be withheld on a row whose p50 still shows. "gone"/"new" (pm/bm respectively
        # absent) are per-half reasons here too, same precedence as an ordinary gate() withhold.
        preason50 = missing_reason(pr_j, "isr") if pm is None else gate(pr_j, pm)
        preason99 = missing_reason(pr_j, "isr") if pm is None else gate_p99(pr_j, pm)
        breason50 = missing_reason(base_j, "isr", "new") if bm is None else gate(base_j, bm)
        breason99 = missing_reason(base_j, "isr", "new") if bm is None else gate_p99(base_j, bm)
        def _half(m, reason, key):
            if reason is None:
                return f"{m[key]:.1f}"
            return "—" if reason in ("new", "gone") else f"– ⚠︎ {reason}"
        def _cell(m, r50, r99):
            # spliced CALL/RET pairs are already dropped by sysview_report.py (dropped_pairs)
            return (f"{_half(m, r50, 'p50_us')} / {_half(m, r99, 'p99_us')} / "
                    f"{_half(m, r50, 'max_us')} µs")
        def _one_delta(preason, breason, bm_, pm_, key):
            if preason == "gone" and breason is None:
                return "**gone**"
            if preason or (breason not in (None, "new")):
                return "—"
            if breason == "new":
                return "new"
            return fmt_delta(bm_[key], pm_[key])
        def _delta():
            d50 = _one_delta(preason50, breason50, bm, pm, 'p50_us')
            d99 = _one_delta(preason99, breason99, bm, pm, 'p99_us')
            return f"{d50} / {d99}"
        lines.append(f"| {iname} p50 / p99 | {_cell(bm, breason50, breason99)} | "
                     f"{_cell(pm, preason50, preason99)} | {_delta()} |")
    bfn, pfn = by_name(bm_all.get("functions")), by_name(pm_all.get("functions"))
    # PR-side rows keep their existing CPU-occupancy order; base-only rows have no PR
    # occupancy to rank by, so they sort alphabetically after every PR-ranked row.
    order = sorted(pfn, key=lambda k: pfn[k]["n"] * pfn[k]["p50_us"], reverse=True)
    order += sorted(set(bfn) - set(pfn))
    for fname in order:
        r = row(f"`{fname}` p50", bfn, pfn, fname, base_j, pr_j, "functions")
        if r: lines.append(r)
    # CPU load rows prefer cpu_pct_workload -- busy time summed from the raw scheduling
    # events over the workload window (span of tud_cdc_read activity), the number that
    # answers "load while the throughput test ran". Falls back to the
    # whole-recording cpu_pct, labelled as such, when no workload window was found.
    bwl, pwl = by_name(bm_all.get("contexts_workload")), by_name(pm_all.get("contexts_workload"))
    use_wl = bool(pwl)
    bctx, pctx = (bwl, pwl) if use_wl else \
        (by_name(bm_all.get("contexts")), by_name(pm_all.get("contexts")))
    key = "cpu_pct_workload" if use_wl else "cpu_pct"
    label = "CPU load, workload win" if use_wl else "CPU load, whole recording"
    for cname in sorted(set(pctx) | set(bctx)):
        if cname.lower() in ("usbd", "usbh", "isr") or cname.lower().startswith("isr "):
            b, c = bctx.get(cname), pctx.get(cname)
            bcell, pcell, d = gated_pair(
                base_j, pr_j, b, c, lambda m: "—" if m[key] is None else f"{m[key]:.1f} %",
                # sysview_report.py nulls a load it cannot compute
                lambda b_, p_: ("—" if None in (b_[key], p_[key]) or abs(p_[key] - b_[key]) < 0.1
                                else f"{p_[key]-b_[key]:+.1f} pt"),
                "contexts_workload" if use_wl else "contexts")
            lines.append(f"| {label} ({cname}) | {bcell} | {pcell} | {d} |")
    bst, pst = by_name(bm_all.get("stack")), by_name(pm_all.get("stack"))
    for sname in sorted(set(pst) | set(bst)):
        b, sm = bst.get(sname), pst.get(sname)
        bcell, pcell, d = gated_pair(
            base_j, pr_j, b, sm, lambda m: f"{m['bytes_used']} B",
            lambda b_, p_: ("—" if b_['bytes_used'] == p_['bytes_used'] else
                            f"{p_['bytes_used']-b_['bytes_used']:+d} B"),
            "stack")
        lines.append(f"| `{sname}` stack high-water | {bcell} | {pcell} | {d} |")
    # chart: occupancy order, PR-gate-eligible only, capped; base series only if
    # every charted function is ungated on the base side too (no holes in a mermaid series).
    # `c in pfn` excludes base-only rows: the chart is explicitly "hot functions ... in the
    # PR capture" (see LEGEND), and a base-only row has no PR bar to plot.
    eligible = [c for c in order if c in pfn and gate(pr_j, pfn[c]) is None][:CHART_MAX_BARS]
    if eligible:
        base_ok = (gate(base_j, None) is None and
                   all(c in bfn and gate(base_j, bfn[c]) is None for c in eligible))
        short = [re.sub(r'^(tud_|tuh_|dcd_|hcd_)', '', c) for c in eligible]
        pv = [f"{pfn[c]['p50_us']:.1f}" for c in eligible]
        bars = [f"    bar [{', '.join(pv)}]"]
        values = list(pv)
        if base_ok:
            bv = [f"{bfn[c]['p50_us']:.1f}" for c in eligible]
            bars = [f"    bar [{', '.join(bv)}]"] + bars
            values += bv
        peak = max(float(v) for v in values)
        # every charted p50 rounding to 0.0 gives mermaid a "0 --> 0" axis, which fails to
        # render and leaves a raw, broken code fence in the PR comment -- skip the chart
        # entirely rather than publish that (the table above already shows every value).
        if peak > 0:
            ymax = max(peak * 1.3, 0.1)
            lines += ["", "```mermaid", "xychart-beta",
                      '    title "hot functions p50 µs (base vs PR)"',
                      f"    x-axis [{', '.join(short)}]",
                      f'    y-axis "µs" 0 --> {ymax:.1f}'] + bars + ["```"]
    return "\n".join(lines) + "\n"

CAPTURE_CFG_KEYS = ("example", "workload", "duration_s")

def capture_cfg_changes(base_j, pr_j):
    """Config differences that make a baseline incomparable: a PR that changes a board's
    sysview example, workload or duration_s measures different work, so base-vs-PR deltas
    would be noise dressed up as a regression."""
    if not base_j:
        return []
    return [f"{k} `{base_j.get(k)}`→`{pr_j.get(k)}`"
            for k in CAPTURE_CFG_KEYS if base_j.get(k) != pr_j.get(k)]

def report(base_dir, pr_dir):
    pr = load_set(pr_dir)
    if not pr:
        return ""
    base = load_set(base_dir)
    if base and not (set(base) & set(pr)):
        return ""
    boards = sorted(pr)
    any_pr = next(iter(pr.values()))
    # .get throughout: one malformed capture JSON must not abort the whole report -- the same
    # rule gate()/board_section() already follow one layer down.
    base_commit = next(iter(base.values())).get("commit", "?") if base else "(none)"
    head = (f"*{any_pr.get('example', '?')} `SYSVIEW=4`, workload `{any_pr.get('workload', '?')}` "
            f"{any_pr.get('duration_s', '?')} s, OpenOCD rtt @1 ms · "
            f"base `{base_commit}` → PR `{any_pr.get('commit', '?')}`*")
    parts = [HEADER, "", head, ""]
    for b in boards:
        changed = capture_cfg_changes(base.get(b), pr[b])
        parts.append(board_section(b, None if changed else base.get(b), pr[b], changed))
    parts.append(LEGEND)
    return "\n".join(parts) + "\n"

# ---------------------------------------------------------------- capture
def select_boards(cfg, board_args):
    picked = [b for b in cfg["boards"] if "sysview" in b]
    if board_args:
        picked = [b for b in picked if b["name"] in set(board_args)]
    return picked

def capture_ocd_args(board):
    args = board["sysview"].get("ocd_args") or (
        board["flasher"]["args"] if board["flasher"]["name"] == "openocd" else None)
    if not args:
        raise ValueError(f"{board.get('name')}: non-openocd flasher and no sysview.ocd_args")
    return args  # shell-quoted, as rtt.py's --cfg takes it

def session_resets(board):
    """Whether the capture session issues its own `reset run`. A board flagged
    sysview.attach_only attaches without one (metro_m4_express: after openocd's reset on
    atsame5x under an attached debugger the core stays held, so the RTT control block never
    appears); the flasher's post-flash reset already supplied the fresh boot."""
    return not board["sysview"].get("attach_only")

def board_result(board, commit, metrics=None, capture_info=None, error=None):
    sv = board["sysview"]
    return {"board": board["name"], "commit": commit,
            "example": sv["example"], "workload": sv["workload"],
            "duration_s": sv["duration_s"],
            "capture": capture_info or {}, "metrics": metrics, "error": error}

def _workload_cdc_burst(node, duration_s):
    """Returns True if traffic ran for the full window, False if the serial link died early
    (device dropped off the bus) or the device stayed enumerated but echoed nothing back, or
    stopped echoing -- never raises."""
    import serial, time
    # write_timeout: without it, a device that stops draining its OUT endpoint blocks s.write()
    # forever, wedging PHASE 2 while the board flock is held -- the same failure mode hil_test.py
    # guards against in open_serial_dev() (SERIAL_WRITE_TIMEOUT, default 10s, sized for
    # whole-firmware-image transfers there). Hardcoded here rather than imported from hil_test:
    # this workload only ever writes 64-byte micro-bursts, so a much shorter deadline catches a
    # wedge faster, and importing hil_test would pull in its pymtp dependency for one constant.
    s = serial.Serial(node, 115200, timeout=0.02, write_timeout=2)
    end = time.monotonic() + duration_s
    echoed = 0
    while time.monotonic() < end:
        t, burst = time.monotonic(), 0
        while time.monotonic() - t < 0.30 and time.monotonic() < end:
            try:
                s.write(b"x" * 64); burst += len(s.read(64))
            except Exception:
                s.close()
                return False
        # a whole burst window without an echo after earlier ones: the device stopped echoing
        # (a window the deadline cut short may end before its echo arrives)
        if not burst and echoed and time.monotonic() - t >= 0.30:
            s.close()
            return False
        echoed += burst
        time.sleep(min(1.0, max(0, end - time.monotonic())))
    s.close()
    # a device that stays enumerated but echoes nothing gives a capture of an idle bus
    return echoed > 0

WORKLOADS = {"cdc_burst": _workload_cdc_burst,
             "idle": lambda node, duration_s: __import__("time").sleep(duration_s) or True}

def _err_excerpt(text, head=500, tail=500):
    """Excerpt a long error from BOTH ends. cmake prints the diagnosis first and the
    call stack last, ninja the reverse -- keeping only one end drops the useful half
    (a tail-only slice reported 'eCache.txt' plus a bare call stack for a missing
    hw/mcu dependency)."""
    t = (text or "").strip()
    if len(t) <= head + tail:
        return t
    return f"{t[:head]}\n[...{len(t) - head - tail} chars omitted...]\n{t[-tail:]}"

def build_one(board, repo_root):
    """PHASE 1 (no board lock): cmake configure + build. Returns (fw_path, error) --
    fw_path INCLUDES the flasher's extension. Never raises."""
    import subprocess, hil_flash
    sv, name = board["sysview"], board["name"]
    try:
        exdir = os.path.join(repo_root, "examples", sv["example"])
        bdir = os.path.join(repo_root, "examples", f"cmake-build-sysview-{name}")
        buf = [f"-DSYSVIEW_BUFFER_SIZE={sv['buffer']}"] if "buffer" in sv else []
        for cmd in ([ "cmake", "-B", bdir, f"-DBOARD={name}", "-G", "Ninja",
                      "-DCMAKE_BUILD_TYPE=MinSizeRel", "-DSYSVIEW=4", *buf, "." ],
                    [ "cmake", "--build", bdir ]):
            # ninja prints the compiler's diagnostics on stdout, cmake its configure errors on stderr
            r = subprocess.run(cmd, cwd=exdir, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               text=True, timeout=1800)
            if r.returncode:
                return None, f"build failed: {_err_excerpt(r.stdout)}"
        # Both JLinkExe's `loadfile` and OpenOCD's `program` infer the image format from
        # the extension, so the flasher needs the real filename: an extension-less path
        # dies with "File is of unknown / unsupported format" after taking the board lock.
        base = os.path.basename(sv["example"])
        ext = hil_flash.FLASHER_SUFFIX.get(board["flasher"]["name"].lower(), ".elf")
        fw = os.path.join(bdir, base + ext)
        if not os.path.exists(fw):
            return None, f"build produced no {ext}: {fw}"
        return fw, None
    except Exception as e:
        return None, f"{type(e).__name__}: {e}"

# flash_and_capture_one's bounds besides its flasher calls: nm 30 s, the RTT connect 17 s, the
# enumeration wait 25 s, the workload's last write 2 s, the reader join 5 s, the session close 10 s
CAPTURE_FIXED_S = 90

# sysview_record.py's worst case from SIGTERM to exit, sized for its --gui decode: its error screenshot
# 30 s, sending -quit (connect and banner recv 10 s each), SystemView's -quit QUIT_S 20 s, then SIGTERM
# and SIGKILL to SystemView's and Xvfb's groups GROUP_S 5 s each. The native decode used here exits at once.
DECODE_STOP_S = 30 + 2 * 10 + 20 + 4 * 5

# what PHASE 3 keeps of --budget-s after a board's decode is cut, to stop it, reap it (REAP_GRACE 10 s),
# write its report and exit
DECODE_MARGIN_S = DECODE_STOP_S + 10 + 5

def capture_bound_s(board):
    """Worst-case seconds of flash_and_capture_one: three flasher calls at the harness's
    CMD_TIMEOUT plus the REAP_GRACE each spends when killed (the capture flash; its DAP rescue or
    the capture; the park reflash) plus the capture's own bounds and its workload."""
    from helper import hil_util
    return (3 * (hil_util.CMD_TIMEOUT + hil_util.REAP_GRACE) + CAPTURE_FIXED_S
            + board["sysview"]["duration_s"])

def flash_and_capture_one(board, fw, out_dir):
    """PHASE 2 (under the board lock): flash, RTT-capture raw SVDat bytes over agentrc's
    rtt.py OpenOCD route while the workload runs, then reflash the park image before
    returning. Returns (capture_dict, error); never raises. A failed park reflash is kept as
    capture_dict["park_error"], or appended to the error."""
    name, flasher = board["name"], board["flasher"]
    try:
        import hil_flash
        from helper import agentrc
        rtt = agentrc.load_file("skills/rtt/scripts/rtt.py")
        # device/board_test, the image hil_test.py parks a board with (its USB disabled)
        park = hil_flash.find_firmware(name, "device/board_test", flasher["name"])
    except Exception as e:
        return None, f"{type(e).__name__}: {e}"
    if not park:
        return None, "no park (device/board_test) firmware to restore the board with"
    capture, err = None, None
    try:
        capture, err = _capture(board, fw, out_dir, rtt)
    finally:
        park_err = _reflash_pristine(board, park)
    if park_err:
        if capture:
            capture["park_error"] = park_err
        else:
            err = f"{err}; {park_err}"
    return capture, err

def _capture(board, fw, out_dir, rtt):
    import threading, time
    import hil_flash
    from helper import hil_util
    sv, name, flasher = board["sysview"], board["name"], board["flasher"]
    try:
        r = hil_flash.flash_primitive(flasher["name"])(board, fw)
        if r.returncode:
            # run_cmd merges stderr into stdout and fills it even on timeout (rc=124);
            # without the flasher's own output 'rc=124' cannot distinguish a wedged
            # probe from a dead target.
            out = hil_util.cmd_stdout_text(r.stdout)
            # a wedged RP2040/RP2350 DAP: POR it via the Rescue DP for the jobs that follow
            # (no-op otherwise), as hil_test.py does
            hil_flash.rescue_openocd(board, out)
            out = _err_excerpt(out)
            return None, f"flash failed: rc={r.returncode}" + (f"\n{out}" if out else "")
        node = hil_util.get_serial_dev(board['uid'], 'TinyUSB', 'TinyUSB_Device', 0)
        try:
            # fw carries the flasher's extension, which is not always .elf (lm4flash and
            # esptool take .bin); the RTT control block always comes from the ELF's symbols.
            cb = rtt.nm_rtt_addr(os.path.splitext(fw)[0] + ".elf")
        except SystemExit as e:
            # SystemExit derives from BaseException: uncaught it would abort the whole
            # multi-board run before the boards already captured are written out.
            return None, f"no RTT symbol: {e}"
        raw = os.path.join(out_dir, f"{name}-capture.SVDat")
        # reset_before_attach: SystemView's one-shot Init record (sync + timestamp frequency) is
        # emitted at boot, so the session resets the target itself and drains from byte 0.
        conn = rtt.OpenocdRtt(cfg=capture_ocd_args(board), addr=cb, channel=SYSVIEW_CHANNEL,
                              serial_no=flasher["uid"], vid_pid=flasher.get("vid_pid"),
                              reset_before_attach=session_resets(board))
        workload_ok, wl_err, wl_start = True, None, None
        stop, pump_err = threading.Event(), []
        try:
            with open(raw, "wb") as f:
                def pump():
                    try:
                        while not stop.is_set() and not conn.eof:
                            f.write(conn.read(65536))
                    except Exception as e:
                        pump_err.append(e)
                reader = threading.Thread(target=pump, daemon=True)
                reader.start()
                try:
                    # the session's own reset (when session_resets()) has already happened by
                    # now, so this waits for THIS boot's node, which the workload needs
                    for _ in range(25):
                        if os.path.exists(node): break
                        time.sleep(1)
                    else:
                        return None, "device never enumerated after flash"
                    wl_start = time.monotonic()
                    try:
                        workload_ok = bool(WORKLOADS[sv["workload"]](node, sv["duration_s"]))
                    except Exception as e:
                        workload_ok, wl_err = False, e
                finally:
                    stop.set()
                    reader.join(5)
            if reader.is_alive():
                return None, "RTT capture reader did not stop"
            if pump_err:
                return None, f"RTT capture failed: {type(pump_err[0]).__name__}: {pump_err[0]}"
            if conn.eof:
                return None, "openocd RTT server closed during the capture"
        finally:
            conn.close()
        # A serial death in the first half of the window means the whole capture is
        # mostly idle bus -- fail it outright. A late death still leaves a mostly-live
        # capture; keep it, but flag workload_ok=false so report() gates the metrics.
        if not workload_ok and wl_start is not None and \
                (time.monotonic() - wl_start) < sv["duration_s"] / 2:
            return None, f"workload died: {wl_err or 'stopped early'}"
        return {"raw": raw, "workload_ok": workload_ok}, None
    except Exception as e:
        return None, f"{type(e).__name__}: {e}"

def _stop_decoder(p):
    """SIGTERM first: sysview_record.py's signal handler records the failure (and under --gui closes
    the SystemView and Xvfb it started), which a SIGKILL would skip. Then SIGKILL to whatever of the
    decoder's own group outlived the grace."""
    import subprocess
    p.terminate()
    try:
        p.communicate(timeout=DECODE_STOP_S)
        return
    except subprocess.TimeoutExpired:
        pass
    # p.pid still names the group: the wrapper is unreaped, or terminate()'s poll() reaped it
    # and a member of its group still holds the pipes
    from helper import hil_util
    try:
        os.killpg(p.pid, signal.SIGKILL)
    except OSError:
        pass
    try:
        p.communicate(timeout=hil_util.REAP_GRACE)
    except subprocess.TimeoutExpired:
        pass  # a member stuck in D state still holds the pipes: abandoned, as hil_process does

def decode_one(board, commit, out_dir, capture, deadline=None):
    """PHASE 3 (no board lock): decode the raw SVDat capture with agentrc's sysview scripts
    and report it, each step cut at the time.monotonic() deadline if one is given. Never raises."""
    import subprocess, time
    from helper import agentrc
    sv, name = board["sysview"], board["name"]

    def run(cmd, timeout):
        cut_by_budget = False
        if deadline is not None:
            left = deadline - time.monotonic()
            if left <= 0:
                raise TimeoutError("decode skipped: --budget-s spent")
            cut_by_budget = left < timeout
            timeout = min(timeout, left)
        # its own session, so the final SIGKILL reaches its children but not this script
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                             start_new_session=True)
        try:
            out, err = p.communicate(timeout=timeout)
        except subprocess.TimeoutExpired:
            _stop_decoder(p)
            raise TimeoutError(f"decode timed out after {timeout:.0f} s"
                               + (" (--budget-s)" if cut_by_budget else ""))
        except BaseException:
            _stop_decoder(p)
            raise
        return subprocess.CompletedProcess(cmd, p.returncode, out, err)
    try:
        dec = os.path.join(out_dir, f"{name}-decoded")
        r = run([sys.executable, agentrc.path_of(SCRIPTS + "sysview_record.py"),
                 "--from-raw", capture["raw"], "--out", dec], 900)
        if r.returncode:
            return board_result(board, commit, error=f"record decode failed: {r.stderr[-200:]}")
        r = run([sys.executable, agentrc.path_of(SCRIPTS + "sysview_report.py"),
                 dec, "--json", "--event-map", EVENT_MAP,
                 "--workload-event", str(workload_event())], 600)
        if r.returncode:
            return board_result(board, commit, error=f"decode failed: {r.stderr[-200:]}")
        m = json.loads(r.stdout)
        info = {"live_window_s": m.get("live_window_s"),
                "workload_ok": capture["workload_ok"], "park_error": capture.get("park_error")}
        return board_result(board, commit, metrics=m, capture_info=info)
    except TimeoutError as e:
        return board_result(board, commit, error=str(e))
    except Exception as e:
        return board_result(board, commit, error=f"{type(e).__name__}: {e}")

def _reflash_pristine(board, park):
    """None once the park image is back on the board, else why not; never raises."""
    import hil_flash
    try:
        r = hil_flash.flash_primitive(board["flasher"]["name"])(board, str(park))
        return f"park reflash failed rc={r.returncode}" if r.returncode else None
    except Exception as e:
        return f"park reflash raised {type(e).__name__}: {e}"

def _get_pico_sdk(repo_root):
    """pico-sdk is no get_deps.py dep: CI's get_deps action checks it out into <workspace>/pico-sdk
    and exports PICO_SDK_PATH, which the HIL job never runs. Do the same at the action's ref.
    None once PICO_SDK_PATH is set, else why not; never raises."""
    import subprocess
    if os.environ.get("PICO_SDK_PATH"):
        return None
    try:
        with open(os.path.join(repo_root, ".github", "actions", "get_deps", "action.yml")) as f:
            m = re.search(r"repository:\s*raspberrypi/pico-sdk\s+ref:\s*(\S+)", f.read())
        if not m:
            return "no pico-sdk ref in .github/actions/get_deps/action.yml"
        ref, path = m.group(1), os.path.join(repo_root, "pico-sdk")
        if not os.path.isdir(path):
            cmds = [["git", "clone", "--depth", "1", "--branch", ref,
                     "https://github.com/raspberrypi/pico-sdk.git", path]]
        else:  # a self-hosted runner's workspace can keep an older checkout
            cmds = [["git", "-C", path, "fetch", "--depth", "1", "origin", ref],
                    ["git", "-C", path, "checkout", "-q", "--detach", "FETCH_HEAD"]]
        for cmd in cmds:
            r = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
            if r.returncode:
                return f"pico-sdk {ref}: {' '.join(cmd[:4])} failed: {_err_excerpt(r.stderr)}"
        os.environ["PICO_SDK_PATH"] = path
        return None
    except Exception as e:
        return f"pico-sdk: {type(e).__name__}: {e}"

def _git_commit(repo_root):
    import subprocess
    try:
        r = subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=repo_root,
                            capture_output=True, text=True, timeout=10)
        return r.stdout.strip() if r.returncode == 0 else "unknown"
    except Exception:
        return "unknown"

def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    rp = sub.add_parser("report")
    rp.add_argument("base_dir"); rp.add_argument("pr_dir")
    rp.add_argument("-o", "--out", default="sysview_report.md")
    cp = sub.add_parser("capture")
    cp.add_argument("config")
    cp.add_argument("-b", "--board", action="append", default=[],
                     help="Boards to capture, all sysview-flagged boards if not specified")
    cp.add_argument("--out", default="sysview-out")
    cp.add_argument("--budget-s", type=float, default=None,
                    help="seconds before the caller kills this run: a board's flash and capture start "
                         "only while the rest covers capture_bound_s(), so no kill lands before its park reflash; "
                         "decoding is cut at it too")
    cp.add_argument("--get-deps", action="store_true",
                    help="fetch lib/SystemView and the selected boards' deps first (CI's clean checkout)")
    args = ap.parse_args()
    if args.cmd == "report":
        md = report(args.base_dir, args.pr_dir)
        with open(args.out, "w") as f:
            f.write(md)
        print(f"{'empty (no captures)' if not md else args.out}")
    elif args.cmd == "capture":
        import time
        from helper import hil_lock
        start = time.monotonic()
        try:
            with open(args.config) as f:
                cfg = json.load(f)
        except (OSError, ValueError) as e:
            print(f"error: cannot load {args.config}: {e}", file=sys.stderr)
            sys.exit(1)
        boards = select_boards(cfg, args.board)
        if not boards:
            # not an error: a roster without sysview boards (hfp.json), or none of them selected
            print(f"no sysview board selected in {args.config}, nothing to capture")
            sys.exit(0)
        os.makedirs(args.out, exist_ok=True)
        repo_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        rp2_boards = {b["name"] for b in boards
                      if os.path.isdir(os.path.join(repo_root, "hw", "bsp", "rp2040", "boards", b["name"]))}
        sdk_error = None
        if args.get_deps:
            import subprocess
            subprocess.run([sys.executable, os.path.join(repo_root, "tools", "get_deps.py"), "lib/SystemView",
                            *(a for b in boards for a in ("-b", b["name"]))], cwd=repo_root, check=False)
            if rp2_boards:
                sdk_error = _get_pico_sdk(repo_root)
        commit = _git_commit(repo_root)
        # 3 phases so the board lock, which blocks concurrent CI on that board, is held only
        # for the part that touches hardware (PHASE 2).
        # PHASE 1: build every selected board, no lock held.
        builds = {board["name"]: (None, sdk_error) if sdk_error and board["name"] in rp2_boards
                  else build_one(board, repo_root) for board in boards}
        # PHASE 2: per board, flash + RTT-capture under that board's lock only.
        captures = {}
        for board in boards:
            name = board["name"]
            fw, err = builds[name]
            if err:
                captures[name] = (None, err)
                continue
            if args.budget_s is not None and \
                    time.monotonic() - start + capture_bound_s(board) > args.budget_s:
                captures[name] = (None, f"skipped: less than the {capture_bound_s(board):.0f} s a flash, "
                                        f"capture and park reflash may take is left of the {args.budget_s:.0f} s budget")
                continue
            try:
                lock_fh = hil_lock.acquire_board_lock(name, reason="sysview capture")
            except RuntimeError as e:
                captures[name] = (None, f"board locked: {e}")
                continue
            try:
                captures[name] = flash_and_capture_one(board, fw, args.out)
            finally:
                if lock_fh:
                    hil_lock.release_board_lock(lock_fh)
        def write_result(board, result, capture):
            if result["error"] and capture and capture.get("park_error"):
                result["error"] += f"; {capture['park_error']}"
            with open(os.path.join(args.out, f"sysview-{board['name']}.json"), "w") as f:
                json.dump(result, f)
            return result
        # Every board's report is on disk before decoding starts: a kill that still lands
        # during PHASE 3 leaves each captured board's placeholder, not nothing.
        results = {}
        for board in boards:
            capture, err = captures[board["name"]]
            results[board["name"]] = write_result(board, board_result(board, commit, error=err or
                                                  "decode did not finish before the run was killed"), capture)
        # PHASE 3: decode + report every board with a successful capture, no lock held.
        deadline = None if args.budget_s is None else start + args.budget_s - DECODE_MARGIN_S
        for board in boards:
            name = board["name"]
            capture, err = captures[name]
            result = results[name] if err else \
                write_result(board, decode_one(board, commit, args.out, capture, deadline), capture)
            park_error = result["capture"].get("park_error")
            print(f"{name}: {result['error'] or 'ok'}" + (f" ({park_error})" if park_error else ""))
        sys.exit(0)

if __name__ == "__main__":
    main()
