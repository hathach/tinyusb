#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""SystemView CI: per-board capture on the HIL rig + PR performance report.

report: pure -- two directories of sysview-<board>.json in, markdown out.
capture_one: flash the SYSVIEW image hil-build made, drive the workload, RTT-capture; hil_test.py --sysview-out runs it
under its board lock, before its final park, and records it with capture_result.
decode: turn each recorded capture into metrics, no hardware. Capture and decode run agentrc's rtt.py and
target-debug sysview scripts from lib/agentrc at the get_deps.py pin (agentrc's skills/target-debug/sysview.md is
their manual).
"""
import argparse, glob, html, json, os, re, signal, sys

SCRIPTS = "skills/target-debug/scripts/"
REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
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
          "upstream and counted in `dropped_pairs`.</sub>")

def load_set(d):
    out = {}
    # recursive: a base download puts each artifact in its own subdirectory
    for p in glob.glob(os.path.join(d, "**", "sysview-*.json"), recursive=True):
        try:
            with open(p) as f:
                j = json.load(f)
        except (OSError, ValueError):
            continue
        if isinstance(j, dict) and isinstance(j.get("board"), str):
            why = malformed(j)
            out[j["board"]] = {"board": j["board"], "error": f"malformed record: {why}"} if why else j
    return out

# the report formats these as numbers: a record holding anything else there is malformed, never rendered
METRIC_SCALARS = ("overflow", "overflow_total", "dropped_pairs", "live_window_s", "workload_window_s")
# each table's row fields the report formats: an int, a number, or a number it renders null as "—"
DURATION_FIELDS = {"n": int, "p50_us": float, "p99_us": float, "max_us": float}
ROW_FIELDS = {"isr": DURATION_FIELDS, "functions": DURATION_FIELDS, "contexts": {"cpu_pct": None},
              "contexts_workload": {"cpu_pct_workload": None}, "stack": {"bytes_used": int}}

def _number(v):
    return v is None or (isinstance(v, (int, float)) and not isinstance(v, bool))

def _field_ok(v, kind):
    return _number(v) and (kind is None or v is not None and (kind is float or isinstance(v, int)))

def malformed(j):
    """Why record `j` cannot be rendered as data (PR-built, so never trusted), else None."""
    if not isinstance(j.get("capture") or {}, dict):
        return "capture is not an object"
    if not _number((j.get("capture") or {}).get("live_window_s")):
        return "capture.live_window_s is not a number"
    m = j.get("metrics")
    if m is None:
        return None
    if not isinstance(m, dict):
        return "metrics is not an object"
    for k in METRIC_SCALARS:
        if not _number(m.get(k)):
            return f"metrics.{k} is not a number"
    if not (m.get("warnings") is None or isinstance(m["warnings"], list)
            and all(isinstance(w, str) for w in m["warnings"])):
        return "metrics.warnings is not a list of text"
    for t, fields in ROW_FIELDS.items():
        rows = m.get(t)
        if rows is None:
            continue
        if not isinstance(rows, list):
            return f"metrics.{t} is not a list"
        for r in rows:
            if not (isinstance(r, dict) and isinstance(r.get("name"), str)
                    and all(k in r and _field_ok(r[k], kind) for k, kind in fields.items())):
                return f"metrics.{t} has a row without a name and its {', '.join(fields)}"
    return None

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

def md(text):
    """PR-built text as plain Markdown inside a line it does not start: no HTML, emphasis, link, image, table cell
    break, code span, issue reference or autolink of its own."""
    t = html.escape(re.sub(r"\s+", " ", str(text)).strip(), quote=False)
    t = re.sub(r"([\\`*_\[\]|~#])", r"\\\1", t)
    return t.replace("://", ":\u200b//").replace("www.", "www\u200b.")

def code(text):
    """PR-built text as one code span, also inside a table cell: GFM splits a cell on | even in code."""
    return "`" + re.sub(r"\s+", " ", str(text)).strip().replace("`", "'").replace("|", "\\|") + "`"

def mermaid_label(text):
    """PR-built text as a mermaid xychart label: no quote, bracket, comma or newline to end it."""
    return re.sub(r"[^A-Za-z0-9_.:-]", "_", str(text))

def config_line(j):
    """The capture's own configuration (capture_config), as recorded beside its numbers."""
    c = (j.get("capture") or {}).get("config")
    if not isinstance(c, dict) or not c:
        return "*configuration not recorded*"
    b, r = (v if isinstance(v, dict) else {} for v in (c.get("build"), c.get("route")))
    events = "all events" if c.get("event_mask") == "none" else f"events masked {c.get('event_mask')}"
    return "*" + md(f"{j.get('example')} · {c.get('variant')} · SYSVIEW={b.get('SYSVIEW')} buffer "
                 f"{c.get('buffer')} · {events} · workload {j.get('workload')} "
                 f"r{c.get('workload_rev')} {j.get('duration_s')} s · OpenOCD RTT channel {r.get('channel')}") + "*"

