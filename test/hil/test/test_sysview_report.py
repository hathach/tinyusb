#!/usr/bin/env python3
"""Tests for sysview_report.py's data-quality guards (spliced pairs, live window)."""
import importlib.util, json, os, re, statistics, subprocess, sys, tempfile, unittest

REPORT = os.path.join(os.path.dirname(__file__), '..', '..', '..',
                      '.claude', 'skills', 'sysview', 'scripts', 'sysview_report.py')
TUSB_SYSVIEW_H = os.path.join(os.path.dirname(__file__), '..', '..', '..',
                              'src', 'common', 'tusb_sysview.h')

def _load_report_module():
    """Import sysview_report.py by path to unit-test its internals (pct()) directly, without
    going through the subprocess/--json path run_report() below uses for the higher-level
    behaviors. Module-level code only defines functions; main() is guarded by __name__."""
    spec = importlib.util.spec_from_file_location("sysview_report", REPORT)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod

sysview_report = _load_report_module()

EV_HEADER = ("sequencenum,timestamp,context,event,detail,timestampint,"
             "contextinint,contextint,contextoutint,eventint,eventoffset,eventsize,eventdata\n")
CTX_HEADER = "Name,Type,Activations,CPU Load,Total Run Time,Total Blocked Time,Min Run Time,Avg Run Time,Max Run Time\n"

def ev(seq, ts, event, detail=""):
    return f'{seq},0.0,"ctx","{event}","{detail}",{ts},0x0,0x0,0x0,0,0,0,\n'

INIT = ev(0, 0, "Init", "Cycle Freq.: 1000000, CPU Freq.: 48000000, ID Base: 0x20000000, ID Shift: 0")

def run_report(events_rows, contexts_rows="", include_init=True):
    with tempfile.TemporaryDirectory() as d:
        with open(os.path.join(d, 'events.txt'), 'w') as f:
            f.write(EV_HEADER + (INIT if include_init else "") + "".join(events_rows))
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

    def test_nested_call_ret_both_kept(self):
        # CALL,CALL,RET,RET on the same id (e.g. a preempted task): LIFO pairing must match
        # each RET to its innermost still-open CALL, keeping BOTH durations instead of
        # dropping one and double-counting dropped_pairs (F5b).
        rows = [ev(1, 1000, "Function #512"),                       # outer call
                ev(2, 1010, "Function #512"),                       # inner call
                ev(3, 1015, "Function #512", "Returns after 5.000 us"),   # inner ret
                ev(4, 1030, "Function #512", "Returns after 30.000 us")]  # outer ret
        j = run_report(rows)
        fn = {f['name']: f for f in j['functions']}
        self.assertEqual(fn['tud_task']['n'], 2)                    # both pairs kept, none dropped
        self.assertEqual(fn['tud_task']['max_us'], 30.0)             # outer (30 us), not spliced
        self.assertEqual(j['dropped_pairs'], 0)

    def test_call_before_large_gap_ret_after_kept(self):
        # F5a: a CALL must still enter open_calls even when a large gap in timestamps follows it,
        # or its RET on the far side of the gap is wrongly discarded as data loss --
        # systematically losing the first invocation of every instrumented function. (There is no
        # "window" boundary here to speak of -- pairing bookkeeping runs for every row regardless
        # of the capture's live span; this is purely about surviving an arbitrarily large gap
        # between a CALL and its RET.)
        rows = [ev(1, 500, "Function #512")]                        # call
        t = 99_000_000                                              # a large gap follows
        rows.append(ev(2, t, "Function #512", "Returns after 5.000 us"))  # ret, well after it
        j = run_report(rows)
        fn = {f['name']: f for f in j['functions']}
        self.assertEqual(fn['tud_task']['n'], 1)                    # kept, not lost across the gap
        self.assertEqual(j['dropped_pairs'], 0)

