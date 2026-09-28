#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Unit tests proving hil_test's storage and MTP helpers cannot hang the worker: a
# wedged device blocks the call in D state forever (child process or in-process ioctl),
# so these paths go through a bounded runner. Fakes stand in for the wedge (a real one
# cannot be manufactured on demand): a PATH-injected `mtype` script and a
# PYTHONPATH-injected `pymtp` module, each with a mode that blocks forever.
# Scope: mtype, the gio unmount, the libmtp session, the arecord/iperf reaps, and the
# printer read (a process now, via run_alongside, so a killed reader takes its fd with
# it -- usblp allows ONE opener, and a blocked thread kept the node for the worker's life).
# Known residue (unbounded, backstopped only by the pool guard): hid open/write and
# midi's read(64).
#
# hil_test imports pyserial, which GitHub's bare pre-commit runner does not have — so
# an inert serial module is stubbed into sys.modules BEFORE the import (nothing here
# exercises serial paths). MTP traffic never touches hil_test: it all goes through the
# mtp_test.py subprocess, which gets the fake pymtp via PYTHONPATH.
# Run directly:
#   python3 test/hil/test/test_hil_bounded.py
import io
import json
import os
import stat
import subprocess
import sys
import threading
from contextlib import redirect_stderr, redirect_stdout
from multiprocessing import TimeoutError as MpTimeoutError
import time
import types
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

TEST_DIR = os.path.dirname(os.path.abspath(__file__))
# the modules under test live in the parent dir (test/hil), not here
sys.path.insert(0, os.path.dirname(TEST_DIR))

import usbtest_harness
import hil_flash
import hil_test


def write_script(path: Path, body: str) -> None:
    path.write_text('#!/bin/sh\n' + body + '\n')
    path.chmod(path.stat().st_mode | stat.S_IEXEC)


def no_settle(case):
    """Zero test_device_usbtest's post-flash settle for one test.

    Real hardware needs it -- the enumeration can bounce once after a flash, and on
    dual-port parts the stale same-serial node lingers. A fake rig has neither, and ten
    tests drive that path, so leaving it real cost 30s of every suite run.
    """
    case.addCleanup(setattr, hil_test, 'USBTEST_SETTLE', hil_test.USBTEST_SETTLE)
    hil_test.USBTEST_SETTLE = 0


def run_bounded(fn, timeout: float):
    """Run fn in a daemon thread; return (finished, exception). A still-running thread is
    the hang under test — leave it to die with the interpreter."""
    exc = []

    def wrapper():
        try:
            fn()
        except BaseException as e:  # noqa: BLE001 - tests inspect the exception
            exc.append(e)

    t = threading.Thread(target=wrapper, daemon=True)
    t.start()
    t.join(timeout)
    return not t.is_alive(), exc[0] if exc else None


class ReadDiskFile(unittest.TestCase):
    def setUp(self):
        self.tmp = TemporaryDirectory()
        tmp = Path(self.tmp.name)
        self.addCleanup(self.tmp.cleanup)
        # fake block device node: get_disk_dev is patched to this existing path
        self.dev = tmp / 'fakedev'
        self.dev.write_bytes(b'')
        # addCleanup, not tearDown: tearDown does NOT run when setUp raises, and a leaked
        # PATH entry points at a temp bin dir this class already deleted.
        for name in ('get_disk_dev', '_enum_timeout', 'MTYPE_TIMEOUT'):
            self.addCleanup(setattr, hil_test, name, getattr(hil_test, name))
        hil_test.get_disk_dev = lambda uid, vendor, lun: str(self.dev)
        hil_test._enum_timeout = 1   # the wait these tests must outlast; keep it small
        self.bin = tmp / 'bin'
        self.bin.mkdir()
        self.addCleanup(os.environ.__setitem__, 'PATH', os.environ['PATH'])
        os.environ['PATH'] = f'{self.bin}:{os.environ["PATH"]}'
        self.pidfile = tmp / 'mtype.pid'
        self.addCleanup(self._reap_mtype)

    def _reap_mtype(self):
        if self.pidfile.exists():  # reap a leaked hang-mode mtype
            try:
                os.kill(int(self.pidfile.read_text()), 9)
            except (OSError, ValueError):
                pass

    def test_returns_exact_bytes_despite_stderr_noise(self):
        # \377 is invalid UTF-8 and stderr noise must not leak into the data
        write_script(self.bin / 'mtype', r"printf 'R\377EADME-DATA'; printf 'vfat warning' >&2")
        data = hil_test.read_disk_file('uid0', 0, 'README.TXT')
        self.assertEqual(data, b'R\xffEADME-DATA')

    def test_failure_message_carries_mtype_stderr_and_fname(self):
        write_script(self.bin / 'mtype', "printf 'mtype: cannot read' >&2; exit 1")
        with self.assertRaises(AssertionError) as cm:
            hil_test.read_disk_file('uid0', 0, 'README.TXT')
        self.assertIn('cannot read', str(cm.exception))
        self.assertIn('README.TXT', str(cm.exception))

    def test_empty_read_fails_immediately_with_fname(self):
        # rc 0 with no data is a real answer (bad sectors, empty file), not "not ready":
        # fail at once like the old assert did, naming the file — don't spin the budget
        write_script(self.bin / 'mtype', 'exit 0')
        t0 = time.monotonic()
        with self.assertRaises(AssertionError) as cm:
            hil_test.read_disk_file('uid0', 0, 'README.TXT')
        # BELOW one full _enum_timeout wait, not above it: "fails immediately" is the
        # claim, and a bound of 1.5 against a 1s budget passes for code that spun the
        # whole budget -- which is the regression this test exists to catch.
        self.assertLess(time.monotonic() - t0, hil_test._enum_timeout,
                        'read_disk_file spun the enumeration budget on a real answer')
        self.assertIn('README.TXT', str(cm.exception))

    def test_hung_mtype_cannot_hang_the_worker(self):
        # a D-state child never exits; the bounded runner must give up without it
        write_script(self.bin / 'mtype', f'echo $$ > {self.pidfile}; exec sleep 1000')
        hil_test.MTYPE_TIMEOUT = 2
        finished, exc = run_bounded(lambda: hil_test.read_disk_file('uid0', 0, 'README.TXT'), 20)
        self.assertTrue(finished, 'read_disk_file hung on a stuck mtype')
        self.assertIsInstance(exc, AssertionError)


class CompactOutput(unittest.TestCase):
    def test_strips_workflow_command_markers(self):
        """Defense-in-depth: the historical marker source was worker-side run_cmd
        (now suppressed at the emitter); anything future that pipes markers into a
        captured stdout would land them mid-row where GitHub renders them literally."""
        raw = '::group::COMMAND TIMEOUT (1s): x\nboom\n::endgroup::\ntail'
        self.assertEqual(hil_test.compact_output(raw), 'COMMAND TIMEOUT (1s): x | boom | tail')


class UsbtestRecovery(unittest.TestCase):
    def test_recovery_flags_and_flash_bound_fit_the_reserve(self):
        """The post-hang reflash plumbing: the CLI flags exist, and the bounded reflash
        and the reserve that pays for them is derived per flasher (see the two tests
        below), not pinned."""
        import subprocess
        hil_dir = Path(TEST_DIR).parents[0]
        r = subprocess.run([sys.executable, str(hil_dir / 'usbtest.py'), '--help'],
                           capture_output=True, text=True, timeout=30)
        self.assertEqual(r.returncode, 0, r.stderr)
        for flag in ('--recover-board', '--recover-fw'):
            self.assertIn(flag, r.stdout)

    def test_the_reserve_covers_the_window_one_step_and_the_checks_after_it(self):
        """Enumerated from the SIDE EFFECTS usbtest performs, so dropping a step from
        recovery_reserve() fails here. Overrun means run_cmd's outer kill lands MID-STEP and
        orphans the flasher (start_new_session, so killpg misses it) on the probe."""
        import usbtest
        from helper import hil_util as _hu
        fixed = (usbtest.WEDGE_CONFIRM_S + _hu.REAP_GRACE + usbtest.RECOVER_SETTLE
                 + usbtest.RECOVER_REAP + usbtest.RECOVER_OVERHEAD)
        # a flasher with a reset primitive resets, nothing else; one without reflashes
        self.assertEqual(usbtest.recovery_reserve({'name': 'openocd', 'args': '-f target/rp2040.cfg'}),
                         usbtest.RECOVER_RESET_TIMEOUT + fixed)
        self.assertEqual(usbtest.recovery_reserve({'name': 'esptool', 'args': ''}),
                         usbtest.RECOVER_FLASH_TIMEOUT + fixed)

    def test_the_reserve_leaves_room_for_the_work_no_step_bounds(self):
        """The step timeout does not cover the roster json.loads, the child's first import,
        or the JSON print. With zero margin any env-overridable bound moving up puts the
        outer killpg inside the step."""
        import usbtest
        self.assertGreater(usbtest.RECOVER_OVERHEAD, 0)
        self.assertGreater(usbtest.WEDGE_CONFIRM_S, 5, 'must outlast the 5 s reap that produced HUNG')


class UsbtestRunHelper(unittest.TestCase):
    """usbtest.run() is the bounded replacement for subprocess.run: sysfs_write feeds it
    input=, and every battery calls that before case 1."""

    def setUp(self):
        import usbtest
        self.usbtest = usbtest

    def test_input_kwarg_is_honoured(self):
        r = self.usbtest.run(['cat'], input='payload', timeout=10)
        self.assertEqual(r.returncode, 0)
        self.assertEqual(r.stdout, 'payload')

    def test_capture_output_kwarg_is_accepted(self):
        r = self.usbtest.run(['printf', 'x'], capture_output=True, timeout=10)
        self.assertEqual(r.stdout, 'x')

    def test_timeout_is_bounded_and_raises(self):
        import subprocess
        t0 = time.monotonic()
        with self.assertRaises(subprocess.TimeoutExpired):
            self.usbtest.run(['sleep', '30'], timeout=1)
        self.assertLess(time.monotonic() - t0, 15)


