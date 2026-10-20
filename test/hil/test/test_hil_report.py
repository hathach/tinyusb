#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Unit tests for the report document: the vocabulary, the one cell classifier, rendering,
# the writers, and the fold to per-board verdicts.
# Run directly:
#   python3 test/hil/test/test_hil_report.py
import json
import os
import subprocess
import sys
import unittest
from pathlib import Path
from unittest import mock
import contextlib
import io
from contextlib import redirect_stdout
from tempfile import TemporaryDirectory

TEST_DIR = os.path.dirname(os.path.abspath(__file__))
HIL_DIR = os.path.dirname(TEST_DIR)
sys.path.insert(0, HIL_DIR)

from helper import hil_report

CAVEAT = '**HIL run aborted: worker pool timed out after 3600s.**\n'
ROSTER = Path(HIL_DIR) / 'tinyusb.json'


def begin(rd, accumulate=True):
    """A run's attempt, as hil_test.py begins one: every accumulating write joins one."""
    hil_report.start_attempt(Path(rd), accumulate, ROSTER, [])


class OneClassifierForBothArtifacts(unittest.TestCase):
    """The markdown tally and the agent's verdict used to classify cells with two separate
    copies of one rule -- hil_test's cell_kind against REPORT_CELL, and hil_summary's
    cell_state against its own re-typed '❌'/'⚪' literals. Change the icons and the table
    and the verdict silently disagree."""

    def test_bare_states(self):
        self.assertEqual(hil_report.cell_state('fail'), 'fail')
        self.assertEqual(hil_report.cell_state('skip'), 'skip')
        self.assertEqual(hil_report.cell_state('pass'), 'pass')

    def test_icon_prefixed_metrics_carry_their_verdict(self):
        self.assertEqual(hil_report.cell_state(f'{hil_report.REPORT_CELL["fail"]} 29/30'), 'fail')
        self.assertEqual(hil_report.cell_state(f'{hil_report.REPORT_CELL["skip"]} board wedged'),
                         'skip')

    def test_an_unprefixed_metric_is_a_pass(self):
        """Load-bearing: a passing test may return a plain metric string. Classifying
        unknown shapes as fail would publish a green table as a red verdict."""
        self.assertEqual(hil_report.cell_state('480.0 MBps'), 'pass')
        self.assertEqual(hil_report.cell_state('1103 KB/s'), 'pass')

    def test_a_non_string_cell_does_not_raise(self):
        """render_matrix's copy guarded with isinstance; hil_summary's did not, because its
        caller str()'d first. The merged one keeps the guard -- it is the safer superset."""
        self.assertEqual(hil_report.cell_state(None), 'pass')

    def test_the_icons_come_from_REPORT_CELL(self):
        """No second copy of the emoji anywhere in the module."""
        src = (Path(HIL_DIR) / 'helper' / 'hil_report.py').read_text(encoding='utf-8')
        # CODE only: prose may quote an icon to explain a rule. The old assertion counted
        # the single-quoted spelling `'❌'`, which a second copy written as "❌" would have
        # sailed past.
        code = '\n'.join(line.split('#', 1)[0] for line in src.splitlines())
        for icon in ('❌', '⚪', '✅'):
            self.assertEqual(code.count(icon), 1,
                             f'{icon} is spelled in code more than once; REPORT_CELL is'
                             f' meant to be the one source')


class BoardVariantsIsTheRostersOneReading(unittest.TestCase):
    def test_a_board_without_a_list_builds_as_itself(self):
        self.assertEqual(hil_report.board_variants({'name': 'b'}),
                         [{'name': 'b', 'defines': [], 'flags': []}])

    def test_each_variant_carries_its_defines_and_split_flags(self):
        board = {'name': 'b', 'variant': [{'name': 'b-x', 'defines': ['RHPORT_DEVICE=1'], 'flags': '-DA=1  -DB=2'},
                                          {'name': 'b'}]}
        self.assertEqual(hil_report.board_variants(board),
                         [{'name': 'b-x', 'defines': ['RHPORT_DEVICE=1'], 'flags': ['-DA=1', '-DB=2']},
                          {'name': 'b', 'defines': [], 'flags': []}])

    def test_a_malformed_variant_raises(self):
        for variant in [[{'name': 'v', 'flags': None}], [{'flags': '-DA=1'}], [{'name': 'v', 'defines': 'X=1'}],
                        [{'name': ''}], ['x'], [None], {'name': 'v'}, 'v', 5, {}, 0, False, '', None]:
            with self.assertRaises(ValueError, msg=variant):
                hil_report.board_variants({'name': 'b', 'variant': variant})


class ModuleWorksImportedAndAsAScript(unittest.TestCase):
    """It is imported as helper.hil_report by hil_test, and run as a script by the operator
    (the HIL contract, .claude/skills/hil-contract/SKILL.md). A script run puts helper/ on sys.path, NOT test/hil,
    so a plain `from helper import ...` breaks the CLI and only the CLI."""

    def test_importable_as_a_package_module(self):
        r = subprocess.run(
            [sys.executable, '-c',
             f'import sys; sys.path.insert(0, {HIL_DIR!r}); '
             f'from helper import hil_report; print(hil_report.REPORT_JSON)'],
            capture_output=True, text=True, timeout=60)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('hil_report.json', r.stdout)

    def test_runnable_as_a_script(self):
        r = subprocess.run(
            [sys.executable, str(Path(HIL_DIR) / 'helper' / 'hil_report.py'), '--help'],
            capture_output=True, text=True, timeout=60)
        self.assertEqual(r.returncode, 0, r.stderr)


class RenderReportIsPureFunctionOfTheDocument(unittest.TestCase):
    """Four writers used to compose the markdown independently, so a table could carry
    something the sidecar did not. One renderer, and the ordering it guarantees, is what
    stops that -- pinned here rather than left to the order of three concatenations."""

    def _doc(self, **kw):
        d = {'rows': [{'board': 'boardA', 'cells': {'cdc_msc': 'pass'}, 'duration': '1s'}],
             'scope': '', 'caveat': ''}
        d.update(kw)
        return d

    def test_table_comes_from_rows(self):
        md = hil_report.render_report(self._doc())
        self.assertIn('boardA', md)
        self.assertIn('cdc_msc', md)

    def test_scope_note_appears_above_the_table(self):
        md = hil_report.render_report(self._doc(scope='-b boardA'))
        self.assertLess(md.index('Scoped run'), md.index('boardA'))

    def test_caveat_is_outermost(self):
        md = hil_report.render_report(self._doc(scope='-b boardA',
                                              caveat='**HIL run aborted.**\n'))
        self.assertLess(md.index('aborted'), md.index('Scoped run'))

    def test_a_document_with_no_rows_still_renders(self):
        md = hil_report.render_report(self._doc(rows=[]))
        self.assertIn('No tests were run.', md)

    def test_a_malformed_row_does_not_raise(self):
        """A report dir can hold an older version's sidecar or a torn one."""
        md = hil_report.render_report(self._doc(
            rows=[{'board': 'boardA', 'cells': {'cdc_msc': 'pass'}}, {'board': 'half'},
                  {}]))
        self.assertIn('boardA', md)      # the intact row still renders ...
        self.assertIn('half', md)        # ... and a cell-less one becomes a blank row


