#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# The pre-flash sweep for a testusb a cancelled or SIGKILLed run left in D state on a board's
# usbfs node (#4126): /proc, sysfs and the udev database are temp trees, flashers are fakes.
# Run directly:
#   python3 test/hil/test/test_hil_wedge_sweep.py
import io
import os
import sys
import tempfile
import unittest
from contextlib import contextmanager, redirect_stdout
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import usbtest_harness   # noqa: E402 - stubs pyserial before hil_test imports it
import hil_flash         # noqa: E402
import hil_test          # noqa: E402
import usbtest           # noqa: E402
from helper import hil_lock, hil_util   # noqa: E402

NODE = '/dev/bus/usb/003/007'
TESTUSB_ARGV = ['testusb', '-A', NODE, '-D', NODE, '-t', '11', '-c', '256']
RESET_BOARD = {'name': 'b', 'uid': 'ABC123', 'flasher': {'name': 'jlink', 'uid': 'P'},
               'flasher_recover': {'name': 'openocd', 'args': '-f interface/jlink.cfg'}}


class FakeRig:
    """/proc, /sys/bus/usb/devices and /run/udev/data under one temp dir."""

    def __init__(self, test):
        tmp = tempfile.TemporaryDirectory()
        test.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        for name, attr in (('proc', 'PROC_ROOT'), ('sys', 'SYS_USB_DEVICES'), ('udev', 'UDEV_DATA')):
            (self.root / name).mkdir()
            usbtest_harness.patch(test, hil_util, attr, str(self.root / name))

    def process(self, pid, comm='testusb', state='D', argv=TESTUSB_ARGV, start='4242'):
        d = self.root / 'proc' / str(pid)
        d.mkdir(exist_ok=True)
        # fields 4..21 are filler; starttime is field 22
        (d / 'stat').write_text(f'{pid} ({comm}) {state} ' + ' '.join(['0'] * 18) + f' {start} 0 0\n')
        (d / 'cmdline').write_bytes(b'\0'.join(a.encode() for a in argv) + b'\0')

    def reap(self, pid):
        d = self.root / 'proc' / str(pid)
        for f in d.iterdir():
            f.unlink()
        d.rmdir()

    def device(self, busport='3-1.2', bus=3, dev=7, serial='ABC123', minor=262, udev_serial=None):
        d = self.root / 'sys' / busport
        d.mkdir()
        (d / 'busnum').write_text(f'{bus}\n')
        (d / 'devnum').write_text(f'{dev}\n')
        (d / 'dev').write_text(f'189:{minor}\n')
        if serial is not None:
            (d / 'serial').write_text(f'{serial}\n')
        if udev_serial is not None:
            (self.root / 'udev' / f'c189:{minor}').write_text(
                f'I:1\nE:ID_SERIAL=X_{udev_serial}\nE:ID_SERIAL_SHORT={udev_serial}\n')
        (self.root / 'sys' / f'{busport}:1.0').mkdir()   # an interface dir, never a match
        return str(d)


class HolderLookup(unittest.TestCase):
    def setUp(self):
        self.rig = FakeRig(self)

    def test_only_a_dstate_testusb_with_a_node_is_a_holder(self):
        self.rig.process(10)
        self.rig.process(11, state='S')                    # a case in flight between ioctls
        self.rig.process(12, comm='sudo', argv=['sudo', '-n'] + TESTUSB_ARGV)
        self.rig.process(13, argv=['testusb', '-a'])       # no -D node
        self.rig.process(14, comm='a) b', argv=TESTUSB_ARGV)
        self.assertEqual(hil_util.dstate_holders('testusb'),
                         [{'pid': 10, 'start': '4242', 'node': NODE}])

    def test_comm_with_a_paren_parses(self):
        self.rig.process(20, comm='testusb) D x')
        self.assertEqual(hil_util.dstate_holders('testusb) D x')[0]['pid'], 20)

    def test_reaped_zombie_and_recycled_pids_no_longer_hold(self):
        self.rig.process(30)
        h = hil_util.dstate_holders('testusb')[0]
        self.assertTrue(hil_util.proc_holds(h))
        self.rig.process(30, state='Z')
        self.assertFalse(hil_util.proc_holds(h), 'a zombie has closed the node')
        self.rig.process(30, start='9999')
        self.assertFalse(hil_util.proc_holds(h), 'a recycled pid is another process')
        self.rig.reap(30)
        self.assertFalse(hil_util.proc_holds(h))

    def test_node_maps_to_the_device_by_busnum_and_devnum(self):
        self.rig.device(busport='3-1.1', dev=6)
        d = self.rig.device()
        self.assertEqual(hil_util.usb_dev_dir(NODE), d)
        self.assertIsNone(hil_util.usb_dev_dir('/dev/bus/usb/003/099'))
        self.assertIsNone(hil_util.usb_dev_dir('/dev/null'))

    def test_the_udev_record_spares_the_locked_serial_read(self):
        d = self.rig.device(serial=None, udev_serial='ABC123')
        reads = []
        usbtest_harness.patch(self, hil_util, 'read_sysfs',
                              lambda path, timeout=0: reads.append(path))
        self.assertEqual(hil_util.usb_dev_serial(d), 'ABC123')
        self.assertEqual(reads, [], 'a held device would strand this read for its grace')

    def test_without_a_udev_record_the_bounded_read_decides(self):
        d = self.rig.device(serial='ABC123')
        self.assertEqual(hil_util.usb_dev_serial(d), 'ABC123')
        d2 = self.rig.device(busport='3-2', dev=8, minor=263, serial=None)
        self.assertIsNone(hil_util.usb_dev_serial(d2), 'unidentified, not a guess')


