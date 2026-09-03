#!/usr/bin/env python3
import json, os, shutil, subprocess, sys, tempfile, unittest, unittest.mock
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import sysview_ci

def metrics(overflow=0, funcs=None, isr=None, contexts=None, stack=None, live_window_s=14.2):
    return {"contexts": contexts or [], "isr": isr or [], "ready_run": [],
            "functions": funcs or [], "markers": [], "stack": stack or [],
            "heap": None, "overflow": overflow, "overflow_total": overflow,
            "dropped_pairs": 0, "live_window_s": live_window_s,
            "warnings": []}

def board_json(board="stm32f407disco", err=None, workload_ok=None, **mk):
    cap = {"route": "openocd-rtt", "poll_ms": 1, "live_window_s": 14.2}
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

    def test_missing_metrics_key_renders_capture_failed(self):
        # F4: an older/malformed capture JSON with no "metrics" key at all (and no "error"
        # set either) used to raise KeyError in board_section() and abort the WHOLE report --
        # no comment posted for ANY board, not just this one.
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
        # F1: pct()'s p99 index lands on (or next to) the max sample at n<=100, so it must be
        # withheld even though p50 (GATE_MIN_N=50) is well-supported and stays visible.
        p = board_json(isr=[I("ISR 83", 60, 6.7)])
        md = self.go([board_json(isr=[I("ISR 83", 60, 6.7)])], [p])
        self.assertIn("6.7 / – ⚠︎ n=60", md)

    def test_p99_shown_at_high_n(self):
        p = board_json(isr=[I("ISR 83", 500, 6.7)])
        md = self.go([board_json(isr=[I("ISR 83", 500, 6.7)])], [p])
        self.assertIn("6.7 / 9.7", md)  # I() sets p99_us = p50_us + 3

    def test_cpu_and_stack_rows_gated_on_overflow(self):
        # F2b: CPU-load/stack rows used to bypass gate() entirely, publishing numbers on a
        # board whose every other metric was voided by overflow.
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

    def test_heading_shows_live_window(self):
        md = self.go([board_json()], [board_json()])
        self.assertIn("### stm32f407disco — live 14.2 s", md)

    # ---- N2: base-only rows (present in base, absent from PR) must render, not vanish ----

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

    # ---- N3: a zero (or rounds-to-zero) base must not hide a real jump behind "—" ----

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

    # ---- N7: an all-zero charted series must not emit a broken "0 --> 0" mermaid axis ----

    def test_chart_skipped_when_every_charted_value_is_zero(self):
        funcs = [F("tud_task", 1000, 0.0)]
        md = self.go([board_json(funcs=funcs)],[board_json(funcs=funcs)])
        self.assertNotIn("```mermaid", md)
        self.assertIn("tud_task", md)                                   # table row still present

class P99Threshold(unittest.TestCase):
    """N13: pct()'s corrected nearest-rank index (sysview_report.py) makes p99 distinct from the
    max sample at every n>=2, so P99_MIN_N is no longer justified as "avoid colliding with the
    max" -- gate_p99's docstring and P99_MIN_N's own comment now cite tail-estimate reliability
    instead. This locks the boundary itself so a silent future change notices here too."""
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


