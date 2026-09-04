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
import os
import stat
import sys
import threading
from multiprocessing import TimeoutError as MpTimeoutError
import time
import types
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

TEST_DIR = os.path.dirname(os.path.abspath(__file__))
# the modules under test live in the parent dir (test/hil), not here
sys.path.insert(0, os.path.dirname(TEST_DIR))

serial_stub = types.ModuleType('serial')
serial_stub.Serial = type('Serial', (), {})
serial_stub.SerialException = type('SerialException', (Exception,), {})
serial_stub.SerialTimeoutException = type('SerialTimeoutException', (Exception,), {})
sys.modules.setdefault('serial', serial_stub)
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

    def test_the_reserve_covers_every_step_of_its_own_ladder(self):
        """Enumerated from the SIDE EFFECTS usbtest performs, so dropping a step from
        recovery_reserve() fails here. Overrun means run_cmd's outer kill lands MID-FLASH
        and orphans the flasher (start_new_session, so killpg misses it) on the probe.
        """
        import usbtest
        from helper import hil_util as _hu
        # each bounded step costs its timeout PLUS run_cmd's post-SIGKILL reap
        flash = usbtest.RECOVER_FLASH_TIMEOUT + _hu.REAP_GRACE
        reset = usbtest.RECOVER_RESET_TIMEOUT + _hu.REAP_GRACE
        fixed = 2 * usbtest.RECOVER_SETTLE + usbtest.RECOVER_OVERHEAD
        rp = {'name': 'openocd', 'args': '-f target/rp2040.cfg'}
        for flasher, steps in (
                # an RP openocd board: reset, reflash, then Rescue-DP POR + one retry
                (rp, reset + flash + 2 * flash + fixed),
                # openocd on a NON-RP target: rescue_openocd has no RESCUE_CFG entry for
                # it, so its two legs are time the board can never spend
                ({'name': 'openocd', 'args': '-f target/wch-riscv.cfg'},
                 reset + flash + fixed),
                # esptool: reset_esptool is a stub (no_op) and rescue refuses a
                # non-openocd flasher, so ONE reflash is all it can ever spend
                ({'name': 'esptool', 'args': ''}, flash + fixed)):
            self.assertEqual(usbtest.recovery_reserve(flasher), steps,
                             f'{flasher} reserves time it cannot spend, or too little')

    def test_the_reserve_leaves_room_for_the_work_no_step_bounds(self):
        """The ladder's step timeouts do not cover the two /proc walks, the roster
        json.loads, the child's first import, or the JSON print. With zero margin any
        env-overridable bound moving up puts the outer killpg inside the reflash."""
        import usbtest
        self.assertGreater(usbtest.RECOVER_OVERHEAD, 0)
        rp = {'name': 'openocd', 'args': '-f target/rp2350.cfg'}
        bounded = (usbtest.RECOVER_RESET_TIMEOUT + 3 * usbtest.RECOVER_FLASH_TIMEOUT)
        self.assertGreaterEqual(usbtest.recovery_reserve(rp) - bounded,
                                usbtest.RECOVER_OVERHEAD,
                                'the reserve equals its own worst case with no margin')

    def test_a_flasher_reserves_nothing_for_a_rescue_it_cannot_run(self):
        """rescue_openocd returns False for anything but openocd, so reserving its two
        legs elsewhere holds a pool worker AND a usbtest permit for 200s of dead time."""
        import usbtest
        self.assertLess(usbtest.recovery_reserve({'name': 'esptool', 'args': ''}),
                        usbtest.recovery_reserve({'name': 'openocd',
                                                  'args': '-f target/rp2040.cfg'}))


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
    def test_every_return_path_is_a_pair(self):
        """main() unpacks `_, nfail = build_board(board)`; a bare int on any path
        (the timeout path did) raises TypeError before the pool exists."""
        import ast
        src = (Path(TEST_DIR).parents[0] / 'hil_test.py').read_text()
        fn = next(n for n in ast.walk(ast.parse(src))
                  if isinstance(n, ast.FunctionDef) and n.name == 'build_board')
        for node in ast.walk(fn):
            if isinstance(node, ast.Return) and node.value is not None:
                self.assertIsInstance(node.value, ast.Tuple,
                                      f'build_board returns a non-tuple at line {node.lineno}')


class RemoteStaging(unittest.TestCase):
    def test_import_closure_is_staged_to_the_rig(self):
        # hil_ci.sh stages an explicit scp whitelist; a module that is not on it exists
        # locally and in CI checkouts but silently never reaches the remote rig (how
        # mtp_test.py was first missed). Walk the local-import closure of everything
        # the rig executes and require each file's exact scp entry — a bare-substring
        # match would be satisfied by a mention in a comment or the run line.
        import ast
        hil_dir = Path(TEST_DIR).parents[0]
        staged = (hil_dir / 'hil_ci.sh').read_text()

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
            self.assertIn(f'"$ROOT_DIR/test/hil/{f}"', staged,
                          f'{f} runs on the rig but hil_ci.sh does not scp it')


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


