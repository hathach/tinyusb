#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# hil_pool_check's verdicts with the bus, the flasher and the lock dir faked: no board is
# touched. Run directly:
#   python3 test/hil/test/test_hil_pool_check.py
import json
import os
import sys
import tempfile
import types
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from helper import hil_lock, hil_pool_check   # noqa: E402

BOARD = {'name': 'b', 'uid': 'U', 'flasher': {'name': 'openocd', 'uid': 'P'}}


def patch(test, obj, name, value):
    test.addCleanup(setattr, obj, name, getattr(obj, name))
    setattr(obj, name, value)


class Lock(unittest.TestCase):
    def setUp(self):
        td = tempfile.TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.dir = td.name
        patch(self, hil_lock, 'BOARD_LOCK_DIR', self.dir)

    def test_a_held_board_reports_its_holder(self):
        held = hil_lock.flock_nb('b')
        self.addCleanup(held.close)
        hil_lock.write_record(held, 'hil_test.py')
        got = hil_pool_check.lock_board('b')
        self.assertIsInstance(got, str)
        self.assertIn('hil_test.py', got)
        self.assertFalse(got.startswith('ERROR:'))

    def test_an_unopenable_lock_file_is_an_error_not_a_holder(self):
        os.mkdir(os.path.join(self.dir, 'b.lock'))   # open(O_RDWR) on a dir: EISDIR
        got = hil_pool_check.lock_board('b')
        self.assertTrue(got.startswith('ERROR:'), got)

    def test_a_free_board_is_taken_under_the_protected_reason(self):
        fh = hil_pool_check.lock_board('b')
        self.addCleanup(hil_pool_check.unlock_board, fh)
        self.assertEqual(hil_lock.read_record('b')['reason'], 'pool_check')
        self.assertIn('pool_check', hil_lock.PROTECTED_REASONS)


class DeviceVerdict(unittest.TestCase):
    """check_device: the re-enumerated PID decides between a stale build (warn) and a
    silent flash no-op (fail)."""

    def run_check(self, vidpid, built_this_run):
        patch(self, hil_pool_check, 'get_expected_pid', lambda ex: '4001')
        patch(self, hil_pool_check, 'wait_device',
              lambda *a: ('1-1', vidpid, 2) if vidpid else None)
        patch(self, hil_pool_check, '_built', {('b', 'device/x')} if built_this_run else set())
        note, row = [], {'status': 'failed'}
        return hil_pool_check.check_device(BOARD, 'device/x', 'b', 1, note, row), note, row

    def test_the_expected_pid_passes(self):
        ok, note, row = self.run_check('cafe:4001', built_this_run=False)
        self.assertTrue(ok)
        self.assertEqual(note, [])

    def test_a_stale_prebuilt_image_warns(self):
        ok, note, _ = self.run_check('cafe:4010', built_this_run=False)
        self.assertTrue(ok)
        self.assertIn('stale build or silent flash no-op', note[0])

    def test_a_wrong_pid_after_this_runs_build_is_a_silent_no_op(self):
        ok, note, row = self.run_check('cafe:4010', built_this_run=True)
        self.assertFalse(ok)
        self.assertEqual(row['status'], 'flash-failed')
        self.assertIn('silent flash no-op', note[0])

    def test_no_enumeration_fails_without_a_reset_retry(self):
        patch(self, hil_pool_check.hil_flash, 'reset_primitive',
              lambda name: self.fail('the pool check no longer resets a board'))
        ok, _, row = self.run_check(None, built_this_run=False)
        self.assertFalse(ok)
        self.assertIn('not enumerated', row['device'])


class HostVerdict(unittest.TestCase):
    def alive(self, data, flashed_example):
        patch(self, hil_pool_check, 'check_host_serial', lambda board: data)
        note, row = [], {'status': 'failed'}
        return hil_pool_check.host_alive(BOARD, note, row, flashed_example), note, row

    def test_board_test_output_after_an_example_flash_is_a_silent_no_op(self):
        ok, note, row = self.alive(b'Hello from TinyUSB\r\nU', flashed_example=True)
        self.assertFalse(ok)
        self.assertEqual(row['status'], 'flash-failed')

    def test_example_output_is_alive(self):
        ok, _, _ = self.alive(b'TinyUSB Host Example\r\n', flashed_example=True)
        self.assertTrue(ok)

    def test_silence_is_not_alive_and_nothing_is_reflashed(self):
        patch(self, hil_pool_check, 'call_flasher',
              lambda *a: self.fail('the pool check no longer reflashes a silent board'))
        ok, _, _ = self.alive(b'', flashed_example=False)
        self.assertFalse(ok)


