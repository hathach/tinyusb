#!/usr/bin/env python3
"""Analyze an etm_capture.py output dir: hot functions, coverage, itrace digest,
ISR episode timing, and optimization hints.

Input:  code_profile.txt (Ozone Export.CodeProfile text report), and optionally
        itrace.csv (Export.Trace raw instruction history) with --elf for
        address->function mapping (arm-none-eabi-nm).
Output: markdown report on stdout.

--isr SYM[,SYM..]  episode timing for an interrupt handler: first symbol is the
        entry anchor (its first instruction marks each ISR entry), all symbols
        form the body address set. Example: --isr OTG_HS_IRQHandler,dcd_int_handler
        Durations are calibrated against SysTick_Handler beats (1 ms apart), so
        the itrace must be captured with timestamps enabled.
"""

import argparse
import bisect
import csv
import os
import re
import statistics
import subprocess
import sys


def parse_profile(path):
    """Parse the two report sections. Function rows are indented 2 spaces under
    a non-indented module row; columns are '|'-separated."""
    lines = open(path, errors="replace").read().splitlines()
    try:
        cov_start = lines.index("Code Coverage Summary")
        prof_start = lines.index("Code Profile Summary")
    except ValueError:
        sys.exit(f"error: {path} is not an Ozone code-profile text report")

    def rows(section):
        module = None
        for ln in section:
            if "|" not in ln:
                continue
            cells = ln.split("|")
            name = cells[0].rstrip()
            if (not name or name.startswith("Module/Function")
                    or set(name.strip()) <= {"-", "+"}):
                continue
            if not name.startswith("  "):
                module = name.strip()
                continue
            yield module, name.strip(), cells[1:]

    def num(s):
        s = s.strip().replace(" ", "")
        return int(s) if s else 0

    cov_pat = re.compile(r"^\s*([\d ]+)/\s*([\d ]+)\s+([\d.]+)%")
    funcs, totals = {}, {}
    for module, name, cells in rows(lines[prof_start:]):
        run, fetch = (num(cells[0]) if cells else 0), (num(cells[1]) if len(cells) > 1 else 0)
        if name == "Total":
            totals["run"], totals["fetch"] = run, fetch
        elif name == "[Unaccounted]":
            totals["unaccounted"] = fetch
        elif name in funcs and funcs[name]["module"] != module:
            # same-named static from another module: keep both rows distinct
            funcs[f"{name} [{module}]"] = {"module": module, "run": run,
                                           "fetch": fetch}
        else:
            funcs[name] = {"module": module, "run": run, "fetch": fetch}
    for module, name, cells in rows(lines[cov_start:prof_start]):
        m_src = cov_pat.match(cells[0]) if cells else None
        m_inst = cov_pat.match(cells[1]) if len(cells) > 1 else None
        if name == "Total" and m_inst and m_src:
            totals["src_cov"] = num(m_src.group(1)), num(m_src.group(2))
            totals["inst_cov"] = num(m_inst.group(1)), num(m_inst.group(2))
        elif m_inst:
            # match the de-collided key when a same-named static from another
            # module was renamed during the profile pass
            key = name if (name in funcs and funcs[name]["module"] == module) \
                else f"{name} [{module}]"
            if key in funcs:
                funcs[key]["inst_pct"] = float(m_inst.group(3))
    return funcs, totals


def load_symbols(elf):
    """Sorted (addr, size, name) from nm; for mapping itrace addresses."""
    nm = "arm-none-eabi-nm"
    out = subprocess.run([nm, "-S", "--defined-only", "-C", elf],
                         capture_output=True, text=True)
    if out.returncode != 0:
        sys.exit(f"error: {nm} failed on {elf}: {out.stderr.strip()}")
    syms = []
    for ln in out.stdout.splitlines():
        parts = ln.split(maxsplit=3)
        if len(parts) == 4 and parts[2].lower() in ("t", "w"):
            syms.append((int(parts[0], 16) & ~1, int(parts[1], 16), parts[3]))
        elif len(parts) == 3 and parts[1].lower() in ("t", "w"):
            # sizeless symbol (e.g. weak asm stub): assume a 2-byte body
            syms.append((int(parts[0], 16) & ~1, 2, parts[2]))
    return sorted(syms)