class WedgedPidsFailsClosed(unittest.TestCase):
    """A scan that could not SEE the holder must not report "no holder". The holder is
    root-owned (run_case uses sudo -n when the node is not writable) and that is exactly
    what a hidepid/ProtectProc mount hides — so an unreadable /proc reading as clear
    clears unrecovered_hang and lets cleanup unbind a device whose usbfs lock is still
    held, which deadlocks the bus rather than one board."""

    def test_returns_a_completeness_flag_not_just_pids(self):
        import usbtest
        got = usbtest.wedged_pids('/dev/bus/usb/999/999')
        self.assertIsInstance(got, tuple)
        self.assertEqual(len(got), 2, 'the caller needs (pids, complete)')

    def test_a_restricted_proc_is_reported_incomplete(self):
        import usbtest
        self.addCleanup(setattr, usbtest.os, 'geteuid', usbtest.os.geteuid)
        self.addCleanup(setattr, usbtest.os, 'access', usbtest.os.access)
        usbtest.os.geteuid = lambda: 1000          # not root
        usbtest.os.access = lambda p, m: False     # /proc/1/cmdline unreadable
        _, complete = usbtest.wedged_pids('/dev/bus/usb/999/999')
        self.assertFalse(complete, 'a hidden holder was reported as absent')


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
    """The read-while-we-write runner. Its child can still outlast SIGKILL -- but unlike
    the thread it replaced, an abandoned child is a real process in its own session, so
    the containment sweep finds it and the report names it."""

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


class AbandonExitSurvivesAFailedFork(unittest.TestCase):
    """Pool() forks, and after a convoy -- every stranded read holding a thread and an fd --
    that fork is what hits EAGAIN/ENOMEM. It now runs inside the try, so the finally can
    reach _abandon_exit with pool and mgr still None."""

    def test_none_pool_and_manager_still_write_the_banner(self):
        # a subprocess, because _abandon_exit ends in os._exit: in-process it would take
        # the test runner with it, before any assertion could run
        import json
        import subprocess
        with TemporaryDirectory() as td:
            rd = Path(td)
            # it takes the report DIRECTORY now and re-renders both artifacts from the
            # sidecar, so seed the sidecar -- the markdown is output, not input
            (rd / 'hil_report.json').write_text(json.dumps(
                {'rows': [{'board': 'boardA', 'cells': {'cdc_msc': 'pass'},
                           'duration': '1s'}],
                 'banner': '', 'scope': '', 'caveat': ''}))
            src = (
                'import sys, types\n'
                f'sys.path.insert(0, {str(Path(TEST_DIR).parents[0])!r})\n'
                'st = types.ModuleType("serial")\n'
                'st.Serial = type("Serial", (), {})\n'
                'st.SerialException = type("SerialException", (Exception,), {})\n'
                'st.SerialTimeoutException = type("E2", (Exception,), {})\n'
                'sys.modules.setdefault("serial", st)\n'
                'import hil_test\n'
                f'hil_test._abandon_exit(None, None, True, 1, __import__("pathlib")'
                f'.Path({str(rd)!r}))\n')
            r = subprocess.run([sys.executable, '-c', src], capture_output=True,
                               text=True, timeout=120)
            self.assertEqual(r.returncode, 1, r.stderr)
            self.assertTrue((rd / 'hil_report.md').read_text().startswith(
                '**HIL run abandoned'), 'the abandon banner never reached the report')
            self.assertIn('abandoned',
                          json.loads((rd / 'hil_report.json').read_text())['caveat'])

    def test_kill_pool_children_tolerates_a_pool_that_never_existed(self):
        from helper import hil_health
        self.assertEqual(hil_health.kill_pool_children(None), 0)
        self.assertEqual(hil_health.kill_pool_children(None, None), 0)


