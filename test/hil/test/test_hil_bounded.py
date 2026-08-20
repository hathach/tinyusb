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


@unittest.skipIf(os.name == 'nt', 'POSIX shell fakes')
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
        hil_test._enum_timeout = 2
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
        self.assertLess(time.monotonic() - t0, 1.5)
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
        plus the fixed recovery costs (60s case timeout + 5s kill wait + 5s settle)
        fits inside USBTEST_RECOVERY_BUDGET -- otherwise the outer run_cmd kill lands
        mid-flash and orphans the flasher (own session) on the probe."""
        import subprocess
        hil_dir = Path(TEST_DIR).parents[0]
        r = subprocess.run([sys.executable, str(hil_dir / 'usbtest.py'), '--help'],
                           capture_output=True, text=True, timeout=30)
        self.assertEqual(r.returncode, 0, r.stderr)
        for flag in ('--recover-board', '--recover-fw', '--outer-timeout'):
            self.assertIn(flag, r.stdout)

    def test_the_bounded_reflash_actually_fits_the_reserve(self):
        """The arithmetic the docstring above claims but never checked -- the two
        constants never met in any test, so bumping either silently broke the promise.
        Overrun means run_cmd's outer kill lands MID-FLASH and orphans the flasher
        (start_new_session, so killpg misses it) holding the probe."""
        import re
        import usbtest
        hil_dir = Path(TEST_DIR).parents[0]
        # read the case timeout hil_test actually passes, so this cannot drift silently
        src = (hil_dir / 'hil_test.py').read_text()
        m = re.search(r'--timeout (\d+) --budget', src)
        self.assertIsNotNone(m, 'usbtest invocation changed shape; re-derive this bound')
        case_timeout = int(m.group(1))
        kill_wait, settle, time_left_reserve = 5, 5, 35   # usbtest.py's fixed costs
        worst = (case_timeout + kill_wait + usbtest.RECOVER_FLASH_TIMEOUT
                 + settle + time_left_reserve)
        self.assertLessEqual(
            worst, hil_test.USBTEST_RECOVERY_BUDGET,
            f'a HUNG case needs {worst}s to recover but only '
            f'{hil_test.USBTEST_RECOVERY_BUDGET}s is reserved')


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
        hil_test._enum_timeout = 2
        # the session scratch files land in cwd
        self.addCleanup(os.chdir, os.getcwd())
        os.chdir(tmp)

    def _restore_env(self):
        for k, v in self.saved_env.items():
            if v is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = v


@unittest.skipIf(os.name == 'nt', 'POSIX shell fakes')
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


class BoundedOpen(unittest.TestCase):
    """hil_util.bounded_open must return rather than block, and must not leak the fd if
    the open completes after we gave up (usblp_open takes the device mutex before it
    consults O_NONBLOCK, so a wedged node blocks the open uninterruptibly)."""

    def setUp(self):
        from helper import hil_util
        self.hil_util = hil_util
        self.tmp = TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        # bounded_open counts its stranded threads now, and the counter is process-global
        # with no decrement: three wedged-FIFO tests here reach SYSFS_STUCK_MAX and every
        # later test in this file reads SYSFS_UNKNOWN for perfectly good attributes
        self.addCleanup(setattr, hil_util, '_sysfs_stuck', hil_util._sysfs_stuck)

    def test_opens_a_normal_file(self):
        f = Path(self.tmp.name) / 'plain'
        f.write_text('x')
        fd = self.hil_util.bounded_open(str(f), os.O_RDONLY, 5)
        self.assertIsNotNone(fd)
        os.close(fd)

    def test_missing_path_returns_none_without_raising(self):
        self.assertIsNone(self.hil_util.bounded_open(
            str(Path(self.tmp.name) / 'nope'), os.O_RDONLY, 5))

    @unittest.skipIf(os.name == 'nt', 'POSIX fifo')
    def test_blocking_open_gives_up_and_does_not_leak_fds(self):
        """A reader-less FIFO blocks open(O_WRONLY) forever -- the closest portable
        stand-in for a wedged usblp node."""
        fifo = Path(self.tmp.name) / 'fifo'
        os.mkfifo(fifo)
        before = len(os.listdir('/proc/self/fd'))
        t0 = time.monotonic()
        for _ in range(5):
            self.assertIs(self.hil_util.bounded_open(str(fifo), os.O_WRONLY, 0.2),
                          self.hil_util.SYSFS_UNKNOWN)
        self.assertLess(time.monotonic() - t0, 10, 'bounded_open did not bound')
        self.assertLessEqual(len(os.listdir('/proc/self/fd')) - before, 1,
                             'bounded_open leaked fds on the blocking path')

    @unittest.skipIf(os.name == 'nt', 'POSIX fifo')
    def test_open_completing_during_the_abandon_does_not_leak(self):
        """The window the handoff lock exists for: the worker is at its store-or-close
        decision when the caller gives up and drains the box.

        The `abandoned` Event is instrumented to park the worker there, because timing
        alone never reaches that window -- 1500 tries against the unlocked version leaked
        nothing, so a test that merely completes the open late proves nothing. An empty
        `hit` means the instrumentation no longer bites and the window is untested."""
        hil_util = self.hil_util
        fifo = Path(self.tmp.name) / 'fifo'
        os.mkfifo(fifo)
        caller = threading.current_thread()
        drained, hit = threading.Event(), []

        class RacingEvent(threading.Event):
            def is_set(self):
                v = super().is_set()
                if not v and not hit and threading.current_thread() is not caller:
                    hit.append(True)
                    # bounded: the fixed bounded_open holds the lock across this call, so
                    # the caller cannot reach its abandon (and set drained) until we return
                    drained.wait(0.3)
                return v

        shim = types.ModuleType('threading_shim')
        shim.__dict__.update(threading.__dict__)
        shim.Event = RacingEvent
        hil_util.threading = shim
        self.addCleanup(setattr, hil_util, 'threading', threading)

        before = len(os.listdir('/proc/self/fd'))
        rd = os.open(fifo, os.O_RDONLY | os.O_NONBLOCK)   # the O_WRONLY open completes at once
        try:
            self.assertIs(hil_util.bounded_open(str(fifo), os.O_WRONLY, 0.05),
                          hil_util.SYSFS_UNKNOWN)
            drained.set()
            time.sleep(0.1)      # let an abandoned worker act on what it saw
            self.assertTrue(hit, 'the abandon window was never entered')
            self.assertLessEqual(len(os.listdir('/proc/self/fd')) - before, 1,
                                 'bounded_open stored the fd after the caller drained the box')
        finally:
            drained.set()
            os.close(rd)


class SysfsUnknownIsNotAbsent(unittest.TestCase):
    """read_sysfs must tell "no such attribute" (a fact) from "the read did not answer"
    (not a fact). Every caller that concluded absence from the latter reported a healthy
    board as a firmware regression."""

    def setUp(self):
        from helper import hil_util
        self.hil_util = hil_util
        self.saved = (hil_util._sysfs_stuck, hil_util._sysfs_blind_logged)
        self.tmp = TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)

    def tearDown(self):
        # a blocked read strands a counted daemon thread; leaving the count raised would
        # blind every later test in this process
        self.hil_util._sysfs_stuck, self.hil_util._sysfs_blind_logged = self.saved

    def test_readable_attribute_returns_its_value(self):
        p = Path(self.tmp.name) / 'serial'
        p.write_text('CAFE01\n')
        self.assertEqual(self.hil_util.read_sysfs(str(p)), 'CAFE01')

    def test_missing_attribute_is_none(self):
        self.assertIsNone(self.hil_util.read_sysfs(str(Path(self.tmp.name) / 'nope')))

    @unittest.skipIf(os.name == 'nt', 'POSIX fifo')
    def test_blocking_read_is_unknown_not_absent(self):
        """A reader-less FIFO stands in for the wedged device whose sysfs read never
        returns; None here would read as "the board is gone"."""
        fifo = Path(self.tmp.name) / 'fifo'
        os.mkfifo(fifo)
        t0 = time.monotonic()
        v = self.hil_util.read_sysfs(str(fifo), grace=0.3)
        self.assertLess(time.monotonic() - t0, 10, 'read_sysfs did not bound')
        self.assertIs(v, self.hil_util.SYSFS_UNKNOWN)
        self.assertIsNotNone(v)

    def test_blind_process_answers_unknown_for_a_readable_attribute(self):
        p = Path(self.tmp.name) / 'serial'
        p.write_text('CAFE01')
        self.hil_util._sysfs_stuck = self.hil_util.SYSFS_STUCK_MAX
        self.assertTrue(self.hil_util.sysfs_blind())
        self.assertIs(self.hil_util.read_sysfs(str(p)), self.hil_util.SYSFS_UNKNOWN)
        self.assertIn('blind', self.hil_util.sysfs_blind_note())

    def test_unknown_is_falsy_but_not_none(self):
        # call sites use `(v or '')` idioms; the sentinel must keep working there while
        # still being distinguishable from a real absence
        self.assertFalse(self.hil_util.SYSFS_UNKNOWN)
        self.assertIsNotNone(self.hil_util.SYSFS_UNKNOWN)


class UsbtestEnumerationVerdict(unittest.TestCase):
    """test_device_usbtest must not report a healthy board as "no cafe:4010 device" just
    because its own sysfs reads stopped answering."""

    def setUp(self):
        from helper import hil_util
        self.hil_util = hil_util
        self.td = TemporaryDirectory()
        self.addCleanup(self.td.cleanup)
        # a real device dir: usb_scan reads idVendor/idProduct with a plain open (they are
        # lock-free descriptor fields), and only `serial` through the bounded reader
        dev = Path(self.td.name) / '1-2'
        dev.mkdir()
        (dev / 'idVendor').write_text('cafe\n')
        (dev / 'idProduct').write_text('4010\n')
        (dev / 'serial').write_text('CAFE01\n')
        for obj, name, val in ((hil_util, 'read_sysfs', hil_util.read_sysfs),
                               (hil_util, 'glob', hil_util.glob),
                               (hil_util, '_sysfs_stranded', {}),
                               (hil_test, '_enum_timeout', 1)):
            self.addCleanup(setattr, obj, name, getattr(obj, name))
            setattr(obj, name, val)
        hil_util.glob = types.SimpleNamespace(glob=lambda pat: [str(dev)])

    def _fail(self, reader):
        self.hil_util.read_sysfs = reader
        with self.assertRaises(hil_test.TestFail) as cm:
            hil_test.test_device_usbtest({'uid': 'CAFE01', 'name': 'fake', 'flasher': {}})
        return str(cm.exception)

    def test_unknown_reads_do_not_claim_the_device_is_absent(self):
        msg = self._fail(lambda p, *a, **kw: self.hil_util.SYSFS_UNKNOWN)
        self.assertNotIn('no cafe:4010 device', msg)
        self.assertIn('did not answer', msg)

    def test_a_readable_bus_without_the_device_still_says_absent(self):
        msg = self._fail(lambda p, *a, **kw: 'OTHERUID')
        self.assertIn('no cafe:4010 device', msg)


class UnresolvedControllerBucket(unittest.TestCase):
    """An unresolved controller must budget in ONE bucket. Taking a permit on every slot
    serialized the whole fleet the moment a worker went blind."""

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
        from helper import hil_util
        for speed in (None, hil_util.SYSFS_UNKNOWN, '12', '1.5'):
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


class EnumPollDoesNotReReadAWedgedPath(unittest.TestCase):
    """usbtest_enumerated re-globs every device each 0.2 s pass. One wedged peer therefore
    strands a fresh bounded reader thread per pass, and SYSFS_STUCK_MAX=4 of those blind
    the WHOLE worker for the rest of the run -- measured at 8 s of polling. A path that
    already stranded is known-unknown; reading it again buys nothing and costs the
    blindness budget."""

    def test_a_stranded_path_is_read_at_most_once(self):
        from contextlib import contextmanager
        from helper import hil_lock, hil_util

        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        # A REAL device dir: usb_scan reads idVendor/idProduct with a plain open and
        # `continue`s on OSError, so a bare FIFO is skipped before the bounded read is ever
        # reached -- this test passed identically with the memo deleted until the ids were
        # added. The FIFO must be the `serial` of a device that survives the cheap filter.
        devdir = Path(td.name) / '1-2'
        devdir.mkdir()
        (devdir / 'idVendor').write_text('cafe\n')
        (devdir / 'idProduct').write_text('4010\n')
        wedged = devdir / 'serial'
        os.mkfifo(wedged)                 # open() blocks forever: no writer, ever

        def patch(obj, name, value):
            self.addCleanup(setattr, obj, name, getattr(obj, name))
            setattr(obj, name, value)

        def _permit(uid):
            yield

        from helper import hil_util as _hu2
        patch(_hu2, 'glob', types.SimpleNamespace(glob=lambda p: [str(devdir)]))
        patch(_hu2, '_sysfs_stranded', {})
        patch(hil_lock, 'usbtest_permit', contextmanager(_permit))
        # Long enough for several 2 s reads, but under the blindness cap -- past the cap
        # sysfs_blind() short-circuits reads on its own and would mask the memo entirely.
        patch(hil_test, '_enum_timeout', 8)
        # the blindness counter is process-global and never decrements: restore it or this
        # test blinds every test that runs after it
        patch(hil_util, '_sysfs_stuck', hil_util._sysfs_stuck)

        # Count LEAKED THREADS, not _sysfs_stuck: a strand is booked only the first time a
        # path is seen, so the counter is deduped by the memo's own bookkeeping and stays 1
        # even when the memo is broken. Each re-read blocks a fresh thread on the FIFO
        # forever and leaks its fd -- which is the cost the memo exists to avoid, and the
        # only thing here that actually moves when it regresses.
        before = threading.active_count()
        with self.assertRaises(hil_test.TestFail):      # never enumerates, by construction
            hil_test.test_device_usbtest({'name': 'b', 'uid': 'UID1',
                                          'flasher': {'name': 'openocd'}})
        self.assertLessEqual(threading.active_count() - before, 1,
                             'the poll re-read a path it already knew was stranded')


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


@unittest.skipIf(os.name == 'nt', 'POSIX shell fakes')
@unittest.skipIf(sys.version_info < (3, 11), 'fake-pymtp steering needs PYTHONSAFEPATH')
class StrandMemoRemembersUnstattablePaths(unittest.TestCase):
    """A stranded path whose inode could not be read is stored as None -- which dict.get()
    also returns for a MISS. Testing `is not None` therefore treats 'known stranded' as
    'never seen', and every later call strands ANOTHER permanent thread and fd on a path we
    already know is wedged. That is the exact unbounded growth SYSFS_STUCK_MAX exists to
    stop, and it is invisible: `first = path not in _sysfs_stranded` is False, so the
    blindness counter does not advance either."""

    def setUp(self):
        from helper import hil_util
        self.hil_util = hil_util
        self.addCleanup(hil_util._sysfs_stranded.clear)
        hil_util._sysfs_stranded.clear()
        self.addCleanup(setattr, hil_util, '_sysfs_stuck', hil_util._sysfs_stuck)
        hil_util._sysfs_stuck = 0
        self.td = TemporaryDirectory(); self.addCleanup(self.td.cleanup)
        self.fifo = os.path.join(self.td.name, 'serial')
        os.mkfifo(self.fifo)          # open() succeeds, read() never returns

    def test_an_unstattable_strand_is_not_re_read(self):
        self.hil_util._sysfs_stranded[self.fifo] = None   # as the record path stores it
        before = threading.active_count()
        self.assertIs(self.hil_util.read_sysfs(self.fifo, grace=0.5),
                      self.hil_util.SYSFS_UNKNOWN)
        self.assertEqual(threading.active_count(), before,
                         'a known-stranded path was re-read, stranding another thread')

    def test_a_live_strand_is_still_re_read_when_the_node_is_replaced(self):
        """The memo must not become permanent blindness: a NEW inode at the same path is a
        different device and has to be read."""
        self.hil_util._sysfs_stranded[self.fifo] = 999999999  # inode that is not this one
        with open(os.path.join(self.td.name, 'other'), 'w') as f:
            f.write('ok\n')
        os.replace(os.path.join(self.td.name, 'other'), self.fifo)
        self.assertEqual(self.hil_util.read_sysfs(self.fifo, grace=0.5), 'ok')


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
                                '--uid', 'CAFE01', '--timeout', '3'],
                               capture_output=True, text=True, timeout=60, env=env)
            elapsed = time.monotonic() - t0
        self.assertLess(elapsed, 30, f'did not honour --timeout 3 ({elapsed:.1f}s)')
        self.assertNotEqual(r.returncode, 0)
        # The assertions above are satisfied by an immediate CRASH, which is exactly what
        # shipped through this test once: `pass` left gio unbound and the next line
        # dereferenced it. Assert the behaviour the docstring names -- it POLLED for the
        # device (so it spent its budget) and did not die on a traceback.
        self.assertGreater(elapsed, 2.0,
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
        with self.assertRaises(AssertionError):
            self.hil_util.run_alongside(['sleep', '20'], boom, 1)
        # nothing of ours is left running: the reap ran on the error path too
        import subprocess
        out = subprocess.run(['pgrep', '-f', '^sleep 20'], capture_output=True, text=True)
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


class StrandedPathMemoInvalidates(unittest.TestCase):
    """The memo lives in read_sysfs, so every bounded reader gets it -- call-site memos
    meant each new scanner had to remember (get_printer_dev and the throughput probe did
    not). And it MUST expire on re-enumeration: the key is a bus path, which does not
    change when a device comes back on the same port, so a memo that never invalidates
    makes a board the branch's own HUNG reflash just recovered permanently invisible."""

    def setUp(self):
        from helper import hil_util
        self.hil_util = hil_util
        self.td = TemporaryDirectory()
        self.addCleanup(self.td.cleanup)
        for name in ('_sysfs_stranded', '_sysfs_stuck'):
            self.addCleanup(setattr, hil_util, name, getattr(hil_util, name))
        hil_util._sysfs_stranded = {}
        hil_util._sysfs_stuck = 0

    def test_a_stranded_path_is_not_re_read(self):
        f = Path(self.td.name) / 'serial'
        os.mkfifo(f)                       # never answers
        self.assertIs(self.hil_util.read_sysfs(str(f), 0.3), self.hil_util.SYSFS_UNKNOWN)
        after_first = self.hil_util._sysfs_stuck
        t0 = time.monotonic()
        for _ in range(3):
            self.assertIs(self.hil_util.read_sysfs(str(f), 0.3),
                          self.hil_util.SYSFS_UNKNOWN)
        self.assertLess(time.monotonic() - t0, 0.3, 'the memo did not short-circuit')
        self.assertEqual(self.hil_util._sysfs_stuck, after_first,
                         'repeat reads spent more of the blindness budget')

    def test_re_enumeration_clears_it(self):
        """A new device on the same busport gets a fresh sysfs node, hence a fresh inode.
        Without this the memo outlives the wedge it recorded."""
        f = Path(self.td.name) / 'serial'
        os.mkfifo(f)
        self.assertIs(self.hil_util.read_sysfs(str(f), 0.3), self.hil_util.SYSFS_UNKNOWN)
        f.unlink()
        f.write_text('CAFE01\n')           # same path, new inode = re-enumerated
        self.assertEqual(self.hil_util.read_sysfs(str(f), 0.3), 'CAFE01',
                         'a recovered device stayed invisible')

    def test_a_vanished_path_is_not_remembered_as_stranded(self):
        f = Path(self.td.name) / 'serial'
        os.mkfifo(f)
        self.assertIs(self.hil_util.read_sysfs(str(f), 0.3), self.hil_util.SYSFS_UNKNOWN)
        f.unlink()
        self.assertIsNone(self.hil_util.read_sysfs(str(f), 0.3))


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
        self.addCleanup(setattr, hil_util, '_sysfs_stranded',
                        dict(hil_util._sysfs_stranded))
        self.addCleanup(setattr, hil_util, '_sysfs_stuck', hil_util._sysfs_stuck)

    def _dev(self, name, vid, pid, serial='S1', fifo=False):
        d = self.root / name
        d.mkdir()
        (d / 'idVendor').write_text(vid + '\n')
        (d / 'idProduct').write_text(pid + '\n')
        if fifo:
            os.mkfifo(d / 'serial')          # a read that never answers
        else:
            (d / 'serial').write_text(serial + '\n')
        return d

    def _scan(self, **kw):
        import glob as _g
        real_glob = _g.glob
        self.addCleanup(setattr, self.hil_util.glob, 'glob', real_glob)
        self.hil_util.glob.glob = lambda pat: [str(p) for p in self.root.iterdir()]
        return self.hil_util.usb_scan(**kw)

    def test_a_mismatched_vid_pid_costs_no_serial_read(self):
        self._dev('1-1', '1234', '5678')
        self._dev('1-2', 'cafe', '4010', serial='UID1')
        devs, unknown = self._scan(vid_pid=('cafe', '4010'))
        self.assertEqual([d['serial'] for d in devs], ['UID1'])
        self.assertFalse(unknown)
        # the ruled-out device's locked attribute was never touched
        self.assertNotIn(str(self.root / '1-1' / 'serial'), self.reads)

    def test_a_wedged_device_stays_unproven_on_every_scan(self):
        """The memo lives in read_sysfs now, so usb_scan still CALLS it each pass -- what
        must not repeat is the cost. StrandedPathMemoInvalidates covers the short-circuit;
        here the invariant is that the device stays out of the results and absence stays
        unproven, however many times we look."""
        from helper import hil_util
        self._dev('1-1', 'cafe', '4010', fifo=True)
        first = None
        t0 = time.monotonic()
        for _ in range(3):
            devs, unknown = self._scan()
            self.assertTrue(unknown, 'a stranded read must leave absence unproven')
            self.assertEqual(devs, [])
            if first is None:
                first = hil_util._sysfs_stuck
        self.assertEqual(hil_util._sysfs_stuck, first,
                         'repeat scans spent more of the blindness budget')
        self.assertLess(time.monotonic() - t0, 3.0, 'repeat scans re-paid the grace')