def addr_to_func(syms, addr):
    i = bisect.bisect_right(syms, (addr, 1 << 62, "")) - 1
    if i >= 0 and syms[i][0] <= addr < syms[i][0] + syms[i][1]:
        return syms[i][2]
    return None


def sym_ranges(syms, names):
    """(lo, hi) address ranges for the named symbols (base name match);
    None if any symbol is missing (caller degrades gracefully)."""
    out = []
    for want in names:
        for a, sz, n in syms:
            if n == want or n.split("(")[0] == want:
                out.append((a, a + sz))
                break
        else:
            print(f"note: symbol '{want}' not found in ELF")
            return None
    return out


def iter_itrace(path):
    """Yield (t_raw, addr) chronologically-reversed (file order: newest first).
    Also returns the timestamp unit from the header via generator .send? No -
    caller reads unit separately with itrace_unit()."""
    with open(path, newline="", errors="replace") as f:
        rd = csv.reader(f)
        hdr = next(rd, None) or []
        # --no-timestamps captures drop the Timestamp column entirely: locate
        # the Address column from the header and yield t=None for such rows
        # (consumers count instructions but skip time math)
        has_ts = any("Timestamp" in c for c in hdr)
        try:
            addr_i = next(i for i, c in enumerate(hdr) if "Address" in c)
        except StopIteration:
            addr_i = 1 if has_ts else 0
        for row in rd:
            if not row or len(row) <= addr_i:
                continue
            try:
                addr = int(row[addr_i], 16)
            except ValueError:
                continue
            if has_ts and row[0] != "PC":
                try:
                    yield float(row[0]), addr
                    continue
                except ValueError:
                    pass
            yield None, addr


def itrace_unit(path):
    hdr = open(path, errors="replace").readline()
    m = re.search(r"Timestamp\[([^\]]+)\]", hdr)
    return m.group(1) if m else "?"


def time_by_func(path, syms):
    """Per-function raw-time and instruction attribution from the itrace.
    Rows are newest-first; the gap to the next (older) row is attributed to the
    older instruction's function. Outlier gaps (trace-block boundaries) are
    capped so one discontinuity cannot skew a function. Shares are scale-free."""
    sample = []
    t_prev = None
    for t, _ in iter_itrace(path):
        if t is None:
            continue
        if t_prev is not None and t_prev - t > 0:
            sample.append(t_prev - t)
            if len(sample) >= 200000:
                break
        t_prev = t
    cap = 10000 * statistics.median(sample) if sample else float("inf")
    t_prev = None
    tf, cf = {}, {}
    n = 0
    for t, a in iter_itrace(path):
        n += 1
        fn = addr_to_func(syms, a)
        if fn:
            cf[fn] = cf.get(fn, 0) + 1
        if t is None:
            continue
        if t_prev is not None:
            d = t_prev - t
            if 0 < d < cap and fn:
                tf[fn] = tf.get(fn, 0) + d
        t_prev = t
    return tf, cf, n