class UsbtestOuterBoundIsOneValue(unittest.TestCase):
    """run_cmd's kill is the ONE bound, and it must carry a recovery reserve only when a
    recovery can actually run. Otherwise a board on a path that cannot recover holds a pool
    worker and its battery permit idle for the difference, under a usbtest width of 2."""

    def _invoke(self, flasher, skip_flash=False):
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
        with self.assertRaises(hil_test.TestFail):
            hil_test.test_device_usbtest({'name': 'b', 'uid': 'UID1', 'flasher': flasher})
        return seen

    def test_a_recoverable_board_reserves_the_recovery_budget(self):
        import usbtest
        flasher = {'name': 'openocd', 'vid_pid': '0x1366 0x1024',
                   'args': '-f target/rp2040.cfg'}
        seen = self._invoke(flasher)
        want = (hil_test.USBTEST_BATTERY_BUDGET + hil_test.USBTEST_OVERSHOOT
                + usbtest.recovery_reserve(flasher))
        self.assertEqual(seen['timeout'], want)

    def test_the_reserve_follows_the_board_not_a_fleet_constant(self):
        """Two convoy-safe openocd boards, one RP and one not: the non-RP board cannot
        run rescue_openocd, so reserving its two legs holds a pool worker and a usbtest
        permit for 200s of dead time."""
        rp = self._invoke({'name': 'openocd', 'vid_pid': '0x1366 0x1024',
                           'args': '-f target/rp2040.cfg'})
        wch = self._invoke({'name': 'openocd', 'vid_pid': '0x1366 0x1024',
                            'args': '-f target/wch-riscv.cfg'})
        self.assertLess(wch['timeout'], rp['timeout'])

    def test_a_board_with_no_recovery_does_not_pay_for_one(self):
        seen = self._invoke({'name': 'stlink', 'uid': 'X'})   # never convoy_safe
        # It does not carry the RECOVERY reserve it cannot spend
        self.assertEqual(seen['timeout'],
                         hil_test.USBTEST_BATTERY_BUDGET + hil_test.USBTEST_OVERSHOOT)
        # ...but it MUST still exceed the child's own --budget. The battery checks the
        # budget before dispatching, so it can overshoot by one already-started case; an
        # equal bound SIGKILLs it just as it goes to print, turning ~29 real per-case
        # verdicts into "usbtest did not run" and re-paying the whole battery on retry.
        toks = seen['cmd'].split()
        budget = int(toks[toks.index('--budget') + 1])
        case_timeout = int(toks[toks.index('--timeout') + 1])
        self.assertGreaterEqual(seen['timeout'] - budget, case_timeout,
                                'the outer kill can land mid-case, before the JSON')

    def test_skip_flash_still_bounds_the_child(self):
        """--skip-flash disables recovery, so the child must not be given a reserve it
        cannot spend -- but it MUST still be bounded."""
        seen = self._invoke({'name': 'openocd', 'vid_pid': '0x1366 0x1024'}, skip_flash=True)
        self.assertEqual(seen['timeout'],
                         hil_test.USBTEST_BATTERY_BUDGET + hil_test.USBTEST_OVERSHOOT)


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


class RemoteDirIsScreened(unittest.TestCase):
    """REMOTE_DIR reaches the rig through `rm -rf`, an scp remote path and an rsync
    remote path -- all re-split and expanded by the REMOTE shell, none of them
    protectable by quoting the local variable. So the script screens the value once
    instead: it must survive that re-split unchanged, and `~` must keep working."""

    def _run(self, remote_dir, *args, keep_going=False):
        import subprocess
        with TemporaryDirectory() as td:
            # real ssh/scp/rsync would reach the rig; these just record the argv. Exit 77
            # unless the caller needs the script to run on to the second ssh.
            rc = 0 if keep_going else 77
            for tool in ('ssh', 'scp', 'rsync'):
                write_script(Path(td) / tool, f'echo "stub-{tool} $*" >&2; exit {rc}')
            # hil_ci.sh now refuses an all-boards run with nothing built, so this arg-quoting
            # test needs a checkout stub with one build dir to reach the run invocation
            root = Path(td) / 'root'
            (root / 'test' / 'hil').mkdir(parents=True)
            (root / 'test' / 'hil' / 'hil_test.py').touch()
            (root / 'examples' / 'cmake-build-alpha').mkdir(parents=True)
            env = {**os.environ, 'REMOTE_DIR': remote_dir, 'REMOTE': 'stub',
                   'ROOT_DIR': str(root),
                   'PATH': td + os.pathsep + os.environ['PATH']}
            return subprocess.run(
                ['bash', str(Path(TEST_DIR).parents[0] / 'hil_ci.sh'), *args],
                capture_output=True, text=True, timeout=60, env=env)

    def test_whitespace_is_refused(self):
        # unscreened, the remote `rm -rf -- "$1"` gets a TRUNCATED path and deletes
        # the wrong tree
        r = self._run('/tmp/hil dir')
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('REMOTE_DIR', r.stderr)

    def test_command_substitution_is_refused(self):
        r = self._run('/tmp/$(touch pwned)')
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('REMOTE_DIR', r.stderr)

    def test_bare_root_is_refused(self):
        r = self._run('/')
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('REMOTE_DIR', r.stderr)

    def test_a_tilde_path_is_accepted(self):
        """The one override %q broke: `~` must reach the remote shell UNESCAPED or it
        creates a literal '~' directory in the login dir."""
        r = self._run('~/tinyusb-hil')
        self.assertIn('~/tinyusb-hil', r.stderr)     # got as far as the first ssh
        self.assertNotIn('\\~', r.stderr)            # %q escapes it; the remote shell won't

    def test_paths_that_would_rm_rf_something_huge_are_refused(self):
        """Passing the tilde through UNESCAPED is what makes this dangerous: the remote
        shell expands `~/` to the login dir, so `rm -rf -- "$1"` takes out $HOME -- one
        typo away from the documented REMOTE_DIR=~/dir override. A bare root, a
        no-component path and a foreign ~user are the same class."""
        for bad in ('~/', '~root/x', '~-', '//', '/.', '/tmp/hil/'):
            with self.subTest(remote_dir=bad):
                r = self._run(bad)
                self.assertNotEqual(r.returncode, 0, f'{bad!r} was accepted')
                self.assertIn('REMOTE_DIR', r.stderr)

    def test_an_arg_containing_a_space_survives_the_remote_resplit(self):
        """ssh joins its argv into ONE string the remote shell re-splits, so an unquoted
        `-t 'host/cdc msc'` arrives as two arguments and hil_test.py sees a stray word
        where it expects the config path."""
        r = self._run('/tmp/tinyusb-hil', '-t', 'host/cdc msc', keep_going=True)
        run_line = [l for l in r.stderr.splitlines() if 'bash -s --' in l][-1]
        self.assertIn(r'host/cdc\ msc', run_line)