class BoundedOpenTellsAbsentFromUnknown(unittest.TestCase):
    """Same contract as read_sysfs, in the sibling function of the same file: a real
    OSError is a FACT (EBUSY, ENOENT, EACCES), a blocked open is UNKNOWN. Folding both
    into None made an ordinary EBUSY report as a USB wedge, sending the operator to
    usb-kernel-recover for healthy hardware -- and left the stranded thread uncounted,
    so the cap that exists to stop the fd/thread ceiling never saw it."""

    def setUp(self):
        from helper import hil_util
        self.hil_util = hil_util
        self.td = TemporaryDirectory()
        self.addCleanup(self.td.cleanup)

    def test_a_real_oserror_is_a_fact(self):
        missing = str(Path(self.td.name) / 'nope')
        self.assertIsNone(self.hil_util.bounded_open(missing, os.O_RDONLY, 1))

    def test_a_blocked_open_is_unknown_and_counted(self):
        fifo = Path(self.td.name) / 'fifo'
        os.mkfifo(fifo)                     # no reader: O_WRONLY blocks forever
        self.addCleanup(setattr, self.hil_util, '_sysfs_stuck',
                        self.hil_util._sysfs_stuck)
        before = self.hil_util._sysfs_stuck
        got = self.hil_util.bounded_open(str(fifo), os.O_WRONLY, 0.3)
        self.assertIs(got, self.hil_util.SYSFS_UNKNOWN)
        self.assertEqual(self.hil_util._sysfs_stuck, before + 1,
                         'a stranded open is invisible to the blindness budget')