def board_section(name, base_j, pr_j, base_dropped=None):
    pm_all = pr_j.get("metrics") or {}
    live_window_s = pm_all.get("live_window_s", (pr_j.get("capture") or {}).get("live_window_s"))
    heading = f"### {md(name)}" + (f" — live {live_window_s:.1f} s" if live_window_s is not None else "")
    if base_dropped:
        # The report header states the PR-side config only, so a dropped baseline has to say
        # here why every row reads "new" -- otherwise it looks like base never captured.
        heading += f" — base dropped, {md(', '.join(base_dropped))}"
    lines = [heading, "", config_line(pr_j), ""]
    # the board left without its park image, whether or not the capture itself failed
    park_error = (pr_j.get("capture") or {}).get("park_error")
    if pr_j.get("error"):
        # one plain line each: build and tool output must not add headings, fences or HTML
        return "\n".join(lines + [f"capture failed: {md(pr_j['error'])}", ""]
                         + ([f"> ⚠︎ {md(park_error)}", ""] if park_error else []))
    if not pm_all:
        # a malformed capture JSON must not abort the whole report
        return "\n".join(lines + ["capture failed: missing metrics", ""])
    bm_all = (base_j or {}).get("metrics") or {}
    # reported, not gated: e.g. the decoder's timing warnings (agentrc sysview.md)
    warnings = (pm_all.get("warnings") or []) + ([park_error] if park_error else [])
    warnings = [f"> ⚠︎ {md(w)}" for w in warnings]
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
        lines.append(f"| {md(iname)} p50 / p99 | {_cell(bm, breason50, breason99)} | "
                     f"{_cell(pm, preason50, preason99)} | {_delta()} |")
    bfn, pfn = by_name(bm_all.get("functions")), by_name(pm_all.get("functions"))
    # PR-side rows keep their existing CPU-occupancy order; base-only rows have no PR
    # occupancy to rank by, so they sort alphabetically after every PR-ranked row.
    order = sorted(pfn, key=lambda k: pfn[k]["n"] * pfn[k]["p50_us"], reverse=True)
    order += sorted(set(bfn) - set(pfn))
    for fname in order:
        r = row(f"{code(fname)} p50", bfn, pfn, fname, base_j, pr_j, "functions")
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
            lines.append(f"| {label} ({md(cname)}) | {bcell} | {pcell} | {d} |")
    bst, pst = by_name(bm_all.get("stack")), by_name(pm_all.get("stack"))
    for sname in sorted(set(pst) | set(bst)):
        b, sm = bst.get(sname), pst.get(sname)
        bcell, pcell, d = gated_pair(
            base_j, pr_j, b, sm, lambda m: f"{m['bytes_used']} B",
            lambda b_, p_: ("—" if b_['bytes_used'] == p_['bytes_used'] else
                            f"{p_['bytes_used']-b_['bytes_used']:+d} B"),
            "stack")
        lines.append(f"| {code(sname)} stack high-water | {bcell} | {pcell} | {d} |")
    # chart: occupancy order, PR-gate-eligible only, capped; base series only if
    # every charted function is ungated on the base side too (no holes in a mermaid series).
    # `c in pfn` excludes base-only rows: the chart is explicitly "hot functions ... in the
    # PR capture" (see LEGEND), and a base-only row has no PR bar to plot.
    eligible = [c for c in order if c in pfn and gate(pr_j, pfn[c]) is None][:CHART_MAX_BARS]
    if eligible:
        base_ok = (gate(base_j, None) is None and
                   all(c in bfn and gate(base_j, bfn[c]) is None for c in eligible))
        short = [mermaid_label(re.sub(r'^(tud_|tuh_|dcd_|hcd_)', '', c)) for c in eligible]
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
# capture_config's, with the sub-keys a dict one needs; every one required and non-null on both sides, so an older
# or partial record is never comparable
CONFIG_KEYS = {"variant": (), "build": ("BOARD", "SYSVIEW"), "buffer": (), "event_mask": (),
               "route": ("recorder", "ocd_args", "reset_before_attach", "channel"), "workload_rev": ()}