def isr_report(itrace, elf, isr_arg, top):
    syms = load_symbols(elf)
    names = [s.strip() for s in isr_arg.split(",") if s.strip()]
    body = sym_ranges(syms, names)
    tick = sym_ranges(syms, ["SysTick_Handler"])
    if not body or not tick:
        print("\n## ISR timing: skipped (missing symbols above - needs the ISR "
              "symbol(s) and SysTick_Handler for calibration)")
        return
    entry = body[0][0]
    unit = itrace_unit(itrace)

    usb_rows, tick_rows, tmin, tmax = [], [], None, None
    for t, a in iter_itrace(itrace):
        if t is None:
            continue  # --no-timestamps capture: the <20-rows message below applies
        tmin = t if tmin is None else min(tmin, t)
        tmax = t if tmax is None else max(tmax, t)
        if any(lo <= a < hi for lo, hi in body):
            usb_rows.append((t, a))
        # independent, not elif: when the ISR under test IS SysTick_Handler,
        # its rows must still feed the calibration
        if any(lo <= a < hi for lo, hi in tick):
            tick_rows.append((t, a))
    usb_rows.reverse()
    tick_rows.reverse()
    if len(tick_rows) < 20:
        print(f"\n## ISR timing: not enough SysTick beats for calibration "
              f"({len(tick_rows)} rows) - capture with timestamps enabled")
        return

    # rough raw-units-per-1ms from the large mode of consecutive tick deltas;
    # threshold from the median, not the max - one trace-overflow gap would
    # otherwise inflate the cut and leave only outliers in the sample
    deltas = [b[0] - a[0] for a, b in zip(tick_rows, tick_rows[1:])]
    big = [d for d in deltas if d > 10 * statistics.median(deltas)]
    raw_ms = statistics.median(big) if big else statistics.median(deltas)
    gap = 0.03 * raw_ms       # 30 us in raw units
    edge = 0.05 * raw_ms

    def episodes(rows, g):
        out, cur = [], []
        for t, a in rows:
            if cur and t - cur[-1][0] > g:
                out.append(cur)
                cur = []
            cur.append((t, a))
        if cur:
            out.append(cur)
        return out

    starts = [e[0][0] for e in episodes(tick_rows, 0.1 * raw_ms)]

    def local_scale(t):
        i = bisect.bisect_left(starts, t)
        if 0 < i < len(starts):
            sp = starts[i] - starts[i - 1]
            if 0 < sp < 3 * raw_ms:
                return 1e-3 / sp
        return 1e-3 / raw_ms

    eps = []
    for cluster in episodes(usb_rows, gap):
        cur = None
        for t, a in cluster:
            if a == entry:
                if cur:
                    eps.append(cur)
                cur = [(t, a)]
            elif cur:
                cur.append((t, a))
        if cur:
            eps.append(cur)
    eps = [e for e in eps if e[0][0] - tmin > edge and tmax - e[-1][0] > edge
          and len(e) >= 10]
    print(f"\n## ISR timing: {names[0]} (+{len(names) - 1} body syms), "
          f"unit '{unit}', {len(starts)} SysTick beats")
    if not eps:
        print("- no complete episodes in window (wrong symbols? window missed "
              "the traffic phase?)")
        return
    stats = sorted(((e[-1][0] - e[0][0]) * local_scale(e[0][0]), len(e), e[0][0])
                   for e in eps)
    cal = [s[0] for s in stats]
    print(f"- episodes: {len(cal)}  |  fastest {min(cal) * 1e6:.2f} us, "
          f"median {statistics.median(cal) * 1e6:.2f} us, "
          f"avg {statistics.mean(cal) * 1e6:.2f} us, "
          f"worst {max(cal) * 1e6:.2f} us")
    insts = [s[1] for s in stats]
    print(f"- instructions/episode: min {min(insts)}, avg "
          f"{statistics.mean(insts):.0f}, max {max(insts)}")
    print("- worst episodes (duration, instructions, raw start):")
    for c, n, t0 in stats[-5:][::-1]:
        print(f"  - {c * 1e6:8.2f} us  {n:5d} instr  t={t0:.6f}")
    print("- caveats: timestamps are interpolated between packets (sub-us "
          "values are approximate); episodes may merge if trace overflow "
          "dropped an entry")


def short(name):
    return re.sub(r"\(.*\)$", "()", name)


def branch_bias(insts_csv, funcs, top):
    """One-sided conditional branches in executed functions (profile_insts.csv):
    a conditional fetched N times but taken/executed 0 or N times = a branch
    that never varied -> hot always-true assert, dead path, or an invariant
    that could hoist out of a loop (UM08025 SS5.19)."""
    def n(v):
        v = (v or "").replace(" ", "")
        return int(v) if v.lstrip("-").isdigit() else 0
    biased = []
    with open(insts_csv, newline="", errors="replace") as f:
        for r in csv.DictReader(f):
            if (r.get("Is Conditional") or "0").strip() != "1":
                continue
            fetched = n(r.get("Times Fetched"))
            executed = n(r.get("Times Executed"))
            if fetched < 1000:            # only hot conditionals matter
                continue
            if executed == 0 or executed == fetched:
                biased.append((fetched, r.get("Function", "?"),
                               r.get("Address", ""), r.get("AsmCode", "").strip(),
                               "always-taken" if executed == fetched else "never-taken"))
    if not biased:
        return
    biased.sort(reverse=True)
    print(f"\n## One-sided hot branches (profile_insts.csv)\n")
    print("Conditionals that never varied - candidates to hoist/remove "
          "(UM08025 §5.19):")
    for fetched, fn, addr, asm, kind in biased[:top]:
        print(f"- `{short(fn)}` @{addr} {kind} ({fetched:,}x): `{asm[:50]}`")