class BareReturns(unittest.TestCase):
    """SystemView annotates a return with its duration only rarely -- measured 0.5% of
    returns on stm32f407disco and 0.8% on raspberry_pi_pico; the rest are a bare
    'Returns'. Keying CALL-vs-RET off the duration regex misread those as calls."""

    def test_bare_returns_pair_and_are_not_dropped(self):
        rows = []
        t = 1000
        for i in range(60):                       # 60 pairs, bare returns, 10 ticks = 10 us
            rows.append(ev(len(rows)+1, t, "Function #512"))
            rows.append(ev(len(rows)+1, t+10, "Function #512", "Returns"))
            t += 1000
        j = run_report(rows)
        fn = {f['name']: f for f in j['functions']}
        self.assertEqual(fn['tud_task']['n'], 60)
        self.assertEqual(j['dropped_pairs'], 0)

    def test_computed_duration_matches_annotated(self):
        # INIT declares Cycle Freq. 1000000 -> 1 tick == 1 us. A bare return spanning
        # 10 ticks must yield the same duration as an explicitly annotated 10.000 us one.
        bare = [ev(1, 1000, "Function #512"), ev(2, 1010, "Function #512", "Returns")]
        ann = [ev(1, 1000, "Function #512"),
               ev(2, 1010, "Function #512", "Returns after 10.000 us")]
        self.assertEqual(run_report(bare)['functions'][0]['p50_us'],
                         run_report(ann)['functions'][0]['p50_us'])

    def test_mixed_annotated_and_bare_all_counted(self):
        rows = [ev(1, 1000, "Function #512"),
                ev(2, 1010, "Function #512", "Returns after 10.000 us"),
                ev(3, 2000, "Function #512"),
                ev(4, 2010, "Function #512", "Returns")]
        j = run_report(rows)
        self.assertEqual(j['functions'][0]['n'], 2)
        self.assertEqual(j['dropped_pairs'], 0)


class OverflowCount(unittest.TestCase):
    """Overflow is the real count over the capture -- one number, no live/stale split. The
    reporter no longer re-interprets which part of a capture 'counts'."""
    def test_every_overflow_marker_counted(self):
        t = 99_000_000
        rows = [ev(1, 500, "*** Overflow ***"),
                ev(2, t, "ISR Enter", "Runs for 5.000 us"),
                ev(3, t + 50_000, "*** Overflow ***"),
                ev(4, t + 100_000, "ISR Enter", "Runs for 5.000 us")]
        j = run_report(rows)
        self.assertEqual(j['overflow'], 2)
        self.assertNotIn('overflow_total', j)   # the second, "diagnostic" view is gone

class NoTimeBase(unittest.TestCase):
    """F3: a missing Init row means no trustworthy timestamp frequency -- withhold rather than
    silently assume 1 MHz (wrong by up to 168x on a DWT core)."""
    def test_missing_init_withholds_live_window_and_cpu_pct(self):
        rows = [ev(1, 1000, "ISR Enter", "Runs for 5.000 us")]
        ctx_rows = "usbd,Task,10,50.00%,1.000 000 s,0.000 000 s,0.000 000 s,0.000 000 s,0.000 000 s\n"
        j = run_report(rows, ctx_rows, include_init=False)
        self.assertIsNone(j['live_window_s'])
        self.assertIsNone(j['contexts'][0]['cpu_pct'])
        self.assertTrue(any('Init' in w for w in j['warnings']))

class NegativeLiveWindow(unittest.TestCase):
    """N5: a decreasing timestamp (DWT/timer wrap) makes live_window_s negative -- the
    a raw (negative) subtraction used to sail past both gate()'s `is None` check and the cpu_pct
    rescale's `> 0` guard, publishing a 'live -N s' heading with CPU% mislabelled. A clock that
    ran backwards is corrupt time, not capture data -- withholding is not re-interpreting it."""
    def test_decreasing_timestamps_withhold_as_no_time_base(self):
        rows = [ev(1, 5_000_000, "ISR Enter", "Runs for 5.000 us"),
                ev(2, 1_000_000, "ISR Enter", "Runs for 5.000 us")]  # ts goes backwards
        j = run_report(rows)
        self.assertIsNone(j['live_window_s'])
        self.assertTrue(any('not monotonic' in w for w in j['warnings']), j['warnings'])

    def test_zero_span_also_withheld(self):
        rows = [ev(1, 0, "ISR Enter", "Runs for 5.000 us"),
                ev(2, 0, "ISR Enter", "Runs for 5.000 us")]                 # all one ts -> 0 span
        j = run_report(rows)
        self.assertIsNone(j['live_window_s'])

class CpuPctScope(unittest.TestCase):
    """cpu_pct is SystemView's own CPU Load column, passed through as recorded. It used to be
    recomputed against the window and clamped to 100 -- a derived number wearing the measured
    one's name."""
    def test_cpu_pct_is_systemviews_value_unmodified(self):
        rows = [ev(1, 1000, "ISR Enter", "Runs for 5.000 us"),
                ev(2, 500_000, "ISR Enter", "Runs for 5.000 us")]
        ctx_rows = "usbd,Task,10,50.00%,1.000 000 s,0.000 000 s,0.000 000 s,0.000 000 s,0.000 000 s\n"
        j = run_report(rows, ctx_rows)
        self.assertEqual(j['contexts'][0]['cpu_pct'], 50.0)   # not rescaled, not clamped