class NoPark(unittest.TestCase):
    def test_a_host_board_under_no_park_builds_no_board_test(self):
        host = dict(BOARD, tests={'host': True})
        patch(self, hil_pool_check, 'find_usb', lambda uid: ('1-1', '1d50:6018', 1))
        patch(self, hil_pool_check, 'pick_example',
              lambda *a: ('host/device_info', 'host', 'b', 'device_info.elf'))
        patch(self, hil_pool_check.hil_flash, 'find_firmware', lambda *a, **kw: None)
        patch(self, hil_pool_check, 'build', lambda *a: self.fail('nothing needs a build'))
        patch(self, hil_pool_check, 'lock_board', lambda name: types.SimpleNamespace())
        patch(self, hil_pool_check, 'unlock_board', lambda fh: None)
        patch(self, hil_pool_check, 'find_device', lambda uid, pid: None)
        patch(self, hil_pool_check, 'flash', lambda board, fw, note: True)
        patch(self, hil_pool_check, 'host_alive', lambda *a, **kw: True)
        patch(self, hil_pool_check, 'park_board', lambda *a: self.fail('--no-park parks nothing'))
        args = types.SimpleNamespace(scan_only=False, no_build=False, no_park=True, parked=set())
        self.assertEqual(hil_pool_check.check_board(host, args)['status'], 'ok')


class Build(unittest.TestCase):
    """The build goes through check_build.py; its JSON verdict decides."""

    def setUp(self):
        self.calls = []
        patch(self, hil_pool_check.shutil, 'which', lambda cmd: '/usr/bin/idf.py')

    def run_build(self, board, verdict='', rc=0):
        def run_cmd(cmd, **kw):
            self.calls.append(cmd)
            return types.SimpleNamespace(returncode=rc, stdout=f'log line\n{verdict}\n')
        patch(self, hil_pool_check.hil_util, 'run_cmd', run_cmd)
        note = []
        return hil_pool_check.build(board, ['device/dfu_runtime', 'device/board_test'],
                                    'roster.json', note), note

    def test_it_builds_the_named_images_for_every_roster_variant(self):
        verdict = {'pass': True, 'boards': [
            {'status': 'ok', 'buildDir': 'cmake-build/cmake-build-b', 'okExamples': ['board_test']},
            {'status': 'ok', 'buildDir': 'cmake-build/cmake-build-b-DMA', 'okExamples': []}]}
        built, _ = self.run_build(BOARD, json.dumps(verdict))
        self.assertEqual(built, {'b': {'board_test'}, 'b-DMA': set()})
        cmd = self.calls[0]
        self.assertEqual(cmd[1:], [str(hil_pool_check.CHECK_BUILD), '--board', 'b', '--shared',
                                   '--variants', 'roster.json', '--fetch-deps',
                                   '-e', 'device/dfu_runtime', '-e', 'device/board_test'])

    def test_a_failed_board_names_its_first_error(self):
        verdict = {'pass': False, 'boards': [{'status': 'failed', 'buildDir': 'cmake-build/cmake-build-b',
                                              'firstError': 'undefined reference to foo'}]}
        ok, note = self.run_build(BOARD, json.dumps(verdict), rc=1)
        self.assertIsNone(ok)
        self.assertIn('undefined reference to foo', note[0])

    def test_a_refusal_carries_check_builds_error(self):
        ok, note = self.run_build(BOARD, json.dumps({'error': 'not in roster.json: b'}), rc=2)
        self.assertIsNone(ok)
        self.assertIn('not in roster.json', note[0])

    def test_no_verdict_is_a_failure(self):
        ok, note = self.run_build(BOARD, '', rc=1)
        self.assertIsNone(ok)
        self.assertIn('without a verdict', note[0])

    def test_esp_without_the_idf_env_is_not_built(self):
        patch(self, hil_pool_check.shutil, 'which', lambda cmd: None)
        patch(self, hil_pool_check.os, 'environ', {'IDF_PATH': '/definitely/missing'})
        ok, note = self.run_build(dict(BOARD, flasher={'name': 'esptool', 'uid': 'P'}))
        self.assertIsNone(ok)
        self.assertEqual(self.calls, [])
        self.assertIn('ESP-IDF env missing', note[0])

    def test_esp_sources_export_sh_in_the_build_child_only(self):
        patch(self, hil_pool_check.shutil, 'which', lambda cmd: None)
        with tempfile.TemporaryDirectory(prefix='idf $path ') as td:
            open(os.path.join(td, 'export.sh'), 'w').close()
            patch(self, hil_pool_check.os, 'environ', {'IDF_PATH': td})
            self.run_build(dict(BOARD, flasher={'name': 'esptool', 'uid': 'P'}),
                           json.dumps({'pass': True, 'boards': []}))
        cmd = self.calls[0]
        self.assertEqual(cmd[:2], ['bash', '-c'])
        self.assertTrue(cmd[2].startswith('. "$IDF_PATH/export.sh" >/dev/null && '))
        self.assertIn(' --board b --shared --variants roster.json ', cmd[2])


