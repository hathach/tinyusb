#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
import contextlib, io, json, os, re, shlex, shutil, signal, subprocess, sys, tempfile, time, unittest, unittest.mock
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import sysview_ci

def metrics(overflow=0, funcs=None, isr=None, contexts=None, stack=None, live_window_s=14.2):
    return {"contexts": contexts or [], "isr": isr or [], "ready_run": [],
            "functions": funcs or [], "markers": [], "stack": stack or [],
            "heap": None, "overflow": overflow, "overflow_total": overflow,
            "dropped_pairs": 0, "live_window_s": live_window_s,
            "warnings": []}

def board_json(board="stm32f407disco", err=None, workload_ok=None, **mk):
    cap = {"live_window_s": 14.2}
    if workload_ok is not None:
        cap["workload_ok"] = workload_ok
    return {"board": board, "commit": "abc1234", "example": "device/cdc_msc",
            "workload": "cdc_burst", "duration_s": 15,
            "capture": cap,
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
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)
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

    def test_workload_died_gates_all_metrics(self):
        p = board_json(workload_ok=False, funcs=[F("tud_task",22000,7.4)],
                        isr=[I("ISR 83",24000,6.7)])
        md = self.go([board_json(funcs=[F("tud_task",22000,7.4)], isr=[I("ISR 83",24000,6.7)])],[p])
        self.assertIn("⚠︎ workload", md)
        self.assertNotIn("+", md.split("tud_task")[1].split("\n")[0])  # no delta on gated row

    def test_capture_failed_board(self):
        md = self.go([board_json()],[board_json(err="flash failed: rc=1")])
        self.assertIn("capture failed: flash failed: rc=1", md)

    def test_capture_error_cannot_add_markdown_structure(self):
        err = "build failed: x.c:1: error\n# not a heading\n```\n<details><b>x</b>\n"
        section = self.go([board_json()], [board_json(err=err)]).split("<sub>")[0]
        line = next(l for l in section.splitlines() if l.startswith("capture failed:"))
        self.assertEqual(line, "capture failed: build failed: x.c:1: error # not a heading "
                               "&#96;&#96;&#96; &lt;details&gt;&lt;b&gt;x&lt;/b&gt;")
        self.assertNotIn("<details>", section)

    def test_non_string_capture_error_renders_capture_failed(self):
        # a malformed capture JSON must not abort the whole report
        md = self.go([board_json()], [board_json(err=123)])
        self.assertIn("capture failed: 123", md)

    def test_missing_metrics_key_renders_capture_failed(self):
        # a capture JSON with no "metrics" key and no "error" must not abort the whole report
        p = {"board": "stm32f407disco", "commit": "abc1234", "example": "device/cdc_msc",
             "workload": "cdc_burst", "duration_s": 15, "capture": {}, "error": None}
        md = self.go([board_json()], [p])
        self.assertIn("capture failed", md)

    def test_missing_base_metrics_key_does_not_crash_chart(self):
        # Round-2 residual: gate()'s own `side["metrics"]` bracket access raised KeyError for a
        # malformed BASE-side JSON with no "metrics" key and no "error" -- every OTHER gate()
        # call site happens to short-circuit around it when the matching baseline entry is
        # simply absent (bm/b is None), but the chart's `gate(base_j, None)` in base_ok calls
        # straight into gate() with no such guard, so THIS was the one path that still crashed.
        b = {"board": "stm32f407disco", "commit": "abc1234", "example": "device/cdc_msc",
             "workload": "cdc_burst", "duration_s": 15, "capture": {}, "error": None}
        p = board_json(funcs=[F("tud_task", 22000, 7.4)])
        md = self.go([b], [p])  # must not raise
        self.assertIn("```mermaid", md)          # PR-only chart still renders
        self.assertEqual(md.count("bar ["), 1)   # single series: base is gated "failed"

    def test_no_time_base_gates_all_metrics(self):
        p = board_json(live_window_s=None, funcs=[F("tud_task",22000,7.4)],
                        isr=[I("ISR 83",24000,6.7)])
        md = self.go([board_json(funcs=[F("tud_task",22000,7.4)], isr=[I("ISR 83",24000,6.7)])],[p])
        self.assertIn("⚠︎ no time base", md)
        self.assertNotIn("+", md.split("tud_task")[1].split("\n")[0])  # no delta on gated row

    def test_p99_withheld_at_low_n_p50_shown(self):
        # pct()'s p99 index lands on (or next to) the max sample at n<=100, so it must be
        # withheld even though p50 (GATE_MIN_N=50) is well-supported and stays visible.
        p = board_json(isr=[I("ISR 83", 60, 6.7)])
        md = self.go([board_json(isr=[I("ISR 83", 60, 6.7)])], [p])
        self.assertIn("6.7 / – ⚠︎ n=60", md)

    def test_p99_shown_at_high_n(self):
        p = board_json(isr=[I("ISR 83", 500, 6.7)])
        md = self.go([board_json(isr=[I("ISR 83", 500, 6.7)])], [p])
        self.assertIn("6.7 / 9.7", md)  # I() sets p99_us = p50_us + 3

    def test_cpu_and_stack_rows_gated_on_overflow(self):
        # CPU-load and stack rows are gated like every other metric
        p = board_json(overflow=3, contexts=[{"name": "usbd", "cpu_pct": 7.0}],
                        stack=[{"name": "cdc", "bytes_used": 300}])
        b = board_json(contexts=[{"name": "usbd", "cpu_pct": 6.5}],
                        stack=[{"name": "cdc", "bytes_used": 290}])
        md = self.go([b], [p])
        cpu_line = next(l for l in md.splitlines() if l.startswith("| CPU load"))
        stack_line = next(l for l in md.splitlines() if "stack high-water" in l)
        self.assertIn("⚠︎ overflow 3", cpu_line)
        self.assertIn("⚠︎ overflow 3", stack_line)
        self.assertIn("6.5 %", cpu_line)   # base side (ungated) still shows its real value
        self.assertIn("290", stack_line)

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

    def test_chart_excludes_low_n_pr_function(self):
        funcs = [F("tud_task",22000,7.4), F("mscd_xfer_cb",9,13.0)]
        md = self.go([board_json(funcs=funcs)],[board_json(funcs=funcs)])
        chart = md.split("```mermaid")[1].split("```")[0]
        xaxis = chart.split("x-axis")[1].split("]")[0]
        self.assertIn("task", xaxis)
        self.assertNotIn("xfer_cb", xaxis)                           # n<50 -> excluded from chart
        self.assertIn("⚠︎ n=9", md)                                   # table still shows the gated row

    def test_chart_single_series_on_base_overflow(self):
        b = board_json(overflow=2, funcs=[F("tud_task",22000,7.4)])
        p = board_json(funcs=[F("tud_task",22000,7.4)])
        md = self.go([b],[p])
        chart = md.split("```mermaid")[1].split("```")[0]
        self.assertEqual(chart.count("bar ["), 1)                    # single PR-only series
        table = md.split("```mermaid")[0]
        self.assertIn("⚠︎ overflow 2", table)                        # table still shows base gate cell

    def test_cpu_load_new_when_context_missing_from_present_baseline(self):
        b = board_json(funcs=[F("tud_task",22000,7.4)])               # baseline exists, no contexts
        p = board_json(funcs=[F("tud_task",22000,7.4)],
                        contexts=[{"name": "usbd", "cpu_pct": 7.0}])
        md = self.go([b],[p])
        self.assertIn("| CPU load, whole recording (usbd) | — | 7.0 % | new |", md)

    def test_unmeasured_pr_rows_are_not_gone(self):
        base = board_json(funcs=[F("tud_task", 500, 5.0)], isr=[I("ISR 83", 500, 6.0)],
                          stack=[{"name": "usbd", "bytes_used": 300}])
        pr = board_json(live_window_s=None)
        for k in ("functions", "isr", "stack"):
            pr["metrics"][k] = None
        md = self.go([base], [pr])
        self.assertNotIn("**gone**", md.split("<sub>")[0])
        self.assertIn("| `tud_task` p50 | 5.0 µs | – ⚠︎ no time base | — |", md)

    def test_unmeasured_base_rows_are_not_new(self):
        base = board_json()
        for k in ("functions", "isr", "contexts", "stack"):
            base["metrics"][k] = None
        pr = board_json(funcs=[F("tud_task", 22000, 7.4)], isr=[I("ISR 83", 24000, 6.7)],
                        contexts=[{"name": "usbd", "cpu_pct": 7.0}],
                        stack=[{"name": "cdc", "bytes_used": 300}])
        table = self.go([base], [pr]).split("```mermaid")[0]
        self.assertNotIn("new", table)
        self.assertIn("| `tud_task` p50 | – ⚠︎ not measured | 7.4 µs | — |", table)
        self.assertIn("| ISR 83 p50 / p99 | – ⚠︎ not measured / – ⚠︎ not measured / "
                      "– ⚠︎ not measured µs | 6.7 / 9.7 / 60.3 µs | — / — |", table)
        self.assertIn("| CPU load, whole recording (usbd) | – ⚠︎ not measured | 7.0 % | — |", table)
        self.assertIn("| `cdc` stack high-water | – ⚠︎ not measured | 300 B | — |", table)

    def test_row_missing_from_measured_base_table_is_new(self):
        pr = board_json(funcs=[F("tud_task", 22000, 7.4)], isr=[I("ISR 83", 24000, 6.7)],
                        stack=[{"name": "cdc", "bytes_used": 300}])
        md = self.go([board_json()], [pr])
        self.assertIn("| `tud_task` p50 | — | 7.4 µs | new |", md)
        self.assertIn("| ISR 83 p50 / p99 | — / — / — µs | 6.7 / 9.7 / 60.3 µs | new / new |", md)
        self.assertIn("| `cdc` stack high-water | — | 300 B | new |", md)

    def test_report_warnings_are_shown(self):
        j = board_json()
        j["metrics"]["warnings"] = ["SystemView showed 3 error dialog(s)"]
        md = self.go([], [j])
        self.assertIn("> ⚠︎ SystemView showed 3 error dialog(s)\n\n| metric |", md)

    def test_park_error_is_shown_as_a_warning(self):
        j = board_json()
        j["capture"]["park_error"] = "park reflash failed rc=1"
        md = self.go([], [j])
        self.assertIn("> ⚠︎ park reflash failed rc=1\n\n| metric |", md)

    def test_heading_shows_live_window(self):
        md = self.go([board_json()], [board_json()])
        self.assertIn("### stm32f407disco — live 14.2 s", md)

    def test_heading_prefers_metrics_live_window(self):
        md = self.go([], [board_json(live_window_s=12.3)])
        self.assertIn("### stm32f407disco — live 12.3 s", md)

    def test_heading_falls_back_when_metrics_window_is_missing(self):
        for m in (None, {}, {"overflow": 0}):
            with self.subTest(metrics=m):
                j = board_json()
                j["metrics"] = m
                self.assertIn("### stm32f407disco — live 14.2 s", self.go([], [j]))

    def test_heading_omits_unknown_metrics_window(self):
        md = self.go([], [board_json(live_window_s=None)])
        self.assertIn("### stm32f407disco\n", md)

    # ---- base-only rows (present in base, absent from PR) must render, not vanish ----

    def test_base_only_function_row_flagged_gone(self):
        b = board_json(funcs=[F("tud_task",22000,7.4), F("old_fn",22000,3.0)])
        p = board_json(funcs=[F("tud_task",22000,7.4)])                # old_fn dropped
        md = self.go([b],[p])
        self.assertIn("| `old_fn` p50 | 3.0 µs | — | **gone** |", md)

    def test_base_only_isr_row_flagged_gone(self):
        b = board_json(isr=[I("ISR 83", 24000, 6.7)])
        p = board_json()                                               # ISR 83 dropped
        md = self.go([b],[p])
        line = next(l for l in md.splitlines() if l.startswith("| ISR 83"))
        self.assertIn("6.7 / 9.7 / 60.3 µs", line)                     # base half still shown (p50/p99/max)
        self.assertIn("**gone** / **gone**", line)                      # delta flags disappearance

    def test_base_only_context_row_flagged_gone(self):
        b = board_json(contexts=[{"name": "usbd", "cpu_pct": 6.5}])
        p = board_json()                                                # usbd context dropped
        md = self.go([b],[p])
        self.assertIn("| CPU load, whole recording (usbd) | 6.5 % | — | **gone** |", md)

    def test_base_only_stack_row_flagged_gone(self):
        b = board_json(stack=[{"name": "cdc", "bytes_used": 300}])
        p = board_json()                                                # cdc stack row dropped
        md = self.go([b],[p])
        self.assertIn("| `cdc` stack high-water | 300 B | — | **gone** |", md)

    def test_gone_row_not_flagged_when_base_itself_gated(self):
        # can't confidently call a row "gone" if the base side that would prove it existed is
        # itself unusable (overflow here) -- must fall back to the ordinary withheld cell.
        b = board_json(overflow=3, funcs=[F("old_fn",22000,3.0)])
        p = board_json()
        md = self.go([b],[p])
        line = next(l for l in md.splitlines() if "old_fn" in l)
        self.assertNotIn("gone", line)
        self.assertIn("⚠︎ overflow 3", line)

    # ---- a zero (or rounds-to-zero) base must not hide a real jump behind "—" ----

    def test_zero_base_nonzero_pr_flagged(self):
        b = board_json(funcs=[F("tud_task",22000,0.0)])
        p = board_json(funcs=[F("tud_task",22000,5.0)])
        md = self.go([b],[p])
        line = next(l for l in md.splitlines() if "tud_task" in l)
        self.assertIn("**+∞%**", line)

    def test_zero_base_zero_pr_stays_dash(self):
        b = board_json(funcs=[F("tud_task",22000,0.0)])
        p = board_json(funcs=[F("tud_task",22000,0.0)])
        md = self.go([b],[p])
        line = next(l for l in md.splitlines() if "tud_task" in l)
        self.assertTrue(line.rstrip().endswith("| — |"), line)

    # ---- an all-zero charted series must not emit a broken "0 --> 0" mermaid axis ----

    def test_chart_skipped_when_every_charted_value_is_zero(self):
        funcs = [F("tud_task", 1000, 0.0)]
        md = self.go([board_json(funcs=funcs)],[board_json(funcs=funcs)])
        self.assertNotIn("```mermaid", md)
        self.assertIn("tud_task", md)                                   # table row still present

