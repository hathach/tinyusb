#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# The farm check's tinyusb side: helper/farm_profile.py's verdicts and helper/hil_farm_check.py's
# wiring to agentrc's engine, with the bus, the flasher and the tracked runner faked and the
# engine's lock dir in a temp dir: no board, no ssh. Run directly:
#   python3 test/hil/test/test_hil_farm_check.py
import json
import os
import subprocess
import sys
import tempfile
import types
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from usbtest_harness import patch                  # noqa: E402 - stubs pyserial
import hil_flash                                   # noqa: E402
from helper import farm_profile, hil_farm_check, hil_lock, hil_util   # noqa: E402

BOARD = {'name': 'b', 'uid': 'U', 'flasher': {'name': 'openocd', 'uid': 'P'}}
STLINK = dict(BOARD, flasher={'name': 'stlink', 'uid': 'P', 'args': ''})
CONTACT = 'Device ID   : 0x413\n'
eng = hil_farm_check.load_engine()


def args(**kw):
    return types.SimpleNamespace(**{'verbose': False, **kw})


def op(rc=0, stdout='', stderr=None):
    return types.SimpleNamespace(returncode=rc, stdout=stdout, stderr=stderr)


def context(test, *cleanups, stdout=''):
    """The engine's own Context for BOARD, run_tracked faked: each run prints `stdout` and ends
    with the next of `cleanups` ('done' once they run out), and a recheck finds it unchanged.
    `calls` lists the runs."""
    queue, ends, calls = list(cleanups), {}, []

    def tracked(cmd, cwd=None, *, timeout, settle, state_dir, **kw):
        calls.append((cmd, cwd, timeout, kw))
        cleanup = queue.pop(0) if queue else 'done'
        r = subprocess.CompletedProcess(cmd, 0, stdout, None)
        r.cleanup, r.receipt = cleanup, None
        if cleanup != 'done':
            r.receipt = {'token': str(len(calls)), 'lock': '/nonexistent'}
            ends[r.receipt['token']] = cleanup
        return r
    patch(test, eng.hil_process, 'run_tracked', tracked)
    patch(test, eng.hil_process, 'recheck', lambda receipt: ends[receipt['token']])
    patch(test, eng, 'RECHECK_WAIT', 0)
    ctx = eng.Context(args(), '/nonexistent')
    ctx.calls = calls
    return ctx


def capture(fn):
    """(fn's return, stdout, stderr) at the fd level: the engine moves fd 1 under --json."""
    with tempfile.TemporaryFile() as out, tempfile.TemporaryFile() as err:
        sys.stdout.flush()
        sys.stderr.flush()
        saved = os.dup(1), os.dup(2)
        streams = sys.stdout, sys.stderr
        os.dup2(out.fileno(), 1)
        os.dup2(err.fileno(), 2)
        # onto the fds, whatever a test runner put in sys.stdout/stderr
        sys.stdout, sys.stderr = (open(fd, 'w', encoding='utf-8', closefd=False) for fd in (1, 2))
        try:
            code = fn()
        finally:
            sys.stdout.flush()
            sys.stderr.flush()
            sys.stdout, sys.stderr = streams
            os.dup2(saved[0], 1)
            os.dup2(saved[1], 2)
            os.close(saved[0])
            os.close(saved[1])
        out.seek(0)
        err.seek(0)
        return code, out.read().decode(), err.read().decode()


class Roster(unittest.TestCase):
    def test_a_rig_host_gets_its_roster(self):
        got = farm_profile.roster('ci', None, args())
        # from test/hil the bare name resolves against the cwd first: the same file either way
        self.assertEqual(Path(got['path']).resolve(), hil_util.TINYUSB_ROOT / 'test/hil/tinyusb.json')
        self.assertTrue(got['boards'])
        self.assertIsInstance(got['named_only'], list)

    def test_an_unknown_host_without_a_roster_is_refused(self):
        # no local.json fallback: a dev PC names its own
        with self.assertRaises(LookupError):
            farm_profile.roster('htpc', None, args())

    def test_an_explicit_roster_is_used_on_any_host(self):
        with tempfile.TemporaryDirectory() as td:
            cfg = Path(td, 'r.json')
            cfg.write_text(json.dumps({'boards': [BOARD], 'boards-skip': [dict(BOARD, name='parked')]}))
            got = farm_profile.roster('htpc', str(cfg), args())
        self.assertEqual((got['path'], [b['name'] for b in got['boards']], [b['name'] for b in got['named_only']]),
                         (str(cfg), ['b'], ['parked']))

    def test_the_probe_is_named_by_its_flasher(self):
        self.assertEqual(farm_profile.probe_name(BOARD), 'openocd')


