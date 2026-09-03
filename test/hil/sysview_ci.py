#!/usr/bin/env python3
"""SystemView CI: per-board capture on the HIL rig + PR performance report.

report: pure -- two directories of sysview-<board>.json in, markdown out.
capture (Task 5): flash SYSVIEW build, drive workload, RTT-capture, decode.
Spec: docs/superpowers/specs/2026-07-29-sysview-hil-report-design.md
"""
import argparse, glob, json, os, re, sys

GATE_MIN_N = 50
# N13: wave 3 changed pct() (sysview_report.py) to the conventional nearest-rank index
# int((n-1)*p/100) -- with that formula the p99 index is already strictly below the max index
# for every n>=2 (0.99*(n-1) < n-1 whenever n-1>=1), so P99_MIN_N is no longer about avoiding a
# collision with the max sample (the old "distinct from the max sample" reasoning, true only of
# the pre-wave-3 int(n*p/100) formula). It is kept at 100 for a different, still-valid reason:
# a tail percentile needs roughly n>=1/(1-p) samples to be a meaningful estimate rather than
# effectively picking one extreme sample -- 1/(1-0.99)=100 for p99.
P99_MIN_N = 100
CHART_MAX_BARS = 8
HEADER = "## ⚡ SystemView performance — HIL"
LEGEND = ("<sub>**Legend** — **p50/p99**: median / 99th-percentile duration over all calls "
          "in the capture window (µs; p50 = typical cost, p99 = tail latency). **Δ**: "
          "change vs base branch; **−** is faster/better; **+∞%** marks a jump from an "
          "(effectively) zero base -- not literally infinite, just too big to express as a "
          "normal percentage; **gone** marks a row base had that PR does not (work that used to "
          "run no longer does, or was renamed -- investigate before merging). **pt**: "
          "percentage points. **CPU load, workload win**: context's busy share summed from the raw scheduling events over the workload window (the span of `tud_cdc_read` activity -- the throughput test's own footprint); falls back to SystemView's whole-recording share, labelled as such. "
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
    for p in glob.glob(os.path.join(d, "sysview-*.json")):
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
        # Malformed/older-schema capture JSON: no "metrics" key at all (missing, not just
        # None) used to raise KeyError here on the very next line -- reachable even when the
        # OTHER side is fine, e.g. via the chart's gate(base_j, None) -- and abort the whole
        # report the same way F4's board_section bug did before it was guarded there.
        return "failed"
    if (side.get("capture") or {}).get("workload_ok") is False:
        return "workload"
    if side["metrics"].get("live_window_s") is None:
        return "no time base"  # F3: report.py couldn't establish a timestamp frequency
    if side["metrics"].get("overflow", 0) > 0:
        return f"overflow {side['metrics']['overflow']}"
    if name_metrics is not None and name_metrics.get("n", 0) < GATE_MIN_N:
        return f"n={name_metrics.get('n', 0)}"
    return None

def gate_p99(side, name_metrics):
    """Like gate(), but withholds p99 whenever n<=P99_MIN_N even though p50 (GATE_MIN_N=50) is
    shown (F1). N13: sysview_report.py's pct() now picks the conventional nearest-rank index
    sorted_vals[int((n-1)*p/100)] (wave 3), which is already strictly below the max index for
    every n>=2 -- so this is no longer about avoiding a collision with the max sample. It stays
    a stricter bar than p50's because a 99th-percentile estimate needs roughly n>=1/(1-p)=100
    samples to mean anything -- below that it is effectively just picking one extreme sample and
    calling it "p99". See P99_MIN_N's own comment."""
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
        # N2: base had this row, PR does not -- a real disappearance, not "no change" (the
        # bare "—" a normal near-zero delta would show).
        return "**gone**"
    if b == 0:
        # N3: num_us() rounds to 1 decimal, so a base under 0.05 (µs, or any other unit this is
        # used for) serialises as exactly 0 -- a plain ratio would divide by zero, and the old
        # blanket "b == 0 -> '—'" hid a real jump (base 0.0, PR 5.0) behind a cell that reads as
        # "no change". p == 0 too is a genuine no-op (both sides inactive); only a nonzero PR
        # against a zero base is the regression worth flagging.
        return "—" if p == 0 else "**+∞%**"
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

def gated_pair(base_j, pr_j, b, p, fmt, delta_fn):
    """Render (base_cell, pr_cell, delta) for a base/PR value pair that has no per-sample "n"
    of its own (contexts' cpu_pct, stack's bytes_used) -- gated through gate() the same as every
    duration metric (F2b), passing name_metrics=None so gate()'s n<GATE_MIN_N check no-ops.
    Mirrors row()/metric_cell()'s Δ precedence: a gate reason on either side wins over "new",
    which wins over an actual delta.

    p is None (N2): this name is base-only (union iteration in board_section() found no PR-side
    entry at all) -- gate(pr_j, None) can't tell that apart from "PR side is fine, this metric
    just wasn't asked for", so it must be special-cased here rather than left to fall through."""
    if p is None:
        breason = gate(base_j, None)
        if breason is None:
            return fmt(b), "—", "**gone**"
        return f"– ⚠︎ {breason}", "—", "—"  # base itself unusable -- can't confirm "gone" either
    preason = gate(pr_j, None)
    breason = "new" if b is None else gate(base_j, None)
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

def board_section(name, base_j, pr_j):
    live_window_s = (pr_j.get("capture") or {}).get("live_window_s")
    heading = f"### {name}" + (f" — live {live_window_s:.1f} s" if live_window_s is not None else "")
    lines = [heading, ""]
    if pr_j.get("error"):
        return "\n".join(lines + [f"capture failed: {pr_j['error']}", ""])
    pm_all = pr_j.get("metrics") or {}
    if not pm_all:
        # F4: a malformed/older-schema capture JSON (missing "metrics", no "error" set either)
        # used to raise KeyError here and abort the WHOLE report -- no comment for any board.
        return "\n".join(lines + ["capture failed: missing metrics", ""])
    bm_all = (base_j or {}).get("metrics") or {}
    lines += ["| metric | base | PR | Δ |", "|---|---:|---:|---:|"]
    bisr, pisr = by_name(bm_all.get("isr")), by_name(pm_all.get("isr"))
    # N2: union of both sides, not just pisr -- an ISR/function/context/stack row present only
    # in base (work that used to run and no longer does) must still render, not silently vanish
    # because nothing ever iterates bisr/bfn/bctx/bst on their own. sorted() for a deterministic
    # order (by_name()'s dicts are already insertion-ordered from an alphabetically-sorted JSON
    # list, so this is a no-op for the common both-sides-present case).
    for iname in sorted(set(pisr) | set(bisr)):
        pm, bm = pisr.get(iname), bisr.get(iname)
        # F1: p50 and p99 are gated independently -- p99 needs n>P99_MIN_N (see its comment,
        # N13) for the tail estimate to be meaningful, a stricter bar than p50's GATE_MIN_N, so
        # p99 can be withheld on a row whose p50 still shows. "gone"/"new" (pm/bm respectively
        # absent) are per-half reasons here too, same precedence as an ordinary gate() withhold.
        preason50 = "gone" if pm is None else gate(pr_j, pm)
        preason99 = "gone" if pm is None else gate_p99(pr_j, pm)
        breason50 = "new" if bm is None else gate(base_j, bm)
        breason99 = "new" if bm is None else gate_p99(base_j, bm)
        def _half(m, reason, key):
            if reason is None:
                return f"{m[key]:.1f}"
            return "—" if reason in ("new", "gone") else f"– ⚠︎ {reason}"
        def _cell(m, r50, r99):
            # max is shown, not hidden: it is a real measured duration. The exporter's spliced
            # CALL/RET pairs (the reason it used to be suppressed) are discarded in
            # sysview_report.py and counted as dropped_pairs, so what survives is measured.
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
    # PR-side rows keep their existing CPU-occupancy order; base-only rows (N2) have no PR
    # occupancy to rank by, so they sort alphabetically after every PR-ranked row.
    order = sorted(pfn, key=lambda k: pfn[k]["n"] * pfn[k]["p50_us"], reverse=True)
    order += sorted(set(bfn) - set(pfn))
    for fname in order:
        r = row(f"`{fname}` p50", bfn, pfn, fname, base_j, pr_j)
        if r: lines.append(r)
    # CPU load rows prefer cpu_pct_workload -- busy time summed from the raw scheduling
    # events over the workload window (span of tud_cdc_read activity), the number that
    # answers "load while the throughput test ran". Falls back to SystemView's
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
                base_j, pr_j, b, c, lambda m: f"{m[key]:.1f} %",
                lambda b_, p_: (f"{p_[key]-b_[key]:+.1f} pt"
                                if abs(p_[key] - b_[key]) >= 0.1 else "—"))
            lines.append(f"| {label} ({cname}) | {bcell} | {pcell} | {d} |")
    bst, pst = by_name(bm_all.get("stack")), by_name(pm_all.get("stack"))
    for sname in sorted(set(pst) | set(bst)):
        b, sm = bst.get(sname), pst.get(sname)
        bcell, pcell, d = gated_pair(
            base_j, pr_j, b, sm, lambda m: f"{m['bytes_used']} B",
            lambda b_, p_: ("—" if b_['bytes_used'] == p_['bytes_used'] else
                            f"{p_['bytes_used']-b_['bytes_used']:+d} B"))
        lines.append(f"| `{sname}` stack high-water | {bcell} | {pcell} | {d} |")
    # chart: occupancy order, PR-gate-eligible only, capped; base series only if
    # every charted function is ungated on the base side too (no holes in a mermaid series).
    # `c in pfn` excludes base-only rows (N2): the chart is explicitly "hot functions ... in the
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
        # N7: every charted p50 rounding to 0.0 gives mermaid a "0 --> 0" axis, which fails to
        # render and leaves a raw, broken code fence in the PR comment -- skip the chart
        # entirely rather than publish that (the table above already shows every value).
        if peak > 0:
            ymax = max(peak * 1.3, 0.1)
            lines += ["", "```mermaid", "xychart-beta",
                      '    title "hot functions p50 µs (base vs PR)"',
                      f"    x-axis [{', '.join(short)}]",
                      f'    y-axis "µs" 0 --> {ymax:.1f}'] + bars + ["```"]
    return "\n".join(lines) + "\n"

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
        parts.append(board_section(b, base.get(b), pr[b]))
    parts.append(LEGEND)
    return "\n".join(parts) + "\n"