class BuildBoardContract(unittest.TestCase):
    def build(self, rc, out):
        from unittest import mock
        proc = mock.Mock(returncode=rc, pid=1)
        proc.communicate.return_value = (out, None)
        with mock.patch.object(hil_test.subprocess, 'Popen', return_value=proc) as popen, \
             redirect_stdout(io.StringIO()) as printed:
            r = hil_test.build_board({'name': 'b'}, Path('rig.json'))
        return r, popen.call_args[0][0], printed.getvalue()

    def test_it_builds_every_variant_through_the_build_contract(self):
        ok = {'pass': True, 'boards': [{'buildDir': 'cmake-build/cmake-build-b', 'status': 'ok'},
                                       {'buildDir': 'cmake-build/cmake-build-b-DMA', 'status': 'ok'}]}
        r, cmd, _ = self.build(0, json.dumps(ok) + '\n')
        self.assertEqual(r, (0, True))
        self.assertEqual(cmd[1:], [str(hil_test.CHECK_BUILD), '--board', 'b', '--shared', '--variants', 'rig.json', '-v'])

    def test_failed_variants_and_refusals_count_as_failures(self):
        bad = {'pass': False, 'boards': [{'buildDir': 'd1', 'status': 'failed', 'firstError': 'x.c:1: error: y'},
                                         {'buildDir': 'd2', 'status': 'error', 'firstError': 'z'},
                                         {'buildDir': 'd3', 'status': 'skipped', 'firstError': 'nothing built'}]}
        r, _, printed = self.build(1, json.dumps(bad))
        self.assertEqual(r, (3, True))
        self.assertIn('d1 failed: x.c:1: error: y', printed)
        r, _, printed = self.build(2, json.dumps({'pass': False, 'boards': [], 'error': 'was configured with CFLAGS_CLI'}))
        self.assertEqual(r, (1, False))
        self.assertIn('was configured with CFLAGS_CLI', printed)
        self.assertEqual(self.build(1, 'Traceback')[0], (1, False))

    def run_main(self, cfg, prior_rows, argv, refuse, results=()):
        """main() over a fake rig in a temp report dir: (exit code, report dir, pool, hints)."""
        from unittest import mock
        from helper import hil_report
        for g in ('verbose', 'test_only', 'max_retry', 'skip_flash'):
            self.addCleanup(setattr, hil_test, g, getattr(hil_test, g))
        self.addCleanup(setattr, hil_test.hil_util, 'verbose', hil_test.hil_util.verbose)
        self.addCleanup(setattr, hil_flash, 'build_dir', hil_flash.build_dir)
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        d = Path(td.name)
        (d / 'rig.json').write_text(json.dumps(cfg))
        hil_report.write_report(d, {'rows': prior_rows, 'scope': '', 'caveat': ''})
        (d / 'rig.json.failed').write_text('--accumulate -b stale')
        pool = mock.MagicMock()   # main() enters it as a context manager
        pool.imap_unordered.return_value.next.side_effect = list(results)
        with mock.patch.object(sys, 'argv', ['hil_test.py', str(d / 'rig.json'), '--build', *argv]), \
             mock.patch.dict(os.environ, {'HIL_REPORT_DIR': str(d)}), \
             mock.patch.object(hil_test, 'build_board', lambda b, c: (1, False) if b['name'] in refuse else (0, True)), \
             mock.patch.object(hil_test, 'Manager', mock.Mock()), \
             mock.patch.object(hil_test, '_start_pool', return_value=({}, pool)), \
             mock.patch.object(hil_test, '_load_controller_hints', return_value=({}, {})), \
             mock.patch.object(hil_test, '_save_controller_hints') as hints, \
             mock.patch.object(hil_test, 'log_line'), \
             redirect_stdout(io.StringIO()), self.assertRaises(SystemExit) as exited:
            hil_test.main()
        return exited.exception.code, d, pool, hints

    def test_a_refused_board_is_reported_failed_over_its_accumulated_pass(self):
        from helper import hil_report
        cfg = {'boards': [{'name': 'bad', 'uid': '1', 'flasher': {'name': 'jlink'},
                           'variant': [{'name': 'bad-a'}, {'name': 'bad-b'}]},
                          {'name': 'good', 'uid': '2', 'flasher': {'name': 'jlink'}}]}
        prior = [{'board': n, 'cells': {'device/cdc_msc': 'pass'}, 'duration': '1s'}
                 for n in ('bad-a', 'bad-b', 'good')]
        good_row = ('good', 0, [], [('good', {'device/cdc_msc': 'pass'}, '1s')], 1.0)
        rc, d, pool, hints = self.run_main(cfg, prior, ['--accumulate'], {'bad'}, [good_row])
        self.assertEqual(rc, 1)
        self.assertEqual([b['name'] for b in pool.imap_unordered.call_args[0][1]], ['good'])
        self.assertEqual([r[0] for r in hints.call_args[0][1]], ['good'], 'a refused board never ran')
        doc = json.loads((d / hil_report.REPORT_JSON).read_text())
        cells = {r['board']: r['cells'] for r in doc['rows']}
        for n in ('bad-a', 'bad-b'):
            self.assertEqual(cells[n][hil_report.RUN_ABORTED_CELL], hil_report.BUILD_REFUSED)
        verdict = hil_report.summarize(cfg, ['bad', 'good'], doc)
        self.assertFalse(verdict['pass'])
        self.assertEqual([(r['ran'], r['pass']) for r in verdict['results']], [(False, False), (True, True)])
        self.assertEqual((d / 'rig.json.failed').read_text(), '--accumulate -b bad')

    def all_refused(self, argv):
        from helper import hil_report
        cfg = {'boards': [{'name': 'bad', 'uid': '1', 'flasher': {'name': 'jlink'},
                           'variant': [{'name': 'bad-a'}, {'name': 'bad-b'}]},
                          {'name': 'worse', 'uid': '2', 'flasher': {'name': 'jlink'}}]}
        prior = ([{'board': n, 'cells': {'device/cdc_msc': 'pass'}, 'duration': '1s'}
                  for n in ('bad-a', 'bad-b', 'worse')]
                 + [{'board': 'bad', 'cells': {hil_report.LOCKED_CELL: 'held'}, 'duration': None}])
        rc, d, pool, _ = self.run_main(cfg, prior, argv, {'bad', 'worse'})
        self.assertNotIn(rc, (0, None))
        pool.imap_unordered.assert_not_called()
        doc = json.loads((d / hil_report.REPORT_JSON).read_text())
        cells = {r['board']: r['cells'] for r in doc['rows']}
        for n in ('bad-a', 'bad-b', 'worse'):
            self.assertEqual(cells[n][hil_report.RUN_ABORTED_CELL], hil_report.BUILD_REFUSED)
        self.assertIn('**HIL run selected no boards.**', doc['caveat'])
        verdict = hil_report.summarize(cfg, ['bad', 'worse'], doc)
        self.assertFalse(verdict['pass'])
        self.assertEqual([(r['ran'], r['pass']) for r in verdict['results']], [(False, False)] * 2)
        self.assertEqual((d / 'rig.json.failed').read_text(), '--accumulate -b bad -b worse')
        return cells

    def test_an_all_refused_accumulate_run_reports_every_board_failed(self):
        from helper import hil_report
        cells = self.all_refused(['--accumulate'])
        self.assertEqual(cells['bad'], {hil_report.LOCKED_CELL: 'held'}, 'an earlier lock cell is kept')
        self.assertEqual(cells['bad-a']['device/cdc_msc'], 'pass', 'test history is kept on --accumulate')

    def test_an_all_refused_fresh_run_reports_only_its_refusals(self):
        from helper import hil_report
        cells = self.all_refused([])
        self.assertEqual(sorted(cells), ['bad-a', 'bad-b', 'worse'])
        for c in cells.values():
            self.assertEqual(c, {hil_report.RUN_ABORTED_CELL: hil_report.BUILD_REFUSED})

    def test_an_unselected_malformed_variant_does_not_crash_the_report(self):
        from helper import hil_report
        cfg = {'boards': [{'name': 'good', 'uid': '1', 'flasher': {'name': 'jlink'}},
                          {'name': 'owner', 'uid': '3', 'flasher': {'name': 'jlink'},
                           'variant': [None, {'name': 'owner-a'}]},
                          {'name': 'unselected', 'uid': '2', 'flasher': {'name': 'jlink'}, 'variant': [None]}]}
        good_row = ('good', 0, [], [('good', {'device/cdc_msc': 'pass'}, '1s')], 1.0)
        for refuse, results, cell in (({'good'}, [], {hil_report.RUN_ABORTED_CELL: hil_report.BUILD_REFUSED}),
                                      (set(), [good_row], {'device/cdc_msc': 'pass'})):
            rc, d, _, _ = self.run_main(cfg, [], ['-b', 'good'], refuse, results)
            doc = json.loads((d / hil_report.REPORT_JSON).read_text())
            self.assertEqual({r['board']: r['cells'] for r in doc['rows']}, {'good': cell})
            self.assertEqual(rc, 1 if refuse else 0)


    def test_an_abort_banner_does_not_count_a_refused_board_as_finished(self):
        from helper import hil_report
        refused = ('bad', 1, [], [('bad', {hil_report.RUN_ABORTED_CELL: hil_report.BUILD_REFUSED}, None)], 0.0)
        good = ('good', 0, [], [('good', {'device/cdc_msc': 'pass'}, '1s')], 1.0)
        with TemporaryDirectory() as td:
            rd = Path(td)
            hil_test._abort_report('aborted: a worker raised ValueError: x', [refused, good],
                                   [{'name': 'good'}, {'name': 'stuck'}], rd / 'rig.json.failed',
                                   rd, True, '')
            caveat = json.loads((rd / hil_report.REPORT_JSON).read_text())['caveat']
            spec = (rd / 'rig.json.failed').read_text()
        self.assertTrue(caveat.startswith('**HIL run aborted: a worker raised ValueError: x.** '
                                          '1 board(s) below finished'), caveat)
        self.assertIn('1 never reported and are NOT in the table: stuck.', caveat)
        self.assertIn("check_build.py refused: bad", caveat)
        self.assertEqual(spec, '--accumulate -b stuck -b bad')

class RemoteStaging(unittest.TestCase):
    def test_import_closure_is_staged_to_the_rig(self):
        # hil_remote.py stages an explicit whitelist; a module that is not on it exists
        # locally and in CI checkouts but silently never reaches the remote rig (how
        # mtp_test.py was first missed). Walk the local-import closure of everything
        # the rig executes and require each file in HARNESS_FILES, read by ast so this
        # suite needs nothing the wrapper imports.
        import ast
        hil_dir = Path(TEST_DIR).parents[0]
        wrapper = hil_dir.parents[1] / '.claude/skills/hil/scripts/hil_remote.py'
        staged = next(ast.literal_eval(n.value) for n in ast.parse(wrapper.read_text()).body
                      if isinstance(n, ast.Assign) and ast.unparse(n.targets[0]) == 'HARNESS_FILES')

        def imported_paths(pyfile):
            # ast, not regex: an earlier regex walker went silently vacuous on a
            # multi-line import. ast also sees function-local deferred imports
            # (usbtest.py's `import hil_flash` inside the recovery branch).
            for node in ast.walk(ast.parse(pyfile.read_text())):
                if isinstance(node, ast.Import):
                    for a in node.names:
                        yield a.name.replace('.', '/') + '.py'
                elif isinstance(node, ast.ImportFrom) and node.module:
                    if node.module == 'helper':
                        for a in node.names:
                            yield f'helper/{a.name}.py'
                    else:
                        yield node.module.replace('.', '/') + '.py'

        seeds = ['hil_test.py', 'usbtest.py', 'mtp_test.py']  # CLI + spawned helpers
        for f in seeds:  # a renamed seed must fail loudly, not fall out of the walk
            self.assertTrue((hil_dir / f).exists(), f'stale RemoteStaging seed: {f}')
        todo, seen = list(seeds), set()
        while todo:
            f = todo.pop()
            if f in seen or not (hil_dir / f).exists():
                continue  # stdlib/site-packages imports have no test/hil file
            seen.add(f)
            todo += list(imported_paths(hil_dir / f))
        for f in sorted(seen):
            self.assertIn(f'test/hil/{f}', staged,
                          f'{f} runs on the rig but hil_remote.py does not stage it')