class P99Threshold(unittest.TestCase):
    """p99 is withheld at n == P99_MIN_N and shown just above it."""
    def test_withheld_at_boundary_n(self):
        side = {"metrics": {"live_window_s": 14.2, "overflow": 0}, "capture": {}}
        self.assertEqual(sysview_ci.gate_p99(side, {"n": sysview_ci.P99_MIN_N}),
                          f"n={sysview_ci.P99_MIN_N}")

    def test_shown_just_above_boundary(self):
        side = {"metrics": {"live_window_s": 14.2, "overflow": 0}, "capture": {}}
        self.assertIsNone(sysview_ci.gate_p99(side, {"n": sysview_ci.P99_MIN_N + 1}))

class CaptureSelection(unittest.TestCase):
    CFG = {"boards": [
        {"name": "a", "uid": "U1", "flasher": {"name": "openocd", "uid": "P1",
         "args": "-f interface/x.cfg -c \"transport select swd\" -f target/y.cfg"},
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
        # falls back to the flasher, quoting intact for rtt.py's shlex split
        self.assertEqual(shlex.split(sysview_ci.capture_ocd_args(a)),
                         ["-f", "interface/x.cfg", "-c", "transport select swd", "-f", "target/y.cfg"])
        self.assertIn("target/z.cfg", sysview_ci.capture_ocd_args(b))   # explicit override
        with self.assertRaises(ValueError):                              # jlink flasher, no override
            sysview_ci.capture_ocd_args({"flasher": {"name": "jlink", "args": "-device X"},
                                         "sysview": {}})

    def test_wrapper_error_shape(self):
        j = sysview_ci.board_result(self.CFG["boards"][0], "abc1234", error="flash failed: rc=1")
        self.assertEqual(j["error"], "flash failed: rc=1"); self.assertIsNone(j["metrics"])


class WorkloadCpuRow(unittest.TestCase):
    """When contexts_workload is present, the CPU-load row uses it -- busy share over the
    throughput window -- labelled as such, with deltas computed on that number."""
    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)
        self.base, self.pr = os.path.join(self.tmp, 'b'), os.path.join(self.tmp, 'p')

    def _wl(self, pct, busy):
        j = board_json()
        j["metrics"]["contexts_workload"] = [
            {"name": "usbd", "busy_ms": busy, "cpu_pct_workload": pct}]
        j["metrics"]["workload_window_s"] = 14.0
        j["metrics"]["workload_anchor"] = "cdc-read-span"
        return j

    def test_workload_number_preferred_and_labelled(self):
        write_set(self.base, self._wl(25.0, 500.0))
        write_set(self.pr, self._wl(35.0, 700.0))
        md = sysview_ci.report(self.base, self.pr)
        self.assertIn("| CPU load, workload win (usbd) | 25.0 % | 35.0 % | +10.0 pt |", md)

    def test_unmeasured_base_workload_is_not_new(self):
        for name, base_wl, cells in (("absent", ..., "– ⚠︎ not measured | 35.0 % | —"),
                                     ("null", None, "– ⚠︎ not measured | 35.0 % | —"),
                                     ("empty", [], "— | 35.0 % | new")):
            with self.subTest(name):
                base = board_json()
                if base_wl is not ...:
                    base["metrics"]["contexts_workload"] = base_wl
                write_set(self.base, base)
                write_set(self.pr, self._wl(35.0, 700.0))
                md = sysview_ci.report(self.base, self.pr)
                self.assertIn(f"| CPU load, workload win (usbd) | {cells} |", md)


class SystemExitDoesNotAbortRun(unittest.TestCase):
    """rtt.nm_rtt_addr() raises SystemExit when the target ELF has no _SEGGER_RTT symbol.
    SystemExit derives from BaseException, so flash_and_capture_one's blanket `except Exception`
    does not catch it: uncaught it escapes main()'s PHASE 2 loop and aborts the whole multi-board
    run, so boards captured before the bad one never get their results written in PHASE 3. A board
    hitting this must produce its own (None, error) result, and the next board must still be
    attempted."""

    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)
        import hil_flash
        from helper import agentrc, hil_util
        self.rtt = agentrc.load_file("skills/rtt/scripts/rtt.py")
        # a serial "node" that already exists, so the enumeration wait does not sleep
        node = os.path.join(self.tmp, "node")
        open(node, "w").close()
        for obj, attr, value in (
                (hil_util, "get_serial_dev", lambda *a, **k: node),
                (hil_flash, "flash_openocd", lambda board, fw: subprocess.CompletedProcess([], 0)),
                (hil_flash, "find_firmware", lambda *a, **k: os.path.join(self.tmp, "park.elf")),
                # no probe here: the session itself is refused
                (self.rtt, "OpenocdRtt", unittest.mock.Mock(side_effect=self.rtt.RttError("no probe")))):
            patcher = unittest.mock.patch.object(obj, attr, value)
            patcher.start()
            self.addCleanup(patcher.stop)

    def _board(self, name):
        return {"name": name, "uid": "U", "flasher": {"name": "openocd", "uid": "P",
                 "args": "-f interface/x.cfg -f target/y.cfg"},
                "sysview": {"example": "device/cdc_msc", "workload": "idle", "duration_s": 1}}

    def test_bad_elf_errors_without_raising_and_next_board_is_still_attempted(self):
        def no_symbol(elf):
            raise SystemExit(f"no _SEGGER_RTT in {elf}")
        with unittest.mock.patch.object(self.rtt, "nm_rtt_addr", no_symbol), \
                contextlib.redirect_stderr(io.StringIO()):
            capture, err = sysview_ci.flash_and_capture_one(
                self._board("bad"), os.path.join(self.tmp, "bad"), self.tmp)
        self.assertIsNone(capture)
        self.assertIn("no RTT symbol", err)

        called = []
        with unittest.mock.patch.object(self.rtt, "nm_rtt_addr", lambda elf: called.append(elf) or 0x20000000), \
                contextlib.redirect_stderr(io.StringIO()):
            capture, err = sysview_ci.flash_and_capture_one(
                self._board("good"), os.path.join(self.tmp, "good"), self.tmp)
        self.assertEqual(called, [os.path.join(self.tmp, "good") + ".elf"])
        self.assertIsNone(capture)
        self.assertIn("no probe", err)
        self.rtt.OpenocdRtt.assert_called_once_with(
            cfg="-f interface/x.cfg -f target/y.cfg", addr=0x20000000, channel=sysview_ci.SYSVIEW_CHANNEL,
            serial_no="P", vid_pid=None, reset_before_attach=True)

    def test_no_park_image_refuses_before_flashing(self):
        import hil_flash
        flashed = []
        with unittest.mock.patch.object(hil_flash, "find_firmware", lambda *a, **k: None), \
                unittest.mock.patch.object(hil_flash, "flash_openocd", lambda b, fw: flashed.append(fw)):
            capture, err = sysview_ci.flash_and_capture_one(self._board("b"), "fw.elf", self.tmp)
        self.assertIsNone(capture)
        self.assertIn("no park", err)
        self.assertEqual(flashed, [])

    def test_failed_flash_runs_the_dap_rescue(self):
        import hil_flash
        rescued = []
        with unittest.mock.patch.object(hil_flash, "flash_openocd",
                                        lambda b, fw: subprocess.CompletedProcess([], 1, stdout="DAP init failed")), \
                unittest.mock.patch.object(hil_flash, "rescue_openocd", lambda b, out: rescued.append(out)), \
                contextlib.redirect_stderr(io.StringIO()):
            capture, err = sysview_ci.flash_and_capture_one(self._board("b"), "fw.elf", self.tmp)
        self.assertIn("flash failed: rc=1", err)
        self.assertEqual(rescued, ["DAP init failed"])

    def test_a_reader_failure_fails_the_capture(self):
        conn = unittest.mock.Mock(eof=False)
        conn.read.side_effect = OSError(28, "No space left on device")
        board = self._board("b")
        board["sysview"]["duration_s"] = 0
        with unittest.mock.patch.object(self.rtt, "nm_rtt_addr", lambda elf: 0x20000000), \
                unittest.mock.patch.object(self.rtt, "OpenocdRtt", lambda **k: conn), \
                contextlib.redirect_stderr(io.StringIO()):
            capture, err = sysview_ci.flash_and_capture_one(board, os.path.join(self.tmp, "fw"), self.tmp)
        self.assertIsNone(capture)
        self.assertIn("No space left", err)
        conn.close.assert_called_once_with()

    def _park_fails(self, capture_conn):
        import hil_flash
        flashes = iter([subprocess.CompletedProcess([], 0), subprocess.CompletedProcess([], 1)])
        board = self._board("b")
        board["sysview"]["duration_s"] = 0
        with unittest.mock.patch.object(self.rtt, "nm_rtt_addr", lambda elf: 0x20000000), \
                unittest.mock.patch.object(self.rtt, "OpenocdRtt", lambda **k: capture_conn), \
                unittest.mock.patch.object(hil_flash, "flash_openocd", lambda b, fw: next(flashes)):
            return sysview_ci.flash_and_capture_one(board, os.path.join(self.tmp, "fw"), self.tmp)

    def test_a_failed_park_after_a_capture_is_kept_with_it(self):
        conn = unittest.mock.Mock(eof=False)
        conn.read.return_value = b""
        capture, err = self._park_fails(conn)
        self.assertIsNone(err)
        self.assertEqual(capture["park_error"], "park reflash failed rc=1")

    def test_a_failed_park_after_a_failed_capture_joins_its_error(self):
        conn = unittest.mock.Mock(eof=True)
        capture, err = self._park_fails(conn)
        self.assertIsNone(capture)
        self.assertEqual(err, "openocd RTT server closed during the capture; park reflash failed rc=1")


class EventMap(unittest.TestCase):
    """sysview_events.json names TU_SV_EVENT_BASE + each TU_SV_ID_* of tusb_debug.h, in enum
    order: a reordered or inserted id would otherwise mislabel every function row."""

    def test_matches_the_header_enum(self):
        root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
        with open(os.path.join(root, "src", "common", "tusb_debug.h")) as f:
            header = f.read()
        base = int(re.search(r"#define TU_SV_EVENT_BASE (\d+)", header).group(1))
        expected = {
            "TU_SV_ID_TUD_TASK": "tud_task",
            "TU_SV_ID_USBD_XFER": "usbd_edpt_xfer",
            "TU_SV_ID_DCD_XFER": "dcd_edpt_xfer",
            "TU_SV_ID_CDC_FLUSH": "tud_cdc_write_flush",
            "TU_SV_ID_CDC_READ": "tud_cdc_read",
            "TU_SV_ID_MSC_XFER": "mscd_xfer_cb",
            "TU_SV_ID_TUH_TASK": "tuh_task",
            "TU_SV_ID_HCD_XFER": "hcd_edpt_xfer",
        }
        enum = re.search(r"typedef enum \{([^{}]*TU_SV_ID_COUNT[^{}]*)\}", header, re.S).group(1)
        enum = re.sub(r"//[^\n]*|/\*.*?\*/", "", enum, flags=re.S)
        members = [member.strip() for member in enum.split(",") if member.strip()]
        self.assertEqual(members, ["TU_SV_ID_TUD_TASK = 0", *list(expected)[1:], "TU_SV_ID_COUNT"])
        with open(sysview_ci.EVENT_MAP) as f:
            names = json.load(f)
        self.assertEqual(names, {str(base + i): label for i, label in enumerate(expected.values())})

    def test_workload_anchor_is_tud_cdc_read(self):
        with open(sysview_ci.EVENT_MAP) as f:
            names = json.load(f)
        self.assertEqual(names[str(sysview_ci.workload_event())], "tud_cdc_read")


class SessionReset(unittest.TestCase):
    """A board flagged sysview.attach_only attaches without the capture session's own reset."""
    def test_openocd_board_resets(self):
        b = {"flasher": {"name": "openocd", "args": "-f target/rp2040.cfg"}, "sysview": {}}
        self.assertTrue(sysview_ci.session_resets(b))

    def test_attach_only_board_does_not_reset(self):
        b = {"flasher": {"name": "jlink", "args": "-device ATSAMD51J19"},
             "sysview": {"attach_only": True}}
        self.assertFalse(sysview_ci.session_resets(b))


class TestErrExcerpt(unittest.TestCase):
    def test_short_error_kept_whole(self):
        self.assertEqual(sysview_ci._err_excerpt("  boom  "), "boom")

    def test_long_error_keeps_both_ends(self):
        # cmake puts the diagnosis first and the call stack last; a tail-only slice
        # dropped the "Cannot find source file" line that names the real problem.
        err = "CMake Error: Cannot find source file: startup.s\n" + "x" * 4000 + "\nGenerate step failed."
        got = sysview_ci._err_excerpt(err)
        self.assertIn("Cannot find source file", got)
        self.assertIn("Generate step failed.", got)
        self.assertLess(len(got), len(err))

    def test_none_error_is_empty(self):
        self.assertEqual(sysview_ci._err_excerpt(None), "")


class CdcBurstWorkload(unittest.TestCase):
    """_workload_cdc_burst over a fake port and clock: each read waits out its 20 ms timeout."""

    def run_workload(self, echo_until, duration_s=3.0):
        clock = [0.0]
        class Port:
            def __init__(self, *a, **k): pass
            def write(self, b): pass
            def read(self, n):
                clock[0] += 0.02
                return b"x" * n if clock[0] < echo_until else b""
            def close(self): pass
        def sleep(t): clock[0] += t
        with unittest.mock.patch("serial.Serial", Port), \
                unittest.mock.patch("time.monotonic", lambda: clock[0]), \
                unittest.mock.patch("time.sleep", sleep):
            return sysview_ci._workload_cdc_burst("node", duration_s)

    def test_steady_echo_passes(self):
        self.assertTrue(self.run_workload(echo_until=float("inf")))

    def test_echo_that_stops_after_the_first_burst_fails(self):
        self.assertFalse(self.run_workload(echo_until=0.1))

    def test_no_echo_at_all_fails(self):
        self.assertFalse(self.run_workload(echo_until=0))


class CaptureBudget(unittest.TestCase):
    """--budget-s: a board whose flash, capture and park reflash may not fit in what is left is
    not flashed, so the caller's kill never lands before the park image is back."""

    def run_capture(self, budget_s, capture=(None, "captured")):
        from helper import hil_lock
        tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, tmp, True)
        cfg = os.path.join(tmp, "cfg.json")
        with open(cfg, "w") as f:
            json.dump({"boards": [{"name": "b", "uid": "U", "flasher": {"name": "openocd", "uid": "P", "args": ""},
                                   "sysview": {"example": "device/cdc_msc", "workload": "idle",
                                               "duration_s": 15}}]}, f)
        out = os.path.join(tmp, "out")
        captured = []
        argv = ["sysview_ci.py", "capture", cfg, "--out", out, "--budget-s", str(budget_s)]
        with unittest.mock.patch.object(sys, "argv", argv), \
                unittest.mock.patch.object(sysview_ci, "build_one", lambda b, r: ("fw.elf", None)), \
                unittest.mock.patch.object(sysview_ci, "flash_and_capture_one",
                                           lambda b, fw, d: captured.append(b["name"]) or capture), \
                unittest.mock.patch.object(hil_lock, "acquire_board_lock", lambda *a, **k: None), \
                contextlib.redirect_stdout(io.StringIO()), self.assertRaises(SystemExit):
            sysview_ci.main()
        with open(os.path.join(out, "sysview-b.json")) as f:
            return captured, json.load(f)["error"]

    def test_a_board_that_cannot_finish_in_the_budget_is_not_flashed(self):
        captured, err = self.run_capture(budget_s=60)
        self.assertEqual(captured, [])
        self.assertIn("skipped", err)

    def test_a_board_that_fits_is_captured(self):
        board = {"sysview": {"duration_s": 15}}
        captured, err = self.run_capture(budget_s=sysview_ci.capture_bound_s(board) + 60)
        self.assertEqual(captured, ["b"])
        self.assertEqual(err, "captured")

    def test_capture_bound_reserves_a_reap_grace_per_flasher_call(self):
        from helper import hil_util
        board = {"sysview": {"duration_s": 15}}
        with unittest.mock.patch.object(hil_util, "CMD_TIMEOUT", 100), \
                unittest.mock.patch.object(hil_util, "REAP_GRACE", 7):
            self.assertEqual(sysview_ci.capture_bound_s(board), 3 * (100 + 7) + sysview_ci.CAPTURE_FIXED_S + 15)

    def test_report_is_on_disk_before_decoding(self):
        on_disk = []

        def decode(board, commit, out_dir, capture, deadline):
            with open(os.path.join(out_dir, "sysview-b.json")) as f:
                on_disk.append(json.load(f)["error"])
            return sysview_ci.board_result(board, commit, error="decoded")
        board = {"sysview": {"duration_s": 15}}
        with unittest.mock.patch.object(sysview_ci, "decode_one", decode):
            captured, err = self.run_capture(budget_s=sysview_ci.capture_bound_s(board) + 60,
                                             capture=({"raw": "r", "workload_ok": True}, None))
        self.assertIn("decode did not finish", on_disk[0])
        self.assertEqual(err, "decoded")