class UsbtestSysfsReadIsCapped(unittest.TestCase):
    """find_device re-scans every cafe:4010 peer after EVERY case, so the local twin --
    which had no SYSFS_STUCK_MAX -- stranded a thread and an fd per wedged peer per case.
    Delegating to hil_util gets the cap, and the deferred import keeps usbtest.py
    importable standalone."""

    def test_a_stranded_read_counts_against_the_shared_cap(self):
        import usbtest
        from helper import hil_util
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        wedged = Path(td.name) / 'serial'
        os.mkfifo(wedged)                      # no writer: open() never returns
        self.addCleanup(setattr, hil_util, '_sysfs_stuck', hil_util._sysfs_stuck)
        before = hil_util._sysfs_stuck
        # UNKNOWN, not None: folding them made a blinded scan read as "device dropped
        # off the bus", which aborts past the HUNG reflash
        self.assertIs(usbtest._read_sysfs_bounded(wedged, grace=0.5),
                      hil_util.SYSFS_UNKNOWN)
        self.assertEqual(hil_util._sysfs_stuck, before + 1,
                         'usbtest reads are invisible to the blindness budget')


class AbandonExitSurvivesAFailedFork(unittest.TestCase):
    """Pool() forks, and after a convoy -- every stranded read holding a thread and an fd --
    that fork is what hits EAGAIN/ENOMEM. It now runs inside the try, so the finally can
    reach _abandon_exit with pool and mgr still None."""

    def test_none_pool_and_manager_still_write_the_banner(self):
        # a subprocess, because _abandon_exit ends in os._exit: in-process it would take
        # the test runner with it, before any assertion could run
        import subprocess
        with TemporaryDirectory() as td:
            report = Path(td) / 'hil_report.md'
            report.write_text('| board | test |\n|---|---|\n', encoding='utf-8')
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
                f'.Path({str(report)!r}))\n')
            r = subprocess.run([sys.executable, '-c', src], capture_output=True,
                               text=True, timeout=120)
            self.assertEqual(r.returncode, 1, r.stderr)
            self.assertTrue(report.read_text().startswith('**HIL run abandoned'),
                            'the abandon banner never reached the report')

    def test_kill_pool_children_tolerates_a_pool_that_never_existed(self):
        from helper import hil_health
        self.assertEqual(hil_health.kill_pool_children(None), 0)
        self.assertEqual(hil_health.kill_pool_children(None, None), 0)