class _MtpFakeRig:
    """The fake rig shared by the MTP cases: a udev-marker tree under one tmp root and
    the scripted pymtp on PYTHONPATH. A plain mixin, NOT a TestCase -- subclassing a
    TestCase to reuse a fixture re-runs every inherited test in each subclass."""

    @classmethod
    def setUpClass(cls):
        # both file fixtures come from the example's sources, so drift there fails here:
        # file id 1 is README.TXT (C define), file id 2 is logo.png (C byte array)
        import hashlib
        import re
        src = Path(TEST_DIR).parents[2] / 'examples/device/mtp/src'
        m = re.search(r'#define README_TXT_CONTENT "([^"]+)"', (src / 'mtp_fs_example.c').read_text())
        assert m, 'README_TXT_CONTENT define not found in mtp_fs_example.c'
        cls.readme = m.group(1)
        data = bytes(int(x, 16) for x in
                     re.findall(r'0x([0-9a-fA-F]{2})', (src / 'tinyusb_logo_png.h').read_text()))
        assert hashlib.md5(data).hexdigest() == '40ef23fc2891018d41a05d4a0d5f822f'
        cls.logo = data

    def setUp(self):
        self.tmp = TemporaryDirectory()
        tmp = Path(self.tmp.name)
        self.addCleanup(self.tmp.cleanup)
        logo = tmp / 'logo.bin'
        logo.write_bytes(self.logo)
        self.board = {'uid': 'CAFE01', 'name': 'fakeboard'}
        # addCleanup, not tearDown: tearDown does NOT run when setUp raises, and a leaked
        # chdir into a deleted temp dir breaks every test after it.
        self.saved_env = {k: os.environ.get(k) for k in
                          ('FAKE_PYMTP_MODE', 'FAKE_PYMTP_UID', 'FAKE_PYMTP_LOGO',
                           'FAKE_PYMTP_FILE1', 'PYTHONPATH', 'PYTHONSAFEPATH',
                           'HIL_MTP_FAKE_ROOT', 'FAKE_PYMTP_ERRED_MARKER')}
        self.addCleanup(self._restore_env)
        # A udev-ready marker tree: libmtp-runtime publishes /dev/libmtp-<sysname> only
        # after mtp-probe accepts a device, and mtp_test opens THAT device directly rather
        # than probing every MTP device on the rig (the parallel-probe race #3790 fixed).
        # mirrors the real layout under one root, so <tmp>/sys/bus/usb/devices/1-1 reads
        # as the stand-in for /sys/bus/usb/devices/1-1 that it is
        dev = tmp / 'sys/bus/usb/devices/1-1'
        usbdev = tmp / 'dev/bus/usb/001'
        markers = tmp / 'dev'                  # created by usbdev's parents=True
        dev.mkdir(parents=True); usbdev.mkdir(parents=True)
        (dev / 'idVendor').write_text('cafe\n')
        (dev / 'idProduct').write_text('4017\n')
        (dev / 'serial').write_text(self.board['uid'] + '\n')
        (dev / 'busnum').write_text('1\n')
        (dev / 'devnum').write_text('2\n')
        node = usbdev / '002'
        node.write_bytes(b'')
        (markers / 'libmtp-1-1').symlink_to(node)
        os.environ['HIL_MTP_FAKE_ROOT'] = str(tmp)
        os.environ['FAKE_PYMTP_ERRED_MARKER'] = str(tmp / 'erred')
        os.environ['FAKE_PYMTP_UID'] = self.board['uid']
        os.environ['FAKE_PYMTP_LOGO'] = str(logo)
        os.environ['FAKE_PYMTP_FILE1'] = self.readme
        stubs = os.path.join(TEST_DIR, 'stubs')
        pp = self.saved_env['PYTHONPATH']
        os.environ['PYTHONPATH'] = stubs if not pp else f'{stubs}:{pp}'
        # pymtp is vendored next to mtp_test.py, and a script's own dir (sys.path[0])
        # outranks PYTHONPATH — safe-path mode (3.11+) drops it so the fake wins there
        os.environ['PYTHONSAFEPATH'] = '1'
        for name in ('_enum_timeout', 'MTP_SESSION_MARGIN'):
            self.addCleanup(setattr, hil_test, name, getattr(hil_test, name))
        hil_test._enum_timeout = 1   # the wait these tests must outlast; keep it small
        # the session scratch files land in cwd
        self.addCleanup(os.chdir, os.getcwd())
        os.chdir(tmp)

    def _restore_env(self):
        for k, v in self.saved_env.items():
            if v is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = v


@unittest.skipIf(sys.version_info < (3, 11), 'fake-pymtp steering needs PYTHONSAFEPATH')
class DeviceMtp(_MtpFakeRig, unittest.TestCase):
    """test_device_mtp end to end: the real mtp_test.py subprocess under run_cmd,
    with the scripted pymtp fake steered in via PYTHONPATH."""

    def test_mtp_session_passes_against_scripted_device(self):
        os.environ['FAKE_PYMTP_MODE'] = 'ok'
        hil_test.test_device_mtp(self.board)  # no exception

    def test_absent_device_fails_cleanly(self):
        os.environ['FAKE_PYMTP_MODE'] = 'absent'
        finished, exc = run_bounded(lambda: hil_test.test_device_mtp(self.board), 30)
        self.assertTrue(finished)
        self.assertIsInstance(exc, AssertionError)
        self.assertIn('MTP device not found', str(exc))

    def test_libmtp_error_on_one_poll_retries_instead_of_dying(self):
        """pymtp raises for USB_LAYER/PTP_LAYER errors -- routine on the first poll
        after a flash. An unguarded raise skipped the whole enumeration budget."""
        os.environ['FAKE_PYMTP_MODE'] = 'error_then_ok'
        hil_test.test_device_mtp(self.board)   # retries past the error, then passes

    def test_libmtp_error_every_poll_fails_cleanly(self):
        os.environ['FAKE_PYMTP_MODE'] = 'error'
        finished, exc = run_bounded(lambda: hil_test.test_device_mtp(self.board), 30)
        self.assertTrue(finished)
        self.assertIsInstance(exc, AssertionError)

    def test_hung_mtp_stack_cannot_hang_the_worker(self):
        # in-process libmtp blocking in a usbfs ioctl (D state) hangs whatever thread
        # made the call, forever — the session must be somewhere disposable
        os.environ['FAKE_PYMTP_MODE'] = 'hang'
        hil_test.MTP_SESSION_MARGIN = 3
        finished, exc = run_bounded(lambda: hil_test.test_device_mtp(self.board), 25)
        self.assertTrue(finished, 'test_device_mtp hung on a wedged MTP stack')
        self.assertIsInstance(exc, AssertionError)


class ConvoySafeFlasher(unittest.TestCase):
    """hil_flash.convoy_safe decides whether a board gets post-HUNG recovery at all.

    It must be true ONLY for flashers that can reach their probe without opening the
    poisoned usbfs node: openocd pinned with a roster vid_pid (filters on kernel-cached
    sysfs descriptors) and esptool (delivers to a named tty, never enumerates usbfs).
    Anything else enumerates by opening nodes, would block in D state on the wedged one
    and become a second stray -- JLinkExe included, whose selection is serial-only and
    so cannot be pinned at all."""

    def setUp(self):
        import hil_flash
        self.f = hil_flash.convoy_safe

    def test_pinned_openocd_is_safe(self):
        self.assertTrue(self.f({'name': 'openocd', 'vid_pid': '0x2e8a 0x000c'}))

    def test_unpinned_openocd_is_not(self):
        self.assertFalse(self.f({'name': 'openocd'}))
        self.assertFalse(self.f({'name': 'openocd', 'vid_pid': ''}))

    def test_esptool_is_safe_without_a_pin(self):
        """Delivery is `-p <ttyACM>`; there is no usbfs walk to poison."""
        self.assertTrue(self.f({'name': 'esptool'}))

    def test_enumerating_flashers_are_not(self):
        for name in ('jlink', 'stlink', 'lm4flash', 'dfu-util'):
            self.assertFalse(self.f({'name': name, 'vid_pid': '0x1366 0x1024'}),
                             f'{name} must not be treated as convoy-safe')

    def test_missing_or_odd_name_is_not_safe(self):
        for flasher in ({}, {'name': None}, {'name': ''}):
            self.assertFalse(self.f(flasher))


class UnresolvedControllerBucket(unittest.TestCase):
    """An unresolved controller must budget in ONE bucket. Taking a permit on every slot
    serialized the whole fleet the moment a single board could not be resolved."""

    def setUp(self):
        import threading
        from helper import hil_lock
        self.hil_lock = hil_lock
        self.saved = (hil_lock.controller_map, hil_lock.controller_meta,
                      hil_lock.controller_hints, hil_lock.log)
        hil_lock.controller_map, hil_lock.controller_meta = {}, threading.Lock()
        hil_lock.controller_hints, hil_lock.log = {}, lambda *a, **k: None

    def tearDown(self):
        (self.hil_lock.controller_map, self.hil_lock.controller_meta,
         self.hil_lock.controller_hints, self.hil_lock.log) = self.saved

    def _slots(self, uid, warn):
        import threading
        sems = self.hil_lock.make_permit_sems(threading.Semaphore, 2)
        return self.hil_lock.controller_permit(sems, uid, warn_unknown=warn).slots

    def test_unresolved_boards_share_one_slot(self):
        for warn in (False, True):
            slots = self._slots('NOSUCHUID', warn)
            self.assertEqual(len(slots), 1, 'unresolved uid took more than one slot')
            self.assertEqual(slots, self._slots('OTHERUID', warn),
                             'unresolved boards must share the bucket, not spread over it')

    def test_the_semaphore_array_is_long_enough_for_the_unknown_slot(self):
        """UNKNOWN_SLOT indexes one PAST the real slots. An array sized to
        CONTROLLER_SLOTS IndexErrors on the first unresolved board, inside a pool worker,
        which map_async turns into a total loss of every board's results."""
        import threading
        sems = self.hil_lock.make_permit_sems(threading.Semaphore, 2)
        self.assertGreater(len(sems), self.hil_lock.UNKNOWN_SLOT)

    def test_the_unknown_bucket_never_lends_a_controller_a_second_budget(self):
        """A private FULL budget let 2 unknown batteries join 2 resolved ones on the same
        physical controller -- 4 where the width is 2. One at a time caps that at +1."""
        import threading
        sems = self.hil_lock.make_permit_sems(threading.Semaphore, 2)
        first = self.hil_lock.controller_permit(sems, 'NOSUCHUID')
        first.__enter__()
        self.addCleanup(first.__exit__)
        second = self.hil_lock.controller_permit(sems, 'OTHERUID')
        self.assertFalse(sems[second.slots[0]].acquire(blocking=False),
                         'a second unresolved board got in alongside the first')

    def test_every_real_slot_keeps_the_full_width(self):
        import threading
        sems = self.hil_lock.make_permit_sems(threading.Semaphore, 2)
        for s in sems[:self.hil_lock.CONTROLLER_SLOTS]:
            self.assertTrue(s.acquire(blocking=False) and s.acquire(blocking=False))
            self.assertFalse(s.acquire(blocking=False))


class ThroughputPayloadBound(unittest.TestCase):
    """An unknown link speed must pick the FS payload, and each dd must be bounded by the
    payload actually requested."""

    def test_only_a_read_high_speed_gets_the_big_payload(self):
        for speed in (None, '12', '1.5'):
            self.assertTrue(hil_test.link_is_fs(speed), f'{speed!r} must scale as FS')
        for speed in ('480', '5000', '10000'):
            self.assertFalse(hil_test.link_is_fs(speed))

    def test_dd_bound_scales_with_the_payload_and_stays_bounded(self):
        self.assertGreater(hil_test.dd_timeout(16), hil_test.dd_timeout(1))
        self.assertGreaterEqual(hil_test.dd_timeout(1), 30)  # setup + flush floor
        # still an INNER bound: run_cmd's own timeout must stay the outer one
        self.assertLess(hil_test.dd_timeout(16), hil_test.hil_util.CMD_TIMEOUT)