def _unknown(value, subs=()):
    """A required value absent or null, or a dict one lacking a required sub-key; False and 0 are values."""
    return value is None or bool(subs) and (not isinstance(value, dict) or any(value.get(s) is None for s in subs))

def capture_cfg_changes(base_j, pr_j):
    """Config differences that make a baseline incomparable: a PR that changes a board's
    sysview example, workload, duration_s, traced build, event filter or capture route measures
    different work, so base-vs-PR deltas would be noise dressed up as a regression."""
    if not base_j:
        return []
    changes = []
    for k in CAPTURE_CFG_KEYS:
        if _unknown(base_j.get(k)) or _unknown(pr_j.get(k)):
            changes.append(f"{k} unknown")
        elif base_j[k] != pr_j[k]:
            changes.append(f"{k} {base_j[k]}→{pr_j[k]}")
    bc, pc = (c if isinstance(c := (j.get("capture") or {}).get("config"), dict) else {} for j in (base_j, pr_j))
    for k, subs in CONFIG_KEYS.items():
        if _unknown(bc.get(k), subs) or _unknown(pc.get(k), subs):
            changes.append(f"{k} unknown")
        elif isinstance(bc[k], dict) and isinstance(pc[k], dict):
            changes += [f"{k}.{sub} {bc[k].get(sub)}→{pc[k].get(sub)}"
                        for sub in sorted(set(bc[k]) | set(pc[k])) if bc[k].get(sub) != pc[k].get(sub)]
        elif bc[k] != pc[k]:
            changes.append(f"{k} {bc[k]}→{pc[k]}")
    return changes

def baseline_line(info, base, any_pr):
    """Which base this report compares against: the baseline lookup's (pr_comment.yml), else the records' own."""
    pr_commit = code(any_pr.get("commit", "?"))
    if info is None:
        base_commit = next(iter(base.values())).get("commit", "?") if base else "(none)"
        return f"*base {code(base_commit)} → PR {pr_commit}*"
    if not info.get("sha"):
        return f"*no baseline: {md(info.get('note') or 'none found')} · PR {pr_commit}*"
    kind = "exact" if info.get("exact") else f"approximate: {md(info.get('note') or '')}"
    return f"*base {code(str(info['sha'])[:10])} ({kind}) → PR {pr_commit}*"

def report(base_dir, pr_dir, baseline_info=None):
    """The PR comment's SystemView section. With `baseline_info` (sysview_ci.py baseline), base records come from
    its run only, and one recorded at another commit is dropped, never compared."""
    pr = load_set(pr_dir)
    if not pr:
        return ""
    base = load_set(base_dir)
    if baseline_info is None and base and not (set(base) & set(pr)):
        return ""
    boards = sorted(pr)
    any_pr = next(iter(pr.values()))
    # .get throughout: one malformed capture JSON must not abort the whole report -- the same
    # rule gate()/board_section() already follow one layer down.
    parts = [HEADER, "", baseline_line(baseline_info, base, any_pr), ""]
    for b in boards:
        if baseline_info is not None and b in base and base[b].get("sha") != baseline_info.get("sha"):
            changed = [f"base record of commit {str(base[b].get('sha'))[:10]}, not the baseline's"]
        else:
            changed = [f"capture config changed ({', '.join(c)})" for c in [capture_cfg_changes(base.get(b), pr[b])]
                       if c]
        parts.append(board_section(b, None if changed else base.get(b), pr[b], changed))
    parts.append(LEGEND)
    return "\n".join(parts) + "\n"