class UsbtestOuterBoundIsOneValue(unittest.TestCase):
    """The bound usbtest is TOLD and the bound run_cmd ENFORCES must be the same number.
    Three separate expressions disagreed: --skip-flash appended no --outer-timeout at all
    (usbtest reads 0 as no limit), and the no-recovery branch narrowed only the CHILD's
    view while run_cmd still waited for a recovery reserve nothing on that path can
    spend -- a pool worker and its battery permit idle for the difference."""

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
        # the blindness latch and the stranded memo are process-global: another class's
        # wedged-FIFO test would otherwise make every read here answer SYSFS_UNKNOWN
        patch(_hu, '_sysfs_stuck', 0)
        patch(_hu, '_sysfs_stranded', {})
        patch(hil_lock, 'usbtest_permit', contextmanager(_permit))
        patch(hil_test, 'skip_flash', skip_flash)
        patch(hil_test, '_current_fw', '/tmp/fw.elf')
        patch(hil_util, 'run_cmd', fake_run)
        with self.assertRaises(hil_test.TestFail):
            hil_test.test_device_usbtest({'name': 'b', 'uid': 'UID1', 'flasher': flasher})
        return seen

    def _outer_flag(self, cmd):
        toks = cmd.split()
        self.assertIn('--outer-timeout', toks, 'usbtest reads a missing bound as UNLIMITED')
        return int(toks[toks.index('--outer-timeout') + 1])

    def test_a_recoverable_board_reserves_the_recovery_budget(self):
        seen = self._invoke({'name': 'openocd', 'vid_pid': '0x1366 0x1024'})
        want = hil_test.USBTEST_BATTERY_BUDGET + hil_test.USBTEST_RECOVERY_BUDGET
        self.assertEqual(self._outer_flag(seen['cmd']), want)
        self.assertEqual(seen['timeout'], want)

    def test_a_board_with_no_recovery_does_not_pay_for_one(self):
        seen = self._invoke({'name': 'stlink', 'uid': 'X'})   # never convoy_safe
        outer = self._outer_flag(seen['cmd'])
        self.assertEqual(seen['timeout'], outer, 'the two bounds disagree')
        # It does not carry the RECOVERY reserve it cannot spend...
        self.assertLess(outer, hil_test.USBTEST_BATTERY_BUDGET
                        + hil_test.USBTEST_RECOVERY_BUDGET)
        # ...but it MUST still exceed the child's own --budget. The battery checks the
        # budget before dispatching, so it can overshoot by one already-started case; an
        # equal bound SIGKILLs it just as it goes to print, turning ~29 real per-case
        # verdicts into "usbtest did not run" and re-paying the whole battery on retry.
        toks = seen['cmd'].split()
        budget = int(toks[toks.index('--budget') + 1])
        case_timeout = int(toks[toks.index('--timeout') + 1])
        self.assertGreaterEqual(outer - budget, case_timeout,
                                'the outer kill can land mid-case, before the JSON')

    def test_skip_flash_still_bounds_the_child(self):
        seen = self._invoke({'name': 'openocd', 'vid_pid': '0x1366 0x1024'}, skip_flash=True)
        self.assertEqual(self._outer_flag(seen['cmd']), seen['timeout'])


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