class FindDeviceCache(unittest.TestCase):
    """usbtest.find_device's cache is keyed by sysname, a bus-topology path: after a
    renumber it can name a different cafe:4010 board, and idVendor/idProduct are identical
    on every one of them. Only `serial` tells them apart."""

    def setUp(self):
        import usbtest
        self.usbtest = usbtest
        self.tmp = TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.saved_sys_usb = usbtest.SYS_USB
        usbtest.SYS_USB = Path(self.tmp.name)
        usbtest._DEV_CACHE.clear()
        self._dev('1-2', 'AAAA', devnum=2)
        self._dev('1-3', 'BBBB', devnum=3)

    def tearDown(self):
        self.usbtest.SYS_USB = self.saved_sys_usb
        self.usbtest._DEV_CACHE.clear()

    def _dev(self, sysname, serial, devnum):
        d = Path(self.tmp.name) / sysname
        d.mkdir()
        for name, val in (('idVendor', self.usbtest.VID), ('idProduct', self.usbtest.PID),
                          ('serial', serial), ('busnum', '1'), ('devnum', str(devnum)),
                          ('speed', '480'), ('bcdDevice', '0104')):
            (d / name).write_text(val + '\n')

    def test_cached_sysname_with_another_boards_serial_is_rejected(self):
        self.usbtest._DEV_CACHE['bbbb'] = '1-2'   # renumbered: 1-2 is board AAAA now
        dev = self.usbtest.find_device('BBBB')
        self.assertEqual(dev['sysname'], '1-3')
        self.assertEqual(dev['serial'], 'BBBB')
        self.assertEqual(self.usbtest._DEV_CACHE['bbbb'], '1-3')

    def test_cached_sysname_with_the_right_serial_is_kept(self):
        self.usbtest._DEV_CACHE['bbbb'] = '1-3'
        dev = self.usbtest.find_device('BBBB')
        self.assertEqual((dev['sysname'], dev['serial']), ('1-3', 'BBBB'))

    def test_a_cached_device_that_vanished_falls_back_to_the_scan(self):
        self.usbtest._DEV_CACHE['bbbb'] = '1-9'   # gone from sysfs
        self.assertEqual(self.usbtest.find_device('BBBB')['sysname'], '1-3')


class ReRunSpecNamesOnlyWhatFailed(unittest.TestCase):
    """The pool-guard path used to leave this unwritten -- and a fresh run has already
    unlinked it -- so build.yml's re-run step found nothing and GitHub re-tested all ~26
    boards to find the one that wedged."""

    def test_only_failed_boards_and_their_failed_tests(self):
        with TemporaryDirectory() as td:
            d = Path(td)
            spec = d / 'cfg.failed'
            hil_test._write_failed_spec(spec, d, [
                ('good', 0, [], None, 1.0),
                ('bad', 2, ['device/cdc_msc'], None, 1.0),
                ('wedged', 1, [], None, 0.0),          # never reported: no test list
            ])
            got = spec.read_text()
        self.assertIn('-b bad', got)
        self.assertIn('-bt bad:device/cdc_msc', got)
        self.assertIn('-b wedged', got)
        self.assertNotIn('good', got)

    def test_an_all_green_run_removes_a_stale_spec(self):
        with TemporaryDirectory() as td:
            d = Path(td)
            spec = d / 'cfg.failed'
            spec.write_text('--accumulate -b stale')
            hil_test._write_failed_spec(spec, d, [('good', 0, [], None, 1.0)])
            self.assertFalse(spec.exists(), 'a stale spec would re-run last time\'s boards')


class _FakeProc:
    """A killed testusb child: `reaps` says whether wait() returns or times out."""
    def __init__(self, reaps: bool):
        self.reaps, self.waits = reaps, []

    def wait(self, timeout=None):
        self.waits.append(timeout)
        if not self.reaps:
            raise subprocess.TimeoutExpired('testusb', timeout)


class RunCaseConfirmsByReap(unittest.TestCase):
    """HUNG is 'SIGKILL not reaped in 5 s', which testusb blocked on a PEER's held device lock
    also produces (its own device walk opens every node). Watching our child for
    WEDGE_CONFIRM_S tells the two apart: reaped late is a timeout, not a wedge (#3944).
    Under sudo the child is the wrapper, so its reaping proves nothing."""

    def _run_case(self, sudo: bool, reaps_late: bool):
        import usbtest

        class Popen:
            def __init__(self, cmd, **kw):
                self.cmd = cmd
                self.communicates = 0

            def communicate(self, timeout=None):
                self.communicates += 1
                raise subprocess.TimeoutExpired(self.cmd, timeout)

            def kill(self):
                pass

            def wait(self, timeout=None):
                self.waited = timeout
                if not reaps_late:
                    raise subprocess.TimeoutExpired(self.cmd, timeout)

        usbtest_harness.patch(self, usbtest.subprocess, 'Popen', Popen)
        usbtest_harness.patch(self, usbtest, 'dmesg_tail', lambda: '')
        usbtest_harness.patch(self, usbtest.os, 'access', lambda p, m: not sudo)
        usbtest_harness.patch(self, usbtest.os, 'geteuid', lambda: 1000)
        return usbtest.run_case(1, dict(usbtest_harness.DEV), '/bin/true', False, 7)

    def test_a_child_reaped_within_the_window_is_a_late_timeout(self):
        r = self._run_case(sudo=False, reaps_late=True)
        self.assertEqual(r['status'], 'FAIL')
        self.assertTrue(r['late_cleared'])
        self.assertIn('landed', r['detail'])
        self.assertNotIn('_proc', r)

    def test_a_child_still_stuck_after_the_window_is_hung_with_its_handle(self):
        import usbtest
        r = self._run_case(sudo=False, reaps_late=False)
        self.assertEqual(r['status'], 'HUNG')
        self.assertEqual(r['_proc'].waited, usbtest.WEDGE_CONFIRM_S)

    def test_under_sudo_the_wrapper_reaping_proves_nothing(self):
        r = self._run_case(sudo=True, reaps_late=True)
        self.assertEqual(r['status'], 'HUNG')
        self.assertIsNone(r['_proc'], 'the wrapper is not the child to watch')


class HangRecoveryOnTheMainPath(unittest.TestCase):
    """The transitions through usbtest.main(): a HUNG case aborts the battery; ONE recovery
    step runs through the convoy-safe flasher (reset where it has one, else reflash); the
    child reaping afterwards is the only thing that clears `wedged`; an unverifiable child
    (sudo) keeps the hang; a late-cleared timeout also aborts but is not a wedge."""

    def _main(self, proc=None, recover=None, reset=True, raising_reset=False, late=False):
        """Returns (json or None, stderr, exception or None, sysfs writes). self.ladder
        records the recovery's lookups, resets, settles and reflashes in order."""
        import usbtest
        writes = []
        self.ladder = []

        def patch(obj, name, value):
            usbtest_harness.patch(self, obj, name, value)

        def run_case(num, d, tu, quick, timeout):
            if late:
                return {'num': num, 'name': 'x', 'params': '', 'status': 'FAIL', 'late_cleared': True,
                        'detail': 'timeout after 7s (the kill landed 3s late)'}
            return {'num': num, 'name': 'x', 'params': '', 'status': 'HUNG',
                    'detail': f'testusb stuck in D state after {timeout}s', '_proc': proc}
        usbtest_harness.stub_device(self, usbtest, run_case)
        patch(usbtest, 'bind_usbtest', lambda d: None)
        patch(usbtest, 'register_usbtest_id', lambda: None)
        patch(usbtest, 'sysfs_write', lambda path, data, check=True: writes.append((str(path), data)))
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        patch(usbtest, 'DRIVER', Path(td.name))          # no bound interfaces to unbind
        patch(usbtest.time, 'sleep', lambda s: self.ladder.append(('sleep', s)))
        if recover:
            # a convoy-safe openocd board whose reset and reflash are stubs
            patch(hil_flash, 'convoy_safe', lambda f: True)
            self.flashed = []          # every in-run reflash attempted
            patch(hil_flash, 'flash_openocd', lambda board, fw, **kw: self.flashed.append((fw, kw.get('timeout')))
                  or self.ladder.append('flash') or types.SimpleNamespace(returncode=0, stdout=b'', stderr=b''))

            def reset_primitive(name):
                self.ladder.append(('lookup', name))
                if not reset:
                    return None

                def _reset(board, timeout):
                    self.ladder.append(('reset', board['name'], timeout))
                    if raising_reset:
                        raise RuntimeError('probe gone')
                return _reset
            patch(hil_flash, 'reset_primitive', reset_primitive)
        usbtest_harness.argv(self, '--timeout', '7')
        if recover:
            sys.argv += ['--recover-board', json.dumps({'name': 'b', 'flasher': {
                'name': 'openocd', 'vid_pid': '0x1 0x2', 'args': '', **(recover if isinstance(recover, dict) else {})}}),
                         '--recover-fw', '/tmp/fw.elf']
        out, err, exc = usbtest_harness.Out(), io.StringIO(), None
        with redirect_stdout(out), redirect_stderr(err):
            try:
                usbtest.main()
            except Exception as e:   # noqa: BLE001 - the test inspects it
                exc = e
        data = json.loads(out.getvalue()) if out.getvalue().strip() else None
        return data, err.getvalue(), exc, writes

    def test_a_late_cleared_timeout_aborts_the_battery_but_is_not_a_wedge(self):
        data, err, exc, writes = self._main(late=True)
        self.assertIsNone(exc)
        self.assertFalse(data['wedged'])
        self.assertEqual(data['cases'][0]['status'], 'FAIL')
        self.assertIn('not a wedge', err)
        self.assertNotIn('unrecovered hang', err)
        self.assertEqual(writes, [], 'a standalone run removed the id or unbound a peer')

    def test_a_hang_without_recovery_flags_stays_wedged_and_nothing_is_written(self):
        data, err, _exc, writes = self._main(proc=_FakeProc(reaps=True))
        self.assertTrue(data['wedged'])
        self.assertEqual(data['cases'][0]['status'], 'HUNG')
        self.assertNotIn('_proc', data['cases'][0])
        self.assertIn('unrecovered hang', err)
        self.assertEqual(writes, [])

    def test_an_unknown_recovery_flasher_leaves_the_wedge_unrecovered(self):
        data, err, exc, writes = self._main(proc=_FakeProc(reaps=True), recover={'name': 'nosuch'})
        self.assertIsNone(exc)
        self.assertTrue(data['wedged'])
        self.assertIn('recovery unavailable', err)
        self.assertEqual(self.ladder, [], 'an unknown flasher must not reach the reset or the reflash')
        self.assertEqual(writes, [])

    def _steps(self):
        import usbtest
        names = {('reset', 'b', usbtest.RECOVER_RESET_TIMEOUT): 'reset',
                 ('sleep', usbtest.RECOVER_SETTLE): 'settle', 'flash': 'flash'}
        return [names.get(e, e) for e in self.ladder if e[0] != 'lookup']

    def test_a_reset_that_frees_the_child_clears_the_wedge(self):
        proc = _FakeProc(reaps=True)
        data, err, exc, _w = self._main(proc=proc, recover=True)
        self.assertIsNone(exc)
        self.assertFalse(data['wedged'])
        self.assertEqual(self._steps(), ['reset', 'settle'], 'reset first, then settle, never a reflash')
        self.assertEqual(self.flashed, [])
        self.assertEqual(proc.waits, [__import__('usbtest').RECOVER_REAP])
        self.assertIn('recovery freed the device', err)

    def test_a_reset_that_leaves_the_child_stuck_keeps_the_wedge(self):
        data, err, _exc, _w = self._main(proc=_FakeProc(reaps=False), recover=True)
        self.assertTrue(data['wedged'])
        self.assertEqual(self._steps(), ['reset', 'settle'])
        self.assertIn('still in D state', err)
        self.assertIn('unrecovered hang', err)

    def test_no_reset_primitive_reflashes_the_firmware_under_test(self):
        import usbtest
        data, _err, exc, _w = self._main(proc=_FakeProc(reaps=True), recover=True, reset=False)
        self.assertIsNone(exc)
        self.assertFalse(data['wedged'])
        self.assertEqual(self._steps(), ['flash', 'settle'])
        self.assertEqual(self.flashed, [('/tmp/fw.elf', usbtest.RECOVER_FLASH_TIMEOUT)])

    def test_a_step_that_raises_still_settles_and_checks_the_child(self):
        proc = _FakeProc(reaps=True)
        data, err, exc, _w = self._main(proc=proc, recover=True, raising_reset=True)
        self.assertIsNone(exc)
        self.assertIn('recovery step raised', err)
        self.assertEqual(self._steps(), ['reset', 'settle'])
        self.assertFalse(data['wedged'], 'the reset landed before the flasher raised')

    def test_under_sudo_the_hang_cannot_be_cleared(self):
        data, err, _exc, _w = self._main(proc=None, recover=True)
        self.assertTrue(data['wedged'])
        self.assertEqual(self._steps(), ['reset', 'settle'], 'the step still runs; only the verdict is withheld')
        self.assertIn('cannot confirm', err)

    def test_the_settle_keeps_its_validated_minimum(self):
        import usbtest
        self.assertGreaterEqual(usbtest.RECOVER_SETTLE, 5, 'validated minimum: metro_m4_express UF2 double-tap')