class EveryBoardIsStaged(unittest.TestCase):
    """One hil_test.py run takes several `-b` flags, and hil-operator hands it the whole board
    set that way. The `-b` parse loop kept a single BOARD, so only the LAST board's binaries
    were rsynced and every other board died on the rig with a missing firmware path -- after
    its flash slot and lock were already spent.

    Three of these five fail against the pre-fix script (the discriminating unbuilt case
    puts the board FIRST, because the old single-BOARD parse happened to handle a trailing
    one correctly); the run-line and variant-dir tests are characterization -- the old script
    already forwarded ARGS whole and read variants from the config for its one board."""

    def _run(self, boards, cfg_boards=None, variants=None):
        import json
        import subprocess
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        root = Path(td.name) / 'root'
        (root / 'test' / 'hil').mkdir(parents=True)
        (root / 'test' / 'hil' / 'hil_test.py').touch()
        built = cfg_boards if cfg_boards is not None else boards
        (root / 'examples').mkdir(parents=True, exist_ok=True)
        for b in built:
            (root / 'examples' / f'cmake-build-{b}').mkdir(parents=True)
        roster = [{'name': b} for b in boards]
        for entry in roster:
            for v in (variants or {}).get(entry['name'], []):
                entry.setdefault('variant', []).append({'name': v})
        cfg = root / 'test' / 'hil' / 'cfg.json'
        cfg.write_text(json.dumps({'boards': roster}))
        stubs = Path(td.name) / 'bin'
        stubs.mkdir()
        # real ssh/scp/rsync would reach the rig; these just record the argv
        for tool in ('ssh', 'scp', 'rsync'):
            write_script(stubs / tool, f'echo "stub-{tool} $*" >&2; exit 0')
        env = {**os.environ, 'REMOTE': 'stub', 'ROOT_DIR': str(root), 'CONFIG': str(cfg),
               'PATH': str(stubs) + os.pathsep + os.environ['PATH']}
        args = [a for b in boards for a in ('-b', b)]
        r = subprocess.run(['bash', str(Path(TEST_DIR).parents[0] / 'hil_ci.sh'), *args],
                           capture_output=True, text=True, timeout=60, env=env)
        r.rsyncs = [l for l in r.stderr.splitlines() if l.startswith('stub-rsync')]
        # the RUN ssh is the one carrying hil_test.py's args; the setup ssh is not
        r.run_lines = [l for l in r.stderr.splitlines() if '--retry 1' in l]
        return r

    def test_binaries_for_every_requested_board_are_copied(self):
        r = self._run(['alpha', 'beta', 'gamma'])
        self.assertEqual(r.returncode, 0, r.stderr)
        for b in ('alpha', 'beta', 'gamma'):
            self.assertTrue(any(f'cmake-build-{b} ' in l for l in r.rsyncs),
                            f'{b} binaries never staged: {r.rsyncs}')

    def test_every_board_reaches_hil_test(self):
        r = self._run(['alpha', 'beta'])
        self.assertEqual(len(r.run_lines), 1, r.stderr)
        self.assertIn('-b alpha', r.run_lines[0])
        self.assertIn('-b beta', r.run_lines[0])

    def test_an_unbuilt_board_aborts_before_anything_is_staged(self):
        """The discriminating case: the unbuilt board is FIRST. The pre-fix script kept only
        the last -b, found it built, and ran happily while silently testing one board. It also
        has to fail BEFORE staging -- the old in-loop check fired after the remote tree was
        wiped and earlier boards were rsynced, costing a run and leaving a half-staged rig."""
        r = self._run(['alpha', 'beta'], cfg_boards=['beta'])
        self.assertNotEqual(r.returncode, 0, 'unbuilt first board was accepted')
        self.assertIn('alpha', r.stdout + r.stderr)
        self.assertEqual(r.rsyncs, [], f'staged despite an unbuilt board: {r.rsyncs}')
        self.assertEqual(r.run_lines, [], 'reached the run despite an unbuilt board')

    def test_a_board_whose_firmware_is_only_a_variant_dir_is_accepted(self):
        """Variant names are not required to be prefixed with the board name, so a board can
        own no `cmake-build-<board>` dir at all. A pre-flight that only globs the board name
        rejects it and tells the user to build firmware that is already there."""
        r = self._run(['alpha', 'beta'], cfg_boards=['alpha', 'odd-name-v'],
                      variants={'beta': ['odd-name-v']})
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertTrue(any('cmake-build-odd-name-v ' in l for l in r.rsyncs),
                        f"beta's variant dir never staged: {r.rsyncs}")

    def test_all_unbuilt_boards_are_named_at_once(self):
        """One build round should fix every complaint, so the guard reports the whole set."""
        r = self._run(['alpha', 'beta', 'gamma'], cfg_boards=['beta'])
        self.assertNotEqual(r.returncode, 0)
        out = r.stdout + r.stderr
        self.assertIn('alpha', out)
        self.assertIn('gamma', out)