class FreshFirmware(unittest.TestCase):
    def test_an_image_the_build_skipped_is_not_picked_up(self):
        patch(self, hil_pool_check.hil_flash, 'find_firmware', lambda *a, **kw: 'old.elf')
        board = dict(BOARD, variant=[{'name': 'b'}, {'name': 'b-DMA'}])
        self.assertEqual(hil_pool_check.fresh_firmware(board, 'device/dfu_runtime', {'b': set()}),
                         (None, None))
        self.assertEqual(hil_pool_check.fresh_firmware(board, 'device/dfu_runtime',
                                                       {'b': set(), 'b-DMA': {'dfu_runtime'}}),
                         ('b-DMA', 'old.elf'))


class ParkedBoard(unittest.TestCase):
    def test_a_boards_skip_board_without_firmware_is_not_built(self):
        patch(self, hil_pool_check, 'say', lambda msg: None)
        patch(self, hil_pool_check, 'find_usb', lambda uid: ('1-1', '1366:0105', 1))
        patch(self, hil_pool_check.hil_flash, 'find_firmware', lambda *a, **kw: None)
        patch(self, hil_pool_check, 'build', lambda *a: self.fail('check_build refuses boards-skip'))
        args = types.SimpleNamespace(scan_only=False, no_build=False, no_park=False, parked={'b'})
        row = hil_pool_check.check_board(dict(BOARD, tests={'device': True}), args)
        self.assertEqual(row['status'], 'flash-failed')
        self.assertIn('boards-skip board needs prebuilt firmware', '; '.join(row['note']))


class Park(unittest.TestCase):
    """A park is verified: board_test never enumerates, so the device must leave the bus."""

    def park(self, on_bus_after, flash_rc=0):
        state = {'on_bus': True}
        patch(self, hil_pool_check, 'find_device',
              lambda uid, pid: ('1-1', 'cafe:4001', 1) if state['on_bus'] else None)
        patch(self, hil_pool_check.hil_flash, 'find_firmware', lambda *a, **kw: 'board_test.elf')

        def flash(board, fw):
            state['on_bus'] = on_bus_after
            return types.SimpleNamespace(returncode=flash_rc, stdout='Error: no target')
        patch(self, hil_pool_check.hil_flash, 'flash_primitive', lambda name: flash)
        patch(self, hil_pool_check.time, 'sleep', lambda s: None)
        clock = iter(range(0, 1000, 5))
        patch(self, hil_pool_check.time, 'monotonic', lambda: next(clock))
        note, row = [], {'status': 'ok'}
        hil_pool_check.park_board(BOARD, 'device', row, note)
        return note, row

    def test_a_device_that_leaves_the_bus_is_parked(self):
        note, row = self.park(on_bus_after=False)
        self.assertEqual(row['status'], 'ok')
        self.assertEqual(note, [])

    def test_a_device_still_enumerated_is_a_silent_park(self):
        note, row = self.park(on_bus_after=True)
        self.assertEqual(row['status'], 'flash-failed')
        self.assertIn('park unverified', note[-1])

    def test_a_failed_park_flash_fails_an_ok_row(self):
        note, row = self.park(on_bus_after=True, flash_rc=1)
        self.assertEqual(row['status'], 'flash-failed')
        self.assertIn('park flash failed', note[-1])


def row(name, status):
    return {'name': name, 'probe': '✅ 1-1', 'probe_busport': '1-1', 'flash': '–', 'device': '–',
            'note': [], 'status': status}