class Probe(unittest.TestCase):
    def bus(self, devices):
        patch(self, farm_profile, 'scan_usb', lambda: {bp: {'serial': s, 'vidpid': vp} for bp, s, vp in devices})

    def candidates(self, uid):
        return farm_profile.probe_candidates(dict(BOARD, flasher={'name': 'jlink', 'uid': uid}))

    def test_a_cafe_device_sharing_the_probe_serial_is_never_the_probe(self):
        # an Espressif USB-Serial-JTAG and its DUT share the MAC serial
        self.bus([('1-1', 'p', '303a:1001'), ('1-2', 'p', 'cafe:4001')])
        self.assertEqual(self.candidates('P'), ['1-1'])
        self.bus([('1-2', 'p', 'cafe:4001')])
        self.assertEqual(self.candidates('P'), [])

    def test_every_exact_match_is_returned_for_the_engine_to_refuse(self):
        self.bus([('1-2', '123', '1366:0101'), ('1-1', '123', '1366:0101'), ('1-3', '000123', '1366:0101')])
        self.assertEqual(self.candidates('123'), ['1-1', '1-2'])

    def test_a_zero_padded_jlink_serial_matches_only_without_an_exact_one(self):
        self.bus([('1-1', '000681295394', '1366:0101'), ('1-2', '0681295394', '1366:0101')])
        self.assertEqual(self.candidates('681295394'), ['1-1', '1-2'])
        self.assertEqual(self.candidates('0681295394'), ['1-2'])

    def test_an_all_zero_uid_never_matches_by_padding(self):
        self.bus([('1-1', '000000', '1366:0101'), ('1-2', '123456', '1366:0101')])
        self.assertEqual(self.candidates('0'), [])

    def test_a_placeholder_serial_is_matched_exactly_before_by_padding(self):
        # metro_m4_express's probe legitimately reports the placeholder '123456'
        self.bus([('1-1', '123456', '1366:0101'), ('1-2', '000123456', '1366:0101'), ('1-3', '000000', '1366:0101')])
        self.assertEqual(self.candidates('123456'), ['1-1'])
        self.assertEqual(self.candidates('000123456'), ['1-2'])
        self.assertEqual(self.candidates('0123456'), ['1-1', '1-2'])
        self.assertEqual(self.candidates('000'), [])


class Reset(unittest.TestCase):
    def test_a_reset_with_the_targets_answer_returns_its_operation(self):
        ctx = context(self, stdout=CONTACT)
        got = farm_profile.reset(ctx, STLINK)
        (cmd, _, timeout, kw), = ctx.calls
        self.assertEqual((cmd, timeout, kw['verbose']),
                         (['STM32_Programmer_CLI', '--connect', 'port=swd', 'sn=P', '--rst', '--go'],
                          hil_util.CMD_TIMEOUT, False))
        self.assertEqual((got.returncode, got.cleanup, len(got.attempts)), (0, 'done', 1))   # a provider Operation

    def test_a_reset_without_the_targets_answer_raises_with_its_operation(self):
        ctx = context(self, stdout='ST-LINK SN  : P\n')
        with self.assertRaises(RuntimeError) as cm:
            farm_profile.reset(ctx, STLINK)
        self.assertEqual(cm.exception.operation.returncode, 0)
        self.assertEqual(len(ctx.calls), 1)

    def test_a_failed_reset_is_returned_for_the_engine_to_fail(self):
        patch(self, hil_flash, 'reset_primitive', lambda name: lambda board, run: op(rc=1, stdout='Error: no target'))
        self.assertEqual(farm_profile.reset(context(self), STLINK).returncode, 1)

    def test_lm4flash_has_no_reset_and_starts_nothing(self):
        ctx = context(self)
        self.assertIsNone(farm_profile.reset(ctx, dict(BOARD, flasher={'name': 'lm4flash', 'uid': 'P'})))
        self.assertEqual(ctx.calls, [])

    def test_esptool_resets_through_the_providers_rom_loader_on_its_port(self):
        seen = []

        def reset_esptool(flasher, *, port, timeout, run):
            seen.append((flasher['uid'], port, timeout))
            return run(['esptool', '-p', port, 'chip_id'], timeout=timeout)
        real = hil_util.provider()
        fake = types.SimpleNamespace(reset_esptool=reset_esptool, reset_problem=real.reset_problem,
                                     hil_process=real.hil_process)
        patch(self, hil_util, 'provider', lambda: fake)
        patch(self, hil_util, 'get_serial_dev', lambda uid, *a: f'/dev/serial/by-id/usb-x_{uid}-if00')
        ctx = context(self, stdout='Hard resetting via RTS pin...\n')
        got = farm_profile.reset(ctx, dict(BOARD, flasher={'name': 'esptool', 'uid': 'E', 'args': ''}))
        self.assertEqual(seen, [('E', '/dev/serial/by-id/usb-x_E-if00', hil_util.CMD_TIMEOUT)])
        self.assertEqual((got.returncode, len(ctx.calls)), (0, 1))
        self.assertIsNone(hil_flash.reset_primitive('esptool'), "hil_test's console callers keep no esptool reset")