class DecodeDeadline(unittest.TestCase):
    """decode_one cuts each decode step at the --budget-s deadline main() passes it."""
    BOARD = {"name": "b", "sysview": {"example": "device/cdc_msc", "workload": "idle", "duration_s": 15}}

    @staticmethod
    def step(returncode=1, stdout="", stderr="boom", communicate=None):
        """A Popen stand-in whose communicate() returns (stdout, stderr), or follows communicate."""
        p = unittest.mock.Mock(pid=4321, returncode=returncode)
        p.communicate.side_effect = communicate or [(stdout, stderr)]
        return p

    def decode(self, deadline, *steps):
        popen = unittest.mock.Mock(side_effect=list(steps))
        with unittest.mock.patch("subprocess.Popen", popen), \
                unittest.mock.patch("os.killpg") as killpg, \
                unittest.mock.patch("helper.agentrc.path_of", lambda p: p):
            err = sysview_ci.decode_one(self.BOARD, "c", tempfile.gettempdir(),
                                        {"raw": "r", "workload_ok": True}, deadline)["error"]
        return err, popen, killpg

    def test_timeout_is_capped_by_the_deadline(self):
        step = self.step()
        err, _, _ = self.decode(time.monotonic() + 30, step)
        self.assertIn("record decode failed", err)
        self.assertLessEqual(step.communicate.call_args.kwargs["timeout"], 30)

    def test_no_deadline_keeps_the_step_timeout(self):
        step = self.step()
        self.decode(None, step)
        self.assertEqual(step.communicate.call_args.kwargs["timeout"], 900)

    def test_the_second_step_gets_what_the_first_left(self):
        clock = [1000.0]
        def record_takes_20_s(timeout):
            clock[0] += 20
            return "", ""
        record = self.step(returncode=0, communicate=record_takes_20_s)
        report = self.step()
        with unittest.mock.patch("time.monotonic", lambda: clock[0]):
            err, _, _ = self.decode(clock[0] + 30, record, report)
        self.assertIn("decode failed", err)
        self.assertLessEqual(report.communicate.call_args.kwargs["timeout"], 10)

    def test_a_spent_budget_skips_the_decode(self):
        err, popen, _ = self.decode(time.monotonic() - 1)
        self.assertIn("decode skipped", err)
        popen.assert_not_called()

    def test_a_cut_decoder_gets_sigterm_then_its_group_sigkill(self):
        # SIGKILL alone would skip sysview_record.py's handler, which records the failure
        cut = subprocess.TimeoutExpired("sysview_record.py", 30)
        step = self.step(returncode=None, communicate=[cut, cut, ("", "")])
        order = unittest.mock.Mock()
        step.terminate.side_effect = lambda: order.terminate()
        with unittest.mock.patch("os.killpg", side_effect=lambda pid, sig: order.killpg(pid, sig)):
            popen = unittest.mock.Mock(return_value=step)
            with unittest.mock.patch("subprocess.Popen", popen), \
                    unittest.mock.patch("helper.agentrc.path_of", lambda p: p):
                err = sysview_ci.decode_one(self.BOARD, "c", tempfile.gettempdir(),
                                            {"raw": "r", "workload_ok": True}, time.monotonic() + 30)["error"]
        self.assertIn("decode timed out", err)
        self.assertTrue(popen.call_args.kwargs["start_new_session"])
        self.assertEqual(order.mock_calls, [unittest.mock.call.terminate(),
                                            unittest.mock.call.killpg(4321, signal.SIGKILL)])
        self.assertEqual(step.communicate.call_args_list[1].kwargs["timeout"], sysview_ci.DECODE_STOP_S)
        from helper import hil_util
        self.assertEqual(step.communicate.call_args_list[2].kwargs["timeout"], hil_util.REAP_GRACE)

    def test_a_step_cut_by_its_own_timeout_is_not_blamed_on_the_budget(self):
        cut = subprocess.TimeoutExpired("sysview_record.py", 900)
        err, _, _ = self.decode(time.monotonic() + 5000, self.step(returncode=None, communicate=[cut, ("", "")]))
        self.assertIn("decode timed out after 900 s", err)
        self.assertNotIn("--budget-s", err)

    def test_a_decoder_that_exits_on_sigterm_is_not_sigkilled(self):
        cut = subprocess.TimeoutExpired("sysview_record.py", 30)
        step = self.step(returncode=None, communicate=[cut, ("", "")])
        err, _, killpg = self.decode(time.monotonic() + 30, step)
        self.assertIn("decode timed out", err)
        step.terminate.assert_called_once()
        killpg.assert_not_called()