class StraysOnBoard(unittest.TestCase):
    def setUp(self):
        self.rig = FakeRig(self)

    def test_matches_this_boards_node_only(self):
        self.rig.device(serial=None, udev_serial='abc123')
        self.rig.device(busport='3-2', dev=8, minor=263, serial='OTHER')
        self.rig.process(40)
        self.rig.process(41, argv=['testusb', '-D', '/dev/bus/usb/003/008', '-t', '1'])
        self.assertEqual([s['pid'] for s in usbtest.strays_on('ABC123')], [40])
        self.assertEqual([s['pid'] for s in usbtest.strays_on('other')], [41])
        self.assertEqual(usbtest.strays_on('NOPE'), [])

    def test_a_holder_of_a_gone_device_matches_nothing(self):
        self.rig.process(42)
        self.assertEqual(usbtest.strays_on('ABC123'), [])


class RecoverStrays(unittest.TestCase):
    def setUp(self):
        self.rig = FakeRig(self)
        usbtest_harness.patch(self, usbtest, 'RECOVER_SETTLE', 0)
        usbtest_harness.patch(self, usbtest, 'RECOVER_REAP', 0.2)
        self.rig.process(50)
        self.strays = hil_util.dstate_holders('testusb')
        self.resets = []

    def recover(self, board):
        with redirect_stdout(io.StringIO()):
            return usbtest.recover_strays(board, self.strays)

    def fake_reset(self, frees):
        def reset(board, timeout=None):
            self.resets.append((board['flasher'], timeout))
            if frees:
                self.rig.reap(50)
        usbtest_harness.patch(self, hil_flash, 'reset_primitive', lambda name: reset)

    def test_reset_through_the_recovery_flasher_frees_the_node(self):
        self.fake_reset(frees=True)
        self.assertEqual(self.recover(RESET_BOARD), '')
        flasher, timeout = self.resets[0]
        self.assertEqual(flasher['name'], 'openocd', 'the convoy-safe flasher_recover, not JLinkExe')
        self.assertEqual(flasher['uid'], 'P', "flasher_recover drives the primary's probe")
        self.assertEqual(timeout, usbtest.RECOVER_RESET_TIMEOUT)

    def test_a_reset_that_leaves_the_holder_reports_the_board_wedged(self):
        self.fake_reset(frees=False)
        why = self.recover(RESET_BOARD)
        self.assertIn('still in D state', why)
        self.assertIn('pid 50', why)

    def test_a_raising_reset_still_gets_its_reap_check(self):
        def boom(board, timeout=None):
            raise RuntimeError('probe gone')
        usbtest_harness.patch(self, hil_flash, 'reset_primitive', lambda name: boom)
        self.assertIn('still in D state', self.recover(RESET_BOARD))

    def test_no_convoy_safe_flasher_never_resets(self):
        self.fake_reset(frees=True)
        board = {'name': 'b', 'uid': 'ABC123', 'flasher': {'name': 'jlink', 'uid': 'P'}}
        self.assertIn('cannot deliver', self.recover(board))
        self.assertEqual(self.resets, [], 'JLinkExe would block on the held node and add a stray')

    def test_a_convoy_safe_flasher_without_reset_leaves_it_to_the_flash(self):
        board = {'name': 'b', 'uid': 'ABC123', 'flasher': {'name': 'esptool', 'uid': 'P'}}
        self.assertEqual(self.recover(board), '')


class TestBoardSweep(unittest.TestCase):
    """test_board runs the sweep before any flash, and a board it cannot free is skipped
    and counted rather than flashed into its held node."""

    def setUp(self):
        self.addCleanup(setattr, hil_test, 'board_wedged', hil_test.board_wedged)
        p = lambda obj, name, value: usbtest_harness.patch(self, obj, name, value)  # noqa: E731
        p(hil_test, 'skip_flash', False)
        p(hil_test, 'shuffle_seed', None)
        p(hil_test, 'test_only', [])
        p(hil_test, 'board_test', {})
        p(hil_test, 'log_line', lambda msg: None)
        p(hil_lock, 'acquire_board_lock', lambda name: open(os.devnull))
        p(hil_lock, 'clear_record', lambda fh: None)
        p(hil_lock, 'flash_permit', contextmanager(lambda uid: iter([None])))
        self.ran = []
        p(hil_test, 'test_example', lambda board, v, ex: self.ran.append(ex) or (0, 'pass', None))
        p(usbtest, 'strays_on', lambda serial: [{'pid': 1, 'start': '1', 'node': NODE}])
        self.board = {'name': 'b', 'uid': 'ABC123', 'flasher': {'name': 'jlink', 'uid': 'P'},
                      'tests': {'only': ['device/cdc_msc']}}

    def test_an_unfreed_board_is_skipped_and_counted(self):
        usbtest_harness.patch(self, usbtest, 'recover_strays', lambda board, strays: 'b: held')
        name, err, failed, rows, _ = hil_test.test_board(self.board)
        self.assertEqual(err, 1, 'a board left wedged must fail the run')
        self.assertEqual(self.ran, [], 'no test and no park may flash into the held node')
        self.assertIn(hil_test.hil_report.WEDGED_CELL, rows[0][1])
        self.assertEqual(failed, [], 'the whole board re-runs')

    def test_a_freed_board_runs_as_usual(self):
        usbtest_harness.patch(self, usbtest, 'recover_strays', lambda board, strays: '')
        _, err, _, _, _ = hil_test.test_board(self.board)
        self.assertEqual(err, 0)
        self.assertEqual(self.ran, ['device/cdc_msc', 'device/board_test'])

    def test_skip_flash_runs_no_sweep(self):
        hil_test.skip_flash = True
        usbtest_harness.patch(self, usbtest, 'strays_on', lambda serial: self.fail('swept'))
        hil_test.test_board(self.board)


if __name__ == '__main__':
    unittest.main()