class RunPlumbing(unittest.TestCase):
    """tinyusb's hil_flash adapters take a keyword-only run=, which the profile fills with ctx.run;
    without one they keep hil_util.run_cmd, read per call."""

    def test_every_adapter_but_openocd_esp_hands_its_run_to_the_provider(self):
        calls = []

        class Provider:
            def __getattr__(self, name):
                return lambda *a, **kw: calls.append((name, kw['run']))
        patch(self, hil_util, 'provider', Provider)
        patch(self, hil_util, 'get_serial_dev', lambda *a: '/dev/ttyX')
        patch(self, hil_flash, '_idf_target', lambda fw_dir: 'esp32s3')
        mine = object()
        board = {'name': 'b', 'flasher': {'name': 'x', 'uid': 'P', 'args': ''}}
        with tempfile.TemporaryDirectory() as td:
            (Path(td) / 'flash_args').write_text('0x0 fw.bin')
            for name in ('jlink', 'stlink', 'openocd', 'lm4flash', 'pyocd', 'esptool'):
                for fn in (hil_flash.flash_primitive(name), hil_flash.reset_primitive(name)):
                    if fn is None:
                        continue
                    a = (str(Path(td) / 'fw.bin'),) if fn.__name__.startswith('flash_') else ()
                    calls.clear()
                    fn(board, *a, run=mine)
                    fn(board, *a)
                    self.assertEqual(calls, [(fn.__name__, mine), (fn.__name__, hil_util.run_cmd)], fn.__name__)
        # openocd_esp's server is the provider's session: run= reaches only its esptool children (test_hil_flash)


class Launcher(unittest.TestCase):
    def test_the_engine_comes_from_lib_agentrc_at_the_pin(self):
        self.assertEqual(Path(eng.__file__), hil_util.TINYUSB_ROOT / 'lib/agentrc/skills/hil/scripts/hil_farm_check.py')
        self.assertEqual(eng.REASON, 'farm_check')
        self.assertIsNone(eng._lock_api_problem(hil_lock))

    def test_the_profile_and_lock_module_are_passed_and_the_exit_status_passed_through(self):
        for code in (0, 1, 2, 4):
            calls = []
            fake = types.SimpleNamespace(main=lambda profile, argv, lock_module: calls.append(
                (profile, argv, lock_module)) or code)
            patch(self, hil_farm_check, 'load_engine', lambda: fake)
            self.assertEqual(hil_farm_check.main(['-b', 'x']), code)
            self.assertEqual(calls, [(farm_profile, ['-b', 'x'], hil_lock)])

    def test_an_engine_that_cannot_load_refuses(self):
        def missing():
            raise ImportError('lib/agentrc must be at X')
        patch(self, hil_farm_check, 'load_engine', missing)
        code, out, err = capture(lambda: hil_farm_check.main([]))
        self.assertEqual((code, out), (2, ''))
        self.assertIn('lib/agentrc must be at X', err)