class PctFunction(unittest.TestCase):
    """V5: pct() must use the conventional nearest-rank index (int((n-1)*p/100)), not
    int(n*p/100) -- the latter lands one slot too high, e.g. reporting the MAX as the p50 median
    at n=2, and biasing every low-n p50 upward."""
    def test_p50_two_elements_is_not_max(self):
        self.assertEqual(sysview_report.pct([10, 100], 50), 10)

    def test_p50_matches_nearest_rank_median(self):
        # Nearest-rank median == statistics.median_low() for both odd and even n.
        for vals in ([1, 2, 3, 4, 5], [1, 2, 3, 4], list(range(1, 51))):
            with self.subTest(vals=vals):
                self.assertEqual(sysview_report.pct(list(vals), 50), statistics.median_low(vals))

class LiveWindow(unittest.TestCase):
    """The reported window is the capture itself: first event to last, nothing trimmed. An idle
    stretch is a real observation about the target, so it stays in the numbers."""
    def _mixed_rows(self):
        rows, t = [], 0
        for i in range(55):
            rows.append(ev(len(rows) + 1, t, "ISR Enter", "Runs for 100.000 us")); t += 15
        t = 99_000_000                             # a long quiet stretch, then more traffic
        for i in range(60):
            rows.append(ev(len(rows) + 1, t, "ISR Enter", "Runs for 5.000 us")); t += 100_000
        return rows

    def test_all_samples_kept(self):
        j = run_report(self._mixed_rows())
        self.assertEqual(j['isr'][0]['n'], 115)    # every recorded sample, not a chosen subset

    def test_window_spans_the_whole_capture(self):
        j = run_report(self._mixed_rows())
        self.assertAlmostEqual(j['live_window_s'], 104.9, delta=0.2)

    def test_no_gap_means_full_span(self):
        rows = [ev(1, 0, "ISR Enter", "Runs for 5.000 us"),
                ev(2, 500_000, "ISR Enter", "Runs for 5.000 us")]
        j = run_report(rows)
        self.assertAlmostEqual(j['live_window_s'], 0.5, delta=0.01)


class NoTrimming(unittest.TestCase):
    """The reporter reports the capture, it does not decide which part of it counts.

    A gap heuristic used to pick a 'live window' and silently drop everything before it. Every
    variant was wrong for some real capture: anchoring on the last qualifying gap emptied
    stm32f072disco's tables when its capture merely ended quiet (the OpenOCD route's own ~2.0 s
    pre-stop settle measured 2.019 s), and anchoring on the largest instead discarded half the
    events whenever a device idled mid-capture."""

    def _burst(self, rows, seq, ts, count, step=10_000):
        for _ in range(count):
            rows.append(ev(seq, ts, "ISR Enter", "Runs for 10.000 us")); seq += 1; ts += step
        return seq, ts

    def test_trailing_lull_keeps_everything(self):
        rows, seq, ts = [], 1, 200_000
        seq, ts = self._burst(rows, seq, ts, 400)
        ts += 2_019_000                                   # the f072 case
        rows.append(ev(seq, ts, "ISR Enter", "Runs for 50.000 us"))
        self.assertEqual(run_report(rows)['isr'][0]['n'], 401)

    def test_mid_capture_idle_keeps_everything(self):
        rows, seq, ts = [], 1, 1_000_000
        seq, ts = self._burst(rows, seq, ts, 300)
        ts += 30_000_000                                  # device quiet between bursts
        seq, ts = self._burst(rows, seq, ts, 300)
        self.assertEqual(run_report(rows)['isr'][0]['n'], 600)

    def test_long_leading_gap_keeps_everything(self):
        rows = [ev(1, 0, "ISR Enter", "Runs for 5.000 us")]
        self._burst(rows, 2, 90_000_000, 300)
        j = run_report(rows)
        self.assertEqual(j['isr'][0]['n'], 301)
        self.assertAlmostEqual(j['live_window_s'], 92.99, delta=0.2)