class DecodeStopsARealChild(unittest.TestCase):
    """A real decoder stand-in cut by the deadline: SIGTERM lets it clean up, SIGKILL takes its group."""
    BOARD = DecodeDeadline.BOARD

    def run_child(self, setup):
        """Run setup in the stand-in, which then signals ready and sleeps; its 2 s timeout starts
        only once it is ready, so a slow start cannot take SIGTERM before setup has run."""
        tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, tmp, True)
        script, ready = os.path.join(tmp, "decoder.py"), os.path.join(tmp, "ready")
        with open(script, "w") as f:
            f.write("import os, signal, subprocess, sys, time\n"
                    f"TMP = {tmp!r}\n" + setup +
                    f"open({ready!r}, 'w').close()\n"
                    "time.sleep(60)\n")
        real_popen = subprocess.Popen

        def popen_until_ready(*args, **kwargs):
            p = real_popen(*args, **kwargs)
            for _ in range(1500):
                if os.path.exists(ready) or p.poll() is not None:
                    break
                time.sleep(0.02)
            return p
        with unittest.mock.patch("helper.agentrc.path_of", lambda p: script), \
                unittest.mock.patch("subprocess.Popen", popen_until_ready):
            start = time.monotonic()
            err = sysview_ci.decode_one(self.BOARD, "c", tmp, {"raw": "r", "workload_ok": True},
                                        time.monotonic() + 2)["error"]
        return err, tmp, time.monotonic() - start

    def test_sigterm_runs_the_decoders_cleanup(self):
        err, tmp, took = self.run_child(
            "def on_term(*_):\n"
            "    open(os.path.join(TMP, 'cleaned'), 'w').close()\n"
            "    sys.exit(3)\n"
            "signal.signal(signal.SIGTERM, on_term)\n")
        self.assertIn("decode timed out", err)
        self.assertTrue(os.path.exists(os.path.join(tmp, "cleaned")))
        self.assertLess(took, 10)

    def test_a_decoder_ignoring_sigterm_is_killed_with_its_group(self):
        with unittest.mock.patch.object(sysview_ci, "DECODE_STOP_S", 0.5):
            err, tmp, took = self.run_child(
                "signal.signal(signal.SIGTERM, signal.SIG_IGN)\n"
                "c = subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(60)'])\n"
                "open(os.path.join(TMP, 'child'), 'w').write(str(c.pid))\n")
        self.assertIn("decode timed out", err)
        self.assertLess(took, 10)
        with open(os.path.join(tmp, "child")) as f:
            child = int(f.read())
        for _ in range(100):
            try:
                with open(f"/proc/{child}/stat") as f:
                    if f.read().rsplit(")", 1)[1].split()[0] == "Z":
                        break
            except (FileNotFoundError, ProcessLookupError):
                break
            time.sleep(0.02)
        else:
            os.kill(child, signal.SIGKILL)
            self.fail(f"the decoder's child {child} outlived its group's SIGKILL")