class EngineFixture(unittest.TestCase):
    """The real engine with this profile and tinyusb's own hil_lock, its lock dir and the state
    dir in a temp dir, the bus faked."""

    def setUp(self):
        td = tempfile.TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.d = Path(td.name)
        self.state = self.d / 'state'
        self.state.mkdir(mode=0o700)
        self.cfg = self.d / 'roster.json'
        self.roster(STLINK)
        patch(self, hil_lock, 'BOARD_LOCK_DIR', str(self.d / 'locks'))
        patch(self, eng, 'RECHECK_WAIT', 0)
        patch(self, farm_profile, 'scan_usb', lambda: {'1-1': {'serial': 'p', 'vidpid': '0483:374b'}})

    def roster(self, board):
        self.cfg.write_text(json.dumps({'boards': [board]}))

    def run_engine(self, *argv, host='htpc'):
        code, out, _ = capture(lambda: eng.main(farm_profile, ['--json', *argv, str(self.cfg)],
                                                state_dir=str(self.state), host=host, lock_module=hil_lock))
        return code, json.loads(out)


class EngineWithProfile(EngineFixture):
    """run_tracked faked: each run prints self.out and exits self.rc."""

    def setUp(self):
        super().setUp()
        patch(self, eng.hil_process, 'run_tracked', self.tracked)
        self.ran, self.out, self.rc = [], CONTACT, 0

    def tracked(self, cmd, cwd=None, *, timeout, settle, state_dir, **kw):
        self.assertEqual(state_dir, str(self.state))
        self.ran.append(cmd[0])
        r = subprocess.CompletedProcess(cmd, self.rc, self.out, None)
        r.cleanup, r.receipt = 'done', None
        return r

    def test_a_board_its_probe_resets_is_ok_and_released(self):
        code, doc = self.run_engine()
        r, = doc['rows']
        self.assertEqual((code, r['status'], r['probe'], r['location'], r['coverage'], self.ran),
                         (0, 'ok', '✅ stlink', '1-1', 'reset-attempted', ['STM32_Programmer_CLI']))
        self.assertEqual((len(r['operations']), r['cleanup']['result'], r['cleanup']['released']), (1, 'released', True))
        self.assertIsNone(hil_lock.read_record('b'))   # no record: free

    def test_a_reset_that_shows_no_target_fails_and_still_releases(self):
        for out, rc, note in (('ST-LINK SN  : P\n', 0, 'no line shows the target answering'),
                              ('Error: No STM32 target found!\n', 1, 'reset rc=1: Error: No STM32 target found!')):
            with self.subTest(rc=rc):
                self.out, self.rc = out, rc
                code, doc = self.run_engine()
                r, = doc['rows']
                self.assertEqual((code, r['status'], len(r['operations']), r['cleanup']['released']),
                                 (1, 'failed', 1, True))
                self.assertIn(note, r['note'][-1])

    def test_an_lm4flash_board_is_unsupported_and_untouched(self):
        self.roster(dict(BOARD, flasher={'name': 'lm4flash', 'uid': 'P', 'args': ''}))
        code, doc = self.run_engine()
        r, = doc['rows']
        self.assertEqual((code, r['status'], r['coverage'], self.ran), (0, 'unsupported', 'unsupported', []))
        self.assertIsNone(hil_lock.read_record('b'))

    def test_a_held_board_is_locked_with_its_probe_shown(self):
        fh = hil_lock.take_board_lock('b', 'hil_test.py')
        self.addCleanup(fh.close)
        code, doc = self.run_engine()
        r, = doc['rows']
        self.assertEqual((code, r['status'], r['probe'], self.ran), (0, 'locked', '✅ stlink', []))

    def test_a_missing_probe_fails_before_any_lock(self):
        patch(self, farm_profile, 'scan_usb', lambda: {'1-1': {'serial': 'p', 'vidpid': 'cafe:4001'}})
        patch(self, hil_lock, 'take_board_lock', lambda *a, **kw: self.fail('locked'))
        code, doc = self.run_engine()
        r, = doc['rows']
        self.assertEqual((code, r['status'], r['probe'], r['coverage'], self.ran),
                         (1, 'failed', '❌ stlink', 'reset-not-attempted', []))

    def test_an_unknown_host_is_refused_before_any_board(self):
        code, out, err = capture(lambda: eng.main(farm_profile, ['--json'], state_dir=str(self.state), host='htpc',
                                                  lock_module=hil_lock))
        self.assertEqual((code, out, self.ran), (2, '', []))
        self.assertIn('host htpc has no roster here', err)

    def test_the_removed_flags_are_refused(self):
        for flag in ('--scan-only', '--no-park', '-B'):
            with self.subTest(flag=flag), self.assertRaises(SystemExit):
                capture(lambda: eng.main(farm_profile, [flag, 'x', str(self.cfg)], state_dir=str(self.state),
                                         host='htpc', lock_module=hil_lock))
        self.assertEqual(self.ran, [])