class WorkloadWindowCpu(unittest.TestCase):
    """cpu_pct_workload: per-context busy time summed from the raw scheduling events
    (Task Run / System Idle / ISR Enter+Exit transitions), over a window DEFINED by the
    workload's own footprint -- the span of tud_cdc_read events (Function #516) -- or an
    explicit --window. A derived metric under its own name; SystemView's cpu_pct column is
    never touched. Busy time inside a window cannot exceed the window, so no clamp exists."""

    def _sched(self, seq, ts, event, ctx="ctx"):
        return f'{seq},0.0,"{ctx}","{event}","",{ts},0x0,0x0,0x0,0,0,0,\n'

    def _capture(self):
        """1s idle | 2s workload (usbd 50% / Idle 50%, CDC reads bracket it) | 1s idle."""
        rows, seq = [], 1
        rows.append(self._sched(seq, 0, "System Idle", "Idle")); seq += 1
        t = 1_000_000                                  # workload starts
        rows.append(ev(seq, t, "Function #516")); seq += 1          # first tud_cdc_read CALL
        # alternate 100ms usbd / 100ms Idle for 2s => usbd busy 50% of window
        for i in range(10):
            rows.append(self._sched(seq, t, "Task Run", "usbd")); seq += 1; t += 100_000
            rows.append(self._sched(seq, t, "System Idle", "Idle")); seq += 1; t += 100_000
        rows.append(ev(seq, t, "Function #516", "Returns after 5.000 us")); seq += 1  # last read RET
        rows.append(self._sched(seq, t + 1_000_000, "System Idle", "Idle")); seq += 1
        return rows

    def test_workload_window_anchored_on_cdc_reads(self):
        j = run_report(self._capture())
        self.assertAlmostEqual(j['workload_window_s'], 2.0, delta=0.01)
        self.assertEqual(j['workload_anchor'], 'cdc-read-span')

    def test_busy_share_within_window(self):
        j = run_report(self._capture())
        by = {c['name']: c for c in j['contexts_workload']}
        self.assertAlmostEqual(by['usbd']['cpu_pct_workload'], 50.0, delta=1.0)
        self.assertAlmostEqual(by['Idle']['cpu_pct_workload'], 50.0, delta=1.0)

    def test_isr_time_attributed_to_isr_not_task(self):
        rows, seq = [], 1
        t = 0
        rows.append(ev(seq, t, "Function #516")); seq += 1
        rows.append(self._sched(seq, t, "Task Run", "usbd")); seq += 1
        # 1s window: usbd runs, but a 200ms ISR preempts in the middle
        rows.append(self._sched(seq, t + 400_000, "ISR Enter", "ISR 83")); seq += 1
        rows.append(self._sched(seq, t + 600_000, "ISR Exit", "ISR 83")); seq += 1
        rows.append(ev(seq, t + 1_000_000, "Function #516", "Returns after 5.000 us")); seq += 1
        j = run_report(rows)
        by = {c['name']: c for c in j['contexts_workload']}
        self.assertAlmostEqual(by['usbd']['cpu_pct_workload'], 80.0, delta=1.0)
        self.assertAlmostEqual(by['ISR 83']['cpu_pct_workload'], 20.0, delta=1.0)

    def test_no_cdc_activity_means_null(self):
        rows = [ev(1, 0, "ISR Enter", "Runs for 5.000 us"),
                ev(2, 1_000_000, "ISR Enter", "Runs for 5.000 us")]
        j = run_report(rows)
        self.assertIsNone(j['workload_window_s'])
        self.assertEqual(j['contexts_workload'], [])

    def test_never_exceeds_100(self):
        j = run_report(self._capture())
        for c in j['contexts_workload']:
            self.assertLessEqual(c['cpu_pct_workload'], 100.0)


class ClockCorruptWithholdsWorkloadMetrics(unittest.TestCase):
    """W5: the backwards-step check already withholds live_window_s when timestamps are
    non-monotonic (counter wrap), but workload_window_s/workload_anchor/contexts_workload are
    computed from that same valid_ts/freq and used to still get published -- repro'd as
    cpu_pct_workload 100.0 showing up right alongside the "no time base" warning that says the
    clock can't be trusted. All of it must be withheld together."""
    def test_wrapped_timestamps_withhold_workload_metrics_too(self):
        rows = [
            ev(1, 5_000_000, "ISR Enter", "Runs for 5.000 us"),
            ev(2, 1_000_000, "ISR Enter", "Runs for 5.000 us"),  # ts goes backwards -> corrupt clock
            ev(3, 2_000_000, "Function #516"),                    # tud_cdc_read CALL
            ev(4, 2_000_000, "Task Run"),                         # "ctx" busy across the span
            ev(5, 3_000_000, "Function #516", "Returns after 5.000 us"),  # tud_cdc_read RET
        ]
        j = run_report(rows)
        self.assertIsNone(j['live_window_s'])           # sanity: the existing guard still fires
        self.assertIsNone(j['workload_window_s'])
        self.assertIsNone(j['workload_anchor'])
        self.assertEqual(j['contexts_workload'], [])