class Document(unittest.TestCase):
    def test_coverage_says_what_each_status_covers(self):
        doc = hil_pool_check.pool_document('ci', 'tinyusb.json', False,
                                           [row('a', 'ok'), row('b', 'locked'), row('c', 'failed')])
        self.assertEqual([r['coverage'] for r in doc['rows']],
                         ['full-attempted', 'skipped-locked', 'full-attempted'])
        self.assertEqual(doc['counts'], {'ok': 1, 'flash-failed': 0, 'failed': 1, 'locked': 1})
        self.assertEqual((doc['host'], doc['config'], doc['mode']), ('ci', 'tinyusb.json', 'full'))

    def test_every_scan_only_row_is_probe_only(self):
        doc = hil_pool_check.pool_document('ci', 'x.json', True, [row('a', 'ok'), row('b', 'flash-failed')])
        self.assertEqual({r['coverage'] for r in doc['rows']}, {'probe-only'})
        self.assertEqual(doc['mode'], 'scan-only')

    def test_the_table_is_a_rendering_of_the_document(self):
        doc = hil_pool_check.pool_document('ci', 'x.json', False, [dict(row('a', 'locked'), note=['held'])])
        lines = hil_pool_check.render_table(doc).splitlines()
        self.assertTrue(lines[1].startswith('| Board'))
        self.assertIn('🔒 locked', lines[3])
        self.assertIn('held', lines[3])
        self.assertTrue(lines[-1].startswith('0 ok · 0 flash-failed · 0 failed · 1 locked'))


class Main(unittest.TestCase):
    """main() on a one-board roster with check_board faked; fds 1 and 2 are captured."""

    def setUp(self):
        td = tempfile.TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.cfg = os.path.join(td.name, 'roster.json')
        with open(self.cfg, 'w') as f:
            json.dump({'boards': [dict(BOARD, tests={'device': True})]}, f)
        for obj, name in ((hil_pool_check.hil_flash, 'build_dir'), (hil_pool_check.hil_flash, 'EXTRA_BUILD_DIRS'),
                          (hil_pool_check.hil_util, 'verbose')):
            patch(self, obj, name, getattr(obj, name))

    def noisy_board(self, board, args):
        print('python noise')
        os.write(1, b'child noise\n')
        return row(board['name'], 'failed')

    def run_main(self, *argv):
        patch(self, sys, 'argv', ['hil_pool_check.py', self.cfg, *argv])
        with tempfile.TemporaryFile() as out, tempfile.TemporaryFile() as err:
            sys.stdout.flush()
            sys.stderr.flush()
            saved = os.dup(1), os.dup(2)
            os.dup2(out.fileno(), 1)
            os.dup2(err.fileno(), 2)
            code = None
            try:
                hil_pool_check.main()
            except SystemExit as e:
                code = e.code
            except RuntimeError as e:
                code = e
            finally:
                sys.stdout.flush()
                sys.stderr.flush()
                os.write(1, b'after\n')   # must land where fd 1 pointed before main()
                os.dup2(saved[0], 1)
                os.dup2(saved[1], 2)
                os.close(saved[0])
                os.close(saved[1])
            out.seek(0)
            err.seek(0)
            return code, out.read().decode(), err.read().decode()

    def assert_json_only(self, *argv):
        patch(self, hil_pool_check, 'check_board_safe', self.noisy_board)
        code, out, err = self.run_main('--json', *argv)
        doc_line, after = out.splitlines()
        self.assertEqual(after, 'after')
        doc = json.loads(doc_line)
        self.assertEqual(doc['rows'][0]['name'], 'b')
        self.assertEqual(doc['config'], os.path.realpath(self.cfg))
        self.assertEqual(code, 1)
        self.assertIn('pool check: host', err)
        return err

    def test_json_stdout_carries_only_the_document(self):
        err = self.assert_json_only()
        self.assertIn('child noise', err)

    def test_json_stdout_stays_pure_under_verbose(self):
        err = self.assert_json_only('-v')
        self.assertIn('python noise', err)
        self.assertIn('child noise', err)

    def test_an_exception_restores_stdout(self):
        def boom(*a):
            raise RuntimeError('boom')
        patch(self, hil_pool_check, 'check_pool', boom)
        code, out, _ = self.run_main('--json')
        self.assertIsInstance(code, RuntimeError)
        self.assertEqual(out, 'after\n')

    def test_the_default_output_is_the_table(self):
        patch(self, hil_pool_check, 'check_board_safe', lambda board, args: row(board['name'], 'ok'))
        code, out, _ = self.run_main()
        self.assertEqual(code, 0)
        self.assertIn('| Board', out)
        self.assertIn('1 ok · 0 flash-failed', out)


if __name__ == '__main__':
    unittest.main()