class Retain(unittest.TestCase):
    """_retain: an openocd_esp session the provider left running joins the engine's ledger once,
    as a pending run the engine never rechecks, so nothing more starts on the board."""

    def test_a_pending_session_stops_the_reservation(self):
        ctx = context(self)
        patch(self, hil_flash, 'cleanup_pending', {})
        farm_profile._retain(ctx, BOARD)
        tracked = {'token': 't', 'supervisor': {'pid': 1, 'start': '1', 'lock': [1, 2]}, 'lock': '/s/r.lock',
                   'record': '/s/r.json', 'log': '/s/r.log', 'survivors': None}
        hil_flash.cleanup_pending['b'] = (tracked,)       # run_tracked's: ctx.run's own, ledgered there
        farm_profile._retain(ctx, BOARD)
        self.assertEqual(ctx.ledger, [])
        hil_flash.cleanup_pending['b'] = ('/x/receipt.json',)
        farm_profile._retain(ctx, BOARD)
        farm_profile._retain(ctx, BOARD)
        self.assertEqual(ctx.ledger, [{'cmd': 'openocd_esp session, receipt /x/receipt.json', 'cleanup': 'pending',
                                       'receipt': None}])
        with self.assertRaises(eng.Outstanding):
            ctx.run(['reset'], timeout=1)
        self.assertEqual(ctx.calls, [])

    def test_a_sessions_native_record_is_retained_as_openocd_evidence(self):
        # the provider's Session.operation keeps its record when the receipt file could not be written
        ctx = context(self)
        native = {'pid': 4242, 'startTime': '99', 'argv': ['openocd'], 'ports': [None, None, 6666], 'token': 'x',
                  'log': '/w/openocd.log', 'closed': False, 'tool': 'hil_openocd_esp', 'init': 'ok', 'submitted': 3}
        patch(self, hil_flash, 'cleanup_pending', {'b': (native,)})
        farm_profile._retain(ctx, BOARD)
        self.assertEqual(ctx.ledger, [{'cmd': 'openocd_esp session, receipt openocd_esp session pid 4242 start 99, '
                                              'receipt file unwritten, log /w/openocd.log',
                                       'cleanup': 'pending', 'receipt': None}])