class StagingCoversEveryBoardForm(unittest.TestCase):
    """hil_test.py declares `-b, --board` with action='append', so argparse accepts --board X,
    --board=X and -bX too. Staging only the bare form sent boards to the rig with no firmware,
    where every test logs `Skip (no binary)` and counts zero errors -- a green row for a board
    that was never flashed. Also covers the roster check, which has to fire BEFORE the remote
    tree is wiped, since hil_test.py rejects an unknown -b for the whole run."""

    def _run(self, argv, built, roster=None, variants=None, env_extra=None, stale=None):
        import json
        import subprocess
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        root = Path(td.name) / 'root'
        (root / 'test' / 'hil').mkdir(parents=True)
        (root / 'test' / 'hil' / 'hil_test.py').touch()
        (root / 'examples').mkdir(parents=True, exist_ok=True)
        for b in built:
            (root / 'examples' / f'cmake-build-{b}').mkdir(parents=True)
        entries = [{'name': b} for b in (roster if roster is not None else built)]
        for e in entries:
            for v in (variants or {}).get(e['name'], []):
                e.setdefault('variant', []).append({'name': v})
        cfg = root / 'test' / 'hil' / 'cfg.json'
        cfg.write_text(json.dumps({'boards': entries}))
        stubs = Path(td.name) / 'bin'
        stubs.mkdir()
        # ssh joins its argv into ONE string that the REMOTE shell re-splits, and feeds the
        # heredoc on stdin. A stub that echoes "$*" hides exactly that, which is how a
        # completely broken env-forwarding change once passed its own test -- so this stub
        # re-splits like the real thing and reports the script body separately.
        write_script(stubs / 'ssh', 'shift; printf "REMOTE-ARGV: %s\\n" "$*" >&2; '
                                    'body=$(cat); printf "REMOTE-BODY: %s\\n" "$body" >&2; exit 0')
        for tool in ('scp', 'rsync'):
            write_script(stubs / tool, f'echo "stub-{tool} $*" >&2; exit 0')
        for name, content in (stale or {}).items():
            (root / name).write_text(content)
        env = {**os.environ, 'REMOTE': 'stub', 'ROOT_DIR': str(root), 'CONFIG': str(cfg),
               'PATH': str(stubs) + os.pathsep + os.environ['PATH'], **(env_extra or {})}
        r = subprocess.run(['bash', str(Path(TEST_DIR).parents[0] / 'hil_ci.sh'), *argv],
                           capture_output=True, text=True, timeout=60, env=env)
        r.rsyncs = [l for l in r.stderr.splitlines() if l.startswith('stub-rsync')]
        r.run_lines = [l for l in r.stderr.splitlines() if '--retry 1' in l]
        r.body = '\n'.join(l for l in r.stderr.splitlines() if l.startswith('REMOTE-BODY'))
        r.stale_left = {name: (root / name).exists() for name in (stale or {})}
        return r

    def test_long_board_forms_are_staged_and_only_that_board(self):
        """Two boards are built so the pre-fix 'copy all built binaries' else-branch cannot
        stage the right one by accident -- that is what made the first version of this test
        pass against master while the feature was broken. -balpha is the glued short form
        argparse resolves to --board alpha; unparsed it fell through to the all-boards branch
        and silently staged everything built with no roster check."""
        for argv in (['--board', 'alpha'], ['--board=alpha'], ['-balpha']):
            with self.subTest(argv=argv):
                r = self._run(argv, built=['alpha', 'beta'], roster=['alpha', 'beta'])
                self.assertEqual(r.returncode, 0, r.stderr)
                self.assertTrue(any('cmake-build-alpha ' in l for l in r.rsyncs),
                                f'{argv} never staged: {r.rsyncs}')
                self.assertFalse(any('cmake-build-beta ' in l for l in r.rsyncs),
                                 f'{argv} staged an unrequested board: {r.rsyncs}')

    def test_board_test_flag_is_not_mistaken_for_a_board(self):
        """-bt is hil_test.py's --board-test and is exactly what <config>.failed contains, so
        a glued -b?* pattern turns the documented retry into 'not in the roster: t'."""
        for argv in (['-b', 'alpha', '-bt', 'alpha:device/cdc_msc'],
                     ['-b', 'alpha', '-btalpha:device/cdc_msc']):
            with self.subTest(argv=argv):
                r = self._run(argv, built=['alpha'])
                self.assertEqual(r.returncode, 0, r.stderr)
                self.assertNotIn('not in', r.stderr)
                self.assertTrue(any('alpha:device/cdc_msc' in l for l in r.run_lines),
                                f'-bt never reached the rig: {r.run_lines}')

    def test_a_board_outside_the_roster_is_refused_before_staging(self):
        r = self._run(['-b', 'alpha', '-b', 'ghost'], built=['alpha', 'ghost'], roster=['alpha'])
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('ghost', r.stdout + r.stderr)
        self.assertEqual(r.rsyncs, [], 'staged despite an unknown board')
        self.assertEqual(r.run_lines, [], 'reached the run despite an unknown board')

    def test_a_variant_with_no_build_dir_warns_instead_of_passing_silently(self):
        r = self._run(['-b', 'alpha'], built=['alpha'], variants={'alpha': ['alpha-DMA']})
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('alpha-DMA', r.stderr)
        self.assertIn('skipped, not tested', r.stderr)

    def test_no_build_dirs_at_all_aborts_the_all_boards_form(self):
        """`hil_ci.sh` with no -b stages everything built. With nothing built it used to wipe
        the rig, stage nothing, and return a green all-skip table."""
        r = self._run([], built=[], roster=['alpha'])
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('nothing to test', r.stdout + r.stderr)
        self.assertEqual(r.run_lines, [], 'reached the run with nothing staged')
        self.assertNotIn('Setting up remote', r.stdout + r.stderr,
                         'the guard fired only after the remote tree was already wiped')

    def test_hil_env_reaches_the_rig_as_environment_not_argv(self):
        """An authorized force is HIL_NO_BOARD_LOCK=1. Passed through ssh's argv it arrives as a
        positional argument and argparse exits 2, so it has to travel in the script body."""
        r = self._run(['-b', 'alpha'], built=['alpha'], env_extra={'HIL_NO_BOARD_LOCK': '1'})
        self.assertEqual(r.returncode, 0, r.stderr)
        # one %q-quoted word of `export NAME=value; ` fragments, evaluated by the remote —
        # NOT a bare NAME=value element, which hil_test.py's argparse takes as a positional.
        # %q backslash-escapes the spaces, so match the pieces rather than the plain phrase.
        run = '\n'.join(r.run_lines)
        self.assertIn('HIL_NO_BOARD_LOCK=1', run)
        self.assertIn('export', run)
        self.assertFalse(any(' HIL_NO_BOARD_LOCK=1 ' in l for l in r.run_lines),
                         'env reached argv unquoted, where hil_test.py sees a positional')

    def test_a_value_with_spaces_survives_forwarding(self):
        r = self._run(['-b', 'alpha'], built=['alpha'],
                      env_extra={'HIL_SCRATCH': '/tmp/my scratch'})
        self.assertEqual(r.returncode, 0, r.stderr)
        run = '\n'.join(r.run_lines)
        self.assertIn('HIL_SCRATCH', run)
        self.assertIn('scratch', run)

    def test_hil_report_dir_is_never_forwarded(self):
        """Where the report lands on the rig is this script's contract (REMOTE_DIR, where all
        three copy-backs look); forwarding a local HIL_REPORT_DIR relocates it there and every
        copy-back comes home empty -- two of the three silently."""
        r = self._run(['-b', 'alpha'], built=['alpha'],
                      env_extra={'HIL_REPORT_DIR': '/tmp/elsewhere'})
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertFalse(any('HIL_REPORT_DIR' in l for l in r.run_lines),
                         f'HIL_REPORT_DIR reached the rig: {r.run_lines}')

    def test_a_stale_local_failed_spec_does_not_survive_a_green_run(self):
        """A green run writes no .failed on the rig, so the copy-back scp no-ops; the local
        spec from a previous FAILED run must not survive it looking current -- a later
        "retry from the spec" would re-flash boards that already passed."""
        r = self._run(['-b', 'alpha'], built=['alpha'],
                      stale={'cfg.json.failed': '--accumulate -b alpha'})
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertFalse(r.stale_left['cfg.json.failed'],
                         "last run's re-run spec survived a green run")


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
        rows = [('fast1', 0, [], [], 1.0, False), ('fast2', 0, [], [], 1.0, False)]
        with self.assertRaises(hil_test.PoolDrainTimeout) as cm:
            hil_test.drain_pool(self._It(rows), boards, time.monotonic() + 5)
        self.assertEqual([r[0] for r in cm.exception.finished], ['fast1', 'fast2'])

    def test_an_expired_deadline_stops_before_asking_for_more(self):
        """Left <= 0 must not be handed to it.next() as a zero/negative timeout."""
        boards = [{'name': 'a'}, {'name': 'b'}]
        it = self._It([('a', 0, [], [], 1.0, False)])
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
        rows = [(n, 0, [], [], 1.0, False) for n in ('a', 'b', 'c', 'd')]
        with self.assertRaises(hil_test.PoolDrainTimeout) as cm:
            hil_test.drain_pool(Slow(rows), boards, time.monotonic() + 0.3)
        self.assertTrue(cm.exception.finished, 'rows collected before the expiry were lost')

    def test_every_board_finishing_returns_them_all(self):
        boards = [{'name': 'a'}, {'name': 'b'}]
        rows = [('a', 0, [], [], 1.0, False), ('b', 1, [], [], 2.0, False)]
        got = hil_test.drain_pool(self._It(rows), boards, time.monotonic() + 5)
        self.assertEqual(got, rows)