def suggestions(funcs, totals, tshare, cshare, exclude):
    """Rule-based optimization hints (after UM08025 §5.19)."""
    print("\n## Optimization hints")
    total_fetch = totals.get("fetch") or 1
    ex = [n for n in funcs if exclude and re.search(exclude, n)]
    ex_fetch = sum(funcs[n]["fetch"] for n in ex)
    if ex:
        print(f"- excluded from load ranking (--exclude): {len(ex)} functions, "
              f"{100.0 * ex_fetch / total_fetch:.1f}% of fetches")
    rest = {n: v for n, v in funcs.items() if n not in ex}
    rt = sum(v["fetch"] for v in rest.values()) or 1
    hot = sorted(rest.items(), key=lambda kv: -kv[1]["fetch"])[:5]
    print("- top load after exclusion: " + ", ".join(
        f"`{short(n)}` {100.0 * v['fetch'] / rt:.1f}%" for n, v in hot))
    polls = [(n, v) for n, v in rest.items()
             if v["fetch"] / total_fetch > 0.02 and v["run"]
             and v["fetch"] / v["run"] < 40]
    if polls:
        print("- busy-poll candidates (>2% load, <40 instr/entry - called in a "
              "tight loop; consider event-driven or rate-limiting):")
        for n, v in sorted(polls, key=lambda kv: -kv[1]["fetch"]):
            print(f"  - `{short(n)}`: {v['run']:,} calls, "
                  f"{v['fetch'] / v['run']:.0f} instr/call, "
                  f"{100.0 * v['fetch'] / total_fetch:.1f}% load")
    if tshare:
        stalls = []
        for n, ts in tshare.items():
            cs = cshare.get(n, 0)
            if ts > 0.01 and cs and ts / cs > 4:
                stalls.append((n, ts, ts / cs))
        if stalls:
            print("- stall/wait-dominated (time share >> instruction share - "
                  "waiting on hardware, consider DMA/IRQ instead of polling):")
            for n, ts, ratio in sorted(stalls, key=lambda x: -x[1])[:5]:
                print(f"  - `{short(n)}`: {100 * ts:.1f}% of time, "
                      f"{ratio:.0f}x its instruction share")


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("capture_dir", help="etm_capture.py output dir")
    p.add_argument("--top", type=int, default=10, help="table size")
    p.add_argument("--elf", help="firmware ELF, enables itrace analyses")
    p.add_argument("--isr", help="entry[,body..] symbols for ISR episode timing")
    p.add_argument("--exclude", help="regex of functions to exclude from the "
                   "optimization load ranking (e.g. the idle poll loop)")
    args = p.parse_args()

    profile = os.path.join(args.capture_dir, "code_profile.txt")
    itrace = os.path.join(args.capture_dir, "itrace.csv")
    funcs, totals = parse_profile(profile)
    total_fetch = totals.get("fetch") or 1
    hot = sorted(funcs.items(), key=lambda kv: kv[1]["fetch"], reverse=True)[:args.top]

    print(f"# ETM profile: {args.capture_dir}\n")
    print(f"## Top {args.top} hottest functions (instruction-fetch share)\n")
    print("| # | Function | Module | Run Count | Fetch Count | Load % |")
    print("|---|----------|--------|-----------|-------------|--------|")
    for i, (name, v) in enumerate(hot, 1):
        print(f"| {i} | `{short(name)}` | {v['module']} | {v['run']:,} "
              f"| {v['fetch']:,} | {100.0 * v['fetch'] / total_fetch:.2f}% |")
    print(f"\nTotal fetches {totals.get('fetch', 0):,} "
          f"(runs {totals.get('run', 0):,}, unaccounted {totals.get('unaccounted', 0):,})\n")

    ic, sc = totals.get("inst_cov"), totals.get("src_cov")
    print("## Coverage (NOPs excluded)\n")
    if ic:
        print(f"- instructions fully executed: {ic[0]:,} / {ic[1]:,} "
              f"({100.0 * ic[0] / ic[1]:.1f}%)")
    if sc:
        print(f"- source lines fully covered:  {sc[0]:,} / {sc[1]:,} "
              f"({100.0 * sc[0] / sc[1]:.1f}%)")
    partial = [n for n, v in funcs.items()
               if v["fetch"] > 0 and 0 < v.get("inst_pct", 100) < 100]
    print(f"- executed but only partially covered: {len(partial)} functions")
    dead = sorted(n for n, v in funcs.items()
                  if v["fetch"] == 0 and "(always inlined)" not in n)
    print(f"- never-executed out-of-line functions: {len(dead)}")
    by_mod = {}
    for n in dead:
        by_mod.setdefault(funcs[n]["module"], []).append(short(n))
    for mod in sorted(by_mod, key=str):
        print(f"  - {mod}: {', '.join('`%s`' % f for f in by_mod[mod])}")

    tshare, cshare = {}, {}
    if os.path.isfile(itrace) and args.elf:
        syms = load_symbols(args.elf)
        tf, cf, n = time_by_func(itrace, syms)
        tt, tc = sum(tf.values()) or 1, sum(cf.values()) or 1
        tshare = {k: v / tt for k, v in tf.items()}
        cshare = {k: v / tc for k, v in cf.items()}
        unit = itrace_unit(itrace)
        print(f"\n## Instruction history (itrace.csv, unit '{unit}')\n")
        if not tf:
            print(f"- {n:,} instructions, NO timestamps (--no-timestamps "
                  f"capture): time shares unavailable, top {args.top} by "
                  f"instruction share:")
            for name, cs in sorted(cshare.items(), key=lambda kv: -kv[1])[:args.top]:
                print(f"  - `{short(name)}`: {100 * cs:.1f}% instructions")
        else:
            print(f"- {n:,} instructions; top {args.top} by TIME share "
                  f"(vs instruction share):")
            for name, ts in sorted(tshare.items(), key=lambda kv: -kv[1])[:args.top]:
                print(f"  - `{short(name)}`: {100 * ts:.1f}% time, "
                      f"{100 * cshare.get(name, 0):.1f}% instructions")

    lines_csv = os.path.join(args.capture_dir, "profile_lines.csv")
    if os.path.isfile(lines_csv):
        def n(v):  # Ozone groups thousands with spaces
            v = (v or "").replace(" ", "")
            return int(v) if v.isdigit() else 0
        with open(lines_csv, newline="", errors="replace") as f:
            rows = [r for r in csv.DictReader(f)
                    if r.get("File") and n(r.get("Instructions Fetched")) > 0]
        rows.sort(key=lambda r: -n(r["Instructions Fetched"]))
        print(f"\n## Hottest source lines (profile_lines.csv)\n")
        for r in rows[:args.top]:
            src = (r.get("Content") or "").strip()
            print(f"- {os.path.basename(r['File'])}:{r['Line']}  "
                  f"{n(r['Instructions Fetched']):,} fetches  `{src[:60]}`")

    insts_csv = os.path.join(args.capture_dir, "profile_insts.csv")
    if os.path.isfile(insts_csv):
        branch_bias(insts_csv, funcs, args.top)

    if args.isr:
        if not (os.path.isfile(itrace) and args.elf):
            sys.exit("error: --isr needs itrace.csv (--trace-csv capture) and --elf")
        isr_report(itrace, args.elf, args.isr, args.top)

    suggestions(funcs, totals, tshare, cshare, args.exclude)
    return 0


if __name__ == "__main__":
    sys.exit(main())