class TestBuildOneFirmwarePath(unittest.TestCase):
    """build_one must return a path the flasher can actually open. JLinkExe's loadfile
    and OpenOCD's program both infer the image format from the extension, so an
    extension-less path dies with 'File is of unknown / unsupported format' -- and only
    after phase 2 has taken the board lock. Observed on stm32f407disco + raspberry_pi_pico."""

    def _board(self, flasher="jlink"):
        return {"name": "stm32f407disco", "flasher": {"name": flasher},
                "sysview": {"example": "device/cdc_msc"}}

    def _repo(self, artifacts):
        root = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, root, True)
        bdir = os.path.join(root, "examples", "cmake-build-sysview-stm32f407disco")
        os.makedirs(bdir)
        for a in artifacts:
            open(os.path.join(bdir, a), "w").close()
        return root

    def test_returns_existing_file_with_extension(self):
        root = self._repo(["cdc_msc.elf", "cdc_msc.bin", "cdc_msc.hex"])
        with unittest.mock.patch("subprocess.run",
                                 return_value=subprocess.CompletedProcess([], 0, "", "")):
            fw, err = sysview_ci.build_one(self._board(), root)
        self.assertIsNone(err)
        self.assertTrue(fw.endswith(".elf"), fw)
        self.assertTrue(os.path.exists(fw), fw)

    def test_missing_binary_is_an_error_not_a_bad_path(self):
        # a build that emitted nothing must fail in phase 1, before the board lock
        root = self._repo([])
        with unittest.mock.patch("subprocess.run",
                                 return_value=subprocess.CompletedProcess([], 0, "", "")):
            fw, err = sysview_ci.build_one(self._board(), root)
        self.assertIsNone(fw)
        self.assertIn("no .elf", err)

    def test_failed_build_reports_the_diagnostics_ninja_prints_on_stdout(self):
        failed = subprocess.CompletedProcess([], 1, "main.c:1:20: error: 'x' undeclared", None)
        with unittest.mock.patch("subprocess.run", return_value=failed) as run:
            fw, err = sysview_ci.build_one(self._board(), self._repo([]))
        self.assertIsNone(fw)
        self.assertIn("error: 'x' undeclared", err)
        self.assertEqual(run.call_args_list[0].kwargs["stdout"], subprocess.PIPE)
        self.assertEqual(run.call_args_list[0].kwargs["stderr"], subprocess.STDOUT)