# ---------------------------------------------------------------- capture
def select_boards(cfg, board_args):
    picked = [b for b in cfg["boards"] if "sysview" in b]
    if board_args:
        picked = [b for b in picked if b["name"] in set(board_args)]
    return picked

def capture_ocd_args(board):
    # Only the plain "openocd" flasher name exists since upstream #3804 folded
    # openocd_wch away (openocd_adi likewise); the rig binary is the unified fork.
    args = board["sysview"].get("ocd_args") or (
        board["flasher"]["args"] if board["flasher"]["name"] == "openocd" else None)
    if not args:
        raise ValueError(f"{board.get('name')}: non-openocd flasher and no sysview.ocd_args")
    return [a.strip('"') for a in re.findall(r'"[^"]*"|\S+', args)]

def is_wch_board(board):
    """WCH parts are detected by their target config -- the only spelling left after
    #3804 folded the openocd_wch flasher name into openocd."""
    return "wch-riscv" in (board["flasher"].get("args") or "")

def session_resets(board):
    """Whether the capture session issues its own `reset run`. Two board classes must
    attach without one: WCH -- under SDI the target never comes back and USB never
    re-enumerates -- and boards flagged sysview.attach_only (metro_m4_express: after
    openocd's reset on atsame5x under an attached debugger the core stays held, the
    SAMD5x DSU reset extension is the prime suspect, so the app never reboots and the
    RTT control block never appears; measured 2026-08-11: attach-without-reset streams
    immediately while both reset-run captures died with 'No control block found').
    Either way the flasher's own post-flash reset already supplied the fresh boot."""
    return not (is_wch_board(board) or board["sysview"].get("attach_only"))