class WindowCliValidation(unittest.TestCase):
    """W6: --window's own CLI-anchor path had no last>first check -- both fallback anchors
    (markers, cdc-read-span) already reject a non-positive span, so --window 5:2 was the one way
    left to silently produce a negative workload_window_s."""
    def test_inverted_window_range_errors(self):
        with tempfile.TemporaryDirectory() as d:
            with open(os.path.join(d, 'events.txt'), 'w') as f:
                f.write(EV_HEADER + INIT)
            with open(os.path.join(d, 'contexts.csv'), 'w') as f:
                f.write(CTX_HEADER)
            r = subprocess.run([sys.executable, REPORT, d, '--json', '--window', '5:2'],
                               capture_output=True, text=True)
            self.assertNotEqual(r.returncode, 0)
            self.assertIn('--window end must be after start', r.stderr)


class FuncNameTableMatchesHeader(unittest.TestCase):
    """W13: TU_SV_FUNC_NAMES (sysview_report.py) hand-mirrors the id->name table
    src/common/tusb_sysview.h maintains for exactly this purpose -- its own comment says the
    table is kept "so Task 5's host-side reporter (sysview_report.py) can map id -> name
    itself". Nothing tied the two together before: inserting/reordering a tu_sysview_id_t member
    would silently relabel every function after it with no test noticing. This parses the
    header's id->name comment and its enum member count, and cross-checks both against
    TU_SV_FUNC_NAMES / TU_SV_EVENT_BASE."""

    def setUp(self):
        with open(TUSB_SYSVIEW_H) as f:
            self.text = f.read()

    def test_id_name_comment_matches_func_names_table(self):
        m = re.search(r"map id -> name itself:\s*\n((?:\s*//.*\n)+?)\s*typedef enum", self.text)
        self.assertIsNotNone(m, "couldn't find the id->name comment above tu_sysview_id_t")
        pairs = re.findall(r"(\d+)\s+([A-Za-z_][A-Za-z0-9_]*)", m.group(1))
        self.assertTrue(pairs)
        header_names = {int(i): name for i, name in pairs}
        self.assertEqual(header_names, sysview_report.TU_SV_FUNC_NAMES)

    def test_enum_member_count_matches_table(self):
        m = re.search(r"typedef enum\s*\{(.*?)\}\s*tu_sysview_id_t;", self.text, re.S)
        self.assertIsNotNone(m, "couldn't find the tu_sysview_id_t enum body")
        members = [ln.strip().split(",")[0].split("=")[0].strip()
                   for ln in m.group(1).splitlines() if ln.strip().startswith("TU_SV_ID_")]
        members = [name for name in members if not name.endswith("_COUNT")]
        self.assertEqual(len(members), len(sysview_report.TU_SV_FUNC_NAMES))

    def test_event_base_matches_header(self):
        m = re.search(r"#define\s+TU_SV_EVENT_BASE\s+(\d+)", self.text)
        self.assertIsNotNone(m, "couldn't find #define TU_SV_EVENT_BASE in the header")
        self.assertEqual(int(m.group(1)), sysview_report.TU_SV_EVENT_BASE)


class OverflowInExitReasons(unittest.TestCase):
    """SystemView marks data loss in TWO ways: explicit "*** Overflow ***" event rows, and exit
    reasons reading "Returns to *** OVERFLOW ***" -- the decoder lost the return context to ring
    loss. A dual-role dogfood measured 96-99% of ISR exits carrying the second form while the
    JSON overflow field read 0 or 1: the report was drastically understating real loss."""
    def test_lost_context_exits_counted(self):
        rows = [ev(1, 1000, "ISR Enter", "Runs for 5.000 us"),
                ev(2, 2000, "ISR Exit", "Returns to *** OVERFLOW ***"),
                ev(3, 3000, "ISR Enter", "Runs for 5.000 us"),
                ev(4, 4000, "ISR Exit", "Returns to Idle"),
                ev(5, 5000, "*** Overflow ***")]
        j = run_report(rows)
        self.assertEqual(j['overflow'], 2)   # 1 event row + 1 lost-context exit
    def test_clean_capture_still_zero(self):
        rows = [ev(1, 1000, "ISR Enter", "Runs for 5.000 us"),
                ev(2, 2000, "ISR Exit", "Returns to Idle")]
        self.assertEqual(run_report(rows)['overflow'], 0)


if __name__ == '__main__':
    unittest.main()
