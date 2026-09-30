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
from usbtest_harness import patch               # noqa: E402

BOARD = {'name': 'b', 'uid': 'U', 'flasher': {'name': 'openocd', 'uid': 'P'}}


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
    def run_check(self, vidpid):
        patch(self, hil_pool_check, 'wait_device', lambda *a: ('1-1', vidpid, 2) if vidpid else None)
        row = {'status': 'failed'}
        return hil_pool_check.check_device(BOARD, 1, row), row

    def test_a_re_enumerated_device_passes(self):
        ok, row = self.run_check('cafe:4001')
        self.assertTrue(ok)
        self.assertEqual(row['device'], '✅ cafe:4001')

    def test_no_enumeration_fails(self):
        ok, row = self.run_check(None)
        self.assertFalse(ok)
        self.assertIn('not enumerated', row['device'])


class HostVerdict(unittest.TestCase):
    def alive(self, data):
        patch(self, hil_pool_check, 'check_host_serial', lambda board: data)
        note, row = [], {'status': 'failed'}
        return hil_pool_check.host_alive(BOARD, note, row), note, row

    def test_board_test_output_after_an_example_flash_is_a_silent_no_op(self):
        ok, note, row = self.alive(b'Hello from TinyUSB\r\nU')
        self.assertFalse(ok)
        self.assertEqual(row['status'], 'flash-failed')

    def test_example_output_is_alive(self):
        ok, _, _ = self.alive(b'TinyUSB Host Example\r\n')
        self.assertTrue(ok)

    def test_silence_is_not_alive(self):
        ok, _, _ = self.alive(b'')
        self.assertFalse(ok)


class CheckBoard(unittest.TestCase):
    """check_board up to the flash, with the bus, the lock and the flasher faked."""

    def setUp(self):
        self.flashed = []
        patch(self, hil_pool_check, 'say', lambda msg: None)
        patch(self, hil_pool_check, 'find_usb', lambda uid: '1-1')
        patch(self, hil_pool_check, 'lock_board', lambda name: types.SimpleNamespace())
        patch(self, hil_pool_check, 'unlock_board', lambda fh: None)
        patch(self, hil_pool_check, 'find_device', lambda uid: None)
        patch(self, hil_pool_check, 'flash', lambda board, fw, note: self.flashed.append(fw) or True)
        patch(self, hil_pool_check, 'host_alive', lambda *a: True)
        patch(self, hil_pool_check, 'check_device', lambda *a: True)
        self.args = types.SimpleNamespace(scan_only=False, no_park=False)

    def images(self, *examples):
        patch(self, hil_pool_check.hil_flash, 'find_firmware', lambda variant, ex, **kw: (
            f'{variant}/{ex}.elf' if ex in examples else None))

    def test_a_host_board_under_no_park_parks_nothing(self):
        self.images('host/device_info')
        patch(self, hil_pool_check, 'park_board', lambda *a: self.fail('--no-park parks nothing'))
        self.args.no_park = True
        row = hil_pool_check.check_board(dict(BOARD, tests={'host': True}), self.args)
        self.assertEqual(row['status'], 'ok')

    def test_the_park_image_comes_from_the_light_images_variant(self):
        parked = []
        patch(self, hil_pool_check, 'park_board', lambda board, kind, fw, row, note: parked.append(fw))
        patch(self, hil_pool_check.hil_flash, 'find_firmware', lambda variant, ex, **kw: (
            f'{variant}/{ex}.elf' if variant == 'b-DMA' or ex == 'device/board_test' else None))
        board = dict(BOARD, tests={'device': True}, variant=[{'name': 'b'}, {'name': 'b-DMA'}])
        row = hil_pool_check.check_board(board, self.args)
        self.assertEqual((row['status'], self.flashed, parked),
                         ('ok', ['b-DMA/device/dfu_runtime.elf'], ['b-DMA/device/board_test.elf']))

    def test_no_park_image_fails_before_flashing(self):
        self.images('device/dfu_runtime')
        row = hil_pool_check.check_board(dict(BOARD, tests={'device': True}), self.args)
        self.assertEqual((row['status'], self.flashed), ('flash-failed', []))
        self.assertIn('no board_test firmware', row['note'][-1])

    def test_no_light_image_fails_before_flashing(self):
        self.images('device/board_test')
        row = hil_pool_check.check_board(dict(BOARD, tests={'device': True}), self.args)
        self.assertEqual((row['status'], self.flashed), ('flash-failed', []))

    def test_an_rtt_host_board_is_unsupported_and_untouched(self):
        self.images('host/device_info', 'device/board_test')
        row = hil_pool_check.check_board(dict(BOARD, tests={'host': True}, logger='rtt'), self.args)
        self.assertEqual((row['status'], self.flashed), ('failed', []))
        self.assertIn('RTT host boards unsupported', row['note'])

    def test_an_only_list_board_gets_one_of_its_own_examples(self):
        self.images('device/dfu_runtime', 'device/cdc_dual_ports', 'device/board_test')
        board = dict(BOARD, tests={'only': ['device/cdc_dual_ports']})
        patch(self, hil_pool_check, 'park_board', lambda *a: None)
        hil_pool_check.check_board(board, self.args)
        self.assertEqual(self.flashed, ['b/device/cdc_dual_ports.elf'])


class Park(unittest.TestCase):
    """A park is verified: board_test never enumerates, so the device must leave the bus."""

    def park(self, on_bus_after, flash_rc=0):
        state = {'on_bus': True}
        patch(self, hil_pool_check, 'find_device',
              lambda uid: ('1-1', 'cafe:4001', 1) if state['on_bus'] else None)

        def flash(board, fw):
            state['on_bus'] = on_bus_after
            return types.SimpleNamespace(returncode=flash_rc, stdout='Error: no target')
        patch(self, hil_pool_check.hil_flash, 'flash_primitive', lambda name: flash)
        patch(self, hil_pool_check.time, 'sleep', lambda s: None)
        clock = iter(range(0, 1000, 5))
        patch(self, hil_pool_check.time, 'monotonic', lambda: next(clock))
        note, row = [], {'status': 'ok'}
        hil_pool_check.park_board(BOARD, 'device', 'board_test.elf', row, note)
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
    return hil_pool_check.new_row(name, probe='✅ 1-1', probe_busport='1-1', status=status)


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