class ScopeSurvivesInTheJson(unittest.TestCase):
    """A scoped run's small table is indistinguishable from a full run that lost boards.
    The markdown says so; the JSON did not, so any JSON consumer could not tell."""

    def _rows(self, board, cell):
        return [(board, 0, 0, [(board, {cell: 'OK'}, '1s')], 0)]

    def test_scope_is_recorded_in_the_sidecar(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_report.accumulate_report(self._rows('boardA', 'cdc_msc'), rd, True,
                                   '-b boardA', '')
        doc = json.loads((rd / hil_report.REPORT_JSON).read_text())
        self.assertEqual(doc['scope'], '-b boardA')

    def test_an_unscoped_run_records_an_empty_scope(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_report.accumulate_report(self._rows('boardA', 'cdc_msc'), rd, True, '', '')
        self.assertEqual(json.loads((rd / hil_report.REPORT_JSON).read_text())['scope'], '')


class EveryExitPathLeavesBothArtifacts(unittest.TestCase):
    """summarize() builds an agent's verdicts from the JSON. A path that writes only
    markdown reports the whole fleet as 'no report row' while a human sees the real story."""

    def test_the_no_boards_exit_writes_json_too(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_report.write_report(rd, {'rows': [], 'scope': '',
                                   'caveat': '**HIL run selected no boards.** why\n'})
        self.assertIn('selected no boards', (rd / hil_report.REPORT_MD).read_text())
        doc = json.loads((rd / hil_report.REPORT_JSON).read_text())
        self.assertEqual(doc['rows'], [])
        self.assertIn('selected no boards', doc['caveat'])

    def test_write_report_raises_so_its_callers_can_report_it(self):
        """write_report is NOT best-effort. Swallowing the OSError made
        write_timeout_report's warning and hil_test's fallback-of-the-fallback dead
        code -- an unwritable report dir produced no artifact and no message."""
        with self.assertRaises(hil_report.core().ReportError):
            hil_report.write_report(Path('/proc/nonexistent/nope'),
                                    {'rows': [], 'scope': '', 'caveat': 'x\n'})

    def test_the_guarded_callers_still_do_not_raise(self):
        """They run on an abort path, where a raise would replace the abort's own error."""
        bad = Path('/proc/nonexistent/nope')
        hil_report.mark_report_no_boards(bad, 'filters intersected to nothing')
        import io
        from contextlib import redirect_stdout
        with redirect_stdout(io.StringIO()):
            hil_report.write_timeout_report(bad, [{'name': 'b1'}], CAVEAT)


class MarkdownIsAlwaysARenderingOfTheJson(unittest.TestCase):
    """The property this whole change buys: whatever wrote the report, re-rendering the
    sidecar reproduces the markdown byte for byte. Four writers, one renderer -- asserted
    directly rather than inferred from the writers."""

    def _check(self, rd):
        doc = json.loads((rd / hil_report.REPORT_JSON).read_text())
        self.assertEqual((rd / hil_report.REPORT_MD).read_text(),
                         hil_report.render_report(doc) + '\n')

    def test_normal_path(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_report.accumulate_report(
            [('boardA', 0, 0, [('boardA', {'cdc_msc': 'OK'}, '1s')], 0)], rd, True,
            '-b boardA')
        self._check(rd)

    def test_after_an_accumulate_rerun(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_report.accumulate_report(
            [('boardA', 0, 0, [('boardA', {'cdc_msc': 'OK'}, '1s')], 0)], rd, True, '', '')
        begin(rd)
        hil_report.accumulate_report(
            [('boardB', 0, 0, [('boardB', {'cdc_msc': 'OK'}, '1s')], 0)], rd, False, '', '')
        self._check(rd)

    def test_no_boards_exit(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_report.write_report(rd, {'rows': [], 'scope': '',
                                   'caveat': '**HIL run selected no boards.** why\n'})
        self._check(rd)

    def test_the_pool_guard_fallback(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_report.accumulate_report(
            [('done', 0, 0, [('done', {'cdc_msc': 'OK'}, '1s')], 0)], rd, True, '', '')
        begin(rd)
        hil_report.write_timeout_report(rd, [{'name': 'stuck'}], CAVEAT)
        self._check(rd)

class WriteTimeoutReport(unittest.TestCase):
    def test_writes_a_report_where_there_would_be_none(self):
        with TemporaryDirectory() as td:
            begin(Path(td))
            hil_report.write_timeout_report(Path(td), [{'name': 'ra6m5_ek'}], CAVEAT)
            md = (Path(td) / hil_report.REPORT_MD).read_text()
        self.assertIn(CAVEAT, md)
        self.assertIn('ra6m5_ek', md)

    def test_the_prior_attempts_rows_survive(self):
        """Was: the prior MARKDOWN TEXT survives below the banner. It now re-renders from
        the merged sidecar, so the guarantee is stated against rows -- one table with the
        stuck boards in it, rather than a banner stapled above a duplicate table."""
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_report.accumulate_report(
            [('done', 0, 0, [('done', {'cdc_msc': 'OK'}, '1s')], 0)], rd, True, '', '')
        begin(rd)
        hil_report.write_timeout_report(rd, [{'name': 'stuck'}], CAVEAT)
        doc = json.loads((rd / hil_report.REPORT_JSON).read_text())
        self.assertEqual([r['board'] for r in doc['rows']], ['done', 'stuck'])
        md = (rd / hil_report.REPORT_MD).read_text()
        self.assertIn('done', md)
        self.assertIn('stuck', md)
        self.assertIn('aborted', md)
        self.assertLess(md.index('aborted'), md.index('done'))
        self.assertEqual(md.count('| Board'), 1, 'the prior table was duplicated, not merged')

    def test_timeout_report_writes_the_sidecar(self):
        """This path used to write markdown only, so summarize() -- which is all an
        agent gets -- reported the whole fleet as 'no report row' on exactly the runs
        that failed."""
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        begin(rd)
        hil_report.write_timeout_report(rd, [{'name': 'boardA'}], CAVEAT)
        self.assertTrue((rd / hil_report.REPORT_JSON).is_file())
        self.assertIn('boardA', (rd / hil_report.REPORT_JSON).read_text())

    def test_the_sidecar_keeps_a_previous_attempts_rows(self):
        """An earlier attempt's finished boards are real results and this attempt has none
        of its own, so the rows merge rather than replace."""
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        (rd / hil_report.REPORT_JSON).write_text(json.dumps(
            {'rows': [{'board': 'done', 'cells': {'cdc_msc': 'pass'}, 'duration': '1s'}],
             'scope': '', 'caveat': ''}))
        begin(rd)
        hil_report.write_timeout_report(rd, [{'name': 'stuck'}], CAVEAT)
        rows = json.loads((rd / hil_report.REPORT_JSON).read_text())['rows']
        self.assertEqual([r['board'] for r in rows], ['done', 'stuck'])

    def test_a_torn_sidecar_does_not_lose_the_stuck_boards(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        (rd / hil_report.REPORT_JSON).write_text('{ truncated mid-')
        begin(rd)
        hil_report.write_timeout_report(rd, [{'name': 'stuck'}], CAVEAT)
        rows = json.loads((rd / hil_report.REPORT_JSON).read_text())['rows']
        self.assertEqual([r['board'] for r in rows], ['stuck'])

    def test_a_roster_entry_without_a_name_does_not_escape(self):
        """The broad handler exists to stop a KeyError here stranding the runner, but a
        report that silently loses its only board is worse than one saying '?'."""
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        begin(rd)
        hil_report.write_timeout_report(rd, [{}], CAVEAT)
        rows = json.loads((rd / hil_report.REPORT_JSON).read_text())['rows']
        self.assertEqual([r['board'] for r in rows], ['?'])

    def test_unwritable_dir_does_not_raise(self):
        """The caller raises its abort right after; a raise here would replace it."""
        hil_report.write_timeout_report(Path('/proc/nonexistent/nope'), [], CAVEAT)


class SummaryFoldsReportToBoards(unittest.TestCase):
    """summarize() replaces the agent retyping the markdown table. Report rows are named per
    VARIANT and a variant need not start with the board name, so the config is what maps them
    back -- the previous string-matching design produced a defect in each of four review rounds."""

    def _sum(self, boards, rows, cfg_boards=None):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        d = Path(td.name)
        (d / 'hil_report.json').write_text(json.dumps(
            {'rows': [{'board': b, 'cells': c, 'duration': '1s'} for b, c in rows]}))
        cfg = d / 'cfg.json'
        cfg.write_text(json.dumps({'boards': cfg_boards or [{'name': b} for b in boards]}))
        args = [a for b in boards for a in ('-b', b)]
        r = subprocess.run(['python3', str(Path(TEST_DIR).parents[0] / 'helper' / 'hil_report.py'),
                            str(cfg), *args, '--report-dir', str(d)],
                           capture_output=True, text=True, timeout=60)
        self.assertEqual(r.returncode, 0, r.stderr)
        return json.loads(r.stdout)['results']

    def test_variant_rows_fold_onto_their_board(self):
        """nanoch32v203 never produces a row named after the board."""
        got = self._sum(['nanoch32v203'],
                        [('nanoch32v203-fsdev', {'usbtest': 'pass'}),
                         ('nanoch32v203-usbfs', {'usbtest': 'pass'})],
                        cfg_boards=[{'name': 'nanoch32v203',
                                     'variant': [{'name': 'nanoch32v203-fsdev'},
                                                 {'name': 'nanoch32v203-usbfs'}]}])
        self.assertEqual([r['board'] for r in got], ['nanoch32v203'])
        self.assertTrue(got[0]['pass'])
        self.assertTrue(got[0]['ran'])

    def test_one_failing_variant_fails_the_board(self):
        got = self._sum(['nano'],
                        [('nano-a', {'usbtest': 'pass'}), ('nano-b', {'usbtest': '❌ 29/30'})],
                        cfg_boards=[{'name': 'nano', 'variant': [{'name': 'nano-a'},
                                                                 {'name': 'nano-b'}]}])
        self.assertFalse(got[0]['pass'])
        self.assertIn('29/30', got[0]['detail'])

    def test_lock_contention_is_a_field_not_a_prefix(self):
        got = self._sum(['alpha'], [('alpha', {'board-locked': 'fail'})])
        self.assertTrue(got[0]['locked'])
        self.assertFalse(got[0]['pass'])

    def test_an_unusable_lock_dir_is_not_contention(self):
        cell = "❌ board lock unavailable: [Errno 17] File exists: '/tmp/tinyusb-hil-locks'"
        got = self._sum(['alpha'], [('alpha', {hil_report.LOCK_UNAVAILABLE_CELL: cell})])
        self.assertEqual(got[0], {'board': 'alpha', 'ran': False, 'pass': False, 'locked': False,
                                  'wedged': False, 'skipped': [], 'detail': cell})

    def test_a_board_with_no_row_is_marked_not_run(self):
        got = self._sum(['alpha', 'beta'], [('alpha', {'usbtest': 'pass'})])
        self.assertTrue(got[0]['ran'])
        self.assertFalse(got[1]['ran'])
        self.assertFalse(got[1]['pass'])

    def test_a_metric_cell_counts_by_its_icon(self):
        got = self._sum(['a', 'b'], [('a', {'cdc_msc_throughput': '✅ C 1.2 M 3.4'}),
                                     ('b', {'cdc_msc_throughput': '❌ C 0.0 M 0.0'})])
        self.assertTrue(got[0]['pass'])
        self.assertFalse(got[1]['pass'])

    def test_a_skipped_cell_is_not_an_executed_success(self):
        """A skipped requested test never ran: the row says so in `skipped` and `detail`
        and does not fold `pass: true`, so the caller derives not-run from it."""
        got = self._sum(['a'], [('a', {'usbtest': 'skip', 'cdc_msc': 'pass',
                                       'msc': f'{hil_report.REPORT_CELL["skip"]} board wedged'})])
        self.assertTrue(got[0]['ran'])
        self.assertFalse(got[0]['pass'])
        self.assertEqual(got[0]['skipped'], ['a msc', 'a usbtest'])
        self.assertEqual(got[0]['detail'], '1 variant(s), 1 cell(s) ok, 2 skipped: a msc, a usbtest')

    def test_a_failure_beside_a_skip_names_both(self):
        got = self._sum(['a'], [('a', {'usbtest': '❌ 29/30', 'cdc_msc': 'skip'})])
        self.assertEqual(got[0]['skipped'], ['a cdc_msc'])
        self.assertEqual(got[0]['detail'], 'a usbtest: ❌ 29/30; skipped: a cdc_msc')

    def test_a_row_without_skips_reads_as_before(self):
        got = self._sum(['a'], [('a', {'usbtest': 'pass', 'cdc_msc': 'pass'})])
        self.assertTrue(got[0]['pass'])
        self.assertEqual(got[0]['skipped'], [])
        self.assertNotIn('history', got[0])
        self.assertEqual(got[0]['detail'], '1 variant(s), 2 cell(s) ok')

    def test_a_plain_metric_cell_is_a_pass(self):
        """Mirrors hil_test.py's own tally (cell_kind): failures are ALWAYS marked -- 'fail'
        or a ❌ prefix, per TestFail's docstring -- while a passing test may return a plain
        metric string that lands in the cell unprefixed. Treating unknown shapes as fail
        would publish a green table as a red verdict."""
        got = self._sum(['a'], [('a', {'device_speed': '480.0 MBps'})])
        self.assertTrue(got[0]['pass'])

    def test_a_declared_variant_of_another_board_is_not_stolen(self):
        """A declared variant need not start with its own board's name, so it may start with
        a DIFFERENT board's name plus '-'. The prefix fallback must not attribute it twice."""
        got = self._sum(['alpha', 'beta'],
                        [('beta-x', {'usbtest': 'fail'})],
                        cfg_boards=[{'name': 'alpha', 'variant': [{'name': 'beta-x'}]},
                                    {'name': 'beta'}])
        self.assertTrue(got[0]['ran'])
        self.assertFalse(got[0]['pass'])
        self.assertFalse(got[1]['ran'], "beta must not inherit alpha's row")


    def test_an_unknown_board_matching_a_declared_variant_is_not_run(self):
        """hil_test.py rejects a name outside the config, so no row can be its own; a declared
        variant of another board that happens to carry that name must not make it ran:true."""
        got = self._sum(['beta', 'alpha'],
                        [('alpha', {'usbtest': 'pass'})],
                        cfg_boards=[{'name': 'beta', 'variant': [{'name': 'alpha'}]}])
        self.assertTrue(got[0]['ran'])
        self.assertTrue(got[0]['pass'])
        self.assertFalse(got[1]['ran'], 'alpha is not a configured board')
        self.assertFalse(got[1]['pass'])
        self.assertIn('not a board', got[1]['detail'])

    def test_an_unknown_board_with_a_stale_row_is_not_run(self):
        got = self._sum(['known', 'gone'],
                        [('known', {'usbtest': 'pass'}), ('gone', {'usbtest': 'pass'})],
                        cfg_boards=[{'name': 'known'}])
        self.assertTrue(got[0]['ran'])
        self.assertFalse(got[1]['ran'], 'a stale row must not report a removed board as run')

    def test_a_malformed_variant_is_a_config_error_row_not_a_traceback(self):
        rows = [('good', {'usbtest': 'pass'}), ('flags-v', {'usbtest': 'pass'})]
        bad = [{'name': 'nonlist', 'variant': {'name': 'x'}},
               {'name': 'nondict', 'variant': ['x']},
               {'name': 'noname', 'variant': [{'name': ''}]},
               {'name': 'flags', 'variant': [{'name': 'flags-v', 'flags': 5}]}]
        alone = self._sum(['good'], rows, cfg_boards=[{'name': 'good'}])
        got = self._sum(['good'] + [b['name'] for b in bad], rows, cfg_boards=[{'name': 'good'}] + bad)
        self.assertEqual(got[0], alone[0])
        for r in got[1:]:
            self.assertEqual((r['ran'], r['pass']), (False, False), r)
            self.assertTrue(r['detail'].startswith('config error:'), r)

    def test_a_wedge_outranks_a_lock_cell(self):
        """the caller re-runs LOCKED boards; a wedged one must never read as locked."""
        got = self._sum(['b'], [('b', {hil_report.LOCKED_CELL: 'fail', hil_report.WEDGED_CELL: 'fail'})])
        self.assertEqual((got[0]['locked'], got[0]['wedged'], got[0]['pass']), (False, True, False))

    def _acc(self, d, mret, fresh):
        if not fresh:
            begin(d)
        return json.loads((d / 'hil_report.json').read_text()) if hil_report.accumulate_report(
            mret, d, fresh) is not None else None

    def test_an_accumulated_rerun_that_ran_clears_last_attempts_wedge(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        d = Path(td.name)
        cfg = {'boards': [{'name': 'b', 'variant': [{'name': 'b-fs'}, {'name': 'b-hs'}]}]}
        wedge = [('b', 1, [], [('b-fs', {'usbtest': '❌ 3/30', hil_report.WEDGED_CELL: 'fail'}, '9s'),
                               ('b-hs', {'cdc_msc': '⚪ board wedged', hil_report.WEDGED_CELL: 'fail'}, '1s')], None)]
        doc = self._acc(d, wedge, True)
        self.assertTrue(hil_report.summarize(cfg, ['b'], doc)['results'][0]['wedged'])
        # re-run green: no wedge cell survives on any row
        clean = [('b', 0, [], [('b-fs', {'usbtest': 'pass'}, '8s'), ('b-hs', {'cdc_msc': 'pass'}, '2s')], None)]
        doc = self._acc(d, clean, False)
        for row in doc['rows']:
            self.assertNotIn(hil_report.WEDGED_CELL, row['cells'], row)
        r = hil_report.summarize(cfg, ['b'], doc)['results'][0]
        self.assertEqual((r['ran'], r['wedged'], r['pass']), (True, False, True))

    def test_a_build_refusal_keeps_the_earlier_cells_and_a_real_run_clears_it(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        d = Path(td.name)
        cfg = {'boards': [{'name': 'b', 'variant': [{'name': 'b-fs'}]}]}
        hil_report.write_report(d, {'rows': [
            {'board': 'b', 'cells': {hil_report.LOCKED_CELL: 'fail'}, 'duration': None},
            {'board': 'b-fs', 'cells': {'usbtest': '❌ 3/30', hil_report.WEDGED_CELL: 'fail'}, 'duration': '9s'}],
            'scope': '', 'caveat': ''})
        unbuilt = [('b', 1, [], [('b-fs', {hil_report.RUN_ABORTED_CELL: hil_report.BUILD_REFUSED}, None)], None)]
        doc = self._acc(d, unbuilt, False)
        cells = {r['board']: r['cells'] for r in doc['rows']}
        self.assertEqual(cells['b'], {hil_report.LOCKED_CELL: 'fail'})
        self.assertEqual(cells['b-fs'], {'usbtest': '❌ 3/30', hil_report.WEDGED_CELL: 'fail',
                                         hil_report.RUN_ABORTED_CELL: hil_report.BUILD_REFUSED})
        r = hil_report.summarize(cfg, ['b'], doc)['results'][0]
        self.assertEqual((r['ran'], r['pass'], r['locked'], r['wedged']), (False, False, False, True))
        self.assertIn('build refused', r['detail'])
        clean = [('b', 0, [], [('b-fs', {'usbtest': 'pass'}, '8s')], None)]
        doc = self._acc(d, clean, False)
        # the emptied board-name row stays only for its history, out of the table
        self.assertEqual([(r['board'], r['cells']) for r in doc['rows'] if r['cells']],
                         [('b-fs', {'usbtest': 'pass'})])
        self.assertEqual([(r['board'], r['history']) for r in doc['rows'] if not r['cells']],
                         [('b', [{hil_report.LOCKED_CELL: 'fail'}])])
        self.assertNotIn('| b ', (d / hil_report.REPORT_MD).read_text())
        r = hil_report.summarize(cfg, ['b'], doc)['results'][0]
        self.assertEqual((r['ran'], r['pass']), (True, True))

    def test_a_wedge_during_the_run_is_wedged_and_ran(self):
        got = self._sum(['b'], [('b', {'usbtest': '❌ 3/30', hil_report.WEDGED_CELL: 'fail'})])
        self.assertEqual((got[0]['ran'], got[0]['pass'], got[0]['wedged']), (True, False, True))
        self.assertIn('wedged (usbtest reported', got[0]['detail'])

    def test_an_ordinary_row_carries_wedged_false(self):
        got = self._sum(['b'], [('b', {'cdc_msc': 'pass'})])
        self.assertIs(got[0]['wedged'], False)

    def test_the_caveat_reaches_the_agents_verdict(self):
        """The abort/no-boards notice lives in the document now, and this JSON is all an
        agent gets -- dropping it here puts the caveat back where only a human sees it."""
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        d = Path(td.name)
        (d / 'hil_report.json').write_text(json.dumps(
            {'rows': [{'board': 'boardA', 'cells': {'cdc_msc': 'pass'}, 'duration': '1s'}],
             'scope': '',
             'caveat': '**HIL run aborted: the worker pool would not shut down.**\n'}))
        cfg = d / 'cfg.json'
        cfg.write_text(json.dumps({'boards': [{'name': 'boardA'}]}))
        r = subprocess.run(
            ['python3', str(Path(TEST_DIR).parents[0] / 'helper' / 'hil_report.py'),
             str(cfg), '-b', 'boardA', '--report-dir', str(d)],
            capture_output=True, text=True, timeout=60)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('aborted', json.loads(r.stdout)['caveat'])

    def test_an_older_sidecar_without_a_caveat_still_summarises(self):
        got = self._sum(['boardA'], [('boardA', {'cdc_msc': 'pass'})])
        self.assertTrue(got[0]['pass'])

    def test_the_old_entry_point_is_gone(self):
        """hil_summary.py's CLI moved here. A leftover file would keep working while
        drifting from the module that now owns the fold."""
        self.assertFalse((Path(HIL_DIR) / 'helper' / 'hil_summary.py').exists())


class WriteReportFailsLoudly(unittest.TestCase):
    def test_an_unwritable_dir_reaches_the_callers_warning(self):
        """write_report swallowing OSError made write_timeout_report's broad handler --
        and hil_test's fallback-of-the-fallback -- dead code: no artifact, no message."""
        import io
        from contextlib import redirect_stdout
        buf = io.StringIO()
        with redirect_stdout(buf):
            hil_report.write_timeout_report(Path('/proc/nonexistent/nope'),
                                            [{'name': 'b1'}], CAVEAT)
        self.assertIn('warning', buf.getvalue().lower(), 'the failure was silent')


class PoolTimeoutCellIsHonest(unittest.TestCase):
    def test_a_stuck_board_with_a_prior_row_still_gets_the_cell(self):
        """`not in done` skipped the cell for any board carrying an earlier attempt's row,
        so a board that just ate the 60-minute guard summarized as pass:true."""
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_report.accumulate_report(
            [('stm32f4', 0, 0, [('stm32f4', {'cdc_msc': 'OK'}, '1s')], 0)], rd, True, '', '')
        begin(rd)
        hil_report.write_timeout_report(rd, [{'name': 'stm32f4'}], CAVEAT)
        doc = json.loads((rd / hil_report.REPORT_JSON).read_text())
        verdict = hil_report.summarize({'boards': [{'name': 'stm32f4'}]}, ['stm32f4'], doc)
        self.assertFalse(verdict['results'][0]['pass'],
                         'a board that hung the pool was published as a pass')

    def test_a_clean_retry_clears_the_cell(self):
        """accumulate_report clears stale board-locked and BOUNDARY_CELL cells but not
        this one, so a board that passed clean on the retry stayed red forever."""
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        begin(rd)
        hil_report.write_timeout_report(rd, [{'name': 'stuck'}], CAVEAT)
        begin(rd)
        hil_report.accumulate_report(
            [('stuck', 0, 0, [('stuck', {'cdc_msc': 'OK'}, '2s')], 0)], rd, False, '', '')
        doc = json.loads((rd / hil_report.REPORT_JSON).read_text())
        self.assertNotIn('pool-timeout', doc['rows'][0]['cells'])
        verdict = hil_report.summarize({'boards': [{'name': 'stuck'}]}, ['stuck'], doc)
        self.assertTrue(verdict['results'][0]['pass'])

    def test_a_torn_sidecar_does_not_destroy_an_intact_markdown(self):
        """Re-rendering from an unusable sidecar threw away real results the human copy
        still had. Master concatenated below its banner and kept them."""
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        (rd / hil_report.REPORT_MD).write_text('| Board | t |\n| a | OK |\n| b | OK |\n')
        (rd / hil_report.REPORT_JSON).write_text('{ truncated')
        begin(rd)
        hil_report.write_timeout_report(rd, [{'name': 'stuck'}], CAVEAT)
        md = (rd / hil_report.REPORT_MD).read_text()
        self.assertIn('| a | OK |', md, "an earlier attempt's real results were destroyed")
        doc = json.loads((rd / hil_report.REPORT_JSON).read_text())
        self.assertEqual([r['board'] for r in doc['rows']], ['stuck'])


class AccumulateKeepsEarlierAttempts(unittest.TestCase):
    """A retry used to overwrite the first attempt's cells, so a pass after a failure read
    exactly like a first-try pass. A row an --accumulate attempt changes keeps its earlier
    cells in `history`, oldest first; the markdown shows the latest only."""

    CFG = {'boards': [{'name': 'a'}, {'name': 'b'},
                      {'name': 'nano', 'variant': [{'name': 'nano-fs'}, {'name': 'nano-hs'}]}]}

    def setUp(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.rd = Path(td.name)

    def attempt(self, rows, fresh=False):
        begin(self.rd)
        hil_report.accumulate_report([(b, 0, [], [(b, cells, '1s')], None) for b, cells in rows],
                                     self.rd, fresh, '', '')
        return self.doc()

    def doc(self):
        return json.loads((self.rd / hil_report.REPORT_JSON).read_text())

    def results(self, doc, boards):
        return {r['board']: r for r in hil_report.summarize(self.CFG, boards, doc)['results']}

    def test_each_changed_row_keeps_its_earlier_cells(self):
        self.attempt([('a', {'usbtest': '❌ 29/30', 'cdc': 'pass'}), ('b', {'cdc': 'pass'})], fresh=True)
        self.attempt([('a', {'usbtest': '❌ 28/30'})])
        doc = self.attempt([('a', {'usbtest': '✅ 30/30'}), ('b', {'cdc': 'pass'})])
        rows = {r['board']: r for r in doc['rows']}
        self.assertEqual(rows['a']['cells'], {'usbtest': '✅ 30/30', 'cdc': 'pass'})
        self.assertEqual(rows['a']['history'], [{'usbtest': '❌ 29/30', 'cdc': 'pass'},
                                                {'usbtest': '❌ 28/30', 'cdc': 'pass'}])
        self.assertNotIn('history', rows['b'], 'a re-run that changed nothing gained a history')
        bare = {**doc, 'rows': [{k: v for k, v in r.items() if k != 'history'} for r in doc['rows']]}
        self.assertEqual((self.rd / hil_report.REPORT_MD).read_text(), hil_report.render_report(bare) + '\n',
                         'the history reached the markdown')
        res = self.results(doc, ['a', 'b'])
        self.assertTrue(res['a']['pass'])
        self.assertEqual(res['a']['history'], {'a': rows['a']['history']})
        self.assertNotIn('history', res['b'])

    def test_an_unusable_lock_dir_keeps_the_earlier_attempts(self):
        unusable = '❌ board lock unavailable: [Errno 13] x'
        self.attempt([('a', {'usbtest': '❌ 29/30'})], fresh=True)
        self.attempt([('a', {'usbtest': '✅ 30/30'})])
        res = self.results(self.attempt([('a', {hil_report.LOCK_UNAVAILABLE_CELL: unusable})]), ['a'])['a']
        self.assertEqual((res['ran'], res['pass'], res['detail']), (False, False, unusable))
        self.assertEqual([h['usbtest'] for h in res['history']['a']], ['❌ 29/30', '✅ 30/30'])

    def test_a_cleared_lock_or_restamped_timeout_is_an_earlier_attempt(self):
        self.attempt([('a', {hil_report.LOCKED_CELL: 'fail'}), ('b', {'cdc': 'fail'})], fresh=True)
        begin(self.rd)
        hil_report.write_timeout_report(self.rd, [{'name': 'b'}], CAVEAT)
        begin(self.rd)
        hil_report.write_timeout_report(self.rd, [{'name': 'b'}], CAVEAT)
        doc = self.attempt([('a', {'cdc': 'pass'}), ('b', {'cdc': 'pass'})])
        rows = {r['board']: r for r in doc['rows']}
        self.assertEqual(rows['a']['history'], [{hil_report.LOCKED_CELL: 'fail'}])
        self.assertEqual(rows['b']['history'], [{'cdc': 'fail'},
                                                {'cdc': 'fail', hil_report.POOL_TIMEOUT_CELL: 'fail'}])

    def test_a_variant_boards_board_name_row_keeps_its_lock(self):
        """Its board-name row held only `board-locked`; the retry's rows are variant-named,
        so the emptied row stays in the sidecar for its history, out of the table."""
        self.attempt([('nano', {hil_report.LOCKED_CELL: 'fail'})], fresh=True)
        begin(self.rd)
        hil_report.accumulate_report(
            [('nano', 0, [], [('nano-fs', {'cdc': 'pass'}, '1s'), ('nano-hs', {'cdc': 'pass'}, '1s')], None)],
            self.rd, False, '', '')
        doc = self.doc()
        row = next(r for r in doc['rows'] if r['board'] == 'nano')
        self.assertEqual((row['cells'], row['history'], row['retired']),
                         ({}, [{hil_report.LOCKED_CELL: 'fail'}], True))
        md = (self.rd / hil_report.REPORT_MD).read_text()
        self.assertNotIn('| nano ', md)
        res = self.results(doc, ['nano'])['nano']
        self.assertTrue(res['pass'])
        self.assertEqual(res['detail'], '2 variant(s), 2 cell(s) ok')
        self.assertEqual(res['history'], {'nano': [{hil_report.LOCKED_CELL: 'fail'}]})
        # and it survives a further retry
        begin(self.rd)
        hil_report.accumulate_report(
            [('nano', 0, [], [('nano-fs', {'cdc': 'pass'}, '1s')], None)], self.rd, False, '', '')
        self.assertIn('nano', [r['board'] for r in self.doc()['rows']])

    def test_an_emptied_variant_row_still_renders_and_keeps_its_history(self):
        """Only a retired board-name row leaves the table: a variant row an accumulated run
        emptied (its filters selected no tests, clearing a boundary failure) renders blank
        as at HEAD, counts as a variant, and its earlier failure stays visible."""
        boundary = hil_report.BOUNDARY_CELL
        cfg = {'boards': [{'name': 'b', 'variant': [{'name': 'b-fs'}, {'name': 'b-hs'}]}]}
        hil_report.accumulate_report(
            [('b', 1, [], [('b-fs', {'cdc': 'pass'}, '1s'), ('b-hs', {boundary: 'fail'}, '1s')], None)],
            self.rd, True, '', '')
        begin(self.rd)
        hil_report.accumulate_report(
            [('b', 0, [], [('b-fs', {}, None), ('b-hs', {}, None)], None)], self.rd, False, '', '')
        doc = self.doc()
        rows = {r['board']: r for r in doc['rows']}
        self.assertEqual((rows['b-hs']['cells'], rows['b-hs']['history']), ({}, [{boundary: 'fail'}]))
        self.assertNotIn('retired', rows['b-hs'])
        self.assertIn('| b-hs ', (self.rd / hil_report.REPORT_MD).read_text())
        res = hil_report.summarize(cfg, ['b'], doc)['results'][0]
        self.assertEqual(res['detail'], '2 variant(s), 1 cell(s) ok')
        self.assertEqual(res['history'], {'b-hs': [{boundary: 'fail'}]})

    def test_a_retired_row_that_gets_a_cell_again_renders(self):
        self.attempt([('nano', {hil_report.LOCKED_CELL: 'fail'})], fresh=True)
        begin(self.rd)
        hil_report.accumulate_report(
            [('nano', 0, [], [('nano-fs', {'cdc': 'pass'}, '1s')], None)], self.rd, False, '', '')
        begin(self.rd)
        hil_report.write_timeout_report(self.rd, [{'name': 'nano'}], CAVEAT)
        row = next(r for r in self.doc()['rows'] if r['board'] == 'nano')
        self.assertNotIn('retired', row)
        self.assertIn('| nano ', (self.rd / hil_report.REPORT_MD).read_text())

    def test_a_fresh_run_starts_without_history(self):
        self.attempt([('a', {'cdc': 'fail'})], fresh=True)
        self.attempt([('a', {'cdc': 'pass'})])
        doc = self.attempt([('a', {'cdc': 'pass'})], fresh=True)
        self.assertNotIn('history', doc['rows'][0])

    def test_history_survives_the_other_writers(self):
        self.attempt([('a', {'cdc': 'fail'})], fresh=True)
        self.attempt([('a', {'cdc': 'pass'})])
        begin(self.rd)
        hil_report.mark_report_no_boards(self.rd, 'filters emptied', fresh=False)
        self.assertEqual(self.doc()['rows'][0]['history'], [{'cdc': 'fail'}])

    def test_a_malformed_history_is_dropped_not_fatal(self):
        (self.rd / hil_report.REPORT_JSON).write_text(json.dumps({'rows': [
            {'board': 'a', 'cells': {'cdc': 'pass'}, 'history': [{'cdc': 'fail', 'x': 3}, 'no', None]},
            {'board': 'b', 'cells': {'cdc': 'pass'}, 'history': {'cdc': ['fail']}, 'retired': True},
            {'board': 'c', 'cells': {}, 'retired': 'yes'}]}))
        doc, _ = hil_report._load(self.rd)
        self.assertEqual(doc['rows'][0]['history'], [{'cdc': 'fail'}])
        self.assertNotIn('history', doc['rows'][1])
        self.assertNotIn('retired', doc['rows'][1], 'a retired flag hid a row with cells')
        self.assertNotIn('retired', doc['rows'][2])
        doc = self.attempt([('a', {'cdc': 'skip'})])
        self.assertEqual(doc['rows'][0]['history'], [{'cdc': 'fail'}, {'cdc': 'pass'}])


class SummarizeSeesEveryRow(unittest.TestCase):
    def test_board_name_rows_reach_a_variant_boards_verdict(self):
        """hil_test writes lock-contention and pool-timeout rows keyed by BOARD name, but
        variants_of returns only declared variant names -- so for nanoch32v203 and
        ch32v307v_r1_1v0 those rows were invisible and a held lock published as a
        hardware FAIL that the caller never retried."""
        cfg = {'boards': [{'name': 'nano',
                           'variant': [{'name': 'nano-fsdev'}, {'name': 'nano-usbfs'}]}]}
        doc = {'rows': [{'board': 'nano', 'cells': {'board-locked': 'fail'},
                         'duration': None}], 'scope': '', 'caveat': ''}
        r = hil_report.summarize(cfg, ['nano'], doc)['results'][0]
        self.assertTrue(r['ran'])
        self.assertTrue(r['locked'], 'a held lock was published as a hardware failure')

    def test_a_malformed_row_does_not_kill_the_cli(self):
        """summarize is the one reader with no defense, and it is the only one an agent's
        verdict depends on."""
        out = hil_report.summarize({'boards': [{'name': 'a'}]}, ['a'],
                                   {'rows': [{'cells': {}}, {'board': 'a',
                                                             'cells': {'t': 'pass'}}]})
        self.assertTrue(out['results'][0]['pass'])


class RunVerdictIsASnapshot(unittest.TestCase):
    """`pass` is what the caller reads instead of re-deriving "every row passed and no caveat".
    It judges THIS report only: accumulate_report() drops an earlier attempt's caveat, so the
    verdict of a retry sequence is the caller's from every attempt's result."""

    ABORTED = '**HIL run aborted: worker pool timed out after 1s.** treat board results as unverified.\n'

    def _verdict(self, boards, rows, cfg_boards=None, caveat=''):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        d = Path(td.name)
        hil_report.write_report(d, {'rows': [{'board': b, 'cells': c, 'duration': '1s'} for b, c in rows],
                                    'scope': '', 'caveat': caveat})
        cfg = {'boards': cfg_boards or [{'name': b} for b in boards]}
        return hil_report.summarize(cfg, boards, hil_report._load(d)[0])

    def test_every_row_passing_and_no_caveat_is_a_pass(self):
        v = self._verdict(['a', 'b'], [('a', {'usbtest': 'pass'}), ('b', {'usbtest': 'pass'})])
        self.assertTrue(v['pass'])

    def test_a_caveat_fails_a_run_whose_rows_all_pass(self):
        v = self._verdict(['a'], [('a', {'usbtest': 'pass'})], caveat=self.ABORTED)
        self.assertTrue(all(r['pass'] for r in v['results']))
        self.assertFalse(v['pass'])

    def test_one_failing_locked_wedged_missing_or_unknown_row_fails_it(self):
        self.assertFalse(self._verdict(['a', 'b'], [('a', {'usbtest': 'pass'}), ('b', {'usbtest': 'fail'})])['pass'])
        self.assertFalse(self._verdict(['a'], [('a', {hil_report.LOCKED_CELL: 'fail'})])['pass'])
        self.assertFalse(self._verdict(['a'], [('a', {hil_report.WEDGED_CELL: 'fail'})])['pass'])
        self.assertFalse(self._verdict(['a', 'b'], [('a', {'usbtest': 'pass'})])['pass'])
        self.assertFalse(self._verdict(['a', 'zz'], [('a', {'usbtest': 'pass'})],
                                       cfg_boards=[{'name': 'a'}])['pass'])

    def test_no_rows_is_not_a_pass(self):
        self.assertFalse(self._verdict(['a'], [])['pass'])

    def test_the_verdict_is_per_snapshot_across_an_accumulate_retry(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        cfg = {'boards': [{'name': 'boardA'}]}
        rows = [('boardA', 0, 0, [('boardA', {'cdc_msc': 'OK'}, '1s')], 0)]
        # an aborted first attempt: the caveat is set on a report whose rows pass
        hil_report.accumulate_report(rows, rd, True, '', '')
        doc, _ = hil_report._load(rd)
        doc['caveat'] = self.ABORTED
        hil_report.write_report(rd, doc)
        self.assertFalse(hil_report.summarize(cfg, ['boardA'], hil_report._load(rd)[0])['pass'])
        # a clean accumulated re-run clears the caveat, and THIS snapshot passes: the sequence
        # verdict is the caller's, who still holds the first attempt's False
        begin(rd)
        hil_report.accumulate_report(rows, rd, False, '', '')
        self.assertTrue(hil_report.summarize(cfg, ['boardA'], hil_report._load(rd)[0])['pass'])
        # the other direction: a clean first attempt, then a re-run that aborted
        doc, _ = hil_report._load(rd)
        doc['caveat'] = self.ABORTED
        hil_report.write_report(rd, doc)
        self.assertFalse(hil_report.summarize(cfg, ['boardA'], hil_report._load(rd)[0])['pass'])


class NoBoardsExitKeepsWhatRan(unittest.TestCase):
    def test_it_does_not_wipe_an_accumulated_sidecar(self):
        """Master wrote only markdown here, so the sidecar survived. Writing rows:[]
        unconditionally makes an --accumulate rerun whose filters empty erase every
        board that had already passed."""
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_report.accumulate_report(
            [('a', 0, 0, [('a', {'t': 'OK'}, '1s')], 0)], rd, True, '', '')
        begin(rd)
        hil_report.mark_report_no_boards(rd, 'No boards left after the flasher filter',
                                         fresh=False)
        doc = json.loads((rd / hil_report.REPORT_JSON).read_text())
        self.assertEqual([r['board'] for r in doc['rows']], ['a'])
        self.assertIn('selected no boards', doc['caveat'])
        self.assertIn('selected no boards', (rd / hil_report.REPORT_MD).read_text())


class TheMergeBehavioursAreActuallyPinned(unittest.TestCase):
    """accumulate_report's docstring cites these three as the reason not to split it, yet
    deleting any of them left the whole suite green. Mutation-verified."""

    def test_a_cleared_boundary_drops_the_previous_attempts_mark(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_report.accumulate_report(
            [('b', 0, 0, [('b-v', {hil_report.BOUNDARY_CELL: 'fail'}, '1s')], 0)],
            rd, True, '', '')
        begin(rd)
        hil_report.accumulate_report(
            [('b', 0, 0, [('b-v', {'cdc_msc': 'OK'}, '2s')], 0)], rd, False, '', '')
        cells = json.loads((rd / hil_report.REPORT_JSON).read_text())['rows'][0]['cells']
        self.assertNotIn(hil_report.BOUNDARY_CELL, cells)

    def test_a_board_that_really_ran_drops_its_stale_lock_cell(self):
        for key in (hil_report.LOCKED_CELL, hil_report.LOCK_UNAVAILABLE_CELL):
            with self.subTest(key=key):
                td = TemporaryDirectory()
                self.addCleanup(td.cleanup)
                rd = Path(td.name)
                hil_report.accumulate_report(
                    [('b', 0, 0, [('b', {key: 'fail'}, None)], 0)], rd, True, '', '')
                begin(rd)
                hil_report.accumulate_report(
                    [('b', 0, 0, [('b', {'cdc_msc': 'OK'}, '2s')], 0)], rd, False, '', '')
                rows = json.loads((rd / hil_report.REPORT_JSON).read_text())['rows']
                self.assertEqual([r['board'] for r in rows], ['b'])
                self.assertNotIn(key, rows[0]['cells'])

    def test_a_lock_refusal_replaces_the_other_kind(self):
        unusable = (hil_report.LOCK_UNAVAILABLE_CELL, '❌ board lock unavailable: [Errno 13] x')
        held = (hil_report.LOCKED_CELL, 'fail')
        for first, second, locked in ((unusable, held, True), (held, unusable, False)):
            with self.subTest(first=first[0], then=second[0]):
                td = TemporaryDirectory()
                self.addCleanup(td.cleanup)
                rd = Path(td.name)
                for fresh, (key, val) in ((True, first), (False, second)):
                    begin(rd)
                    md = hil_report.accumulate_report(
                        [('b', 1, [], [('b', {key: val}, None)], None)], rd, fresh, '', '')
                doc = json.loads((rd / hil_report.REPORT_JSON).read_text())
                self.assertEqual(doc['rows'][0]['cells'], {second[0]: second[1]})
                self.assertIn('❌ 1 failed', md)
                got = hil_report.summarize({'boards': [{'name': 'b'}]}, ['b'], doc)['results'][0]
                self.assertEqual((got['locked'], got['ran']), (locked, locked))

    def test_a_filtered_rerun_keeps_the_previous_duration(self):
        """A -t-filtered re-run reports duration None; blanking the column loses the only
        record of how long the full run took."""
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_report.accumulate_report(
            [('b', 0, 0, [('b', {'cdc_msc': 'OK'}, '119s')], 0)], rd, True, '', '')
        begin(rd)
        hil_report.accumulate_report(
            [('b', 0, 0, [('b', {'cdc_msc': 'OK'}, None)], 0)], rd, False, '', '')
        self.assertEqual(json.loads(
            (rd / hil_report.REPORT_JSON).read_text())['rows'][0]['duration'], '119s')


class TheFooterCountsAreNotSwapped(unittest.TestCase):
    """SKILL.md tells the operator to paste the footer counts verbatim, and swapping the
    failed/skipped tallies left the suite green."""

    def test_each_kind_is_counted_under_its_own_label(self):
        md = hil_report.render_matrix([
            ('b', {'p1': 'pass', 'p2': 'pass', 'f1': 'fail',
                   's1': f'{hil_report.REPORT_CELL["skip"]} board wedged'}, '1s')])
        self.assertIn(f'{hil_report.REPORT_CELL["pass"]} 2 passed', md)
        self.assertIn(f'{hil_report.REPORT_CELL["fail"]} 1 failed', md)
        self.assertIn(f'{hil_report.REPORT_CELL["skip"]} 1 skipped', md)


class CaveatDoesNotCarryAcrossARetry(unittest.TestCase):
    """`caveat` describes how a RUN ENDED, so it must not carry: a clean retry that reports
    an earlier attempt's abort tells the agent a green run failed."""

    def test_a_clean_retry_drops_the_previous_abort_notice(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_report.accumulate_report([('a', 0, 0, [('a', {'t': 'OK'}, '1s')], 0)],
                                     rd, True, '', '')
        begin(rd)
        hil_report.write_timeout_report(rd, [], CAVEAT)
        begin(rd)
        hil_report.accumulate_report([('a', 0, 0, [('a', {'t': 'OK'}, '2s')], 0)],
                                     rd, False, '', '')
        self.assertEqual(json.loads((rd / hil_report.REPORT_JSON).read_text())['caveat'], '')


class AMalformedSidecarNeverCostsTheReport(unittest.TestCase):
    """hil_remote.py now uploads a sidecar as the merge base, so a non-conforming one is
    reachable from outside the harness."""

    def _write(self, rd, doc):
        (rd / hil_report.REPORT_JSON).write_text(json.dumps(doc))

    def test_a_null_text_field_does_not_kill_a_successful_run(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        self._write(rd, {'rows': [{'board': 'a', 'cells': {'t': 'pass'}, 'duration': '1s'}],
                         'caveat': None, 'scope': None})
        begin(rd)
        hil_report.accumulate_report([('b', 0, 0, [('b', {'t': 'OK'}, '1s')], 0)],
                                     rd, False, '', '')
        self.assertTrue((rd / hil_report.REPORT_MD).is_file())

    def test_a_null_cells_row_still_gets_its_pool_timeout_cell(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        self._write(rd, {'rows': [{'board': 'boardA', 'cells': None, 'duration': '61s'}],
                         'caveat': '', 'scope': ''})
        begin(rd)
        hil_report.write_timeout_report(rd, [{'name': 'boardA'}], CAVEAT)
        doc = json.loads((rd / hil_report.REPORT_JSON).read_text())
        v = hil_report.summarize({'boards': [{'name': 'boardA'}]}, ['boardA'], doc)
        self.assertFalse(v['results'][0]['pass'],
                         'a board that ate the whole pool guard was published as a pass')

    def test_a_malformed_roster_entry_still_leaves_an_artifact(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        import io
        from contextlib import redirect_stdout
        with redirect_stdout(io.StringIO()):
            begin(rd)
            hil_report.write_timeout_report(rd, ['plainstring'], CAVEAT)
        self.assertTrue((rd / hil_report.REPORT_MD).is_file(), 'no artifact at all')


class PoolTimeoutOutranksAStaleLock(unittest.TestCase):
    def test_a_wedge_is_not_published_as_lock_contention(self):
        """locked was computed across every cell and short-circuited detail, so a board
        that wedged the rig on the retry was reported as LOCKED -- and the caller
        re-runs those, paying another pool guard on a board that just hung it."""
        doc = {'rows': [{'board': 'boardX',
                         'cells': {'board-locked': 'fail', 'pool-timeout': 'fail'},
                         'duration': None}], 'caveat': '', 'scope': ''}
        r = hil_report.summarize({'boards': [{'name': 'boardX'}]}, ['boardX'], doc)['results'][0]
        self.assertFalse(r['locked'], 'a wedge was published as lock contention')
        self.assertFalse(r['pass'])

    def test_a_run_aborted_board_is_not_published_as_lock_contention(self):
        """run-aborted is written by the same _abort_report path as pool-timeout, for a
        board the guard never reached. It has to outrank a stale lock cell for the same
        reason -- otherwise the caller re-runs a board whose worker RAISED."""
        doc = {'rows': [{'board': 'boardX',
                         'cells': {'board-locked': 'fail', 'run-aborted': 'fail'},
                         'duration': None}], 'caveat': '', 'scope': ''}
        r = hil_report.summarize({'boards': [{'name': 'boardX'}]}, ['boardX'], doc)['results'][0]
        self.assertFalse(r['locked'], 'an aborted run was published as lock contention')
        self.assertFalse(r['pass'])


class NoBoardsExitRespectsFreshness(unittest.TestCase):
    def test_a_fresh_run_does_not_republish_the_previous_rows(self):
        """It is called BEFORE the fresh wipe, so it re-published last run's green table
        under this run's red job -- the stale-table failure it exists to prevent."""
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_report.accumulate_report([('a', 0, 0, [('a', {'t': 'OK'}, '1s')], 0)],
                                     rd, True, '', '')
        hil_report.mark_report_no_boards(rd, 'filters emptied', fresh=True)
        self.assertEqual(json.loads((rd / hil_report.REPORT_JSON).read_text())['rows'], [])

    def test_an_accumulate_run_keeps_them(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_report.accumulate_report([('a', 0, 0, [('a', {'t': 'OK'}, '1s')], 0)],
                                     rd, True, '', '')
        begin(rd)
        hil_report.mark_report_no_boards(rd, 'filters emptied', fresh=False)
        self.assertEqual([r['board'] for r in json.loads(
            (rd / hil_report.REPORT_JSON).read_text())['rows']], ['a'])

    def test_it_does_not_overwrite_an_abort_notice(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_report.accumulate_report([('a', 0, 0, [('a', {'t': 'OK'}, '1s')], 0)],
                                     rd, True, '', '')
        begin(rd)
        hil_report.write_timeout_report(rd, [], CAVEAT)
        begin(rd)
        hil_report.mark_report_no_boards(rd, 'filters emptied', fresh=False)
        self.assertIn('aborted',
                      json.loads((rd / hil_report.REPORT_JSON).read_text())['caveat'])


class TheNoBoardsCallSiteIsWired(unittest.TestCase):
    """The fresh/accumulate branches of mark_report_no_boards were tested by calling it
    DIRECTLY, so both passed while hil_test.py's one real call site never passed the flag
    at all -- an --accumulate run whose filter emptied still wiped the accumulated rows.
    This drives hil_test.py itself; the no-boards exit needs only a config and a filter
    that matches nothing, so it costs no hardware."""

    def _run(self, *extra):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        (rd / 'cfg.json').write_text(json.dumps(
            {'boards': [{'name': 'alpha', 'uid': '1', 'flasher': {'name': 'jlink', 'uid': '2'}}]}))
        (rd / hil_report.REPORT_JSON).write_text(json.dumps(
            {'rows': [{'board': 'earlier', 'cells': {'t': 'pass'}, 'duration': '1s'}],
             'scope': '', 'caveat': ''}))
        r = subprocess.run(
            [sys.executable, str(Path(HIL_DIR) / 'hil_test.py'),
             '--flasher', 'nonexistent', *extra, str(rd / 'cfg.json')],
            capture_output=True, text=True, timeout=120,
            env={**os.environ, 'HIL_REPORT_DIR': str(rd)})
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        return json.loads((rd / hil_report.REPORT_JSON).read_text())

    def test_an_accumulate_run_keeps_the_accumulated_rows(self):
        doc = self._run('--accumulate')
        self.assertEqual([r['board'] for r in doc['rows']], ['earlier'],
                         "the call site did not pass fresh=not args.accumulate")
        self.assertIn('selected no boards', doc['caveat'])

    def test_a_fresh_run_does_not_republish_them(self):
        doc = self._run()
        self.assertEqual(doc['rows'], [])
        self.assertIn('selected no boards', doc['caveat'])


class EveryWriterRendersBeforeItCommits(unittest.TestCase):
    def test_accumulate_report_does_not_commit_json_then_fail_to_render(self):
        """accumulate_report hand-rolled the write instead of calling write_report, so a
        render failure left the sidecar ahead of the markdown -- the exact ordering
        write_report's docstring forbids."""
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        (rd / hil_report.REPORT_JSON).write_text(json.dumps(
            {'rows': [{'board': 'boardA', 'cells': {'t': 'pass'}, 'duration': 119.0}],
             'caveat': '', 'scope': ''}))
        begin(rd)
        hil_report.accumulate_report([('boardB', 0, 0, [('boardB', {'t': 'OK'}, '1s')], 0)],
                                     rd, False, '', '')
        doc = json.loads((rd / hil_report.REPORT_JSON).read_text())
        self.assertEqual((rd / hil_report.REPORT_MD).read_text(),
                         hil_report.render_report(doc) + '\n')


class MissingSidecarDoesNotDestroyTheMarkdown(unittest.TestCase):
    def test_an_absent_sidecar_keeps_the_prior_table(self):
        """`recovered` was only cleared when the sidecar was TORN, not when it was absent
        -- reachable from hil_remote.py's asymmetric copy-back and build.yml's skip marker."""
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        (rd / hil_report.REPORT_MD).write_text('| Board | t |\n| a | OK |\n| b | OK |\n')
        begin(rd)
        hil_report.write_timeout_report(rd, [{'name': 'stuck'}], CAVEAT)
        self.assertIn('| a | OK |', (rd / hil_report.REPORT_MD).read_text())


class LoadIsTheOnlyTrustBoundary(unittest.TestCase):
    """hil_remote.py uploads a sidecar as the merge base, so these shapes arrive from OUTSIDE
    the harness. Every one of these raised past a handler before."""

    def _seed(self, rd, raw):
        (rd / hil_report.REPORT_JSON).write_text(raw if isinstance(raw, str)
                                                 else json.dumps(raw))

    def test_a_non_list_rows_does_not_raise(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        self._seed(rd, {'rows': 1, 'caveat': '', 'scope': ''})
        begin(rd)
        hil_report.accumulate_report([('a', 0, 0, [('a', {'t': 'OK'}, '1s')], 0)],
                                     rd, False, '', '')
        self.assertTrue((rd / hil_report.REPORT_MD).is_file())

    def test_an_unhashable_cell_value_does_not_raise(self):
        """render_matrix does REPORT_CELL.get(v, v); an unhashable value raised TypeError
        on the NORMAL accumulate path."""
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        self._seed(rd, {'rows': [{'board': 'a', 'cells': {'t': ['x'], 'u': 'pass'},
                                  'duration': '1s'}],
                        'caveat': '', 'scope': ''})
        begin(rd)
        hil_report.accumulate_report([('b', 0, 0, [('b', {'t': 'OK'}, '1s')], 0)],
                                     rd, False, '', '')
        cells = {r['board']: r['cells']
                 for r in json.loads((rd / hil_report.REPORT_JSON).read_text())['rows']}
        self.assertNotIn('t', cells['a'], 'a corrupt cell must drop, not become a pass')
        self.assertIn('u', cells['a'])

    def test_summarize_survives_a_malformed_sidecar_via_load(self):
        """The CLI is the one reader an agent's verdict depends on."""
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        (rd / 'cfg.json').write_text(json.dumps({'boards': [{'name': 'a'}]}))
        self._seed(rd, {'rows': [{'board': 1, 'cells': 'notadict'},
                                 {'board': 'a', 'cells': {'t': 'pass'}}],
                        'caveat': '', 'scope': ''})
        r = subprocess.run(
            [sys.executable, str(Path(HIL_DIR) / 'helper' / 'hil_report.py'),
             str(rd / 'cfg.json'), '-b', 'a', '--report-dir', str(rd)],
            capture_output=True, text=True, timeout=60)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertTrue(json.loads(r.stdout)['results'][0]['pass'])


class NoBoardsGuardOnlyAppliesWhenAccumulating(unittest.TestCase):
    def test_a_fresh_run_carries_nothing_from_the_prior_sidecar(self):
        """rows were reset on fresh but scope was not, so a leftover or uploaded sidecar
        republished a stale scope line under this run's notice."""
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_report.accumulate_report([('old', 0, 0, [('old', {'t': 'OK'}, '1s')], 0)],
                                     rd, True, '3 board(s) — a, b, c')
        hil_report.mark_report_no_boards(rd, 'filters emptied', fresh=True)
        doc = json.loads((rd / hil_report.REPORT_JSON).read_text())
        self.assertEqual(doc['rows'], [])
        self.assertEqual(doc['scope'], '', 'a stale scope note was republished')

    def test_an_accumulate_run_keeps_rows_and_scope(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_report.accumulate_report([('old', 0, 0, [('old', {'t': 'OK'}, '1s')], 0)],
                                     rd, True, '3 board(s) — a, b, c')
        begin(rd)
        hil_report.mark_report_no_boards(rd, 'filters emptied', fresh=False)
        doc = json.loads((rd / hil_report.REPORT_JSON).read_text())
        self.assertEqual(doc['scope'], '3 board(s) — a, b, c')
        self.assertEqual([r['board'] for r in doc['rows']], ['old'])

    def test_a_fresh_run_is_not_blocked_by_a_prior_abort(self):
        """The guard runs BEFORE the fresh wipe, so guarding a fresh run left the previous
        attempt's rows AND its abort notice published as this run's."""
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_report.accumulate_report([('old', 0, 0, [('old', {'t': 'OK'}, '1s')], 0)],
                                     rd, True, '', '')
        begin(rd)
        hil_report.write_timeout_report(rd, [], CAVEAT)
        hil_report.mark_report_no_boards(rd, 'filters emptied', fresh=True)
        doc = json.loads((rd / hil_report.REPORT_JSON).read_text())
        self.assertEqual(doc['rows'], [])
        self.assertIn('selected no boards', doc['caveat'])


REPO = Path(HIL_DIR).parents[1]
# tinyusb 2c992c002's render_report() of each document, recorded by agentrc's report-core tests
GOLDEN_DIR = REPO / 'lib/agentrc/tests/data/hil_report_core'


def _cli(*argv, cwd=None, env=None):
    return subprocess.run([sys.executable, str(Path(HIL_DIR) / 'helper' / 'hil_report.py'), *argv],
                          capture_output=True, text=True, timeout=60, cwd=cwd, env=env)


class TheCoreAdapterKeepsTheNativeDocument(unittest.TestCase):
    """The report is published through agentrc's hil_report_core. What tinyusb's readers see is
    unchanged for a compatible input, with the intended exceptions enumerated here: the sidecar
    gains the reserved `_hil` object after its own fields, at the same indent, and an
    accumulating attempt's the cells it wrote (ATTEMPT_CELLS); an unreadable sidecar's earlier
    markdown goes below the table as labelled legacy text (not above it as the body);
    hil_report.py refuses a report of an unknown schema or version."""

    def setUp(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.rd = Path(td.name)

    def test_the_native_rendering_is_byte_for_byte_the_golden_one(self):
        inputs = json.loads((GOLDEN_DIR / 'inputs.json').read_text(encoding='utf-8'))['tinyusb_docs']
        golden = json.loads((GOLDEN_DIR / 'golden.json').read_text(encoding='utf-8'))['tinyusb_render_report']
        self.assertTrue(inputs)
        for name, doc in inputs.items():
            with self.subTest(name):
                self.assertEqual(hil_report.render_report(doc), golden[name])

    def test_the_sidecar_is_the_native_document_plus_hil(self):
        doc = {'rows': [{'board': 'b', 'cells': {'device/cdc_msc': 'pass'}, 'duration': '1s'}],
               'scope': '1 board(s) — b', 'caveat': ''}
        hil_report.write_report(self.rd, doc)
        text = (self.rd / hil_report.REPORT_JSON).read_text()
        published = json.loads(text)
        meta = published.pop('_hil')
        self.assertEqual(published, doc)
        self.assertEqual(text, json.dumps({**doc, '_hil': meta}, indent=2) + '\n', 'same indent, `_hil` last')
        self.assertEqual((meta['body_schema'], meta['body_version'], meta['termination']),
                         (hil_report.SCHEMA_NAME, hil_report.SCHEMA_VERSION, 'completed'))
        self.assertEqual((self.rd / hil_report.REPORT_MD).read_text(encoding='utf-8'),
                         hil_report.render_report(doc) + '\n')

    def test_the_cli_output_is_that_of_the_native_document(self):
        cfg = {'boards': [{'name': 'b'}, {'name': 'c', 'variant': [{'name': 'c-x'}]}]}
        (self.rd / 'cfg.json').write_text(json.dumps(cfg))
        doc = {'rows': [{'board': 'b', 'cells': {'t': 'pass'}, 'duration': '1s'},
                        {'board': 'c-x', 'cells': {'t': '❌ 3/30', 'u': 'skip'}, 'duration': None}],
               'scope': '', 'caveat': ''}
        out = []
        for write in (lambda: (self.rd / hil_report.REPORT_JSON).write_text(json.dumps(doc)),
                      lambda: hil_report.write_report(self.rd, doc)):
            write()
            r = _cli(str(self.rd / 'cfg.json'), '--report-dir', str(self.rd))
            self.assertEqual(r.returncode, 0, r.stderr)
            out.append(r.stdout)
        self.assertEqual(out[0], out[1])

    def test_an_unknown_schema_or_version_is_never_read_or_replaced(self):
        (self.rd / 'cfg.json').write_text(json.dumps({'boards': [{'name': 'b'}]}))
        future = json.dumps({'rows': [{'board': 'b', 'cells': {'t': 'pass'}}], 'scope': '', 'caveat': '',
                             '_hil': {'version': 2}})
        (self.rd / hil_report.REPORT_JSON).write_text(future)
        r = _cli(str(self.rd / 'cfg.json'), '--report-dir', str(self.rd))
        self.assertEqual((r.returncode, r.stdout), (1, ''))
        self.assertIn('version 2', r.stderr)
        with self.assertRaises(hil_report.core().ReportError):
            hil_report.start_attempt(self.rd, True, self.rd / 'cfg.json', ['b'])
        hil_report.mark_report_no_boards(self.rd, 'filters emptied')
        self.assertEqual((self.rd / hil_report.REPORT_JSON).read_text(), future)

    def test_a_corrupt_hil_document_yields_no_rows(self):
        (self.rd / 'cfg.json').write_text(json.dumps({'boards': [{'name': 'b'}]}))
        (self.rd / hil_report.REPORT_JSON).write_text(json.dumps(
            {'rows': [{'board': 'b', 'cells': {'t': 'pass'}}], 'scope': '', 'caveat': '',
             '_hil': {'version': 1, 'body_schema': hil_report.SCHEMA_NAME, 'body_version': 1}}))
        r = _cli(str(self.rd / 'cfg.json'), '--report-dir', str(self.rd))
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(json.loads(r.stdout)['results'][0]['detail'], 'no report row for this board')

    def test_an_unreadable_sidecars_markdown_is_kept_as_labelled_legacy_text(self):
        (self.rd / hil_report.REPORT_MD).write_text('| Board | t |\n| a | OK |\n', encoding='utf-8')
        (self.rd / hil_report.REPORT_JSON).write_text('{ truncated')
        begin(self.rd)
        hil_report.write_timeout_report(self.rd, [{'name': 'stuck'}], CAVEAT)
        md = (self.rd / hil_report.REPORT_MD).read_text(encoding='utf-8')
        doc = json.loads((self.rd / hil_report.REPORT_JSON).read_text())
        self.assertTrue(md.startswith(hil_report.render_report(doc) + '\n'), 'the table first, as rendered')
        self.assertIn('_Legacy text:', md)
        self.assertTrue(md.endswith('| Board | t |\n| a | OK |\n'))
        self.assertEqual(doc['_hil']['legacy_text'], '| Board | t |\n| a | OK |\n')
        self.assertEqual(doc['_hil']['termination'], 'timeout')


class EveryAttemptIsKept(unittest.TestCase):
    """A report a later attempt replaces is archived under hil_report.attempts/ first."""

    def setUp(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.rd = Path(td.name)
        self.cfg = self.rd / 'cfg.json'
        self.cfg.write_text(json.dumps({'boards': [{'name': 'alpha', 'uid': '1', 'flasher': {'name': 'jlink', 'uid': '2'}}]}))

    def run_once(self, accumulate, cell):
        hil_report.start_attempt(self.rd, accumulate, self.cfg, ['b'], invocation=['hil_test.py'])
        return hil_report.accumulate_report([('b', 0, 0, [('b', {'t': cell}, '1s')], 0)], self.rd, not accumulate)

    def meta(self):
        return json.loads((self.rd / hil_report.REPORT_JSON).read_text())['_hil']

    def test_a_fresh_run_archives_the_previous_and_an_accumulated_one_links_it(self):
        self.run_once(False, 'fail')
        first = self.meta()
        self.assertEqual((first['previous_attempt'], first['selection'], first['invocation']),
                         (None, ['b'], ['hil_test.py']))
        self.assertEqual(first['roster']['name'], 'cfg.json')
        self.run_once(True, 'pass')
        second = self.meta()
        self.assertEqual(second['previous_attempt'], first['attempt_id'])
        archived = self.rd / hil_report.REPORT_ATTEMPTS / first['attempt_id']
        self.assertEqual(json.loads((archived / hil_report.REPORT_JSON).read_text())['_hil'], first)
        self.run_once(False, 'pass')
        self.assertIsNone(self.meta()['previous_attempt'])
        self.assertEqual(len(list((self.rd / hil_report.REPORT_ATTEMPTS).iterdir())), 2)

    def test_repeated_writes_inside_one_attempt_are_one_attempt(self):
        hil_report.start_attempt(self.rd, False, self.cfg, ['b'])
        hil_report.accumulate_report([('b', 0, 0, [('b', {'t': 'pass'}, '1s')], 0)], self.rd, True)
        hil_report.write_timeout_report(self.rd, [{'name': 'b'}], CAVEAT)
        self.assertFalse((self.rd / hil_report.REPORT_ATTEMPTS).exists())
        self.assertEqual(self.meta()['termination'], 'timeout')

    def test_a_legacy_report_is_archived_and_linked(self):
        (self.rd / hil_report.REPORT_JSON).write_text(json.dumps({'rows': [], 'scope': '', 'caveat': ''}))
        self.run_once(True, 'pass')
        self.assertTrue(self.meta()['previous_attempt'].startswith('legacy-'))

    def test_hil_test_begins_its_attempt_before_the_no_boards_notice(self):
        (self.rd / hil_report.REPORT_JSON).write_text(json.dumps(
            {'rows': [{'board': 'earlier', 'cells': {'t': 'pass'}, 'duration': '1s'}], 'scope': '', 'caveat': ''}))
        r = subprocess.run([sys.executable, str(Path(HIL_DIR) / 'hil_test.py'), '--flasher', 'nonexistent',
                            '--accumulate', str(self.cfg)],
                           capture_output=True, text=True, timeout=120, env={**os.environ, 'HIL_REPORT_DIR': str(self.rd)})
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        meta = self.meta()
        self.assertEqual((meta['termination'], meta['selection']), ('no-boards', []))
        self.assertTrue((self.rd / hil_report.REPORT_ATTEMPTS / meta['previous_attempt']).is_dir())

    def test_a_config_error_aborts_the_boards_it_selected(self):
        self.cfg.write_text(json.dumps({'boards': [{'name': 'alpha', 'uid': '1', 'logger': 'bogus',
                                                    'flasher': {'name': 'jlink', 'uid': '2'}}]}))
        r = subprocess.run([sys.executable, str(Path(HIL_DIR) / 'hil_test.py'), str(self.cfg)],
                           capture_output=True, text=True, timeout=120, env={**os.environ, 'HIL_REPORT_DIR': str(self.rd)})
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        doc = json.loads((self.rd / hil_report.REPORT_JSON).read_text())
        self.assertEqual((doc['_hil']['termination'], doc['_hil']['selection']), ('aborted', ['alpha']))
        self.assertTrue(doc['caveat'].startswith('**HIL run aborted: config error: unknown "logger" value'), doc['caveat'])
        seq = hil_report.sequence(self.cfg, ['alpha'], self.rd)
        self.assertEqual(seq['current']['rows'][0]['verdict'], 'blocker', 'never not-run: boards were selected')

    def test_hil_test_refuses_to_start_over_a_report_it_cannot_read(self):
        future = json.dumps({'rows': [], 'scope': '', 'caveat': '', '_hil': {'version': 2}})
        (self.rd / hil_report.REPORT_JSON).write_text(future)
        r = subprocess.run([sys.executable, str(Path(HIL_DIR) / 'hil_test.py'), '--flasher', 'nonexistent',
                            str(self.cfg)],
                           capture_output=True, text=True, timeout=120, env={**os.environ, 'HIL_REPORT_DIR': str(self.rd)})
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('cannot begin the report', r.stdout)
        self.assertEqual((self.rd / hil_report.REPORT_JSON).read_text(), future)


class ConsumerCommands(unittest.TestCase):
    """`prepare` makes the markdown a rendering of the sidecar before a consumer exposes the
    pair; `no-selection` writes the report of a run the selection skipped (CI)."""

    def setUp(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.rd = Path(td.name)

    def test_prepare_renders_a_stale_markdown_again(self):
        doc = {'rows': [{'board': 'b', 'cells': {'t': 'pass'}, 'duration': '1s'}], 'scope': '', 'caveat': ''}
        hil_report.write_report(self.rd, doc)
        (self.rd / hil_report.REPORT_MD).write_text('an earlier table')
        r = _cli('prepare', '--report-dir', str(self.rd))
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('hil_report.json: valid', r.stdout)
        self.assertEqual((self.rd / hil_report.REPORT_MD).read_text(encoding='utf-8'), hil_report.render_report(doc) + '\n')

    @unittest.skipIf(os.name == 'nt', 'flock')
    def test_prepare_leaves_the_markdown_to_an_active_writer(self):
        import fcntl
        (self.rd / hil_report.REPORT_MD).write_text('| b | ✅ |\n', encoding='utf-8')
        with open(self.rd / '.hil_report.lock', 'a') as held:
            fcntl.flock(held, fcntl.LOCK_EX)
            r = _cli('prepare', '--report-dir', str(self.rd))
        self.assertEqual(r.returncode, 1)
        self.assertIn('left to that writer', r.stderr)
        self.assertEqual((self.rd / hil_report.REPORT_MD).read_text(encoding='utf-8'), '| b | ✅ |\n')

    def test_prepare_never_leaves_a_table_without_its_sidecar(self):
        (self.rd / hil_report.REPORT_MD).write_text('| b | ✅ |\n', encoding='utf-8')
        r = _cli('prepare', '--report-dir', str(self.rd))
        self.assertEqual(r.returncode, 0, r.stderr)
        md = (self.rd / hil_report.REPORT_MD).read_text(encoding='utf-8')
        self.assertTrue(md.startswith(hil_report.core().NOTICE), md)
        self.assertIn('hil_report.json is missing', md)

    def test_no_selection_writes_a_fresh_report_the_reader_checks(self):
        cfg = self.rd / 'tinyusb.json'
        cfg.write_text(json.dumps({'boards': [{'name': 'b'}]}))
        hil_report.write_report(self.rd, {'rows': [{'board': 'b', 'cells': {'t': 'pass'}, 'duration': '1s'}],
                                          'scope': '', 'caveat': ''})
        reason = '_Skipped by PR selection: no affected boards on this rig._'
        r = _cli('no-selection', str(cfg), reason, '--report-dir', str(self.rd))
        self.assertEqual(r.returncode, 0, r.stderr)
        reading = hil_report.core().read_document(self.rd / hil_report.REPORT_JSON, hil_report.schema())
        self.assertEqual((reading.outcome, reading.body['caveat'], reading.meta['termination'], reading.meta['selection']),
                         ('empty', reason + '\n', 'no-boards', []))
        self.assertEqual((self.rd / hil_report.REPORT_MD).read_text(), f'{reason}\n\nNo tests were run.\n')
        self.assertEqual(len(list((self.rd / hil_report.REPORT_ATTEMPTS).iterdir())), 1, 'the earlier report is kept')
        verdict = json.loads(_cli(str(cfg), '--report-dir', str(self.rd)).stdout)
        self.assertFalse(verdict['pass'])

    def test_without_lib_agentrc_the_commands_refuse_and_board_variants_still_imports(self):
        """CI's skipped legs run these on a checkout that may lack lib/agentrc; check_build.py
        and the CI matrix import board_variants() from a bare runner."""
        clean = self.rd / 'checkout'
        for rel in ('test/hil/helper/hil_report.py', 'test/hil/helper/agentrc.py', 'tools/get_deps.py'):
            (clean / rel).parent.mkdir(parents=True, exist_ok=True)
            (clean / rel).write_bytes((REPO / rel).read_bytes())
        (clean / 'cfg.json').write_text(json.dumps({'boards': [{'name': 'b'}]}))
        script = str(clean / 'test/hil/helper/hil_report.py')
        env = {**os.environ, 'PYTHONSAFEPATH': '1'}
        (clean / hil_report.REPORT_MD).write_text('| b | ✅ |\n', encoding='utf-8')

        def run(*argv):
            return subprocess.run([sys.executable, script, *argv, '--report-dir', str(clean)], capture_output=True,
                                  text=True, timeout=60, env=env)
        r = run('no-selection', str(clean / 'cfg.json'), 'skipped')
        self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
        self.assertIn('python3 tools/get_deps.py lib/agentrc', r.stderr)
        self.assertFalse((clean / hil_report.REPORT_JSON).exists())
        if os.name != 'nt':
            import fcntl
            with open(clean / '.hil_report.lock', 'a') as held:
                fcntl.flock(held, fcntl.LOCK_EX)
                r = run('prepare')
                self.assertEqual(r.returncode, 1, r.stdout + r.stderr)
                self.assertEqual((clean / hil_report.REPORT_MD).read_text(encoding='utf-8'), '| b | ✅ |\n',
                                 'never written under another writer\'s lock')
        r = run('prepare')
        self.assertEqual(r.returncode, 1, 'it could not prepare the pair')
        self.assertIn('python3 tools/get_deps.py lib/agentrc', r.stderr)
        md = (clean / hil_report.REPORT_MD).read_text(encoding='utf-8')
        self.assertTrue(md.startswith('**HIL report unavailable: hil_report.md could not be prepared'), md)
        self.assertNotIn('✅', md, 'the table the JSON does not back is gone')
        r = subprocess.run([sys.executable, '-c', 'import sys; sys.path.insert(0, "test/hil/helper"); import hil_report; '
                            'print(hil_report.board_variants({"name": "b"})[0]["name"])'],
                           capture_output=True, text=True, timeout=60, cwd=clean)
        self.assertEqual((r.returncode, r.stdout.strip()), (0, 'b'), r.stderr)

    def test_the_cli_runs_as_a_script_under_pythonsafepath(self):
        cfg = self.rd / 'cfg.json'
        cfg.write_text(json.dumps({'boards': [{'name': 'b'}]}))
        hil_report.write_report(self.rd, {'rows': [{'board': 'b', 'cells': {'t': 'pass'}, 'duration': '1s'}],
                                          'scope': '', 'caveat': ''})
        r = _cli(str(cfg), '--report-dir', str(self.rd), env={**os.environ, 'PYTHONSAFEPATH': '1'})
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertTrue(json.loads(r.stdout)['pass'])



class AFailedPublishNeverLeavesThePredecessorAsThisRuns(unittest.TestCase):
    """An abort whose report cannot be published withdraws the report in place: archived,
    replaced by a withdrawal record no preparation renders, which the next attempt links."""

    def setUp(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.rd = Path(td.name)
        self.cfg = self.rd / 'cfg.json'
        self.cfg.write_text(json.dumps({'boards': [{'name': 'a'}]}))
        self.attempt(False, '✅')
        self.first = self.meta()['attempt_id']
        self.second = hil_report.start_attempt(self.rd, True, self.cfg, ['a']).attempt_id
        self.c = hil_report.core()

    def attempt(self, accumulate, cell):
        hil_report.start_attempt(self.rd, accumulate, self.cfg, ['a'])
        hil_report.accumulate_report([('a', 0, 0, [('a', {'t': cell}, '1s')], 0)], self.rd, not accumulate)

    def meta(self):
        return json.loads((self.rd / hil_report.REPORT_JSON).read_text())['_hil']

    def reading(self):
        return self.c.read_document(self.rd / hil_report.REPORT_JSON, hil_report.schema())

    def abort(self, **patches):
        with mock.patch.multiple(self.c, **patches), redirect_stdout(io.StringIO()) as out:
            hil_report.write_timeout_report(self.rd, [{'name': 'a'}], CAVEAT)
        return out.getvalue()

    def refuse(self, *a, **k):
        raise self.c.ReportError('replace', 'disk full')

    def test_the_predecessor_is_withdrawn_and_never_prepared_as_this_runs_pass(self):
        out = self.abort(publish=self.refuse)
        self.assertIn('withdrawn: this run published no report', out)
        r = self.reading()
        self.assertEqual((r.outcome, r.body['attempt_id'], r.body['archived']), ('withdrawn', self.second, self.first))
        self.c.prepare_pair(self.rd, hil_report.schema())
        self.assertTrue((self.rd / hil_report.REPORT_MD).read_text(encoding='utf-8').startswith(self.c.NOTICE))
        verdict = json.loads(_cli(str(self.cfg), '--report-dir', str(self.rd)).stdout)
        self.assertFalse(verdict['pass'], 'the predecessor\'s green row is not this run\'s')
        self.assertTrue((self.rd / hil_report.REPORT_ATTEMPTS / self.first / hil_report.REPORT_JSON).is_file())

    def test_the_next_attempt_links_the_withdrawal_and_merges_the_report_it_withdrew(self):
        self.abort(publish=self.refuse)
        self.attempt(True, '✅')
        self.assertEqual(self.meta()['previous_attempt'], f'withdrawn-{self.second}')
        self.assertEqual([r['board'] for r in json.loads((self.rd / hil_report.REPORT_JSON).read_text())['rows']], ['a'])
        seq = hil_report.sequence(self.cfg, ['a'], self.rd)['sequence']
        self.assertEqual(seq['rows'][0]['verdict'], 'blocker', 'an attempt without a report blocks the sequence')

    def test_a_json_published_before_its_markdown_failed_is_kept(self):
        real = self.c.publish

        def markdown_fails(report_dir, schema, body, meta):
            jpath, mpath = real(report_dir, schema, body, meta)
            mpath.write_text('a stale table')
            raise self.c.ReportError('markdown', 'not replaced', published=(str(jpath),))
        self.abort(publish=markdown_fails)
        meta = self.meta()
        self.assertEqual((meta['attempt_id'], meta['termination']), (self.second, 'timeout'))
        doc = json.loads((self.rd / hil_report.REPORT_JSON).read_text())
        self.assertEqual((self.rd / hil_report.REPORT_MD).read_text(encoding='utf-8'),
                         hil_report.render_report(doc) + '\n')

    def test_a_one_shot_write_whose_markdown_failed_is_repaired_as_its_own_attempt(self):
        real = self.c.publish
        failed = []

        def markdown_fails(report_dir, schema, body, meta):
            jpath, mpath = real(report_dir, schema, body, meta)
            mpath.write_text('a stale table')
            failed.append(self.c.ReportError('markdown', 'not replaced', published=(str(jpath),)))
            raise failed[-1]
        hil_report._runs.clear()
        with mock.patch.object(self.c, 'publish', markdown_fails), redirect_stdout(io.StringIO()) as out:
            hil_report.mark_report_no_boards(self.rd, 'none left')
        self.assertEqual(out.getvalue().count('warning:'), 1, 'only the failed write itself: ' + out.getvalue())
        one_shot = self.meta()['attempt_id']
        self.assertNotIn(one_shot, (self.first, self.second))
        doc = json.loads((self.rd / hil_report.REPORT_JSON).read_text())
        self.assertEqual((self.rd / hil_report.REPORT_MD).read_text(encoding='utf-8'),
                         hil_report.render_report(doc) + '\n')
        self.assertEqual(hil_report._runs, {}, 'a one-shot attempt is not this dir\'s run')
        self.assertEqual(self.c.recover_publish(self.rd, hil_report.schema(), None, failed[0]).outcome, 'refused')
        hil_report.write_report(self.rd, {'rows': [], 'scope': '', 'caveat': ''})
        self.assertNotEqual(self.meta()['attempt_id'], one_shot, 'each one-shot write is an attempt of its own')

    def test_two_withdrawals_then_a_subset_retry_keep_every_row(self):
        hil_report.start_attempt(self.rd, True, self.cfg, ['b'])
        hil_report.accumulate_report([('b', 0, 0, [('b', {'t': '✅'}, '1s')], 0)], self.rd, False)   # a and b
        for _ in range(2):
            hil_report.start_attempt(self.rd, True, self.cfg, ['a'])
            self.abort(publish=self.refuse)
        self.assertEqual(self.reading().outcome, 'withdrawn')
        self.attempt(True, '✅')
        self.assertEqual(sorted(r['board'] for r in json.loads((self.rd / hil_report.REPORT_JSON).read_text())['rows']),
                         ['a', 'b'], 'the second withdrawal names the first, a sibling archive')

    def test_a_cycle_of_withdrawal_references_ends(self):
        root = self.rd / hil_report.REPORT_ATTEMPTS
        for aid, other in (('withdrawn-x', 'withdrawn-y'), ('withdrawn-y', 'withdrawn-x')):
            (root / aid).mkdir(parents=True)
            (root / aid / hil_report.REPORT_JSON).write_text(json.dumps({hil_report.core().WITHDRAWN: {
                'version': 1, 'body_schema': hil_report.SCHEMA_NAME, 'attempt_id': aid[len('withdrawn-'):],
                'archived': other, 'reason': 'r'}}))
        (self.rd / hil_report.REPORT_JSON).write_bytes((root / 'withdrawn-x' / hil_report.REPORT_JSON).read_bytes())
        self.assertEqual(hil_report._load(self.rd)[0]['rows'], [])

    def test_a_malformed_withdrawal_is_never_read_as_rows(self):
        malformed = json.dumps({hil_report.core().WITHDRAWN: {'malformed': True},
                                'rows': [{'board': 'a', 'cells': {'t': 'pass'}}], 'scope': '', 'caveat': ''})
        archived = self.rd / hil_report.REPORT_ATTEMPTS / 'withdrawn-x'
        archived.mkdir(parents=True)
        (archived / hil_report.REPORT_JSON).write_text(malformed)
        through = json.dumps({hil_report.core().WITHDRAWN: {
            'version': 1, 'body_schema': hil_report.SCHEMA_NAME, 'attempt_id': 'y', 'archived': 'withdrawn-x',
            'reason': 'r'}})
        for name, current in (('current', malformed), ('through an archive', through)):
            with self.subTest(name):
                (self.rd / hil_report.REPORT_JSON).write_text(current)
                self.assertEqual(hil_report._load(self.rd), ({'rows': [], 'scope': '', 'caveat': ''}, False))

    def test_a_failed_no_selection_write_withdraws_before_the_upload_prepares(self):
        def refuse(*a, **k):
            raise self.c.ReportError('replace', 'disk full')
        hil_report._runs.clear()
        with mock.patch.object(self.c, 'publish', refuse), redirect_stdout(io.StringIO()), \
                contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(hil_report.main(['no-selection', str(self.cfg), 'skipped', '--report-dir', str(self.rd)]), 1)
        r = _cli('prepare', '--report-dir', str(self.rd))
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('withdrawn', r.stdout)
        self.assertTrue((self.rd / hil_report.REPORT_MD).read_text(encoding='utf-8').startswith(self.c.NOTICE))
        self.assertFalse(json.loads(_cli(str(self.cfg), '--report-dir', str(self.rd)).stdout)['pass'])

    def test_an_accumulating_write_outside_an_attempt_is_refused(self):
        hil_report._runs.clear()
        with self.assertRaises(self.c.ReportError) as e:
            hil_report.accumulate_report([('a', 0, 0, [('a', {'t': '✅'}, '1s')], 0)], self.rd, False)
        self.assertEqual(e.exception.stage, 'attempt')
        self.assertEqual(self.meta()['attempt_id'], self.first, 'nothing was written')


class TheSequenceVerdictReadsEveryAttempt(unittest.TestCase):
    """hil_report.sequence(): each attempt of the chain folded by the shared rules over the
    requested boards it selected; the snapshot CLI is untouched."""

    def setUp(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.rd = Path(td.name)
        self.cfg = self.rd / 'cfg.json'
        self.cfg.write_text(json.dumps({'boards': [{'name': 'a', 'variant': [{'name': 'a-fs'}, {'name': 'a-hs'}]},
                                                   {'name': 'b'}, {'name': 'c'}]}))

    def attempt(self, accumulate, results):
        """results {board: {row: cells}}: one hil_test.py run selecting those boards."""
        hil_report.start_attempt(self.rd, accumulate, self.cfg, list(results), invocation=['hil_test.py'])
        mret = [(b, 0, [], [(row, cells, '1s') for row, cells in rows.items()], None) for b, rows in results.items()]
        hil_report.accumulate_report(mret, self.rd, not accumulate)
        return self.meta()['attempt_id']

    def meta(self):
        return json.loads((self.rd / hil_report.REPORT_JSON).read_text())['_hil']

    def verdicts(self, boards=('a', 'b', 'c')):
        seq = hil_report.sequence(self.cfg, list(boards), self.rd)['sequence']
        return {r['board']: (r['verdict'], r['flaky']) for r in seq['rows']}

    PASS_A = {'a-fs': {'device/cdc_msc': '✅'}, 'a-hs': {'device/cdc_msc': '✅'}}
    FAIL_A = {'a-fs': {'device/cdc_msc': '❌ boom'}, 'a-hs': {'device/cdc_msc': '✅'}}

    def test_an_eligible_retry_of_a_fail_passes_flaky(self):
        self.attempt(False, {'a': self.FAIL_A, 'b': {'b': {'t': '✅'}}, 'c': {'c': {'t': '✅'}}})
        self.attempt(True, {'a': self.PASS_A})
        self.assertEqual(self.verdicts(), {'a': ('pass', True), 'b': ('pass', False), 'c': ('pass', False)})
        current = hil_report.sequence(self.cfg, ['a', 'b', 'c'], self.rd)['current']
        self.assertEqual([r['board'] for r in current['rows']], ['a'], 'the retry ran a alone')

    def test_a_retry_after_a_locked_peer_is_not_eligible(self):
        self.attempt(False, {'a': self.FAIL_A, 'c': {'c': {hil_report.LOCKED_CELL: 'held'}}})
        self.attempt(True, {'a': self.PASS_A})
        got = self.verdicts(['a', 'c'])
        self.assertEqual(got, {'a': ('fail', False), 'c': ('not-run', False)})

    def test_a_usbtest_batterys_fail_is_final(self):
        self.attempt(False, {'b': {'b': {'device/usbtest': '❌ 28/29'}}})
        self.attempt(True, {'b': {'b': {'device/usbtest': '✅ 29/29'}}})
        self.assertEqual(self.verdicts(['b'])['b'], ('fail', False))

    def test_an_aborted_predecessor_stays_a_blocker(self):
        hil_report.start_attempt(self.rd, False, self.cfg, ['a', 'b'])
        hil_report.accumulate_report([('b', 0, [], [('b', {'t': '✅'}, '1s')], None)], self.rd, True)
        hil_report.write_timeout_report(self.rd, [{'name': 'a'}], CAVEAT)
        self.attempt(True, {'a': self.PASS_A})
        self.assertEqual(self.verdicts(['a', 'b']), {'a': ('blocker', False), 'b': ('blocker', False)})

    def test_a_missing_record_denies_every_pass(self):
        first = self.attempt(False, {'a': self.PASS_A, 'b': {'b': {'t': '✅'}}})
        self.attempt(True, {'b': {'b': {'t': '✅'}}})
        import shutil
        shutil.rmtree(self.rd / hil_report.REPORT_ATTEMPTS / first)
        self.assertEqual(self.verdicts(['a', 'b']), {'a': ('not-run', False), 'b': ('blocker', False)})

    def test_another_roster_is_never_read_as_this_ones(self):
        self.attempt(False, {'b': {'b': {'t': '✅'}}})
        self.cfg.write_text(self.cfg.read_text() + ' ')
        seq = hil_report.sequence(self.cfg, ['b'], self.rd)['sequence']
        self.assertEqual((seq['verdict'], [x['attempt_id'] for x in seq['broken']]),
                         ('not-run', [self.meta()['attempt_id']]), 'its rows are not read, so never a pass')

    def test_the_rows_fold_to_the_shared_verdicts(self):
        cfg = json.loads(self.cfg.read_text())
        doc = {'rows': [{'board': 'a-fs', 'cells': {hil_report.WEDGED_CELL: '❌'}, 'duration': None},
                        {'board': 'b', 'cells': {hil_report.RUN_ABORTED_CELL: hil_report.BUILD_REFUSED}, 'duration': None},
                        {'board': 'c', 'cells': {'t': '⚪ no binary'}, 'duration': None}], 'scope': '', 'caveat': ''}
        rows = {r['board']: r['verdict'] for r in hil_report.normalised_rows(cfg, ['a', 'b', 'c'], doc)}
        self.assertEqual(rows, {'a': 'blocker', 'b': 'not-run', 'c': 'not-run'})
        self.assertEqual(hil_report.normalised_rows(cfg, ['a'], {'rows': [], 'scope': '', 'caveat': ''}), [],
                         'a board without a row gets none: apply_run_rules makes it a blocker')

    def test_a_query_for_fewer_boards_still_sees_the_peers_that_bar_a_retry(self):
        for name, peer in (('locked', {hil_report.LOCKED_CELL: 'held'}), ('blocker', {hil_report.WEDGED_CELL: '❌'})):
            with self.subTest(name):
                self.setUp()
                self.attempt(False, {'a': self.FAIL_A, 'b': {'b': peer}})
                self.attempt(True, {'a': self.PASS_A})
                self.assertEqual(self.verdicts(['a'])['a'], ('fail', False), 'never a flaky pass by leaving b out')
                self.assertEqual(self.verdicts(['a', 'b'])['a'], ('fail', False))
                current = hil_report.sequence(self.cfg, ['b'], self.rd)['current']
                self.assertEqual((current['rows'], current['verdict']), ([], 'not-run'),
                                 'only the queried boards are returned')

    def test_a_retry_that_ran_nothing_is_graded_from_its_own_cells(self):
        # a -t host/device_info retry on a device-only board: its empty rows clear the boundary
        # cell from the merged table, but the earlier pass beside it is not this attempt's
        self.attempt(False, {'a': {'a-fs': {'device/cdc_msc': '✅'}, 'a-hs': {hil_report.BOUNDARY_CELL: 'fail'}}})
        self.attempt(True, {'a': {'a-fs': {}, 'a-hs': {}}})
        self.assertEqual(self.verdicts(['a'])['a'], ('fail', False), 'never a flaky pass')
        current = hil_report.sequence(self.cfg, ['a'], self.rd)['current']
        self.assertEqual(current['rows'][0]['verdict'], 'not-run')

    def test_a_retry_of_an_unrelated_test_never_clears_the_fail(self):
        self.attempt(False, {'b': {'b': {'device/cdc_msc': '❌ boom', 'device/dfu_runtime': '✅'}}})
        self.attempt(True, {'b': {'b': {'device/dfu_runtime': '✅'}}})
        self.assertEqual(self.verdicts(['b'])['b'], ('fail', False), 'cdc_msc was never retried')
        current = hil_report.sequence(self.cfg, ['b'], self.rd)['current']
        self.assertEqual(current['rows'][0]['verdict'], 'pass', 'current stays graded from its own cells')

    def test_a_retry_of_only_the_passing_variant_never_clears_the_fail(self):
        self.attempt(False, {'a': {'a-fs': {'device/cdc_msc': '✅'}, 'a-hs': {'device/cdc_msc': '❌ boom'}}})
        self.attempt(True, {'a': {'a-fs': {'device/cdc_msc': '✅'}}})
        self.assertEqual(self.verdicts(['a'])['a'], ('fail', False), 'a-hs was never retried')

    def test_a_retry_of_only_the_failed_cells_clears_the_fail(self):
        for name, first, retry in (
                ('test', self.FAIL_A, {'a-fs': {'device/cdc_msc': '✅'}}),
                ('boundary', {'a-fs': {'device/cdc_msc': '✅'}, 'a-hs': {hil_report.BOUNDARY_CELL: 'fail'}},
                 {'a-hs': {'device/cdc_msc': '✅'}})):
            with self.subTest(name):
                self.setUp()
                self.attempt(False, {'a': first})
                self.attempt(True, {'a': retry})
                self.assertEqual(self.verdicts(['a'])['a'], ('pass', True))

    def test_a_row_without_a_test_cell_never_passes_the_sequence(self):
        self.attempt(False, {'b': {'b': {}}})
        self.assertEqual(self.verdicts(['b'])['b'], ('not-run', False))
        doc = json.loads((self.rd / hil_report.REPORT_JSON).read_text())
        self.assertTrue(hil_report.summarize(json.loads(self.cfg.read_text()), ['b'], doc)['pass'],
                        'the snapshot verdict keeps its old reading')

    def test_the_cli_prints_the_sequence_and_the_snapshot_is_unchanged(self):
        self.attempt(False, {'a': self.FAIL_A})
        self.attempt(True, {'a': self.PASS_A})
        r = _cli('sequence', str(self.cfg), '-b', 'a', '--report-dir', str(self.rd))
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(json.loads(r.stdout)['sequence']['verdict'], 'pass')
        snap = _cli(str(self.cfg), '-b', 'a', '--report-dir', str(self.rd))
        self.assertEqual(sorted(json.loads(snap.stdout)), ['caveat', 'pass', 'results'])



class TheCiLegsUploadAPreparedReport(unittest.TestCase):
    """build.yml's HIL legs: a leg the PR selection skips writes its report with
    hil_report.py, after lib/agentrc is fetched, and every leg prepares the pair before the
    upload takes the JSON and the attempt records with it. The skip branch and the prepare
    step run here as the workflow has them, against a sandbox checkout."""

    WORKFLOW = REPO / '.github/workflows/build.yml'

    def legs(self):
        try:
            import yaml
        except ImportError:
            self.skipTest('PyYAML is not installed')
        jobs = yaml.safe_load(self.WORKFLOW.read_text(encoding='utf-8'))['jobs']
        legs = {n: j for n, j in jobs.items() if any(st.get('name') == 'Upload HIL report' for st in j.get('steps', []))}
        self.assertEqual(sorted(legs), ['hil-hfp-iar', 'hil-tinyusb', 'hil-tinyusb-esp'])
        return legs

    @staticmethod
    def script(step):
        import re
        return re.sub(r'\$\{\{[^}]*\}\}', '', step['run'])

    def sandbox(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        root = Path(td.name)
        for rel in ('test/hil/helper/hil_report.py', 'test/hil/helper/agentrc.py', 'tools/get_deps.py',
                    'lib/agentrc/.git/HEAD', 'lib/agentrc/skills/hil/scripts/hil_report_core.py',
                    'test/hil/tinyusb.json', 'test/hil/hfp.json'):
            (root / rel).parent.mkdir(parents=True, exist_ok=True)
            (root / rel).write_bytes((REPO / rel).read_bytes())
        # get_deps.py would fetch: the sandbox holds lib/agentrc at the pin already
        shim = root / 'bin' / 'python3'
        shim.parent.mkdir()
        shim.write_text(f'#!/bin/sh\n[ "$1" = tools/get_deps.py ] && exit 0\nexec {sys.executable} "$@"\n')
        shim.chmod(0o755)
        return root

    def run_step(self, root, script, env):
        r = subprocess.run(['bash', '-e', '-c', script], cwd=root, capture_output=True, text=True, timeout=120,
                           env={**os.environ, 'PATH': f'{root / "bin"}:{os.environ["PATH"]}',
                                'GITHUB_ENV': str(root / 'github_env'), **env})
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    def test_each_leg_prepares_the_pair_before_its_upload(self):
        for name, job in self.legs().items():
            with self.subTest(name):
                names = [st.get('name') for st in job['steps']]
                upload = job['steps'][names.index('Upload HIL report')]
                prepare = job['steps'][names.index('Upload HIL report') - 1]
                self.assertEqual((prepare['name'], prepare['if']), ('Prepare HIL report', upload['if']))
                self.assertIn('hil_report.py prepare', prepare['run'])
                paths = upload['with']['path']
                for f in ('hil_report.md', 'hil_report.json', 'hil_report.attempts/'):
                    self.assertIn(f, paths)

    @unittest.skipIf(os.name == 'nt', 'the shim is a POSIX shell script')
    def test_a_skipped_leg_uploads_a_no_selection_report(self):
        for name, job in self.legs().items():
            with self.subTest(name):
                steps = job['steps']
                test = next(st for st in steps if 'hil_report.py no-selection' in st.get('run', ''))
                run = test['run']
                if 'tools/get_deps.py lib/agentrc' in run:
                    self.assertLess(run.index('tools/get_deps.py lib/agentrc'), run.index('if [ "$SEL_RUN" = "false" ]'),
                                    'the skip branch needs lib/agentrc')
                else:
                    deps = next(st for st in steps if st.get('name') == 'Get Dependencies')
                    self.assertNotIn('if', deps, 'a skipped leg needs lib/agentrc too')
                    self.assertLess(steps.index(deps), steps.index(test))
                root = self.sandbox()
                report_dir = root / 'report' if name != 'hil-hfp-iar' else root
                env = {'HIL_JSON': 'test/hil/tinyusb.json', 'HIL_REPORT_DIR': str(report_dir), 'SEL_RUN': 'false',
                       'SEL_RUN_TINYUSB': 'false', 'SEL_RUN_HFP': 'false', 'RERUN_ARGS': ''}
                self.run_step(root, self.script(test), env)
                prepare = next(st for st in steps if st.get('name') == 'Prepare HIL report')
                self.run_step(root, self.script(prepare), env)
                reading = hil_report.core().read_document(report_dir / hil_report.REPORT_JSON, hil_report.schema())
                self.assertEqual((reading.outcome, reading.meta['termination']), ('empty', 'no-boards'))
                md = (report_dir / hil_report.REPORT_MD).read_text(encoding='utf-8')
                self.assertTrue(md.startswith('_Skipped by PR selection:'), md)


if __name__ == '__main__':
    unittest.main()