class WedgedBoardCosts(unittest.TestCase):
    """Two decisions the containment latch makes, tested as decisions rather than through
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

    def _run(self, stdout, rc=0):
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
        board = {'name': 'b', 'uid': 'U', 'flasher': {'name': 'openocd', 'vid_pid': '0x1 0x2'}}
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


class StrayNoteSurvivesTheTupleWidth(unittest.TestCase):
    """_stray_note reads r[5] -- and three producers build this tuple at three widths, so
    `len(r) > 5 and r[5]` reads a WRONG SLOT rather than raising if a field is ever
    inserted. The live handoff pr3840-mret-board-result.md proposes exactly that, and the
    report would then say "no strays" while probes and usbfs nodes stay held into the next
    job. The index changed once already in this branch (r[6] -> r[5])."""

    def test_it_names_the_board_and_the_count(self):
        wide = ('dirty', 1, [], [], 9.0, 2)
        clean = ('fine', 0, [], [], 8.0, 0)
        note = hil_test._stray_note([wide, clean])
        self.assertIn('dirty (2)', note)
        self.assertIn('2 process(es)', note)
        self.assertNotIn('fine', note, 'a clean board must not appear in the note')

    def test_a_narrow_row_from_the_timeout_path_is_not_misread(self):
        """The abort paths synthesise 5-field rows for boards that never reported."""
        self.assertEqual(hil_test._stray_note([('stuck', 1, [], None, 0)]), '')
        self.assertEqual(hil_test._stray_note([('fine', 0, [], [], 8.0, 0)]), '')

    def test_the_slot_it_reads_is_the_slot_test_board_writes(self):
        """Pins the index against the producer, so inserting a field fails HERE rather
        than silently reporting a duration as a stray count."""
        import ast
        src = (Path(TEST_DIR).parents[0] / 'hil_test.py').read_text()
        fn = next(n for n in ast.walk(ast.parse(src))
                  if isinstance(n, ast.FunctionDef) and n.name == 'test_board')
        widths = sorted({len(n.value.elts) for n in ast.walk(fn)
                         if isinstance(n, ast.Return) and isinstance(n.value, ast.Tuple)})
        # the board-LOCKED early return is 5 wide and carries no stray count; the normal
        # one is 6, with strays last
        self.assertEqual(widths, [5, 6],
                         'the result tuple changed width; _stray_note reads index 5')


class MixedWidthRowsSurviveTheReportWriters(unittest.TestCase):
    """_abort_report hands `[(n, 1, [], None, 0) for n in stuck] + [r for r in mret ...]`
    to both writers -- 5-field synthetic rows mixed with 6-field worker rows. Every other
    test uses uniform widths, so replacing either `*_` unpack with a fixed-width one keeps
    the suite green and raises only INSIDE the containment path, where a raise costs every
    board's results."""

    def _mixed(self):
        return [('stuck', 1, [], None, 0),                       # synthetic, 5 wide
                ('ran', 1, ['device/dfu'],
                 [('ran', {'device/dfu': '❌ boom'}, '8s')], 8.0, 2)]   # worker, 6 wide

    def test_the_rerun_spec_accepts_both_widths(self):
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

        def render(reason, secs):
            hil_report.accumulate_report = lambda *a, **k: (_ for _ in ()).throw(
                OSError('report dir unwritable'))
            try:
                with TemporaryDirectory() as td:
                    rd = Path(td)
                    hil_test._abort_report(reason, [], [{'name': 'boardA'}],
                                           rd / 'c.failed', rd, True, '',
                                           timeout_secs=secs)
                    return (rd / hil_report.REPORT_MD).read_text()
            finally:
                hil_report.accumulate_report = real

        guard = render('abandoned: worker pool timed out after 3600s', 3600)
        self.assertIn(hil_report.POOL_TIMEOUT_CELL, guard)
        raised = render('aborted: a worker raised ValueError: x', None)
        self.assertIn(hil_report.RUN_ABORTED_CELL, raised)
        self.assertNotIn(hil_report.POOL_TIMEOUT_CELL, raised,
                         'a run that aborted on a raise is not a pool timeout')
        # and the fallback must still fire on BOTH paths -- that is what it is for
        for md in (guard, raised):
            self.assertIn('boardA', md)

    def test_only_the_rerun_spec_sees_the_synthetic_rows(self):
        """accumulate_report gets `mret` alone -- worker rows, always 4th field a real
        list. Widening _abort_report to hand it the synthetic list too would crash the
        containment path: those rows carry rows=None and render_matrix iterates it."""
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

    def test_a_scan_that_gave_up_says_could_not_tell_instead(self):
        """The conflation this whole path exists to avoid: an unreadable DUT is not an
        absent one, and the bare string sends a maintainer after a firmware regression on
        hardware that is merely wedged."""
        from helper import hil_util
        self.addCleanup(setattr, hil_util, '_ever_stranded', hil_util._ever_stranded)
        hil_util._ever_stranded = True
        with self.assertRaises(hil_test.TestFail) as cm:
            hil_test.test_device_usbtest({'name': 'b', 'uid': 'NOPE',
                                          'flasher': {'name': 'stlink', 'uid': 'X'}})
        self.assertIn('could not tell', str(cm.exception))