class MtpGioOrdering(_MtpFakeRig, unittest.TestCase):
    """gio must not run until the device is READY.

    gvfs claims an MTP device only AFTER udev probing, so the mount this unmounts cannot
    exist before /dev/libmtp-<sysname> is published -- an unmount issued earlier is a
    guaranteed no-op that still forks a process, and it leaves the window between the
    unmount and the open unprotected, which is the hang it exists to prevent. Running it
    per poll iteration also forks one gio per second of the enumeration budget."""

    def setUp(self):
        super().setUp()
        tmp = Path(self.tmp.name)
        self.gio_log = tmp / 'gio.log'
        binn = tmp / 'bin'; binn.mkdir()
        (binn / 'gio').write_text('#!/bin/sh\necho "$@" >> "$GIO_LOG"\n')
        (binn / 'gio').chmod(0o755)
        for k in ('PATH', 'GIO_LOG'):
            old = os.environ.get(k)
            self.addCleanup(lambda k=k, v=old: os.environ.__setitem__(k, v)
                            if v is not None else os.environ.pop(k, None))
        os.environ['GIO_LOG'] = str(self.gio_log)
        os.environ['PATH'] = f'{binn}:{os.environ["PATH"]}'

    def _gio_calls(self):
        return self.gio_log.read_text().splitlines() if self.gio_log.exists() else []

    def test_gio_does_not_run_before_the_device_is_ready(self):
        (Path(self.tmp.name) / 'dev' / 'libmtp-1-1').unlink()   # never becomes ready
        os.environ['FAKE_PYMTP_MODE'] = 'absent'
        run_bounded(lambda: hil_test.test_device_mtp(self.board), 30)
        calls = self._gio_calls()
        self.assertEqual(calls, [], f'gio ran {len(calls)}x with no device ready: {calls}')

    def test_gio_still_runs_once_the_device_is_ready(self):
        """The guard must delay the unmount, not delete it."""
        os.environ['FAKE_PYMTP_MODE'] = 'ok'
        hil_test.test_device_mtp(self.board)
        self.assertTrue(self._gio_calls(), 'gio never ran for a ready device')


class MtpGioFallthrough(unittest.TestCase):
    """The missing-gio path must fall THROUGH to detection. `continue` there skips the
    deadline check and the sleep as well, spinning at 100% CPU until the caller's outer
    kill — reported as a wedged DUT for a missing apt package."""

    def test_a_missing_gio_still_bounds_the_session(self):
        import subprocess
        with TemporaryDirectory() as td:
            env = {**os.environ, 'PATH': td,          # no gio, no anything
                   'PYTHONPATH': os.path.join(TEST_DIR, 'stubs'),
                   'FAKE_PYMTP_MODE': 'none', 'PYTHONSAFEPATH': '1'}
            t0 = time.monotonic()
            r = subprocess.run([sys.executable,
                                str(Path(TEST_DIR).parents[0] / 'mtp_test.py'),
                                '--uid', 'CAFE01', '--timeout', '1'],
                               capture_output=True, text=True, timeout=60, env=env)
            elapsed = time.monotonic() - t0
        self.assertLess(elapsed, 30, f'did not honour --timeout 1 ({elapsed:.1f}s)')
        self.assertNotEqual(r.returncode, 0)
        # The assertions above are satisfied by an immediate CRASH, which is exactly what
        # shipped through this test once: `pass` left gio unbound and the next line
        # dereferenced it. Assert the behaviour the docstring names -- it POLLED for the
        # device (so it spent its budget) and did not die on a traceback.
        self.assertGreater(elapsed, 0.8,
                           f'exited without polling ({elapsed:.1f}s) -- it crashed')
        self.assertNotIn('Traceback', r.stderr)
        self.assertIn('MTP device not found', r.stdout + r.stderr)


class RunWhileContract(unittest.TestCase):
    """The read-while-we-write runner. Its child can still outlast SIGKILL, and is then
    abandoned with our pipe ends closed."""

    def setUp(self):
        from helper import hil_util
        self.hil_util = hil_util

    def test_an_error_in_work_is_not_swallowed(self):
        """A `return` inside the reap's `finally` discarded it: an assert in the CDC
        write half vanished and the caller went on to compare data it never sent."""
        def boom():
            raise AssertionError('the write failed')
        with self.assertRaises(AssertionError):
            self.hil_util.run_alongside(['sh', '-c', 'printf X'], boom, 5)

    def test_the_child_is_reaped_even_when_work_raises(self):
        seen = {}

        def boom():
            raise AssertionError('x')
        # a duration no other process would plausibly pick: `pgrep -f` searches the WHOLE
        # machine, so a bare `sleep 20` matched an unrelated background job -- another
        # agent session's retry loop, in the case that exposed this -- and failed a test
        # about our own child. Observed failing 3/3 in isolation while that loop ran.
        sentinel = '20.0451'
        with self.assertRaises(AssertionError):
            self.hil_util.run_alongside(['sleep', sentinel], boom, 1)
        # nothing of ours is left running: the reap ran on the error path too
        import subprocess
        out = subprocess.run(['pgrep', '-f', f'^sleep {sentinel}'],
                             capture_output=True, text=True)
        seen['strays'] = [p for p in out.stdout.split() if p]
        self.assertEqual(seen['strays'], [], 'work() raising leaked the child')

    def test_an_abandoned_child_is_in_its_own_session(self):
        """killpg on it reaps whatever it spawned, and it cannot take our group with it."""
        import subprocess
        pgids = {}

        def check():
            time.sleep(0.2)
            pgids['child'] = os.getpgid(self._proc_pid)

        real_popen = subprocess.Popen

        def spy(argv, **kw):
            p = real_popen(argv, **kw)
            self._proc_pid = p.pid
            return p
        self.addCleanup(setattr, subprocess, 'Popen', real_popen)
        subprocess.Popen = spy
        self.hil_util.run_alongside(['sleep', '0.5'], check, 5)
        subprocess.Popen = real_popen
        self.assertNotEqual(pgids['child'], os.getpgid(0))


class UsbScanIsTheOneWalk(unittest.TestCase):
    """Three call sites each had a different subset of the three things this must get
    right; none had all three. The expensive read is `serial` -- served under the device
    lock a wedged usbfs ioctl holds -- so it must come LAST, only for devices the free
    descriptor fields could not rule out, and never twice for a path that stranded."""

    def setUp(self):
        from helper import hil_util
        self.hil_util = hil_util
        self.td = TemporaryDirectory()
        self.addCleanup(self.td.cleanup)
        self.root = Path(self.td.name)
        self.reads = []
        real = hil_util.read_sysfs

        def counting(path, *a, **k):
            self.reads.append(path)
            return real(path, *a, **k)
        self.addCleanup(setattr, hil_util, 'read_sysfs', real)
        hil_util.read_sysfs = counting

    def _dev(self, name, vid, pid, serial='S1'):
        d = self.root / name
        d.mkdir()
        (d / 'idVendor').write_text(vid + '\n')
        (d / 'idProduct').write_text(pid + '\n')
        (d / 'serial').write_text(serial + '\n')
        return d

    def _scan(self, **kw):
        import glob as _g
        real_glob = _g.glob
        self.addCleanup(setattr, self.hil_util.glob, 'glob', real_glob)
        self.hil_util.glob.glob = lambda pat: [str(p) for p in self.root.iterdir()]
        return self.hil_util.usb_scan(**kw)

    def test_a_mismatched_vid_pid_costs_no_serial_read(self):
        """`serial` is the ONE attribute here served under the device lock, so it is the
        one that can block on a wedged device. Filtering on the lock-free descriptor pair
        first is what keeps a scan for our board off every other board's locked read."""
        self._dev('1-1', '1234', '5678')
        self._dev('1-2', 'cafe', '4010', serial='UID1')
        devs = self._scan(vid_pid=('cafe', '4010'))
        self.assertEqual([d['serial'] for d in devs], ['UID1'])
        # the ruled-out device's locked attribute was never touched
        self.assertNotIn(str(self.root / '1-1' / 'serial'), self.reads)


class UsbtestOuterBoundIsOneValue(unittest.TestCase):
    """run_cmd's kill is the ONE bound, and it must carry a recovery reserve only when a
    recovery can actually run. Otherwise a board on a path that cannot recover holds a pool
    worker and its battery permit idle for the difference, under a usbtest width of 2."""

    def _invoke(self, flasher, skip_flash=False, recover=None):
        from contextlib import contextmanager
        from helper import hil_lock, hil_util

        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        dev = Path(td.name) / 'dev1'
        dev.mkdir()
        for attr, val in (('serial', 'UID1'), ('idVendor', 'cafe'), ('idProduct', '4010')):
            (dev / attr).write_text(val + '\n')

        def patch(obj, name, value):
            self.addCleanup(setattr, obj, name, getattr(obj, name))
            setattr(obj, name, value)

        def _permit(uid):
            yield

        seen = {}

        def fake_run(cmd, **kw):
            import subprocess
            seen['cmd'], seen['timeout'] = cmd, kw.get('timeout')
            return subprocess.CompletedProcess(cmd, 1, stdout=b'', stderr=b'stub')

        from helper import hil_util as _hu
        patch(_hu, 'glob', types.SimpleNamespace(glob=lambda p: [str(dev)]))
        patch(hil_test, 'USBTEST_SETTLE', 0)   # see no_settle
        patch(hil_lock, 'usbtest_permit', contextmanager(_permit))
        patch(hil_test, 'skip_flash', skip_flash)
        patch(hil_test, '_current_fw', '/tmp/fw.elf')
        patch(hil_util, 'run_cmd', fake_run)
        board = {'name': 'b', 'uid': 'UID1', 'flasher': flasher}
        if recover is not None:
            board['flasher_recover'] = recover
        with self.assertRaises(hil_test.TestFail):
            hil_test.test_device_usbtest(board)
        return seen

    def test_a_recoverable_board_reserves_the_recovery_budget(self):
        import usbtest
        flasher = {'name': 'openocd', 'vid_pid': '0x1366 0x1024',
                   'args': '-f target/rp2040.cfg'}
        seen = self._invoke(flasher)
        want = (hil_test.USBTEST_BATTERY_BUDGET + hil_test.USBTEST_OVERSHOOT
                + usbtest.recovery_reserve(flasher))
        self.assertEqual(seen['timeout'], want)

    def test_the_reserve_follows_the_flasher_not_a_fleet_constant(self):
        """A flasher with a reset primitive reserves the reset bound, one without (esptool)
        the reflash bound: the difference is dead time a pool worker and a usbtest permit
        would otherwise hold."""
        import usbtest
        ocd = self._invoke({'name': 'openocd', 'vid_pid': '0x1366 0x1024',
                            'args': '-f target/rp2040.cfg'})
        esp = self._invoke({'name': 'esptool', 'args': ''})
        self.assertEqual(esp['timeout'] - ocd['timeout'],
                         usbtest.RECOVER_FLASH_TIMEOUT - usbtest.RECOVER_RESET_TIMEOUT)

    def test_a_board_with_no_recovery_does_not_pay_for_one(self):
        import usbtest
        seen = self._invoke({'name': 'stlink', 'uid': 'X'})   # never convoy_safe
        # It does not carry the RECOVERY reserve it cannot spend, only the window in which
        # the child watches a HUNG node before calling it a wedge (#3944): that runs on
        # every path, and an outer kill inside it loses the JSON and the verdict.
        self.assertEqual(seen['timeout'],
                         hil_test.USBTEST_BATTERY_BUDGET + hil_test.USBTEST_OVERSHOOT
                         + usbtest.WEDGE_CONFIRM_S)
        # ...but it MUST still exceed the child's own --budget. The battery checks the
        # budget before dispatching, so it can overshoot by one already-started case; an
        # equal bound SIGKILLs it just as it goes to print, turning ~29 real per-case
        # verdicts into "usbtest did not run" and re-paying the whole battery on retry.
        toks = seen['cmd'].split()
        budget = int(toks[toks.index('--budget') + 1])
        case_timeout = int(toks[toks.index('--timeout') + 1])
        self.assertGreaterEqual(seen['timeout'] - budget, case_timeout,
                                'the outer kill can land mid-case, before the JSON')

    def test_a_jlink_board_recovers_through_its_flasher_recover_entry(self):
        """The roster's optional second flasher (#3945): the primary stays JLinkExe, which
        is never convoy-safe, so the reserve and the --recover-board JSON both follow the
        openocd entry -- with the firmware jlink flashed, not a re-derived one."""
        import json
        import shlex
        import usbtest
        prim = {'name': 'jlink', 'uid': '779541626', 'args': '-device stm32f072rb'}
        rec = {'name': 'openocd', 'uid': '779541626',
               'args': '-f interface/jlink.cfg -c "adapter speed 4000" -f target/stm32f0x.cfg'}
        alone = self._invoke(prim)
        self.assertNotIn('--recover-board', alone['cmd'])
        seen = self._invoke(prim, recover=rec)
        self.assertEqual(seen['timeout'], hil_test.USBTEST_BATTERY_BUDGET
                         + hil_test.USBTEST_OVERSHOOT + usbtest.recovery_reserve(rec))
        toks = shlex.split(seen['cmd'])
        shipped = json.loads(toks[toks.index('--recover-board') + 1])
        self.assertEqual(shipped, {'name': 'b', 'flasher': rec})
        self.assertEqual(toks[toks.index('--recover-fw') + 1], '/tmp/fw.elf')
        # the reset step is reserved, not a reflash: reset_openocd exists, unlike esptool's
        self.assertEqual(usbtest.recovery_reserve(rec) - usbtest.recovery_reserve({'name': 'esptool'}),
                         usbtest.RECOVER_RESET_TIMEOUT - usbtest.RECOVER_FLASH_TIMEOUT)

    def test_skip_flash_still_bounds_the_child(self):
        """--skip-flash disables recovery, so the child must not be given a reserve it
        cannot spend -- but it MUST still be bounded, and the bound still covers the window
        in which the child watches a HUNG node before calling it a wedge."""
        import usbtest
        seen = self._invoke({'name': 'openocd', 'vid_pid': '0x1366 0x1024'}, skip_flash=True)
        self.assertEqual(seen['timeout'],
                         hil_test.USBTEST_BATTERY_BUDGET + hil_test.USBTEST_OVERSHOOT
                         + usbtest.WEDGE_CONFIRM_S)


