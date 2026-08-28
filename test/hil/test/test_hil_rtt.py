#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Unit tests for hil_util.JlinkRtt and the rtt.py CLI against a fake JLinkExe
# on PATH — real subprocesses and sockets, no hardware, stdlib only, so the pre-commit
# hil-test hook can run this on GitHub's bare runner. Run directly:
#   python3 test/hil/test/test_hil_rtt.py
import os
import subprocess
import sys
import tempfile
import time
import unittest
from contextlib import suppress as contextlib_suppress
from pathlib import Path

# the module under test lives in the parent dir's helper/ package
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from helper import hil_util

CLI = Path(__file__).resolve().parents[3] / 'tools' / 'rtt.py'

# Serves -RTTTelnetPort like J-Link Commander: greets, echoes input uppercased, exits on
# stdin 'exit' (JlinkRtt.close()'s contract). FAKE_JLINK_MODE=die_after_greet sends the
# greeting then drops the connection and exits — the probe-unplug/crash case;
# FAKE_JLINK_MODE=tick also streams a line every 50 ms — the continuous-capture case.
FAKE_JLINK = '''#!/usr/bin/env python3
import os, socket, sys, threading, time
port = int(sys.argv[sys.argv.index('-RTTTelnetPort') + 1])
srv = socket.socket(); srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(('127.0.0.1', port)); srv.listen(1)
mode = os.environ.get('FAKE_JLINK_MODE', '')
def serve():
    conn, _ = srv.accept()
    # the real server sends its banner AT CONNECT, before the control block is
    # found — target data only flows later; the CLI's -i gate must not release
    # on the banner
    conn.sendall(b'SEGGER J-Link fake - Real time terminal output\\r\\n'
                 b'J-Link FakeProbe V1.0, SN=000\\r\\nProcess: JLinkExe\\r\\n')
    if mode == 'banner_only':
        while True:
            if not conn.recv(4096): os._exit(0)
    if mode == 'rst':
        import struct
        conn.recv(4096)   # wait for the client to speak, then reset the connection
        conn.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', 1, 0))
        conn.close(); os._exit(0)
    if mode == 'late_cb':
        # models JLinkExe before it finds the control block: client bytes sent in
        # this window are silently dropped, output starts only after the "attach"
        end = time.time() + 1.0
        conn.setblocking(False)
        while time.time() < end:
            try:
                conn.recv(4096)   # discard early input like the real server
            except OSError:
                pass
            time.sleep(0.05)
        conn.setblocking(True)
    conn.sendall(b'hello from target\\r\\n')
    if mode == 'die_after_greet':
        conn.close(); os._exit(0)
    if mode == 'tick':
        def tick():
            try:
                while True:
                    time.sleep(0.05); conn.sendall(b'tick\\r\\n')
            except OSError:
                pass
        threading.Thread(target=tick, daemon=True).start()
    while True:
        d = conn.recv(4096)
        if not d: return
        conn.sendall(d.upper())
threading.Thread(target=serve, daemon=True).start()
for line in sys.stdin:
    if line.strip() == 'exit': break
'''

BOARD = {'flasher': {'uid': '000', 'args': '-device FAKE'}}