# ---------------------------------------------------------------- capture
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
    # forever, wedging the capture while the board flock is held -- the same failure mode hil_test.py
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

# bumped with any change to what a workload does, so captures across it are never compared
WORKLOAD_REVISIONS = {"cdc_burst": 1, "idle": 1}
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

# command-line -D keys tools/build.py sets on its own; every other one must be the traced leg's
_BUILD_PY_KEYS = {"BOARD", "TOOLCHAIN", "MEMBROWSE_BOARD", "CFLAGS_CLI"}

def manifest_problems(board, sv, fw, head):
    """Why the image's <target>.sysview.json (family_support.cmake) does not vouch for `fw` as `sv`'s build
    of this checkout, as a list; empty when it does."""
    import hashlib
    try:
        with open(os.path.splitext(fw)[0] + ".sysview.json") as f:
            m = json.load(f)
        with open(fw, "rb") as f:
            sha = hashlib.sha256(f.read()).hexdigest()
    except (OSError, ValueError) as e:
        return [f"no usable manifest: {e}"]
    cli = m.get("commandLine") or {}
    # tools/build.py configures every build MinSizeRel
    want = {"CMAKE_BUILD_TYPE": "MinSizeRel", **dict(d.partition("=")[::2] for d in sv["defines"])}
    got = {k: v for k, v in cli.items() if k not in _BUILD_PY_KEYS}
    problems = []
    if m.get("elfSha256") != sha:
        problems.append("the manifest describes another image")
    if (m.get("board"), m.get("example")) != (board["name"], sv["example"]):
        problems.append(f"built as {m.get('board')} {m.get('example')}")
    if got != want or cli.get("CFLAGS_CLI", "").split() != sv["flags"]:
        problems.append(f"built with {cli}")
    if m.get("eventMask") != want.get("SYSVIEW_DISABLE_EVENTS", "none"):
        problems.append(f"built with event mask {m.get('eventMask')}")
    if m.get("commit") != head or head == "unknown":
        problems.append(f"built at {m.get('commit')}, the checkout is at {head}")
    if m.get("dirty") is not False:
        problems.append("built from uncommitted changes: commit them first")
    return problems

def find_capture_firmware(board, head):
    """The traced image hil-build's sysview leg made for `board` (hil_report.sysview_build) at commit `head`,
    with the flasher's extension: (fw_path, None), or (None, why not) naming the build that makes it. Its
    manifest must match, so a stale or differently configured image is never flashed. The rig never builds.
    Never raises."""
    try:
        import hil_flash
        from helper import hil_report
        sv = hil_report.sysview_build(board)
        cmd = " ".join(["python3 tools/build.py -b", board["name"], "--build-name", sv["name"], "-e", sv["example"],
                        *(f"-D{d}" for d in sv["defines"]), *(f"--cflag={t}" for t in sv["flags"])])
        fw = hil_flash.find_firmware(sv["name"], sv["example"], board["flasher"]["name"])
        if not fw:
            return None, f"no {sv['name']} image of {sv['example']} (built by hil-build, or locally: {cmd})"
        if fw.suffix != ".elf":
            # the manifest hashes the ELF, so it cannot vouch for a .bin flashed beside it
            return None, f"{board['flasher']['name']} flashes {fw.suffix} images; the capture checks only ELF ones"
        problems = manifest_problems(board, sv, str(fw), head)
        if problems:
            return None, f"{fw}: {'; '.join(problems)} (remove cmake-build/cmake-build-{sv['name']} and run: {cmd})"
        return str(fw), None
    except Exception as e:
        return None, f"{type(e).__name__}: {e}"