class EngineOpenocdEsp(EngineFixture):
    """An openocd_esp board through the engine: a reset session ended `done` leaves the board
    released, and one the provider left running stops the board, whose lock the engine hands to a
    cleanup holder naming the receipt."""
    RECEIPT = '/tmp/hil_openocd_esp_x/receipt.json'

    def setUp(self):
        super().setUp()
        patch(self, eng.hil_process, 'run_tracked', lambda *a, **kw: self.fail('ran a tracked child'))
        self.board = dict(BOARD, flasher={'name': 'openocd_esp', 'uid': '30:ED:A0:E0:C9:02',
                                          'args': '-f board/esp32p4-builtin.cfg'})
        self.roster(self.board)
        patch(self, farm_profile, 'scan_usb', lambda: {'1-4': {'serial': '30:ed:a0:e0:c9:02', 'vidpid': '303a:1001'}})
        env = mock.patch.dict(os.environ)
        env.start()
        self.addCleanup(env.stop)
        real = hil_util.openocd_esp()
        for var in real.CI_JOB_ENV:
            os.environ.pop(var, None)
        patch(self, hil_flash, 'cleanup_pending', {})
        patch(self, hil_flash, 'OPENOCD_ESP_DIR', self.d / 'openocd_esp')
        self.sessions, self.outcomes = [], []
        self.addCleanup(self.end_holder)

        def reset(flasher, *, retains_lock, **kw):
            self.assertIs(retains_lock, True)
            self.sessions.append('reset')
            cleanup = self.outcomes.pop(0) if self.outcomes else 'done'
            if isinstance(cleanup, BaseException):
                cleanup.operation = types.SimpleNamespace(cleanup='pending', receipts=(self.RECEIPT,))
                raise cleanup
            return types.SimpleNamespace(
                returncode=0 if cleanup == 'done' else 1, stdout='', stderr=None, cleanup=cleanup,
                receipts=(self.RECEIPT,) if cleanup == 'pending' else (),
                session={'failure': 'reset run failed', 'log': '/x/openocd.log', 'dir': str(self.d / 'none')})
        fake = types.SimpleNamespace(admit=real.admit, refusal=real.refusal, reset_openocd_esp=reset)
        patch(self, hil_util, 'openocd_esp', lambda: fake)
        self.real = real

    def end_holder(self):
        rec = hil_lock.read_record('b')
        if hil_lock.is_cleanup_holder(rec):
            hil_lock.release_token(rec['token'])

    def test_a_done_session_is_ok_and_released(self):
        code, doc = self.run_engine()
        r, = doc['rows']
        self.assertEqual((code, r['status'], r['cleanup']['result'], r['cleanup']['released'], self.sessions),
                         (0, 'ok', 'released', True, ['reset']))
        self.assertIsNone(hil_lock.read_record('b'))

    def assert_held_by_a_cleanup_holder(self, r):
        self.assertEqual(self.sessions, ['reset'])
        self.assertEqual((r['status'], r['coverage'], r['cleanup']['blocker']), ('blocked', 'blocked-after',
                                                                                 'cleanup-holder'))
        self.assertTrue(any(self.RECEIPT in e for e in r['cleanup']['evidence']), r['cleanup'])
        rec = hil_lock.read_record('b')
        self.assertTrue(hil_lock.is_cleanup_holder(rec), rec)
        self.assertEqual((rec['token'], rec['protected']), (r['cleanup']['token'], True))
        self.assertIn(self.RECEIPT, rec['detail'])

    def test_a_session_left_running_hands_the_lock_to_a_cleanup_holder(self):
        self.outcomes = ['pending']
        code, doc = self.run_engine()
        r, = doc['rows']
        self.assertEqual((code, r['check_status']), (4, 'failed'))
        self.assert_held_by_a_cleanup_holder(r)

    def test_one_left_running_by_a_raising_reset_is_held_the_same_way(self):
        self.outcomes = [RuntimeError('interrupted mid-session')]
        code, doc = self.run_engine()
        r, = doc['rows']
        self.assertEqual(code, 4)
        self.assert_held_by_a_cleanup_holder(r)

    def test_under_a_ci_job_it_is_refused_before_any_session(self):
        os.environ[self.real.CI_JOB_ENV[0]] = '1'
        code, doc = self.run_engine()
        r, = doc['rows']
        self.assertEqual((code, r['status'], r['cleanup']['released'], self.sessions), (1, 'failed', True, []))


class RealProcessChain(EngineFixture):
    """profile -> tinyusb's hil_flash adapter -> agentrc's provider -> the real run_tracked: a
    fake STM32_Programmer_CLI on PATH logs its call and prints the device it reached."""

    def setUp(self):
        super().setUp()
        bin_dir = self.d / 'bin'
        bin_dir.mkdir()
        self.log = self.d / 'reset.log'
        tool = bin_dir / 'STM32_Programmer_CLI'
        tool.write_text(f'#!/bin/sh\necho "$*" >> {self.log}\necho "Device ID   : 0x413"\n')
        tool.chmod(0o755)
        env = mock.patch.dict(os.environ, PATH=f'{bin_dir}{os.pathsep}{os.environ["PATH"]}')
        env.start()
        self.addCleanup(env.stop)

    def test_a_real_reset_process_through_the_tracked_runner_is_ok_and_released(self):
        code, doc = self.run_engine()
        r, = doc['rows']
        self.assertEqual(self.log.read_text().split(), ['--connect', 'port=swd', 'sn=P', '--rst', '--go'])
        self.assertEqual((code, r['status'], r['cleanup']['result'], r['cleanup']['released']), (0, 'ok', 'released', True))
        self.assertEqual([o['cleanup'] for o in r['operations']], ['done'])
        self.assertIsNone(hil_lock.read_record('b'))
        self.assertEqual(sorted(p.name for p in self.state.iterdir()), [])   # proven receipts are tidied


if __name__ == '__main__':
    unittest.main()