def board_result(board, commit, metrics=None, capture_info=None, error=None):
    sv = board["sysview"]
    return {"board": board["name"], "commit": commit,
            "example": sv["example"], "workload": sv["workload"],
            "duration_s": sv["duration_s"],
            "capture": capture_info or {}, "metrics": metrics, "error": error}

def _workload_cdc_burst(node, duration_s):
    """Returns True if traffic ran for the full window, False if the serial
    link died early (device dropped off the bus) -- never raises."""
    import serial, time
    # write_timeout: without it, a device that stops draining its OUT endpoint blocks s.write()
    # forever, wedging PHASE 2 while the board flock is held -- the same failure mode hil_test.py
    # guards against in open_serial_dev() (SERIAL_WRITE_TIMEOUT, default 10s, sized for
    # whole-firmware-image transfers there). Hardcoded here rather than imported from hil_test:
    # this workload only ever writes 64-byte micro-bursts, so a much shorter deadline catches a
    # wedge faster, and importing hil_test would pull in its pymtp dependency for one constant.
    s = serial.Serial(node, 115200, timeout=0.02, write_timeout=2)
    end = time.monotonic() + duration_s
    while time.monotonic() < end:
        t = time.monotonic()
        while time.monotonic() - t < 0.30 and time.monotonic() < end:
            try:
                s.write(b"x" * 64); s.read(64)
            except Exception:
                s.close()
                return False
        time.sleep(min(1.0, max(0, end - time.monotonic())))
    s.close()
    return True

