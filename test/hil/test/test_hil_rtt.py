#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Harness-side RTT contracts against a fake JLinkExe on PATH — no hardware, stdlib
# only. The console classes and the CLI themselves are tested where they live, in
# agentrc's tests/test_rtt.py against the same file. Run directly:
#   python3 test/hil/test/test_hil_rtt.py
import os
import sys
import tempfile
import unittest
from pathlib import Path

# the module under test lives in the parent dir's helper/ package
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from helper import hil_util

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

    def test_hil_util_reexports_the_rtt_classes(self):
        for name in ('JlinkRtt', 'OpenocdRtt', 'RttError', 'strip_banner', 'RTT_BANNER_RE'):
            self.assertTrue(hasattr(hil_util, name), name)

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


if __name__ == '__main__':
    unittest.main()