# capture_one's bounds besides its flasher calls: nm 30 s, the RTT connect 17 s, the
# enumeration wait 25 s, the workload's last write 2 s, the reader join 5 s, the session close 10 s
CAPTURE_FIXED_S = 90

# sysview_record.py's worst case from SIGTERM to exit, sized for its --gui decode: its error screenshot
# 30 s, sending -quit (connect and banner recv 10 s each), SystemView's -quit QUIT_S 20 s, then SIGTERM
# and SIGKILL to SystemView's and Xvfb's groups GROUP_S 5 s each. The native decode used here exits at once.
DECODE_STOP_S = 30 + 2 * 10 + 20 + 4 * 5

# what decode keeps of --budget-s after a board's decode is cut, to stop it, reap it (REAP_GRACE 10 s),
# write its report and exit
DECODE_MARGIN_S = DECODE_STOP_S + 10 + 5

def capture_bound_s(board):
    """Worst-case seconds of capture_one: two flasher calls at the harness's CMD_TIMEOUT plus the REAP_GRACE each
    spends when killed (the flash; its DAP rescue or the capture) plus the capture's own bounds and its workload."""
    from helper import hil_util
    return (2 * (hil_util.CMD_TIMEOUT + hil_util.REAP_GRACE) + CAPTURE_FIXED_S
            + board["sysview"]["duration_s"])

SHA_RE = re.compile(r"[0-9a-f]{40}")

# the error of a recorded capture until `decode` replaces it
NOT_DECODED = "decode did not run"

def capture_result(board, commit, capture=None, error=None, park_error=None, identity=None):
    """The record of a board's capture that hil_test.py writes, and `decode`'s whole input: it decodes only a
    capture whose status is "captured", from the raw file it names beside the record. `identity` is the checkout's
    (checkout_identity), which the PR comment finds its baseline by."""
    info = {**(capture or {}), "status": "captured" if capture else "failed", "park_error": park_error}
    return {**board_result(board, commit, capture_info=info, error=error or (NOT_DECODED if capture else None)),
            **(identity or {})}

def write_result(out_dir, result):
    with open(os.path.join(out_dir, f"sysview-{result['board']}.json"), "w") as f:
        json.dump(result, f)

def capture_one(board, fw, out_dir):
    """Flash fw and RTT-capture raw SVDat bytes over agentrc's rtt.py OpenOCD route while the workload runs, under
    the caller's board lock; the caller parks the board after. Returns (capture_dict, error), capture_dict naming
    the raw file in out_dir; never raises an Exception."""
    try:
        from helper import agentrc
        rtt = agentrc.load_file("skills/rtt/scripts/rtt.py")
    except Exception as e:
        return None, f"{type(e).__name__}: {e}"
    capture, err = _capture(board, fw, out_dir, rtt)
    if capture:
        try:
            capture["config"] = capture_config(board, fw, rtt)
        except Exception as e:
            return None, f"capture config unreadable: {type(e).__name__}: {e}"
    return capture, err

def capture_config(board, fw, rtt):
    """What makes two captures of a board comparable besides CAPTURE_CFG_KEYS: the traced build's -D set from its
    manifest (find_capture_firmware checked it), the event filter it holds, the route down to the recorder's code
    (rtt.py fixes the poll interval) and the workload's revision."""
    import hashlib
    from helper import hil_report
    with open(os.path.splitext(fw)[0] + ".sysview.json") as f:
        manifest = json.load(f)
    build = {k: v for k, v in manifest["commandLine"].items() if k != "MEMBROWSE_BOARD"}
    with open(rtt.__file__, "rb") as f:
        recorder = hashlib.sha256(f.read()).hexdigest()[:12]
    return {"variant": hil_report.sysview_build(board)["name"], "build": build,
            "buffer": manifest["bufferSize"], "event_mask": manifest["eventMask"],
            "route": {"recorder": f"openocd rtt.py {recorder}", "ocd_args": capture_ocd_args(board),
                      "reset_before_attach": session_resets(board), "channel": SYSVIEW_CHANNEL},
            "workload_rev": WORKLOAD_REVISIONS[board["sysview"]["workload"]]}

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
        return {"raw": os.path.basename(raw), "workload_ok": workload_ok}, None
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

