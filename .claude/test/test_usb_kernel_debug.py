"""Tests for .claude/skills/usb-kernel-debug/scripts/usbcap.py: bus resolution refuses
to guess between buses, and the CLI reports tshark failures explicitly."""
import importlib.util
import os
import stat
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / 'skills' / 'usb-kernel-debug' / 'scripts' / 'usbcap.py'
spec = importlib.util.spec_from_file_location('usbcap', SCRIPT)
usbcap = importlib.util.module_from_spec(spec)
spec.loader.exec_module(usbcap)

LSUSB = """\
Bus 001 Device 001: ID 1d6b:0002 Linux Foundation 2.0 root hub
Bus 001 Device 004: ID 046d:c52b Logitech, Inc. Unifying Receiver
Bus 003 Device 007: ID cafe:4010 TinyUSB TinyUSB usbtest
Bus 003 Device 009: ID cafe:4001 TinyUSB TinyUSB CDC MSC
Bus 005 Device 012: ID cafe:4010 TinyUSB TinyUSB usbtest
Bus 005 Device 013: ID 1366:0101 SEGGER J-Link PLUS
"""


class ResolveTest(unittest.TestCase):
    def test_bus_numbers_and_auto_pass_through(self):
        self.assertEqual(usbcap.resolve('3', LSUSB), 3)
        self.assertEqual(usbcap.resolve('0', LSUSB), 0)
        self.assertEqual(usbcap.resolve('auto', LSUSB), 0)

    def test_a_single_match_names_its_bus(self):
        self.assertEqual(usbcap.resolve('046d:c52b', LSUSB), 1)
        self.assertEqual(usbcap.resolve('1366:', LSUSB), 5)
        self.assertEqual(usbcap.resolve('CAFE:4001', LSUSB), 3)

    def test_matches_on_one_bus_are_not_ambiguous(self):
        self.assertEqual(usbcap.resolve('1d6b:', LSUSB), 1)

    def test_matches_across_buses_are_refused_with_the_list(self):
        with self.assertRaises(usbcap.Ambiguous) as ctx:
            usbcap.resolve('cafe:', LSUSB)
        self.assertEqual([m[0] for m in ctx.exception.matches], [3, 3, 5])
        self.assertIn('bus 5 device 12: cafe:4010 TinyUSB TinyUSB usbtest', str(ctx.exception))
        with self.assertRaises(usbcap.Ambiguous):
            usbcap.resolve('cafe:4010', LSUSB)

    def test_no_match_and_bad_selector_are_distinct_errors(self):
        with self.assertRaises(LookupError):
            usbcap.resolve('1a86:8010', LSUSB)
        for bad in ('cafe', 'cafe:40', 'xyz:1234', '', '3a'):
            with self.assertRaises(ValueError):
                usbcap.resolve(bad, LSUSB)


class CliTest(unittest.TestCase):
    """lsusb and tshark are stubbed on PATH; tshark records its arguments."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.bin = Path(self.tmp.name) / 'bin'
        self.bin.mkdir()
        self.log = Path(self.tmp.name) / 'tshark.log'
        self._stub('lsusb', f"#!/bin/sh\ncat <<'EOF'\n{LSUSB}EOF\n")
        self.tshark_rc = Path(self.tmp.name) / 'tshark.rc'
        self.tshark_rc.write_text('0')
        self._stub('tshark', '#!/bin/sh\n'
                   f'echo "$@" >> {self.log}\n'
                   f'rc=$(cat {self.tshark_rc})\n'
                   '[ "$rc" = 0 ] || { echo "tshark: The capture session could not be initiated" >&2; exit $rc; }\n'
                   'case "$1" in\n'
                   '  -i) out=""; while [ $# -gt 0 ]; do [ "$1" = -w ] && out=$2; shift; done; : > "$out";;\n'
                   '  -r) printf "1 0.000 host -> 3.7.0 USB 64 URB_CONTROL in\\n2 0.001 3.7.0 -> host USB 64 URB_CONTROL in\\n";;\n'
                   'esac\n')

    def tearDown(self):
        self.tmp.cleanup()

    def _stub(self, name, body):
        path = self.bin / name
        path.write_text(body)
        path.chmod(path.stat().st_mode | stat.S_IEXEC)

    def _run(self, *args):
        env = {**os.environ, 'PATH': f'{self.bin}:{os.environ["PATH"]}'}
        return subprocess.run([sys.executable, str(SCRIPT), *args], capture_output=True, text=True, env=env)

    def test_capture_resolves_the_bus_and_counts_packets(self):
        out = Path(self.tmp.name) / 'cap.pcapng'
        r = self._run('046d:c52b', '3', str(out), '--snaplen', '128')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('capturing usbmon1 for 3s', r.stdout)
        self.assertIn('(2 packets)', r.stdout)
        self.assertTrue(out.exists())
        self.assertIn(f'-i usbmon1 -a duration:3 -w {out} -s 128', self.log.read_text())

    def test_ambiguous_selector_captures_nothing(self):
        r = self._run('cafe:')
        self.assertEqual(r.returncode, 1)
        self.assertIn('several buses; pass the bus number', r.stderr)
        self.assertIn('bus 3 device 7', r.stderr)
        self.assertIn('bus 5 device 12', r.stderr)
        self.assertFalse(self.log.exists(), 'tshark must not run')

    def test_missing_device_and_bad_input_fail_before_capture(self):
        self.assertIn('no device matching', self._run('1a86:8010').stderr)
        self.assertIn('bad target', self._run('nope').stderr)
        self.assertIn('seconds must be positive', self._run('3', '0').stderr)
        self.assertFalse(self.log.exists())

    def test_tshark_failure_is_reported_with_the_access_hint(self):
        self.tshark_rc.write_text('2')
        r = self._run('3', '1')
        self.assertEqual(r.returncode, 1)
        self.assertIn('tshark exited 2', r.stderr)
        self.assertIn('could not be initiated', r.stderr)
        self.assertIn('/dev/usbmon3 access', r.stderr)


if __name__ == '__main__':
    unittest.main()