@unittest.skipIf(os.name == 'nt', 'POSIX PATH/exec semantics')
class JlinkRttFakeProbe(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._dir = tempfile.TemporaryDirectory()
        fake = Path(cls._dir.name) / 'JLinkExe'
        fake.write_text(FAKE_JLINK)
        fake.chmod(0o755)
        cls._path = f'{cls._dir.name}{os.pathsep}{os.environ["PATH"]}'

    @classmethod
    def tearDownClass(cls):
        cls._dir.cleanup()

    def _fake_path(self):
        # register the restore BEFORE mutating, then prepend the fake tool dir
        self.addCleanup(os.environ.__setitem__, 'PATH', os.environ['PATH'])
        os.environ['PATH'] = self._path

    def _console(self, mode=''):
        self._fake_path()
        if mode:
            os.environ['FAKE_JLINK_MODE'] = mode
        self.addCleanup(os.environ.pop, 'FAKE_JLINK_MODE', None)
        con = hil_util.JlinkRtt(BOARD, timeout=0.1)
        self.addCleanup(con.close)
        return con

    def _read_until(self, con, want, timeout=3):
        out = b''
        end = time.monotonic() + timeout
        while want not in out and time.monotonic() < end:
            out += con.read(con.in_waiting or 1)
        return out

    def test_read_and_echo_write(self):
        con = self._console()
        self.assertIn(b'hello from target', self._read_until(con, b'hello from target'))
        self.assertEqual(con.write(b'ping'), 4)
        self.assertIn(b'PING', self._read_until(con, b'PING'))

    def test_eof_latched_when_server_dies(self):
        con = self._console(mode='die_after_greet')
        self._read_until(con, b'hello from target')
        end = time.monotonic() + 3
        while not con.eof and time.monotonic() < end:
            time.sleep(0.05)
        self.assertTrue(con.eof)               # dead server is detected, not spun on
        t0 = time.monotonic()
        self.assertEqual(con.read(64), b'')    # empty, paced like a serial timeout
        elapsed = time.monotonic() - t0
        self.assertLess(elapsed, 0.5)          # bounded by the 0.1 s timeout, not hung
        self.assertGreater(elapsed, 0.02)      # ...but not a busy-spin fast return
        con.timeout = None                     # pyserial's block-forever mode must
        t0 = time.monotonic()                  # ALSO pace (0.1 s default), not spin
        self.assertEqual(con.read(64), b'')
        elapsed = time.monotonic() - t0
        self.assertLess(elapsed, 0.5)
        self.assertGreater(elapsed, 0.02)
        con.timeout = 0.1

    def test_reset_input_buffer(self):
        con = self._console()
        self._read_until(con, b'hello from target')
        con.write(b'x')
        time.sleep(0.3)
        con.reset_input_buffer()
        self.assertEqual(con.in_waiting, 0)

    def test_write_after_close_raises_runtimeerror(self):
        con = self._console()
        con.close()
        with self.assertRaises(RuntimeError):
            con.write(b'x')

    def test_write_after_server_death_raises(self):
        # TCP accepts one send after peer death — write() must refuse instead of
        # "succeeding" into the void
        con = self._console(mode='die_after_greet')
        self._read_until(con, b'hello from target')
        end = time.monotonic() + 3
        while not con.eof and time.monotonic() < end:
            time.sleep(0.05)
        with self.assertRaises(RuntimeError):
            con.write(b'ping')

    def test_read_after_close_raises_runtimeerror(self):
        con = self._console()
        self._read_until(con, b'hello from target')
        con.close()
        with self.assertRaises(RuntimeError):
            con.read(1)

    def test_missing_jlinkexe_raises_runtimeerror(self):
        self._fake_path()
        os.environ['PATH'] = self._dir.name  # no python3 either, but JLinkExe fails first
        os.rename(f'{self._dir.name}/JLinkExe', f'{self._dir.name}/JLinkExe.off')
        self.addCleanup(os.rename, f'{self._dir.name}/JLinkExe.off', f'{self._dir.name}/JLinkExe')
        with self.assertRaises(RuntimeError):
            hil_util.JlinkRtt(BOARD, timeout=0.1)

    def test_close_reaps_the_server(self):
        con = self._console()
        proc = con._proc
        con.close()
        self.assertIsNotNone(proc.poll())      # no zombie, no probe held

    def test_cli_exits_when_server_dies(self):
        # --seconds 0 must end on server EOF (rc 1), not hang forever
        env = dict(os.environ, PATH=self._path, FAKE_JLINK_MODE='die_after_greet')
        r = subprocess.run([sys.executable, str(CLI),
                            '--backend', 'jlink', '--probe', '000', '--device', 'FAKE', '--seconds', '0'],
                           env=env, capture_output=True, timeout=20)
        self.assertEqual(r.returncode, 1)
        self.assertIn(b'hello from target', r.stdout)
        self.assertIn(b'server closed', r.stderr)

    def test_peer_reset_latches_eof(self):
        # a killed server closes with RST when bytes are unread; the read side must
        # LATCH eof (so the harness's `assert not ser.eof` triage fires) and never
        # leak ConnectionResetError/ValueError to in_waiting/eof callers
        con = self._console(mode='rst')
        # rst mode sends only the banner (it RSTs on first input) -- wait for the
        # banner tail, not target output that never comes
        self._read_until(con, b'Process: JLinkExe')
        con.write(b'x')                       # fake resets the connection on input
        end = time.monotonic() + 3
        try:
            while not con.eof and time.monotonic() < end:
                con.in_waiting                # must not raise across the RST
                time.sleep(0.05)
        except Exception as e:                # noqa: BLE001 - the regression this guards
            self.fail(f'{type(e).__name__} escaped the latch-only contract: {e}')
        self.assertTrue(con.eof)
        with self.assertRaises(hil_util.RttError):
            con.write(b'y')                   # dead server refuses writes

    def test_write_timeout_env_rejects_inf(self):
        # hil_util's twin rejects inf for the same reason: an unbounded write is what
        # this knob exists to bound
        import importlib.util as ilu
        from pathlib import Path as _P
        spec = ilu.spec_from_file_location('rtt_env_probe', _P(CLI))
        mod = ilu.module_from_spec(spec)
        old = os.environ.get('HIL_SERIAL_WRITE_TIMEOUT')
        os.environ['HIL_SERIAL_WRITE_TIMEOUT'] = 'inf'
        self.addCleanup(lambda: os.environ.__setitem__('HIL_SERIAL_WRITE_TIMEOUT', old)
                        if old is not None else os.environ.pop('HIL_SERIAL_WRITE_TIMEOUT', None))
        spec.loader.exec_module(mod)
        self.assertEqual(mod.RTT_WRITE_TIMEOUT, 10)

    def test_cli_rejects_bad_seconds_and_jlink_channel(self):
        def run(*a):
            return subprocess.run([sys.executable, str(CLI), *a], capture_output=True, timeout=15)
        for bad in ('-5', 'nan'):
            r = run('--backend', 'jlink', '--probe', '000', '--device', 'FAKE', '--seconds', bad)
            self.assertEqual(r.returncode, 2, f'--seconds {bad} was accepted')
        # the jlink telnet route serves channel 0 only; asking for another is an error,
        # not silence (--dump can read any ring, so it stays allowed there)
        r = run('--backend', 'jlink', '--probe', '000', '--device', 'FAKE', '--channel', '1')
        self.assertEqual(r.returncode, 2)
        self.assertIn(b'channel 0 only', r.stderr)
        # a negative index would walk backwards off aUp[] (dump route included)
        r = run('--backend', 'jlink', '--probe', '000', '--device', 'FAKE', '--channel', '-1')
        self.assertEqual(r.returncode, 2)
        self.assertIn(b'>= 0', r.stderr)

    def test_pyserial_surface_contracts(self):
        con = self._console()
        self._read_until(con, b'hello from target')
        con.write(b'abcdef')
        self._read_until(con, b'ABC')          # echo queued
        before = con.in_waiting
        self.assertEqual(con.read(0), b'')     # pyserial: consumes nothing
        self.assertEqual(con.read(-1), b'')    # never hand over/destroy bytes
        self.assertEqual(con.in_waiting, before)
        con.timeout = None                     # pyserial: block until satisfied
        con.write(b'xy')                       # fresh echo guarantees the read returns
        self.assertEqual(len(con.read(2)), 2)
        con.timeout = 0.1
        con.close()
        with self.assertRaises(hil_util.RttError):
            con.in_waiting                     # closed console reports closed, not healthy
        self.assertTrue(con.eof)

    def test_context_manager_closes(self):
        self._fake_path()
        with hil_util.JlinkRtt(BOARD, timeout=0.1) as con:
            proc = con._proc
        self.assertIsNotNone(proc.poll())      # __exit__ released the probe

    def test_staging_and_banner_coupling(self):
        # tripwires for couplings no import-walk can see:
        # (a) hil_ci.sh must stage tools/rtt.py -- hil_util exec_module's it, so an
        #     unstaged rig tree kills every harness import
        hil_ci = (Path(__file__).resolve().parents[1] / 'hil_ci.sh').read_text()
        self.assertIn('tools/rtt.py', hil_ci)
        # (b) the shared RTT banner filter must drop ALL THREE J-Link banner lines,
        #     including the middle one, which is the PROBE MODEL string and in
        #     libjlinkarm carries no 'SEGGER ' prefix (J-Link OH3, J-Trace H9...)
        banner_re = hil_util.RTT_BANNER_RE
        for line in ('SEGGER J-Link V9.66 - Real time terminal output',
                     'SEGGER J-Link LPC-Link 2 V1.0, SN=611000000',
                     'J-Link OH3 V1.0, SN=123456789',
                     'J-Trace H9 V2.0, SN=123456789002',
                     'Process: JLinkExe'):
            self.assertTrue(banner_re.match(line), f'banner line not filtered: {line!r}')
        for line in ('Hello from TinyUSB', 'USBD init on controller 0',
                     'ID 1a86:8010 SN 7FD88F0604B5', 'echo:p'):
            self.assertFalse(banner_re.match(line), f'target line wrongly filtered: {line!r}')

    def test_pool_check_dead_rtt_board_is_not_alive(self):
        # JLinkExe's banner alone must not score a dead board 'alive': pool_check's
        # rtt aliveness judges only target bytes (the bug: unfiltered, the banner
        # made `not boardtest_output(data)` true on the first poll)
        sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        from helper import hil_pool_check
        # a dead board burns the whole poll window; the verdict is the same at 0.5 s
        self.addCleanup(setattr, hil_pool_check, 'SERIAL_WAIT', hil_pool_check.SERIAL_WAIT)
        hil_pool_check.SERIAL_WAIT = 0.5
        self._fake_path()
        os.environ['FAKE_JLINK_MODE'] = 'banner_only'
        self.addCleanup(os.environ.pop, 'FAKE_JLINK_MODE', None)
        board = dict(BOARD, name='deadboard', logger='rtt')
        got = hil_pool_check.check_host_serial(board, do_reset=False, want_hello=True)
        self.assertEqual(got, b'')            # dead, not "alive on banner"

    def test_cli_arg_contract(self):
        # --backend is explicit (no default); vid-pid is openocd-only; the openocd
        # backend accepts --addr instead of --elf and --vid-pid instead of --probe
        def run(*a, inp=b''):
            return subprocess.run([sys.executable, str(CLI), *a],
                                  input=inp, capture_output=True, timeout=15)
        r = run('--probe', '000', '--device', 'FAKE')          # no --backend
        self.assertEqual(r.returncode, 2)
        self.assertIn(b'--backend', r.stderr)
        r = run('--backend', 'jlink', '--probe', '000', '--device', 'FAKE', '--vid-pid', '0x1 0x2')
        self.assertEqual(r.returncode, 2)                       # vid-pid is openocd-only
        r = run('--backend', 'openocd', '--cfg', '-f x.cfg', '--addr', '0x20000000')
        self.assertEqual(r.returncode, 2)                       # needs --probe or --vid-pid
        self.assertIn(b'vid-pid', r.stderr)
        r = run('--backend', 'openocd', '--probe', '000', '--cfg', '-f x.cfg', '--addr', 'nothex')
        self.assertEqual(r.returncode, 2)
        self.assertIn(b'hex', r.stderr)

    def test_cli_interactive_echo(self):
        env = dict(os.environ, PATH=self._path)
        r = subprocess.run([sys.executable, str(CLI),
                            '--backend', 'jlink', '--probe', '000', '--device', 'FAKE', '--seconds', '2', '-i'],
                           env=env, input=b'hi', capture_output=True, timeout=20)
        self.assertEqual(r.returncode, 0)
        self.assertIn(b'HI', r.stdout)         # bytes forwarded without needing a newline
        self.assertNotIn(b'never forwarded', r.stderr)   # forwarding happened: no false alarm

    def test_cli_interactive_input_held_until_output(self):
        # input piped at process start must survive the server's control-block hunt
        # (the real JLinkExe drops client bytes until the block is found — measured
        # on the rig: instant 'ping' lost, delayed 'ping' echoed)
        env = dict(os.environ, PATH=self._path, FAKE_JLINK_MODE='late_cb')
        r = subprocess.run([sys.executable, str(CLI),
                            '--backend', 'jlink', '--probe', '000', '--device', 'FAKE', '--seconds', '3', '-i'],
                           env=env, input=b'hi', capture_output=True, timeout=25)
        self.assertEqual(r.returncode, 0)
        self.assertIn(b'HI', r.stdout)

    def test_cli_interactive_no_input_diagnostic(self):
        # -i with stdin closed immediately: the diagnostic must say stdin was never
        # forwarded (true), keyed on actual forwarding -- not on the attach gate,
        # which releases after 5 s and forwards anyway on longer runs
        env = dict(os.environ, PATH=self._path, FAKE_JLINK_MODE='banner_only')
        r = subprocess.run([sys.executable, str(CLI),
                            '--backend', 'jlink', '--probe', '000', '--device', 'FAKE', '--seconds', '1', '-i'],
                           env=env, input=b'', capture_output=True, timeout=20)
        self.assertEqual(r.returncode, 0)
        self.assertIn(b'never forwarded', r.stderr)
        self.assertIn(b'no target output', r.stderr)

    def test_cli_downstream_pipe_close(self):
        # a real `rtt.py | head`-style consumer: close the read end mid-stream
        # and the CLI must exit 0 via its BrokenPipe path, not traceback (this test
        # fails if the handler is removed — subprocess.run capture can't cover it)
        env = dict(os.environ, PATH=self._path, FAKE_JLINK_MODE='tick')
        p = subprocess.Popen([sys.executable, str(CLI),
                              '--backend', 'jlink', '--probe', '000', '--device', 'FAKE', '--seconds', '8'],
                             env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        p.stdout.read(10)          # let it stream a little
        p.stdout.close()           # downstream hangs up
        rc = p.wait(timeout=20)
        err = p.stderr.read()
        p.stderr.close()
        self.assertEqual(rc, 0, err)
        self.assertNotIn(b'Traceback', err)

    def test_cli_feeder_races_shutdown(self):
        # a feeder still writing when --seconds expires must not crash the CLI
        # (pump thread vs close() race: historically tracebacks and SIGABRT rc 134)
        env = dict(os.environ, PATH=self._path)
        for _ in range(3):
            p = subprocess.Popen([sys.executable, str(CLI),
                                  '--backend', 'jlink', '--probe', '000', '--device', 'FAKE', '--seconds', '1', '-i'],
                                 env=env, stdin=subprocess.PIPE, stdout=subprocess.DEVNULL,
                                 stderr=subprocess.PIPE)
            try:
                while True:
                    p.stdin.write(b'hi\n')
                    p.stdin.flush()
                    time.sleep(0.01)
            except (BrokenPipeError, OSError):
                pass
            rc = p.wait(timeout=20)
            err = p.stderr.read()
            p.stderr.close()
            with contextlib_suppress(OSError, ValueError):
                p.stdin.close()
            self.assertEqual(rc, 0, err)
            self.assertNotIn(b'Exception in thread', err)



class StripBanner(unittest.TestCase):
    # both harness consumers (device_info verdict, pool_check aliveness) judge
    # target-aliveness through this ONE filter -- pin its shape here
    def test_drops_banner_keeps_target(self):
        raw = (b'SEGGER J-Link V9.66 - Real time terminal output\r\n'
               b'J-Link OH3 V1.0, SN=123456789\r\nProcess: JLinkExe\r\n'
               b'Hello from TinyUSB\r\n')
        self.assertEqual(hil_util.strip_banner(raw), b'Hello from TinyUSB')

    def test_complete_only_drops_split_banner_fragment(self):
        # a poll loop can catch the banner mid-line at a read boundary; the
        # fragment must not defeat the prefix regex and score as target output
        frag = b'SEGGER J-Link V9.66 - Real time terminal output\r\nProce'
        self.assertEqual(hil_util.strip_banner(frag, complete_only=True), b'')
        # the final verdict keeps a genuine unterminated target tail
        self.assertEqual(hil_util.strip_banner(b'tud_task\r\nrunn'), b'tud_task\nrunn')
        self.assertEqual(hil_util.strip_banner(b'', complete_only=True), b'')


# Serves like `openocd ... -c "rtt server start PORT CH"`: parses the port from its
# single shell-quoted command line, greets, echoes uppercased. No banner (matches the
# real openocd rtt server, which sends target data only).
FAKE_OPENOCD = '''#!/usr/bin/env python3
import os, re, socket, sys, threading, time
if os.environ.get('FAKE_OPENOCD_ARGV'):
    open(os.environ['FAKE_OPENOCD_ARGV'], 'w').write(' '.join(sys.argv))
port = int(re.search(r'rtt server start (\\d+)', ' '.join(sys.argv)).group(1))
srv = socket.socket(); srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(('127.0.0.1', port)); srv.listen(1)
conn, _ = srv.accept()
conn.sendall(b'hello from target\\r\\n')
while True:
    d = conn.recv(4096)
    if not d: break
    conn.sendall(d.upper())
'''


@unittest.skipIf(os.name == 'nt', 'POSIX PATH/exec semantics')
class OpenocdRttFakeProbe(unittest.TestCase):
    """The openocd-backend class shares its whole read/write/eof contract with
    JlinkRtt via the base class (covered above); this exercises the parts it owns:
    spawn/connect, echo round-trip, teardown."""

    @classmethod
    def setUpClass(cls):
        cls._dir = tempfile.TemporaryDirectory()
        fake = Path(cls._dir.name) / 'openocd'
        fake.write_text(FAKE_OPENOCD)
        fake.chmod(0o755)
        cls._path = f'{cls._dir.name}{os.pathsep}{os.environ["PATH"]}'

    @classmethod
    def tearDownClass(cls):
        cls._dir.cleanup()

    def _fake_path(self):
        self.addCleanup(os.environ.__setitem__, 'PATH', os.environ['PATH'])
        os.environ['PATH'] = self._path

    def test_reset_before_attach_shapes_the_command(self):
        # SystemView-style consumers need the server draining WHEN the target boots
        # (its Init record is emitted once); the opt-in flag must put `reset run`
        # between init and rtt setup, and must not appear otherwise
        self._fake_path()
        argv_file = os.path.join(self._dir.name, 'argv.txt')
        os.environ['FAKE_OPENOCD_ARGV'] = argv_file
        self.addCleanup(os.environ.pop, 'FAKE_OPENOCD_ARGV', None)
        for flag, want in ((True, True), (False, False)):
            con = hil_util.OpenocdRtt('-f fake.cfg', 0x20000000, 1, serial_no='000',
                                      reset_before_attach=flag)
            try:
                argv = Path(argv_file).read_text()
            finally:
                con.close()
            self.assertEqual('reset run' in argv, want, argv)
            if want:   # ordering is the whole point: reset, settle, THEN attach
                self.assertLess(argv.index('reset run'), argv.index('rtt setup'), argv)
                self.assertIn('sleep 2000', argv)
            self.assertIn('rtt server start', argv)
            self.assertTrue(argv.rstrip().endswith('1'), argv)   # channel threaded through

    def test_openocd_route_echo_and_teardown(self):
        self._fake_path()
        con = hil_util.OpenocdRtt('-f fake.cfg', 0x20000000, 0,
                                  serial_no='000', vid_pid='0x1234 0x5678')
        self.addCleanup(con.close)
        out = b''
        end = time.monotonic() + 3
        while b'hello from target' not in out and time.monotonic() < end:
            out += con.read(con.in_waiting or 1)
        self.assertIn(b'hello from target', out)
        con.write(b'ping')
        end = time.monotonic() + 3
        while b'PING' not in out and time.monotonic() < end:
            out += con.read(con.in_waiting or 1)
        self.assertIn(b'PING', out)
        proc = con._proc
        con.close()
        self.assertIsNotNone(proc.poll())      # no zombie, no probe held
        with self.assertRaises(RuntimeError):
            con.write(b'x')                    # same post-close contract as JlinkRtt


if __name__ == '__main__':
    unittest.main()