class GetPicoSdk(unittest.TestCase):
    def _repo(self, checkout):
        root = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, root, True)
        os.makedirs(os.path.join(root, ".github", "actions", "get_deps"))
        with open(os.path.join(root, ".github", "actions", "get_deps", "action.yml"), "w") as f:
            f.write("        repository: raspberrypi/pico-sdk\n        ref: 2.3.1\n")
        if checkout:
            os.makedirs(os.path.join(root, "pico-sdk"))
        return root

    def _get(self, root, rc=0):
        env = {k: v for k, v in os.environ.items() if k != "PICO_SDK_PATH"}
        with unittest.mock.patch.dict(os.environ, env, clear=True), \
             unittest.mock.patch("subprocess.run",
                                 return_value=subprocess.CompletedProcess([], rc, "", "boom")) as run:
            err = sysview_ci._get_pico_sdk(root)
            sdk = os.environ.get("PICO_SDK_PATH")
        return err, sdk, [c.args[0] for c in run.call_args_list]

    def test_existing_checkout_refreshes_any_ref_kind(self):
        root = self._repo(checkout=True)
        path = os.path.join(root, "pico-sdk")
        err, sdk, cmds = self._get(root)
        self.assertIsNone(err)
        self.assertEqual(sdk, path)
        self.assertEqual(cmds, [["git", "-C", path, "fetch", "--depth", "1", "origin", "2.3.1"],
                                ["git", "-C", path, "checkout", "-q", "--detach", "FETCH_HEAD"]])

    def test_missing_checkout_is_cloned(self):
        root = self._repo(checkout=False)
        err, sdk, cmds = self._get(root)
        self.assertIsNone(err)
        self.assertEqual(sdk, os.path.join(root, "pico-sdk"))
        self.assertEqual(cmds, [["git", "clone", "--depth", "1", "--branch", "2.3.1",
                                 "https://github.com/raspberrypi/pico-sdk.git",
                                 os.path.join(root, "pico-sdk")]])

    def test_failed_refresh_is_an_error_and_leaves_the_path_unset(self):
        err, sdk, cmds = self._get(self._repo(checkout=True), rc=1)
        self.assertIsNone(sdk)
        self.assertEqual(len(cmds), 1)
        self.assertIn("pico-sdk 2.3.1: git -C", err)
        self.assertIn("failed: boom", err)


if __name__ == '__main__':
    unittest.main()
