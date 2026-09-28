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
from tempfile import TemporaryDirectory

TEST_DIR = os.path.dirname(os.path.abspath(__file__))
HIL_DIR = os.path.dirname(TEST_DIR)
sys.path.insert(0, HIL_DIR)

from helper import hil_report


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
    (the HIL contract, .claude/skills/hil/SKILL.md). A script run puts helper/ on sys.path, NOT test/hil,
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
        with self.assertRaises(OSError):
            hil_report.write_report(Path('/proc/nonexistent/nope'),
                                    {'rows': [], 'scope': '', 'caveat': 'x\n'})

    def test_the_guarded_callers_still_do_not_raise(self):
        """They run on an abort path, where a raise would replace the abort's own error."""
        bad = Path('/proc/nonexistent/nope')
        hil_report.mark_report_no_boards(bad, 'filters intersected to nothing')
        import io
        from contextlib import redirect_stdout
        with redirect_stdout(io.StringIO()):
            hil_report.write_timeout_report(bad, [{'name': 'b1'}], 3600)


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
        hil_report.write_timeout_report(rd, [{'name': 'stuck'}], 3600)
        self._check(rd)

class WriteTimeoutReport(unittest.TestCase):
    def test_writes_a_report_where_there_would_be_none(self):
        with TemporaryDirectory() as td:
            hil_report.write_timeout_report(Path(td), [{'name': 'ra6m5_ek'}], 4200)
            md = (Path(td) / hil_report.REPORT_MD).read_text()
        self.assertIn('4200s', md)
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
        hil_report.write_timeout_report(rd, [{'name': 'stuck'}], 3600)
        doc = json.loads((rd / hil_report.REPORT_JSON).read_text())
        self.assertEqual([r['board'] for r in doc['rows']], ['done', 'stuck'])
        md = (rd / hil_report.REPORT_MD).read_text()
        self.assertIn('done', md)
        self.assertIn('stuck', md)
        self.assertIn('aborted', md)
        self.assertLess(md.index('aborted'), md.index('done'))
        self.assertEqual(md.count('| Board'), 1, 'the prior table was duplicated, not merged')

    def test_custom_banner_is_used(self):
        with TemporaryDirectory() as td:
            hil_report.write_timeout_report(Path(td), [], 0,
                                            banner='**refused to start.**\n')
            self.assertIn('refused to start', (Path(td) / hil_report.REPORT_MD).read_text())

    def test_timeout_report_writes_the_sidecar(self):
        """This path used to write markdown only, so summarize() -- which is all an
        agent gets -- reported the whole fleet as 'no report row' on exactly the runs
        that failed."""
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_report.write_timeout_report(rd, [{'name': 'boardA'}], 3600)
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
        hil_report.write_timeout_report(rd, [{'name': 'stuck'}], 3600)
        rows = json.loads((rd / hil_report.REPORT_JSON).read_text())['rows']
        self.assertEqual([r['board'] for r in rows], ['done', 'stuck'])

    def test_a_torn_sidecar_does_not_lose_the_stuck_boards(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        (rd / hil_report.REPORT_JSON).write_text('{ truncated mid-')
        hil_report.write_timeout_report(rd, [{'name': 'stuck'}], 3600)
        rows = json.loads((rd / hil_report.REPORT_JSON).read_text())['rows']
        self.assertEqual([r['board'] for r in rows], ['stuck'])

    def test_a_roster_entry_without_a_name_does_not_escape(self):
        """The broad handler exists to stop a KeyError here stranding the runner, but a
        report that silently loses its only board is worse than one saying '?'."""
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_report.write_timeout_report(rd, [{}], 3600)
        rows = json.loads((rd / hil_report.REPORT_JSON).read_text())['rows']
        self.assertEqual([r['board'] for r in rows], ['?'])

    def test_unwritable_dir_does_not_raise(self):
        """The caller may be about to os._exit; losing the report must not also lose the
        exit path."""
        hil_report.write_timeout_report(Path('/proc/nonexistent/nope'), [], 0)


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

    def test_skipped_cells_do_not_fail_a_board(self):
        got = self._sum(['a'], [('a', {'usbtest': 'skip', 'cdc_msc': 'pass'})])
        self.assertTrue(got[0]['pass'])

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
        return json.loads((d / 'hil_report.json').read_text()) if hil_report.accumulate_report(
            mret, d, fresh) is not None else None

    def test_an_accumulated_rerun_that_ran_clears_last_attempts_wedge(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        d = Path(td.name)
        cfg = {'boards': [{'name': 'b', 'variant': [{'name': 'b-fs'}, {'name': 'b-hs'}]}]}
        wedge = [('b', 1, [], [('b-fs', {'usbtest': '❌ 3/30', hil_report.WEDGED_CELL: 'fail'}, '9s'),
                               ('b-hs', {'cdc_msc': '⚪ board wedged', hil_report.WEDGED_CELL: 'fail'}, '1s')], 10.0)]
        doc = self._acc(d, wedge, True)
        self.assertTrue(hil_report.summarize(cfg, ['b'], doc)['results'][0]['wedged'])
        # re-run green: no wedge cell survives on any row
        clean = [('b', 0, [], [('b-fs', {'usbtest': 'pass'}, '8s'), ('b-hs', {'cdc_msc': 'pass'}, '2s')], 10.0)]
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
        unbuilt = [('b', 1, [], [('b-fs', {hil_report.RUN_ABORTED_CELL: hil_report.BUILD_REFUSED}, None)], 0.0)]
        doc = self._acc(d, unbuilt, False)
        cells = {r['board']: r['cells'] for r in doc['rows']}
        self.assertEqual(cells['b'], {hil_report.LOCKED_CELL: 'fail'})
        self.assertEqual(cells['b-fs'], {'usbtest': '❌ 3/30', hil_report.WEDGED_CELL: 'fail',
                                         hil_report.RUN_ABORTED_CELL: hil_report.BUILD_REFUSED})
        r = hil_report.summarize(cfg, ['b'], doc)['results'][0]
        self.assertEqual((r['ran'], r['pass'], r['locked'], r['wedged']), (False, False, False, True))
        self.assertIn('build refused', r['detail'])
        clean = [('b', 0, [], [('b-fs', {'usbtest': 'pass'}, '8s')], 10.0)]
        doc = self._acc(d, clean, False)
        self.assertEqual([(r['board'], r['cells']) for r in doc['rows']], [('b-fs', {'usbtest': 'pass'})])
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
                                            [{'name': 'b1'}], 3600)
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
        hil_report.write_timeout_report(rd, [{'name': 'stm32f4'}], 3600)
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
        hil_report.write_timeout_report(rd, [{'name': 'stuck'}], 3600)
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
        hil_report.write_timeout_report(rd, [{'name': 'stuck'}], 3600)
        md = (rd / hil_report.REPORT_MD).read_text()
        self.assertIn('| a | OK |', md, "an earlier attempt's real results were destroyed")
        self.assertIn('stuck', md)


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

    ABANDON = '**HIL run aborted: worker pool timed out after 1s.** treat board results as unverified.\n'

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
        v = self._verdict(['a'], [('a', {'usbtest': 'pass'})], caveat=self.ABANDON)
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
        doc['caveat'] = self.ABANDON
        hil_report.write_report(rd, doc)
        self.assertFalse(hil_report.summarize(cfg, ['boardA'], hil_report._load(rd)[0])['pass'])
        # a clean accumulated re-run clears the caveat, and THIS snapshot passes: the sequence
        # verdict is the caller's, who still holds the first attempt's False
        hil_report.accumulate_report(rows, rd, False, '', '')
        self.assertTrue(hil_report.summarize(cfg, ['boardA'], hil_report._load(rd)[0])['pass'])
        # the other direction: a clean first attempt, then a re-run that aborted
        doc, _ = hil_report._load(rd)
        doc['caveat'] = self.ABANDON
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
        hil_report.accumulate_report(
            [('b', 0, 0, [('b-v', {'cdc_msc': 'OK'}, '2s')], 0)], rd, False, '', '')
        cells = json.loads((rd / hil_report.REPORT_JSON).read_text())['rows'][0]['cells']
        self.assertNotIn(hil_report.BOUNDARY_CELL, cells)

    def test_a_board_that_really_ran_drops_its_stale_lock_cell(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_report.accumulate_report(
            [('b', 0, 0, [('b', {hil_report.LOCKED_CELL: 'fail'}, None)], 0)], rd, True, '', '')
        hil_report.accumulate_report(
            [('b', 0, 0, [('b', {'cdc_msc': 'OK'}, '2s')], 0)], rd, False, '', '')
        rows = json.loads((rd / hil_report.REPORT_JSON).read_text())['rows']
        self.assertEqual([r['board'] for r in rows], ['b'])
        self.assertNotIn(hil_report.LOCKED_CELL, rows[0]['cells'])

    def test_a_filtered_rerun_keeps_the_previous_duration(self):
        """A -t-filtered re-run reports duration None; blanking the column loses the only
        record of how long the full run took."""
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_report.accumulate_report(
            [('b', 0, 0, [('b', {'cdc_msc': 'OK'}, '119s')], 0)], rd, True, '', '')
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
        hil_report.write_timeout_report(rd, [], 3600)
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
        hil_report.accumulate_report([('b', 0, 0, [('b', {'t': 'OK'}, '1s')], 0)],
                                     rd, False, '', '')
        self.assertTrue((rd / hil_report.REPORT_MD).is_file())

    def test_a_null_cells_row_still_gets_its_pool_timeout_cell(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        self._write(rd, {'rows': [{'board': 'boardA', 'cells': None, 'duration': '61s'}],
                         'caveat': '', 'scope': ''})
        hil_report.write_timeout_report(rd, [{'name': 'boardA'}], 3600)
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
            hil_report.write_timeout_report(rd, ['plainstring'], 3600)
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
        hil_report.mark_report_no_boards(rd, 'filters emptied', fresh=False)
        self.assertEqual([r['board'] for r in json.loads(
            (rd / hil_report.REPORT_JSON).read_text())['rows']], ['a'])

    def test_it_does_not_overwrite_an_abort_notice(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_report.accumulate_report([('a', 0, 0, [('a', {'t': 'OK'}, '1s')], 0)],
                                     rd, True, '', '')
        hil_report.write_timeout_report(rd, [], 3600)
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
        hil_report.write_timeout_report(rd, [{'name': 'stuck'}], 3600)
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
        hil_report.write_timeout_report(rd, [], 3600)
        hil_report.mark_report_no_boards(rd, 'filters emptied', fresh=True)
        doc = json.loads((rd / hil_report.REPORT_JSON).read_text())
        self.assertEqual(doc['rows'], [])
        self.assertIn('selected no boards', doc['caveat'])


if __name__ == '__main__':
    unittest.main()