class UsbtestStartupDoesNotClaimAbsenceBlind(unittest.TestCase):
    """usbtest.py's own startup lookup, the sibling of the arm above. hil_test relays its
    stderr verbatim into the report cell, so a positive 'no cafe:4010 device' from a scan
    that gave up is the same conflation one process further out. Structural because the
    exit sits mid-main(), behind argparse and the testusb probe."""

    def test_the_sysfs_backed_absence_claims_carry_the_note(self):
        """Both claims that a bounded read can turn into a false absence. The printer one
        was missed: read_sysfs folds a timed-out `serial` into None, so a wedged-but-
        enumerated printer read as 'Printer device not found' -- an enumeration verdict for
        hardware that is merely unreadable. The MIDI lookup is deliberately NOT here: it
        globs /dev/snd/by-id and readlinks it, so no bounded read can blind it."""
        import ast
        tree = ast.parse(Path(hil_test.__file__).read_text())
        claims = [ast.unparse(n) for n in ast.walk(tree)
                  if isinstance(n, (ast.Assert, ast.Raise))
                  and ('Printer device not found' in ast.unparse(n)
                       or 'no cafe:4010 device' in ast.unparse(n))]
        self.assertEqual(len(claims), 2, 'a sysfs-backed absence claim moved or was added')
        for c in claims:
            self.assertIn('strand_note', c, f'absence claimed without the note: {c[:70]}')

    def test_the_absence_exit_carries_the_stranded_caveat(self):
        import ast
        import usbtest
        tree = ast.parse(Path(usbtest.__file__).read_text())
        exits = [n for n in ast.walk(tree)
                 if isinstance(n, ast.Call) and ast.unparse(n.func) == 'sys.exit'
                 and 'no {VID}:{PID} device' in ast.unparse(n)]
        self.assertEqual(len(exits), 1, 'the absence exit moved; retarget this test')
        self.assertIn('strand_note', ast.unparse(exits[0]),
                      'usbtest claims absence without consulting sysfs_stranded()')


class UsbtestGlobalCleanupStaysProcessWide(unittest.TestCase):
    """The strand flag has TWO consumers at different scopes. The per-case verdict is
    per-DUT -- a peer that stranded must not make OUR board report wedged. But the finally
    block's cleanup is GLOBAL: remove_id plus an unbind of every interface under the
    usbtest driver, including that peer's. Those writes take the uninterruptible
    device_lock, so the global path has to stay gated on the process-wide question."""

    def test_the_global_unbind_consults_the_process_wide_flag(self):
        import ast
        import usbtest
        tree = ast.parse(Path(usbtest.__file__).read_text())
        fins = [n for n in ast.walk(tree) if isinstance(n, ast.Try) and n.finalbody
                and 'remove_id' in ast.unparse(ast.Module(body=n.finalbody, type_ignores=[]))]
        self.assertEqual(len(fins), 1, 'the cleanup finally moved; retarget this test')
        body = ast.unparse(ast.Module(body=fins[0].finalbody, type_ignores=[]))
        self.assertIn('sysfs_stranded', body,
                      'global remove_id/unbind runs without the process-wide strand gate')


if __name__ == '__main__':
    unittest.main()