class UsbtestRetryPolicy(unittest.TestCase):
    """The pool guard bounds ONE battery; the retry loop multiplies it by max_retry.
    So the loop must retry only what a retry can fix."""

    def _patch(self, obj, name, value):
        # addCleanup, not a finally: a failing assert must not leave the real module
        # patched for whatever test runs next (max_retry only exists once main() ran,
        # so restoring it means DELETING it again)
        if hasattr(obj, name):
            self.addCleanup(setattr, obj, name, getattr(obj, name))
        else:
            self.addCleanup(delattr, obj, name)
        setattr(obj, name, value)

    def _attempts(self, exc):
        """How many times test_example runs the test fn before giving up."""
        import hil_flash
        calls = []

        def fake_test(board):
            calls.append(1)
            raise exc

        self._patch(hil_flash, 'find_firmware', lambda *a, **k: Path('/nonexistent/fw.elf'))
        self._patch(hil_test, 'skip_flash', True)       # no probe, no hardware
        self._patch(hil_test, 'max_retry', 3)
        self._patch(hil_test, 'log_line', lambda *a, **k: None)
        hil_test.test_fake_example = fake_test
        self.addCleanup(delattr, hil_test, 'test_fake_example')
        hil_test.test_example({'name': 'b', 'uid': 'u', 'flasher': {'name': 'openocd'}},
                              'v', 'fake/example')
        return len(calls)

    def test_a_per_case_verdict_is_not_retried(self):
        # re-running the battery only re-observes a number the JSON already reported
        self.assertEqual(self._attempts(hil_test.TestFail('29/30', parsed=True)), 1)

    def test_a_transient_failure_is_retried(self):
        self.assertEqual(self._attempts(hil_test.TestFail('usbtest did not run')), 3)


class UsbtestOuterKillStaysRetryable(unittest.TestCase):
    """rc 124 is run_cmd's timer expiring, NOT proof the DUT is wedged -- a healthy
    battery can hit it under load. Suppressing the retry to save the budget also
    suppresses the reflash test_example does before each attempt, which is the only
    thing left to unpoison the DUT where usbtest's in-band recovery is off."""

    def setUp(self):
        from contextlib import contextmanager
        from helper import hil_lock
        self.td = TemporaryDirectory()
        self.addCleanup(self.td.cleanup)
        dev = Path(self.td.name) / 'dev1'
        dev.mkdir()
        # a real (readable) fake sysfs node, so the bounded reads run unmodified
        for attr, val in (('serial', 'UID1'), ('idVendor', 'cafe'), ('idProduct', '4010')):
            (dev / attr).write_text(val + '\n')
        self.dev = dev

        def patch(obj, name, value):
            saved = getattr(obj, name)
            self.addCleanup(setattr, obj, name, saved)
            setattr(obj, name, value)

        from helper import hil_util as _hu
        patch(_hu, 'glob', types.SimpleNamespace(glob=lambda p: [str(dev)]))
        patch(hil_test, 'USBTEST_SETTLE', 0)   # see no_settle
        def _permit(uid):        # a real generator: a lambda returning an iterator has
            yield                # no .throw(), so any raise inside the `with` would
                                 # surface as an AttributeError from contextlib instead
        patch(hil_lock, 'usbtest_permit', contextmanager(_permit))
        patch(hil_test, 'skip_flash', True)

    def test_rc_124_stays_retryable(self):
        import subprocess
        from helper import hil_util
        saved = hil_util.run_cmd
        self.addCleanup(setattr, hil_util, 'run_cmd', saved)
        hil_util.run_cmd = lambda *a, **k: subprocess.CompletedProcess(
            'usbtest', 124, stdout=b'', stderr=b'killed on the outer bound')
        with self.assertRaises(hil_test.TestFail) as cm:
            hil_test.test_device_usbtest({'name': 'b', 'uid': 'UID1',
                                          'flasher': {'name': 'openocd'}})
        self.assertFalse(cm.exception.parsed,
                         'the retry is the last reflash a poisoned DUT gets')

    def test_a_crashed_tool_stays_retryable(self):
        import subprocess
        from helper import hil_util
        saved = hil_util.run_cmd
        self.addCleanup(setattr, hil_util, 'run_cmd', saved)
        hil_util.run_cmd = lambda *a, **k: subprocess.CompletedProcess(
            'usbtest', 1, stdout=b'', stderr=b'ImportError: no module named usbtest')
        with self.assertRaises(hil_test.TestFail) as cm:
            hil_test.test_device_usbtest({'name': 'b', 'uid': 'UID1',
                                          'flasher': {'name': 'openocd'}})
        self.assertFalse(cm.exception.parsed)


class PoolGuardKeepsWhatFinished(unittest.TestCase):
    """The guard's 30-minute predecessor fired on 5 of the last 8 HIL jobs, so this is the
    common failure, not an edge case: map_async discarded every board that had finished and
    left the re-run spec unwritten, so CI re-tested all ~26 to find the one that wedged.

    Calls hil_test.drain_pool -- the loop main() actually runs. The predecessor of this test
    built its own ThreadPool and its own drain loop and asserted on those, so deleting the
    production drain outright left it green."""

    class _It:
        """Stands in for imap_unordered: yields, then blocks past any deadline."""

        def __init__(self, ready):
            self.ready, self.i = ready, 0

        def next(self, timeout=None):
            if self.i < len(self.ready):
                self.i += 1
                return self.ready[self.i - 1]
            raise MpTimeoutError

    def test_finished_rows_survive_a_guard_expiry(self):
        boards = [{'name': 'fast1'}, {'name': 'fast2'}, {'name': 'wedged'}]
        rows = [('fast1', 0, [], [], 1.0), ('fast2', 0, [], [], 1.0)]
        with self.assertRaises(hil_test.PoolDrainTimeout) as cm:
            hil_test.drain_pool(self._It(rows), boards, time.monotonic() + 5)
        self.assertEqual([r[0] for r in cm.exception.finished], ['fast1', 'fast2'])

    def test_an_expired_deadline_stops_before_asking_for_more(self):
        """Left <= 0 must not be handed to it.next() as a zero/negative timeout."""
        boards = [{'name': 'a'}, {'name': 'b'}]
        it = self._It([('a', 0, [], [], 1.0)])
        with self.assertRaises(hil_test.PoolDrainTimeout) as cm:
            hil_test.drain_pool(it, boards, time.monotonic() - 1)     # already past
        self.assertEqual(cm.exception.finished, [])
        self.assertEqual(it.i, 0, 'asked the pool for a result after the deadline')

    def test_rows_collected_before_the_deadline_expires_are_kept_too(self):
        """The OTHER raise site: boards finish, then the clock runs out between results.
        Both sites must carry the rows -- a bare raise here loses a worker-width of rig
        time just as map_async did, and the it.next() path alone does not prove it."""
        class Slow(self._It):
            def next(self, timeout=None):
                time.sleep(0.2)                       # each result eats into the deadline
                return super().next(timeout)

        boards = [{'name': n} for n in ('a', 'b', 'c', 'd')]
        rows = [(n, 0, [], [], 1.0) for n in ('a', 'b', 'c', 'd')]
        with self.assertRaises(hil_test.PoolDrainTimeout) as cm:
            hil_test.drain_pool(Slow(rows), boards, time.monotonic() + 0.3)
        self.assertTrue(cm.exception.finished, 'rows collected before the expiry were lost')

    def test_every_board_finishing_returns_them_all(self):
        boards = [{'name': 'a'}, {'name': 'b'}]
        rows = [('a', 0, [], [], 1.0), ('b', 1, [], [], 2.0)]
        got = hil_test.drain_pool(self._It(rows), boards, time.monotonic() + 5)
        self.assertEqual(got, rows)