WORKLOADS = {"cdc_burst": _workload_cdc_burst,
             "idle": lambda node, duration_s: __import__("time").sleep(duration_s) or True}

def _drain_stderr(pipe, buf):
    """Continuously read a subprocess's stderr into a bounded deque, run on a daemon
    thread. OpenOCD logs for the whole capture window; with stderr=PIPE and nothing
    reading it, the pipe's OS buffer fills and OpenOCD blocks writing to it, wedging
    the capture. Only the last ~200 lines are kept, for error reporting."""
    for line in iter(pipe.readline, ""):
        buf.append(line)
    pipe.close()

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
            r = subprocess.run(cmd, cwd=exdir, capture_output=True, text=True, timeout=1800)
            if r.returncode:
                return None, f"build failed: {_err_excerpt(r.stderr)}"
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

def flash_and_capture_one(board, fw, out_dir, repo_root):
    """PHASE 2 (under the board lock): flash, wait for enumeration, RTT-capture raw
    SVDat bytes while the workload runs, then always reflash pristine firmware before
    returning. Returns (capture_dict, error); never raises."""
    import subprocess, time, socket, signal, threading
    from collections import deque
    sv, name = board["sysview"], board["name"]
    try:
        scripts = os.path.join(repo_root, ".claude", "skills", "sysview", "scripts")
        sys.path.insert(0, scripts)
        from sysview_record import rtt_cb_from_elf
        import hil_flash
        from helper import hil_util
        ocd_args = capture_ocd_args(board)
        flash = getattr(hil_flash, f"flash_{board['flasher']['name']}")
        r = flash(board, fw)
        if r.returncode:
            # run_cmd merges stderr into stdout and fills it even on timeout (rc=124);
            # without the flasher's own output 'rc=124' cannot distinguish a wedged
            # probe from a dead target.
            out = _err_excerpt(hil_util.cmd_stdout_text(r.stdout))
            return None, f"flash failed: rc={r.returncode}" + (f"\n{out}" if out else "")
        node = hil_util.get_serial_dev(board['uid'], 'TinyUSB', 'TinyUSB_Device', 0)
        try:
            # fw carries the flasher's extension, which is not always .elf (lm4flash and
            # esptool take .bin); the RTT control block always comes from the ELF's symbols.
            cb = int(rtt_cb_from_elf(os.path.splitext(fw)[0] + ".elf"), 16)
        except SystemExit as e:
            # rtt_cb_from_elf() sys.exit()s on a bad/non-SYSVIEW ELF -- SystemExit derives from
            # BaseException, so it would otherwise sail past the `except Exception` below and
            # abort the whole multi-board capture loop in main() before boards already captured
            # got a chance to be written out. Convert it to this board's own error instead.
            return None, f"no RTT symbol: {e}"
        resets = session_resets(board)  # False: WCH (SDI) and attach_only boards
        # Ask the kernel for a free port instead of a fixed one: a leftover openocd
        # still holding the port turns every later capture into a misleading
        # "rtt server never listened".
        with socket.socket() as s:
            s.bind(("localhost", 0))
            port = s.getsockname()[1]
        ocd = ["openocd", "-c", "tcl_port disabled", "-c", "gdb_port disabled",
               "-c", "telnet_port disabled",
               "-c", f"adapter serial {board['flasher']['uid']}"] + ocd_args + \
              ["-c", "init"] + (["-c", "reset run", "-c", "sleep 2000"] if resets else []) + \
              ["-c", f'rtt setup {cb} 0x1000 "SEGGER RTT"',
               "-c", "rtt polling_interval 1", "-c", "rtt start",
               "-c", f"rtt server start {port} 1"]
        raw = os.path.join(out_dir, f"{name}-capture.SVDat")
        p = subprocess.Popen(ocd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
        stderr_tail = deque(maxlen=200)
        threading.Thread(target=_drain_stderr, args=(p.stderr, stderr_tail), daemon=True).start()
        workload_ok, wl_err, wl_start = True, None, None
        try:
            # Readiness comes from OpenOCD's own log line, never from a throwaway probe
            # connection: a connection registers an RTT sink, and its data is only read while
            # a sink exists -- if the 1 ms poll fires between accept and seeing the probe's
            # close, channel 1's one-shot Init record (sync + timestamp frequency) is drained
            # into the discarded socket and the recording that follows decodes without a time
            # base. The line also proves the listener on `port` is THIS openocd, not another
            # process that won the bind-0/close/rebind race above.
            listening = f"Listening on port {port} for rtt connections"
            for _ in range(80):
                time.sleep(0.2)
                if p.poll() is not None:
                    return None, f"openocd: {''.join(stderr_tail)[-200:]}"
                if any(listening in line for line in list(stderr_tail)):
                    break
            else:
                return None, "rtt server never listened"
            # Wait for enumeration AFTER the capture session's own "reset run" above (baked
            # into `ocd` when session_resets(), i.e. neither WCH nor attach_only) has actually
            # happened, not before it: waiting earlier (right after flash()) validated a boot
            # this reset immediately throws away, and the workload below needs THIS boot's
            # node, which can take real time to reappear. For no-reset boards the flash's own
            # reset supplied the boot, so the node is usually already present here.
            for _ in range(25):
                if os.path.exists(node): break
                time.sleep(1)
            else:
                return None, "device never enumerated after flash"
            with open(raw, "wb") as f:
                nc = subprocess.Popen(["nc", "localhost", str(port)], stdout=f)
                wl_start = time.monotonic()
                try:
                    try:
                        workload_ok = bool(WORKLOADS[sv["workload"]](node, sv["duration_s"]))
                    except Exception as e:
                        workload_ok, wl_err = False, e
                finally:
                    nc.send_signal(signal.SIGINT)
                    try: nc.wait(5)
                    except subprocess.TimeoutExpired: nc.kill()
        finally:
            p.send_signal(signal.SIGINT)
            try: p.wait(8)
            except subprocess.TimeoutExpired: p.kill()
        # A serial death in the first half of the window means the whole capture is
        # mostly idle bus -- fail it outright. A late death still leaves a mostly-live
        # capture; keep it, but flag workload_ok=false so report() gates the metrics.
        if not workload_ok and wl_start is not None and \
                (time.monotonic() - wl_start) < sv["duration_s"] / 2:
            return None, f"workload died: {wl_err or 'stopped early'}"
        return {"raw": raw, "workload_ok": workload_ok}, None
    except Exception as e:
        return None, f"{type(e).__name__}: {e}"
    finally:
        _reflash_pristine(board, repo_root)

def decode_one(board, commit, out_dir, capture, repo_root):
    """PHASE 3 (no board lock): decode the raw SVDat capture + report. Never raises."""
    import subprocess
    sv, name = board["sysview"], board["name"]
    scripts = os.path.join(repo_root, ".claude", "skills", "sysview", "scripts")
    try:
        dec = os.path.join(out_dir, f"{name}-decoded")
        r = subprocess.run([sys.executable, os.path.join(scripts, "sysview_record.py"),
                            "--from-raw", capture["raw"], "--out", dec],
                           capture_output=True, text=True, timeout=900)
        if r.returncode:
            return board_result(board, commit, error=f"record decode failed: {r.stderr[-200:]}")
        r = subprocess.run([sys.executable, os.path.join(scripts, "sysview_report.py"),
                            dec, "--json"], capture_output=True, text=True, timeout=600)
        if r.returncode:
            return board_result(board, commit, error=f"decode failed: {r.stderr[-200:]}")
        m = json.loads(r.stdout)
        info = {"route": "openocd-rtt", "poll_ms": 1, "live_window_s": m.get("live_window_s"),
                "workload_ok": capture["workload_ok"]}
        return board_result(board, commit, metrics=m, capture_info=info)
    except Exception as e:
        return board_result(board, commit, error=f"{type(e).__name__}: {e}")

def _reflash_pristine(board, repo_root):
    import hil_flash, subprocess
    name = board["name"]
    try:
        # Reflash device/board_test, not the sysview example: it's the same park image
        # hil_test.py flashes as its own end-of-board teardown (disables the board's USB) --
        # leaving the SYSVIEW-instrumented example running instead would mean a board the
        # rig's contract calls "parked" is actually still enumerating a live CDC+MSC device.
        # find_firmware's default root only covers ONE park-image layout at a time (its
        # `build_dir` module global -- 'cmake-build' unless a caller like hil_test.main sets it
        # from argparse; nothing sets it for us). The rig mirror builds park images under
        # examples/cmake-build-<board>/, but the CI runner instead uses tools/build.py's
        # cmake-build/cmake-build-<board>/ layout -- mutating the global to "examples" alone (as
        # this used to do) finds the rig's images but makes the lookup silently miss, and the
        # restore silently no-op, on the CI runner. Pass both roots explicitly instead.
        fw = hil_flash.find_firmware(name, "device/board_test", roots=["examples", "cmake-build"])
        if not fw:
            print(f"warning: {name}: no park (device/board_test) firmware found for "
                  f"post-capture reflash (board left running the SYSVIEW-instrumented build)",
                  file=sys.stderr)
            return
        r = getattr(hil_flash, f"flash_{board['flasher']['name']}")(board, str(fw))
        if r.returncode:
            print(f"warning: {name}: pristine reflash failed rc={r.returncode}",
                  file=sys.stderr)
    except Exception as e:
        # pristine reflash is best-effort -- never raise out of here -- but a
        # silent swallow left a board's post-capture firmware state unknowable
        # from the CI log, so at least report what happened.
        print(f"warning: {name}: pristine reflash raised {type(e).__name__}: {e}",
              file=sys.stderr)

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
    args = ap.parse_args()
    if args.cmd == "report":
        md = report(args.base_dir, args.pr_dir)
        with open(args.out, "w") as f:
            f.write(md)
        print(f"{'empty (no captures)' if not md else args.out}")
    elif args.cmd == "capture":
        from helper import hil_lock
        try:
            with open(args.config) as f:
                cfg = json.load(f)
        except (OSError, ValueError) as e:
            print(f"error: cannot load {args.config}: {e}", file=sys.stderr)
            sys.exit(1)
        boards = select_boards(cfg, args.board)
        if not boards:
            # Neither case is an error: a rig config with no sysview-flagged boards at all
            # (e.g. hfp.json) and a -b filter that matched none of this rig's sysview boards
            # (e.g. a board from the CI matrix that isn't on this particular rig) are both
            # "nothing to capture here", not a failure.
            if not any("sysview" in b for b in cfg["boards"]):
                print(f"no sysview-flagged boards in {args.config}, nothing to capture")
            else:
                print("no sysview board selected, nothing to capture")
            sys.exit(0)
        os.makedirs(args.out, exist_ok=True)
        repo_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        commit = _git_commit(repo_root)
        # 3 phases so the board lock -- which blocks concurrent CI on that board -- is only
        # held for the part that actually touches hardware (PHASE 2). Building and decoding
        # (PHASES 1/3) are lock-free and, for builds, could run concurrently across boards;
        # kept sequential here for simplicity, same as before this split.
        # PHASE 1: build every selected board, no lock held.
        builds = {board["name"]: build_one(board, repo_root) for board in boards}
        # PHASE 2: per board, flash + RTT-capture under that board's lock only.
        captures = {}
        for board in boards:
            name = board["name"]
            fw, err = builds[name]
            if err:
                captures[name] = (None, err)
                continue
            try:
                lock_fh = hil_lock.acquire_board_lock(name, reason="sysview capture")
            except RuntimeError as e:
                captures[name] = (None, f"board locked: {e}")
                continue
            try:
                captures[name] = flash_and_capture_one(board, fw, args.out, repo_root)
            finally:
                # mirror hil_test.py's test_board(): clear our pid record before
                # dropping the flock so a freed board never reads as still-locked
                if lock_fh:
                    try:
                        lock_fh.truncate(0)
                    except OSError:
                        pass
                    lock_fh.close()
        # PHASE 3: decode + report every board with a successful capture, no lock held.
        for board in boards:
            name = board["name"]
            capture, err = captures[name]
            result = board_result(board, commit, error=err) if err else \
                decode_one(board, commit, args.out, capture, repo_root)
            with open(os.path.join(args.out, f"sysview-{name}.json"), "w") as f:
                json.dump(result, f)
            print(f"{name}: {'ok' if not result['error'] else result['error']}")
        sys.exit(0)

if __name__ == "__main__":
    main()