class SystemExitDoesNotAbortRun(unittest.TestCase):
    """W1: sysview_record.rtt_cb_from_elf() sys.exit()s when the target ELF has no _SEGGER_RTT
    symbol. SystemExit derives from BaseException, so flash_and_capture_one's blanket
    `except Exception` did not catch it -- it used to escape all the way out of the per-board
    loop in main()'s PHASE 2 and abort the whole multi-board run, so boards captured
    successfully before the bad one never got their results written in PHASE 3. A board hitting
    this must produce its own (None, error) result instead of raising, and any board after it
    in iteration order must still be attempted."""

    def setUp(self):
        self.tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.tmp, ignore_errors=True)
        # sysview_ci.py resolves the scripts/hil_flash modules relative to the real repo root,
        # the same way main() does -- reuse that so this test exercises the genuine
        # sysview_record.rtt_cb_from_elf, not a hand-rolled stand-in.
        self.repo_root = os.path.dirname(os.path.dirname(os.path.dirname(
            os.path.abspath(sysview_ci.__file__))))
        scripts = os.path.join(self.repo_root, ".claude", "skills", "sysview", "scripts")
        sys.path.insert(0, scripts)
        import sysview_record
        import hil_flash
        from helper import hil_util
        self.sysview_record, self.hil_flash, self.hil_util = sysview_record, hil_flash, hil_util
        self._orig_rtt_cb = sysview_record.rtt_cb_from_elf
        self._orig_flash_openocd = hil_flash.flash_openocd
        self._orig_get_serial = hil_util.get_serial_dev
        self.addCleanup(self._restore)
        # a serial "node" that already exists, so the enumeration-wait loops in
        # flash_and_capture_one don't spend real seconds sleeping
        node = os.path.join(self.tmp, "node")
        open(node, "w").close()
        hil_util.get_serial_dev = lambda *a, **k: node
        hil_flash.flash_openocd = lambda board, fw: subprocess.CompletedProcess([], 0)

    def _restore(self):
        self.sysview_record.rtt_cb_from_elf = self._orig_rtt_cb
        self.hil_flash.flash_openocd = self._orig_flash_openocd
        self.hil_util.get_serial_dev = self._orig_get_serial

    def _board(self, name):
        return {"name": name, "uid": "U", "flasher": {"name": "openocd", "uid": "P",
                 "args": "-f interface/x.cfg -f target/y.cfg"},
                "sysview": {"example": "device/cdc_msc", "workload": "idle", "duration_s": 1}}

    def test_bad_elf_errors_without_raising_and_next_board_is_still_attempted(self):
        self.sysview_record.rtt_cb_from_elf = lambda elf: (_ for _ in ()).throw(
            SystemExit(f"error: no _SEGGER_RTT symbol in {elf} -- not a SystemView build?"))
        capture, err = sysview_ci.flash_and_capture_one(
            self._board("bad"), os.path.join(self.tmp, "bad"), self.tmp, self.repo_root)
        self.assertIsNone(capture)
        self.assertIn("no RTT symbol", err)

        # A second, well-formed board must still be reached afterward -- the bug was that the
        # SystemExit above aborted the whole process, not just this one board's capture. Whether
        # THIS board's own capture ultimately succeeds doesn't matter here (no real probe/openocd
        # is attached in this test environment); what matters is that rtt_cb_from_elf actually
        # gets called for it, proving the loop moved on instead of dying on the first board.
        called = []
        self.sysview_record.rtt_cb_from_elf = lambda elf: called.append(elf) or "0x0"
        sysview_ci.flash_and_capture_one(
            self._board("good"), os.path.join(self.tmp, "good"), self.tmp, self.repo_root)
        self.assertEqual(called, [os.path.join(self.tmp, "good") + ".elf"])


class SessionReset(unittest.TestCase):
    """The capture session's own `reset run` must be skipped on two board classes:
    WCH (under SDI the target never comes back -- detected by target config, the only
    spelling left after #3804 folded the openocd_wch flasher name away), and boards
    flagged sysview.attach_only (metro_m4_express: after openocd's reset on atsame5x
    the core stays held and the RTT control block never appears -- the flasher's own
    post-flash reset already supplied the fresh boot)."""
    def test_wch_board_detected_by_target_config(self):
        b = {"flasher": {"name": "openocd", "args": "-f target/wch-riscv.cfg"}}
        self.assertTrue(sysview_ci.is_wch_board(b))
        self.assertFalse(sysview_ci.session_resets({**b, "sysview": {}}))

    def test_non_wch_openocd_board_resets(self):
        b = {"flasher": {"name": "openocd", "args": "-f target/rp2040.cfg"}, "sysview": {}}
        self.assertFalse(sysview_ci.is_wch_board(b))
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


if __name__ == '__main__':
    unittest.main()