# capture_one's raw file name, the only one a record may name
RAW_RE = re.compile(r"[A-Za-z0-9._-]+-capture\.SVDat")

def artifact_file(out_dir, name):
    """The raw file `name` a downloaded record names, refused unless it is a plain capture file name resolving inside
    out_dir: a record is PR-built data, and decode may run with a token beside it."""
    if not isinstance(name, str) or not RAW_RE.fullmatch(name):
        raise ValueError(f"raw file name refused: {name!r}")
    path = os.path.realpath(os.path.join(out_dir, name))
    if os.path.dirname(path) != os.path.realpath(out_dir):
        raise ValueError(f"raw file outside its record's directory: {name!r}")
    return path

def decode_one(result, out_dir, deadline=None):
    """The recorded capture `result` (capture_result) decoded with agentrc's sysview scripts, no hardware, each step
    cut at the time.monotonic() deadline if one is given. Never raises."""
    import subprocess, tempfile, time
    from helper import agentrc
    capture = result["capture"]

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
        raw = artifact_file(out_dir, capture.get("raw"))
        dec = tempfile.mkdtemp(prefix="decoded-", dir=out_dir)
        r = run([sys.executable, agentrc.path_of(SCRIPTS + "sysview_record.py"),
                 "--from-raw", raw, "--out", dec], 900)
        if r.returncode:
            return {**result, "error": f"record decode failed: {r.stderr[-200:]}"}
        r = run([sys.executable, agentrc.path_of(SCRIPTS + "sysview_report.py"),
                 dec, "--json", "--event-map", EVENT_MAP,
                 "--workload-event", str(workload_event())], 600)
        if r.returncode:
            return {**result, "error": f"decode failed: {r.stderr[-200:]}"}
        m = json.loads(r.stdout)
        return {**result, "capture": {**capture, "live_window_s": m.get("live_window_s")}, "metrics": m,
                "error": None}
    except TimeoutError as e:
        return {**result, "error": str(e)}
    except Exception as e:
        return {**result, "error": f"{type(e).__name__}: {e}"}

def has_captures(names):
    """Whether a Build run's unexpired artifact `names` hold SystemView captures."""
    return any(n.startswith("sysview-") for n in names)

def baseline(repo, pr, current):
    """The baseline description (code_size_ci.lookup_sha's) of the PR run whose records are in `current`: the push
    run of the base commit they were built against, on the branch PR `pr` targets, else of its nearest first-parent
    ancestor (approximate). Raises RuntimeError on an API failure."""
    sys.path.insert(0, os.path.join(REPO_ROOT, ".github", "scripts"))
    import code_size_ci
    none = {"sha": None, "url": None, "exact": False, "run_id": None}
    records = []
    for p in glob.glob(os.path.join(current, "**", "sysview-*.json"), recursive=True):
        try:
            with open(p) as f:
                records.append(json.load(f))
        except (OSError, ValueError):
            records.append(None)
    shas = {r.get("base_sha") if isinstance(r, dict) else None for r in records}
    refs = {r.get("base_ref") if isinstance(r, dict) else None for r in records}
    sha = shas.pop() if len(shas) == 1 else None
    if not (isinstance(sha, str) and SHA_RE.fullmatch(sha)) or len(refs) != 1:
        return {**none, "note": "the PR captures record no single base commit"}
    built_on, target = refs.pop(), code_size_ci.gh(f"repos/{repo}/pulls/{pr}")["base"]["ref"]
    if built_on != target:
        return {**none, "note": f"the captures were built against {built_on}, the PR now targets {target}"}
    return code_size_ci.lookup_sha(repo, sha, branch=target, usable=has_captures)