class WedgedBoardCosts(unittest.TestCase):
    """Two decisions the board_wedged latch makes, tested as decisions rather than through
    test_board's loop -- the loop-level predecessor of these tests reimplemented that loop
    and asserted on its own copy, which is how both defects survived it."""

    def setUp(self):
        self.addCleanup(setattr, hil_test, 'board_wedged', hil_test.board_wedged)

    def test_a_board_that_wedged_still_counts_as_an_error(self):
        """It rendered a red cell but returned err_count 0, so main()'s sys.exit(err_count)
        reported success and _write_failed_spec (`if err > 0`) left the board out of the
        re-run entirely: a rig holding a D-state process published as a clean pass."""
        hil_test.board_wedged = 'usbtest HUNG'
        # no real flasher: skip_flash isolates the accounting from hil_flash
        self.addCleanup(setattr, hil_test, 'skip_flash', hil_test.skip_flash)
        hil_test.skip_flash = True
        # a firmware path must resolve or test_example returns 'skip (no binary)' before
        # ever reaching the retry loop this is about
        self.addCleanup(setattr, hil_flash, 'find_firmware', hil_flash.find_firmware)
        hil_flash.find_firmware = lambda *a, **k: Path('fw.elf')

        def boom(*a, **k):
            raise hil_test.TestFail('usbtest did not run')          # unparsed: retryable

        self.addCleanup(setattr, hil_test, 'test_device_usbtest', hil_test.test_device_usbtest)
        hil_test.test_device_usbtest = boom
        board = {'name': 'b', 'uid': 'U', 'flasher': {'name': 'openocd'}, 'tests': []}
        err, _status, _metric = hil_test.test_example(board, 'b', 'device/usbtest')
        self.assertEqual(err, 1, 'a wedged board contributed nothing to the exit status')

    def test_the_teardown_park_does_not_flash_a_wedged_board(self):
        """The park is a flash like any other: on a D-state-held node it blocks, survives
        SIGKILL and leaves a stray -- added by the path that just declared the board wedged
        and skipped every test for exactly that reason."""
        hil_test.board_wedged = ''
        self.assertTrue(hil_test._should_park(False), 'a healthy board must still park')
        hil_test.board_wedged = 'usbtest HUNG'
        self.assertFalse(hil_test._should_park(False),
                         'the teardown park would flash through the poisoned node')
        self.assertFalse(hil_test._should_park(True), '--skip-flash must still suppress it')


class WedgeVerdictReachesTheLatch(unittest.TestCase):
    """usbtest computes `unrecovered_hang` but never reported it, so hil_test inferred the
    latch from `not recovery and 'HUNG' in out` and missed three cases: recovery ran and
    FAILED (convoy-safe boards -- max32666fthr HUNG in the 08-14 run), the `ambiguous`
    abort (which sets the flag but leaves no case at status HUNG), and an unparsable JSON,
    which is the outer-timeout kill and the case where a wedge is most likely."""

    def setUp(self):
        self.addCleanup(setattr, hil_test, 'board_wedged', hil_test.board_wedged)
        hil_test.board_wedged = ''
        no_settle(self)

    def _run(self, stdout, rc=0, flasher=None):
        from helper import hil_lock, hil_util
        class R:
            returncode = rc
            stderr = b''
        R.stdout = stdout.encode()
        self.addCleanup(setattr, hil_util, 'run_cmd', hil_util.run_cmd)
        hil_util.run_cmd = lambda *a, **k: R()
        # usbtest_enumerated is nested in test_device_usbtest, so stub what it calls
        self.addCleanup(setattr, hil_util, 'usb_scan', hil_util.usb_scan)
        hil_util.usb_scan = lambda **k: ([{'busport': '1-1', 'dir': '/x', 'vid': 'cafe',
                                           'pid': '4010', 'serial': 'U'}], False)
        self.addCleanup(setattr, hil_lock, 'usbtest_permit', hil_lock.usbtest_permit)
        from contextlib import contextmanager
        hil_lock.usbtest_permit = contextmanager(lambda uid: iter([None]))
        board = {'name': 'b', 'uid': 'U',
                 'flasher': flasher or {'name': 'openocd', 'vid_pid': '0x1 0x2'}}
        try:
            hil_test.test_device_usbtest(board)
        except Exception:
            pass
        return hil_test.board_wedged

    def test_a_reported_wedge_latches_even_when_recovery_ran(self):
        """`recovery` True means the flags were PASSED, not that they worked."""
        js = '{"serial":"U","speed":"480","tier":1,"passed":1,"failed":1,"notrun":0,'              '"wedged":true,"cases":[{"num":1,"status":"FAIL"}]}'
        self.assertTrue(self._run(js), 'a reported wedge did not latch')

    def test_no_wedge_reported_does_not_latch(self):
        js = '{"serial":"U","speed":"480","tier":1,"passed":2,"failed":0,"notrun":0,'              '"wedged":false,"cases":[]}'
        self.assertFalse(self._run(js))

    def test_a_late_cleared_timeout_on_a_board_without_recovery_does_not_latch(self):
        """usbtest watches a HUNG node before it says wedged (#3944); the old inference
        `not recovery and 'HUNG' in out` latched a healthy stlink board on a slow case."""
        js = ('{"serial":"U","speed":"480","tier":1,"passed":1,"failed":1,"notrun":0,'
              '"wedged":false,"cases":[{"num":10,"status":"FAIL","detail":"timeout after 60s '
              '(the kill landed 7s late; no holder left on the node); was HUNG"}]}')
        self.assertFalse(self._run(js, flasher={'name': 'stlink', 'uid': 'X'}),
                         'a cleared holder latched the board as wedged')

    def test_an_unparseable_battery_that_mentions_HUNG_still_latches(self):
        """rc 124 mid-print: no JSON to read, and this is the likeliest real wedge."""
        self.assertTrue(self._run('TEST 10 HUNG: device wedged mid-transfer', rc=124))


class WedgedBoardCannotReportAPass(unittest.TestCase):
    """The latch alone is not enough: it is set BEFORE the pass return, so an all-green
    battery that still wedged returned `PASS 30/30`. That board then contributes 0 to
    err_count, is omitted from the .failed re-run spec (which keys on err > 0), and the job
    exits 0 with a D-state holder on the rig -- the exact silence this branch exists to end.
    usbtest's `ambiguous` abort fires AFTER the last case, so nothing
    back-fills a BUDGET entry to make failed/notrun non-zero."""

    def setUp(self):
        self.addCleanup(setattr, hil_test, 'board_wedged', hil_test.board_wedged)
        hil_test.board_wedged = ''
        no_settle(self)

    def _cell(self, js):
        """Returns ('pass', cell) or ('fail', message)."""
        from helper import hil_lock, hil_util
        class R:
            returncode = 0
            stderr = b''
        R.stdout = js.encode()
        self.addCleanup(setattr, hil_util, 'run_cmd', hil_util.run_cmd)
        hil_util.run_cmd = lambda *a, **k: R()
        self.addCleanup(setattr, hil_util, 'usb_scan', hil_util.usb_scan)
        hil_util.usb_scan = lambda **k: ([{'busport': '1-1', 'dir': '/x', 'vid': 'cafe',
                                           'pid': '4010', 'serial': 'U'}], False)
        self.addCleanup(setattr, hil_lock, 'usbtest_permit', hil_lock.usbtest_permit)
        from contextlib import contextmanager
        hil_lock.usbtest_permit = contextmanager(lambda uid: iter([None]))
        board = {'name': 'b', 'uid': 'U', 'flasher': {'name': 'openocd', 'vid_pid': '0x1 0x2'}}
        try:
            return ('pass', hil_test.test_device_usbtest(board))
        except hil_test.TestFail as e:
            return ('fail', str(e))

    def test_an_all_pass_battery_that_wedged_is_not_a_pass(self):
        kind, detail = self._cell('{"serial":"U","speed":"480","tier":1,"passed":30,'
                                  '"failed":0,"notrun":0,"wedged":true,"cases":[]}')
        self.assertEqual(kind, 'fail', f'a wedged board reported a green cell: {detail}')
        self.assertIn('wedged', detail)

    def test_an_all_pass_battery_that_did_not_wedge_is_still_a_pass(self):
        """The guard must key on the latch, not merely on having parsed a battery."""
        kind, cell = self._cell('{"serial":"U","speed":"480","tier":1,"passed":30,'
                                '"failed":0,"notrun":0,"wedged":false,"cases":[]}')
        self.assertEqual(kind, 'pass', f'a healthy board was failed: {cell}')
        self.assertIn('30/30', cell)


class WedgeMessageNamesTheCause(unittest.TestCase):
    """The latch text sends the operator to a probe or a setting, so it must name what
    happened: the ambiguous and unreadable-serial aborts run no recovery, and --skip-flash
    is not the flasher's fault."""

    def setUp(self):
        self.addCleanup(setattr, hil_test, 'board_wedged', hil_test.board_wedged)
        self.addCleanup(setattr, hil_test, 'skip_flash', hil_test.skip_flash)
        hil_test.skip_flash = False

    def _latch(self, cases, recovery):
        hil_test.board_wedged = ''
        data = {'passed': 1, 'failed': 1, 'notrun': 0, 'wedged': True, 'cases': cases}
        with self.assertRaises(hil_test.TestFail):
            hil_test._usbtest_verdict({'name': 'b'}, data, '', 1, 1, recovery,
                                      {'name': 'openocd'})
        return hil_test.board_wedged

    def test_a_hang_names_the_recovery_or_what_blocked_it(self):
        hung = [{'num': 10, 'status': 'HUNG'}]
        self.assertIn('after a recovery via openocd', self._latch(hung, True))
        self.assertIn('openocd cannot deliver a recovery', self._latch(hung, False))
        hil_test.skip_flash = True
        msg = self._latch(hung, False)
        self.assertIn('--skip-flash', msg)
        self.assertNotIn('openocd', msg)

    def test_an_abort_without_a_hang_names_no_recovery(self):
        msg = self._latch([{'num': 10, 'status': 'FAIL'}], True)
        self.assertIn('could no longer be identified', msg)
        self.assertNotIn('recovery', msg)


def _gil_stall_available() -> bool:
    """Whether the hid stub can simulate a GIL-HOLDING stall on this host.

    It needs a libc with sleep(3) loaded through ctypes.PyDLL. Everywhere the HIL harness
    actually runs that is present; where it is not, the two tests that depend on it skip
    rather than fail, because their subject is the bound, not ctypes.
    """
    import ctypes
    import ctypes.util
    try:
        ctypes.PyDLL(ctypes.util.find_library('c') or 'libc.so.6')
        return True
    except OSError:
        return False