class SummaryFoldsReportToBoards(unittest.TestCase):
    """hil_summary.py replaces the agent retyping the markdown table. Report rows are named per
    VARIANT and a variant need not start with the board name, so the config is what maps them
    back -- the previous string-matching design produced a defect in each of four review rounds."""

    def _sum(self, boards, rows, cfg_boards=None, banner=''):
        import json
        import subprocess
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        d = Path(td.name)
        (d / 'hil_report.json').write_text(json.dumps(
            {'rows': [{'board': b, 'cells': c, 'duration': '1s'} for b, c in rows],
             'banner': banner}))
        cfg = d / 'cfg.json'
        cfg.write_text(json.dumps({'boards': cfg_boards or [{'name': b} for b in boards]}))
        args = [a for b in boards for a in ('-b', b)]
        r = subprocess.run(['python3', str(Path(TEST_DIR).parents[0] / 'helper' / 'hil_summary.py'),
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


class CaveatSurvivesAccumulate(unittest.TestCase):
    """CI reruns with --accumulate: the sidecar keeps every earlier attempt's cells, but the
    banner was recomputed per attempt. A first attempt on a degraded rig and a clean rerun
    therefore published the degraded attempt's PASSES with no caveat on them -- and the
    generated .failed spec reruns only failures, so those cells are never re-earned."""

    def _rows(self, board, cell):
        return [(board, 0, 0, [(board, {cell: 'OK'}, '1s')], 0)]

    def test_an_earlier_attempts_caveat_is_still_on_the_report(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        banner = '> **Rig note.** 2 process(es) in D state at start.\n'

        hil_test.accumulate_report(self._rows('boardA', 'cdc_msc'), rd, True, '', banner)
        self.assertIn('Rig note', (rd / hil_test.REPORT_MD).read_text())

        # the rerun: clean rig, so this attempt contributes no banner of its own
        md = hil_test.accumulate_report(self._rows('boardB', 'cdc_msc'), rd, False, '', '')
        self.assertIn('boardA', md)                  # the earlier cells are kept ...
        self.assertIn('Rig note', md,
                      'the caveat the earlier cells were collected under was dropped')

    def test_the_same_caveat_twice_is_not_stacked(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        banner = '> **Rig note.** 2 process(es) in D state at start.\n'
        hil_test.accumulate_report(self._rows('boardA', 'cdc_msc'), rd, True, '', banner)
        md = hil_test.accumulate_report(self._rows('boardB', 'cdc_msc'), rd, False, '', banner)
        self.assertEqual(md.count('Rig note'), 1)


class BlindWorkerReachesTheReport(unittest.TestCase):
    """A worker that exhausts its bounded-read budget answers SYSFS_UNKNOWN for every
    attribute, so its "device not found" means "could not tell". That reached the log and
    the per-cell failure text but NOT the table -- and the table is what gets pasted into
    the PR. Seen live: run 31794359407 went blind in 4 workers and published 26 red cells
    with no mention of it, several of them caused by the blindness rather than the board."""

    def test_no_note_when_every_worker_could_see(self):
        mret = [('boardA', 0, [], [], 1.0, False), ('boardB', 0, [], [], 1.0, False)]
        self.assertEqual(hil_test._blind_note(mret), '')

    def test_the_note_names_the_boards_whose_verdicts_are_not_evidence(self):
        mret = [('boardA', 0, [], [], 1.0, True), ('boardB', 0, [], [], 1.0, False),
                ('boardC', 1, [], [], 1.0, True)]
        note = hil_test._blind_note(mret)
        self.assertIn('boardA', note)
        self.assertIn('boardC', note)
        self.assertNotIn('boardB', note)      # it could see; do not smear its result
        self.assertTrue(note.endswith('\n'), 'banners are line-oriented')

    def test_both_row_widths_survive_the_report_writers(self):
        """The blindness flag widened the worker's result tuple to 6, but the pool-timeout
        path still synthesises 5-field rows for boards that never reported and feeds them
        to the same two writers. A fixed-width unpack in either one raises INSIDE the
        containment path, which is where a raise costs every board's results."""
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        wide = ('boardA', 1, ['device/cdc_msc'], [('boardA', {'cdc_msc': '❌'}, '2s')], 2.0, True)
        narrow = ('stuck', 1, [], None, 0)                      # what the timeout path builds
        hil_test._write_failed_spec(rd / 'x.failed', rd, [wide, narrow])
        md = hil_test.accumulate_report([wide], rd, True, '', hil_test._blind_note([wide]))
        self.assertIn('boardA', md)
        self.assertIn('not all verdicts are evidence', md.lower())

    def test_the_stray_note_names_the_board_and_survives_narrow_rows(self):
        """Survivors ride back on the result tuple because main()'s own sweep runs after
        the report is written on both abort paths -- the banner appended there was
        computed and discarded."""
        wide = ('boardA', 0, [], [], 1.0, False, 2)
        clean = ('boardB', 0, [], [], 1.0, False, 0)
        note = hil_test._stray_note([wide, clean])
        self.assertIn('boardA', note)
        self.assertNotIn('boardB', note)
        self.assertIn('2', note)
        self.assertEqual(hil_test._stray_note([clean]), '')
        self.assertEqual(hil_test._stray_note([('stuck', 1, [], None, 0)]), '')

    def test_the_timeout_paths_synthetic_rows_do_not_crash_it(self):
        """The pool-timeout path builds (name, 1, [], None, 0) for boards that never
        reported -- five fields, no blindness to report -- and hands those around."""
        self.assertEqual(hil_test._blind_note([('stuck', 1, [], None, 0)]), '')


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
    FAILED (convoy-safe boards -- max32666fthr HUNG in the 08-14 run), the `inconclusive`
    abort (which sets the flag but leaves no case at status HUNG), and an unparsable JSON,
    which is the outer-timeout kill and the case where a wedge is most likely."""

    def setUp(self):
        self.addCleanup(setattr, hil_test, 'board_wedged', hil_test.board_wedged)
        hil_test.board_wedged = ''

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
    usbtest's `inconclusive` and `ambiguous` aborts fire AFTER the last case, so nothing
    back-fills a BUDGET entry to make failed/notrun non-zero."""

    def setUp(self):
        self.addCleanup(setattr, hil_test, 'board_wedged', hil_test.board_wedged)
        hil_test.board_wedged = ''

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


if __name__ == '__main__':
    unittest.main()