def checkout_identity(repo_root, env=os.environ):
    """The checkout's commits as code-size's snapshots record them, the baseline lookup's key: a pull_request build
    checks out GitHub's merge commit, whose first parent is the base it was built against and second the PR head;
    any other build is its own. Each None where git cannot resolve it (a checkout too shallow for the parents)."""
    import subprocess
    revs = ["HEAD", "HEAD^1", "HEAD^2"] if env.get("GITHUB_EVENT_NAME") == "pull_request" else ["HEAD"]
    try:
        r = subprocess.run(["git", "rev-parse", *revs], cwd=repo_root, capture_output=True, text=True, timeout=10)
        shas = r.stdout.split() if r.returncode == 0 else []
    except Exception:
        shas = []
    if len(shas) != len(revs) or not all(SHA_RE.fullmatch(s) for s in shas):
        shas = [None] * len(revs)
    sha, base, head = shas if len(shas) == 3 else shas * 3
    return {"sha": sha, "base_sha": base, "head_sha": head, "base_ref": env.get("GITHUB_BASE_REF") or None}

def _git_commit(repo_root, short=True):
    import subprocess
    try:
        r = subprocess.run(["git", "rev-parse", *(["--short"] if short else []), "HEAD"], cwd=repo_root,
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
    bp = sub.add_parser("baseline", help="find the base branch Build run a PR run's captures compare against")
    bp.add_argument("--repo", required=True)
    bp.add_argument("--pr", required=True, type=int)
    bp.add_argument("--current", required=True, help="the PR run's downloaded sysview-* artifacts")
    bp.add_argument("--info", required=True, help="where to write the baseline description JSON")
    rp.add_argument("--baseline-info", help="the baseline description `baseline` wrote")
    dp = sub.add_parser("decode")
    dp.add_argument("dir", help="hil_test.py --sysview-out's directory, or downloaded artifacts of it")
    dp.add_argument("--budget-s", type=float, default=None,
                    help="seconds before the caller kills this run: decoding is cut at it")
    args = ap.parse_args()
    if args.cmd == "baseline":
        try:
            info = baseline(args.repo, args.pr, args.current)
        except Exception as e:   # an API failure or an unexpected reply is a result, the comment says so
            info = {"sha": None, "url": None, "exact": False, "run_id": None,
                    "note": f"the baseline lookup failed: {type(e).__name__}: {e}"}
        with open(args.info, "w") as f:
            json.dump(info, f)
        print(json.dumps(info))
    elif args.cmd == "report":
        info = None
        if args.baseline_info:
            with open(args.baseline_info) as f:
                info = json.load(f)
        md = report(args.base_dir, args.pr_dir, info)
        with open(args.out, "w") as f:
            f.write(md)
        print(f"{'empty (no captures)' if not md else args.out}")
    elif args.cmd == "decode":
        import time
        deadline = None if args.budget_s is None else time.monotonic() + args.budget_s - DECODE_MARGIN_S
        # recursive: a downloaded artifact sits in its own subdirectory, its raw files beside its records
        for path in sorted(glob.glob(os.path.join(args.dir, "**", "sysview-*.json"), recursive=True)):
            try:
                with open(path) as f:
                    result = json.load(f)
                capture = result.get("capture") or {}
                # an undecoded capture only: a decoded one may have no raw left beside it to decode again
                if capture.get("status") == "captured" and result.get("error") == NOT_DECODED:
                    result = decode_one(result, os.path.dirname(path), deadline)
                    with open(path, "w") as f:
                        json.dump(result, f)
                park_error = capture.get("park_error")
                print(f"{result['board']}: {result['error'] or 'ok'}" + (f" ({park_error})" if park_error else ""))
            except Exception as e:   # one unreadable record must not cost the others their decode
                print(f"{path}: not decoded: {type(e).__name__}: {e}")
        sys.exit(0)

if __name__ == "__main__":
    main()