class HidEchoRunsInAChild(unittest.TestCase):
    """hidapi's blocking calls hold the GIL -- cython-hidapi wraps hid_enumerate in
    `with nogil` but calls hid_open and hid_close bare -- so a daemon thread cannot bound
    them: the waiter parks off-GIL but must reacquire the GIL to return, which the stuck
    thread never yields. Only a child process can be killed regardless, which is what
    run_cmd's killpg does."""

    def _run(self, mode, uid='CAFE01', budget='0', timeout=20, pid=None):
        saved = {k: os.environ.get(k) for k in ('FAKE_HID_MODE', 'FAKE_HID_UID',
                                                'FAKE_HID_PID', 'PYTHONPATH',
                                                'PYTHONSAFEPATH')}

        def restore():
            for k, v in saved.items():
                os.environ.pop(k, None) if v is None else os.environ.__setitem__(k, v)
        self.addCleanup(restore)
        os.environ['FAKE_HID_MODE'] = mode
        os.environ['FAKE_HID_UID'] = uid
        stubs = os.path.join(TEST_DIR, 'stubs')
        pp = saved['PYTHONPATH']
        os.environ['PYTHONPATH'] = stubs if not pp else f'{stubs}:{pp}'
        # `python3 -c` puts the cwd at sys.path[0], AHEAD of PYTHONPATH, so any hid.py
        # reachable from the suite's cwd would displace the stub and every mode-driven
        # test below would pass or fail for the wrong reason. Safe-path mode drops it --
        # the same practice _MtpFakeRig documents.
        os.environ['PYTHONSAFEPATH'] = '1'
        from helper import hil_util
        want = pid or f'{hil_test.HID_INOUT_PID:#06x}'
        return hil_util.run_cmd(
            [sys.executable, '-c', hil_test.HID_ECHO, uid, budget, want],
            timeout=timeout, split_stderr=True, quiet=True)

    def _stderr(self, r):
        from helper import hil_util
        return hil_util.cmd_stdout_text(r.stderr)

    def test_a_healthy_device_passes(self):
        r = self._run('ok')
        self.assertEqual(r.returncode, 0, self._stderr(r))

    def test_the_pid_matches_the_example(self):
        """The walk filters on BOTH ids, and hidapi applies them before the locked
        manufacturer/product reads. Six examples in this tree expose a HID interface under
        VID cafe, so a stale PID here silently widens the walk back to all of them -- and
        nothing else would fail. Pinned against the descriptor rather than restated."""
        import re
        src = (Path(TEST_DIR).parents[2]
               / 'examples/device/hid_generic_inout/src/usb_descriptors.c').read_text()
        m = re.search(r'#define\s+USB_PID\s+(0x[0-9a-fA-F]+)', src)
        self.assertIsNotNone(m, 'hid_generic_inout no longer defines USB_PID')
        self.assertEqual(hil_test.HID_INOUT_PID, int(m.group(1), 16),
                         'HID_INOUT_PID drifted from the example descriptor')

    def test_a_peer_running_another_example_is_filtered_out(self):
        """The point of the PID filter: a wedged sibling on a different example never
        reaches the locked reads at all."""
        r = self._run('ok', pid='0x400f')      # hid_composite, not ours
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('HID device not found', self._stderr(r))

    @unittest.skipUnless(_gil_stall_available(), 'no libc for a GIL-holding stall')
    def test_a_gil_holding_stall_is_still_killed(self):
        """THE case an in-process bound cannot cover. hid_open is not `with nogil`, so a
        thread-based guard is inert there; the child is killed anyway."""
        t0 = time.monotonic()
        r = self._run('wedged_open_gil', timeout=2)
        self.assertEqual(r.returncode, 124,
                         'a GIL-holding hidapi stall must still be killed on the bound')
        self.assertLess(time.monotonic() - t0, 20, 'run_cmd did not bound the child')

    def test_a_wedged_enumerate_is_killed_on_the_bound(self):
        r = self._run('wedged_enumerate', timeout=2)
        self.assertEqual(r.returncode, 124)

    @unittest.skipUnless(_gil_stall_available(), 'no libc for a GIL-holding stall')
    def test_a_wedged_close_is_killed_on_the_bound(self):
        """close() runs in the child's finally on EVERY failure path and is also
        GIL-holding; hidraw_release takes the same rwsem hidraw_open needs."""
        r = self._run('wedged_close', timeout=3)
        self.assertEqual(r.returncode, 124)

    def test_an_absent_device_reports_why(self):
        r = self._run('absent')
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('HID device not found', self._stderr(r))

    def test_a_bad_echo_reports_both_payloads(self):
        r = self._run('wrong_data')
        self.assertNotEqual(r.returncode, 0)
        msg = self._stderr(r)
        self.assertIn('wrong data', msg)
        self.assertIn('sent', msg)
        self.assertIn('received', msg)

    def test_a_short_echo_is_not_read_as_a_pass(self):
        r = self._run('short_read')
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('short read', self._stderr(r))


class MixedWidthRowsSurviveTheReportWriters(unittest.TestCase):
    """_abort_report hands `[(n, 1, [], None, 0) for n in stuck] + [r for r in mret ...]`
    to the re-run spec: synthetic rows (rows=None) mixed with worker rows."""

    def _mixed(self):
        return [('stuck', 1, [], None, 0),                       # synthetic
                ('ran', 1, ['device/dfu'],
                 [('ran', {'device/dfu': '❌ boom'}, '8s')], 8.0)]      # worker

    def test_the_rerun_spec_accepts_both_kinds(self):
        with TemporaryDirectory() as td:
            rd = Path(td)
            hil_test._write_failed_spec(rd / 'c.json.failed', rd, self._mixed())
            spec = (rd / 'c.json.failed').read_text()
        self.assertIn('stuck', spec)
        self.assertIn('ran', spec)

    def test_the_cell_names_the_cause_of_the_abort(self):
        """A board the pool guard never reached did not "pool-timeout". Marking it so
        sends whoever reads the table after a guard that never fired."""
        from helper import hil_report
        real = hil_report.accumulate_report

        def render(reason, cell):
            hil_report.accumulate_report = lambda *a, **k: (_ for _ in ()).throw(
                OSError('report dir unwritable'))
            try:
                with TemporaryDirectory() as td:
                    rd = Path(td)
                    hil_test._abort_report(reason, [], [{'name': 'boardA'}],
                                           rd / 'c.failed', rd, True, cell=cell)
                    return (rd / hil_report.REPORT_MD).read_text()
            finally:
                hil_report.accumulate_report = real

        guard = render('aborted: worker pool timed out after 3600s',
                       hil_report.POOL_TIMEOUT_CELL)
        self.assertIn(hil_report.POOL_TIMEOUT_CELL, guard)
        raised = render('aborted: a worker raised ValueError: x',
                        hil_report.RUN_ABORTED_CELL)
        self.assertIn(hil_report.RUN_ABORTED_CELL, raised)
        self.assertNotIn(hil_report.POOL_TIMEOUT_CELL, raised,
                         'a run that aborted on a raise is not a pool timeout')
        # and the fallback must still fire on BOTH paths -- that is what it is for
        for md in (guard, raised):
            self.assertIn('boardA', md)

    def test_only_the_rerun_spec_sees_the_synthetic_rows(self):
        """accumulate_report gets `mret` alone -- worker rows, always 4th field a real
        list. Widening _abort_report to hand it the synthetic list too would crash the
        abort path: those rows carry rows=None and render_matrix iterates it."""
        import ast
        src = (Path(TEST_DIR).parents[0] / 'hil_test.py').read_text()
        fn = next(n for n in ast.walk(ast.parse(src))
                  if isinstance(n, ast.FunctionDef) and n.name == '_abort_report')
        calls = {ast.unparse(n.func): ast.unparse(n)
                 for n in ast.walk(fn) if isinstance(n, ast.Call)
                 and ast.unparse(n.func).endswith(('_write_failed_spec',
                                                   'accumulate_report'))}
        self.assertEqual(
            ast.unparse(ast.parse(calls['hil_report.accumulate_report']).body[0]
                        ).split('(', 1)[1].split(',')[0], 'mret',
            'accumulate_report must receive worker rows only -- the synthetic rows carry '
            'rows=None and render_matrix iterates that field')
        self.assertIn('stuck', calls['_write_failed_spec'],
                      'the re-run spec must still name the boards that never reported')


class UsbtestAbsentDeviceVerdict(unittest.TestCase):
    """The arm that fails BEFORE usbtest_permit: an absent device must not queue on the
    battery mutex for minutes just to have usbtest.py report "no device", and the cell
    needs the 0/30 denominator or the row reads as a bare failure."""

    def setUp(self):
        self.addCleanup(setattr, hil_test, 'board_wedged', hil_test.board_wedged)
        hil_test.board_wedged = ''
        no_settle(self)
        from helper import hil_lock, hil_util
        self.addCleanup(setattr, hil_util, 'usb_scan', hil_util.usb_scan)
        hil_util.usb_scan = lambda **k: []          # a readable bus, no such device
        self.addCleanup(setattr, hil_test, '_enum_timeout', hil_test._enum_timeout)
        hil_test._enum_timeout = 0
        self.addCleanup(setattr, hil_lock, 'usbtest_permit', hil_lock.usbtest_permit)
        from contextlib import contextmanager

        def boom(uid):
            raise AssertionError('took the battery permit for an absent device')
            yield
        hil_lock.usbtest_permit = contextmanager(boom)

    def test_a_readable_bus_without_the_device_says_absent_with_a_denominator(self):
        with self.assertRaises(hil_test.TestFail) as cm:
            hil_test.test_device_usbtest({'name': 'b', 'uid': 'NOPE',
                                          'flasher': {'name': 'stlink', 'uid': 'X'}})
        self.assertIn('no cafe:4010 device', str(cm.exception))
        self.assertIn('0/30', cm.exception.metric)

class PermitReleasesOnlyWhatItTook(unittest.TestCase):
    """The bounded acquire skips a slot it could not get ('proceeding over-subscribed') and
    deliberately leaves it out of `taken`, but __exit__ released every slot in self.slots.
    multiprocessing.Semaphore is unbounded, so each timeout permanently widened that
    controller's permit -- the throttle this branch NARROWED (FLASH_PARALLEL 8->4,
    USBTEST_PARALLEL 4->2) for xHCI bandwidth margin."""

    def test_a_timed_out_slot_is_not_released_on_exit(self):
        from helper import hil_lock
        import multiprocessing

        sems = [multiprocessing.Semaphore(1)]
        sems[0].acquire()                      # width 1, already held: the next wait times out
        self.addCleanup(setattr, hil_lock, 'PERMIT_TIMEOUT', hil_lock.PERMIT_TIMEOUT)
        hil_lock.PERMIT_TIMEOUT = 0.1

        permit = hil_lock.controller_permit(sems, 'UID')
        permit.slots = [0]
        with permit:
            pass

        # one holder still holds it, so a correct exit leaves it unavailable
        self.assertFalse(sems[0].acquire(timeout=0.1),
                         'the permit released a slot it never acquired: width grew')


class SudoSoftNeverRaises(unittest.TestCase):
    """Two of its four call sites are inside run_case's timeout handler, where ANY raise
    costs the HUNG verdict, the recovery and the JSON report -- and sudo() sys.exit()s on
    'a password is required', which is a raise like any other."""

    def setUp(self):
        import usbtest
        self.u = usbtest
        self.addCleanup(setattr, usbtest, 'sudo', usbtest.sudo)

    def _check(self, exc):
        def boom(*a, **k):
            raise exc
        self.u.sudo = boom
        r = self.u._sudo_soft(['dmesg'])            # must not propagate
        self.assertEqual(r.returncode, 1)
        self.assertIn(f'dmesg: {type(exc).__name__}: {exc}', r.stderr)   # callers quote it

    def test_systemexit_from_a_password_prompt_is_contained(self):
        self._check(SystemExit('sudo needs a password'))

    def test_oserror_is_contained(self):
        self._check(OSError('no such binary'))

    def test_subprocess_error_is_contained(self):
        self._check(subprocess.SubprocessError('timed out'))


class UsbtestNeverCleansUp(unittest.TestCase):
    """main's finally writes nothing: remove_id makes the next registration wait on peers'
    in-flight cases, and a driver-wide unbind cut peer batteries and has wedged a host xHCI
    through the uninterruptible device_lock. The id and bindings stay for every run."""

    def test_the_finally_block_writes_nothing(self):
        import ast
        import usbtest
        tree = ast.parse(Path(usbtest.__file__).read_text())
        main = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == 'main')
        fins = [n for n in ast.walk(main) if isinstance(n, ast.Try) and n.finalbody]
        self.assertTrue(fins, 'main lost its finally; retarget this test')
        for fin in fins:
            body = ast.unparse(ast.Module(body=fin.finalbody, type_ignores=[]))
            # every registry/bind write goes through sysfs_write
            self.assertNotIn('sysfs_write', body, "main's finally writes to sysfs")


class SysfsWriteTimeoutNamesTheUncertainty(unittest.TestCase):
    def test_a_blocked_write_is_contention_or_a_wedge_and_stays_bounded(self):
        import usbtest
        seen = {}

        def blocked(cmd, **kw):
            seen.update(kw)
            raise subprocess.TimeoutExpired(cmd, kw.get('timeout'))
        self.addCleanup(setattr, usbtest, 'sudo', usbtest.sudo)
        usbtest.sudo = blocked
        with self.assertRaises(SystemExit) as cm:
            usbtest.sysfs_write('/sys/bus/usb/drivers/usbtest/new_id', 'cafe 4010 0 0525 a4a0')
        self.assertEqual(seen.get('timeout'), 15)
        msg = str(cm.exception)
        self.assertIn('possible device-lock contention or a wedged device', msg)
        self.assertNotIn('USB subsystem is wedged', msg)

if __name__ == '__main__':
    unittest.main()
