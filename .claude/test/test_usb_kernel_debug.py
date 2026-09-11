"""Tests for the usb-kernel-debug scripts: usbcap.py's bus resolution refuses
to guess between buses and reports tshark failures explicitly; usb_dyndbg.sh
reads the print flag from a fixture control file and helps without debugfs."""
import importlib.util
import os
import stat
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[1] / 'skills' / 'usb-kernel-debug' / 'scripts' / 'usbcap.py'
DYNDBG = SCRIPT.with_name('usb_dyndbg.sh')
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
    def test_bus_numbers_and_auto_never_run_lsusb(self):
        never = lambda: self.fail('lsusb must not run')
        self.assertEqual(usbcap.resolve('3', never), 3)
        self.assertEqual(usbcap.resolve('0', never), 0)
        self.assertEqual(usbcap.resolve('auto', never), 0)

    def test_a_single_match_names_its_bus(self):
        self.assertEqual(usbcap.resolve('046d:c52b', lambda: LSUSB), 1)
        self.assertEqual(usbcap.resolve('1366:', lambda: LSUSB), 5)
        self.assertEqual(usbcap.resolve('CAFE:4001', lambda: LSUSB), 3)

    def test_matches_on_one_bus_are_not_ambiguous(self):
        self.assertEqual(usbcap.resolve('1d6b:', lambda: LSUSB), 1)

    def test_matches_across_buses_are_refused_with_the_list(self):
        with self.assertRaises(ValueError) as ctx:
            usbcap.resolve('cafe:', lambda: LSUSB)
        self.assertIn('several buses; pass the bus number', str(ctx.exception))
        self.assertIn('bus 3 device 7: cafe:4010', str(ctx.exception))
        self.assertIn('bus 5 device 12: cafe:4010 TinyUSB TinyUSB usbtest', str(ctx.exception))
        with self.assertRaises(ValueError):
            usbcap.resolve('cafe:4010', lambda: LSUSB)

    def test_no_match_and_bad_selector_say_which(self):
        with self.assertRaisesRegex(ValueError, 'no device matching'):
            usbcap.resolve('1a86:8010', lambda: LSUSB)
        for bad in ('cafe', 'cafe:40', 'xyz:1234', '', '3a'):
            with self.assertRaisesRegex(ValueError, 'bad target'):
                usbcap.resolve(bad, lambda: LSUSB)


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
                   'out=""; while [ $# -gt 0 ]; do [ "$1" = -w ] && out=$2; shift; done; : > "$out"\n')
        self._stub('capinfos', '#!/bin/sh\necho "Number of packets:   2"\n')

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


CONTROL = '''\
# filename:lineno [module]function flags format
init/main.c:1116 [main]initcall_blacklist =p "blacklisting initcall %s\\n"
drivers/usb/core/hub.c:100 [usbcore]hub_port_init =p "port %d reset\\n"
drivers/usb/core/hub.c:200 [usbcore]hub_events =pfl "hub event\\n"
drivers/usb/core/hub.c:300 [usbcore]hub_quiesce =_ "quiesce\\n"
drivers/usb/host/xhci-hub.c:559 [xhci_hcd]xhci_disable_port =_ "Ignoring request\\n"
drivers/usb/host/xhci-ring.c:10 [xhci_hcd]xhci_ring =flmt "no print flag\\n"
'''


class DyndbgTest(unittest.TestCase):
    """The control file is a fixture; the kernel format is file:line [module]function =flags "format"."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.ctl = Path(self.tmp.name) / 'dynamic_debug' / 'control'
        self.ctl.parent.mkdir()
        self.ctl.write_text(CONTROL)

    def tearDown(self):
        self.tmp.cleanup()

    def _run(self, *args, ctl=None):
        env = {**os.environ, 'USB_DYNDBG_CTL': str(ctl or self.ctl)}
        return subprocess.run(['bash', str(DYNDBG), *args], capture_output=True, text=True, env=env)

    def test_help_works_without_debugfs(self):
        r = self._run('--help', ctl=Path(self.tmp.name) / 'missing' / 'dynamic_debug' / 'control')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('usbcore xhci_hcd', r.stdout)
        self.assertIn('lsusb -t', r.stdout)
        r = self._run(ctl=Path(self.tmp.name) / 'missing' / 'dynamic_debug' / 'control')
        self.assertEqual(r.returncode, 2, 'no action is a usage error')
        self.assertIn('usage:', r.stderr)

    def test_missing_debugfs_and_unreadable_debugfs_are_distinct(self):
        missing = Path(self.tmp.name) / 'missing' / 'dynamic_debug' / 'control'
        missing.parent.parent.mkdir()
        r = self._run('status', ctl=missing)
        self.assertEqual(r.returncode, 1)
        self.assertIn('dynamic_debug unavailable', r.stderr)
        locked = Path(self.tmp.name) / 'locked'
        locked.mkdir(mode=0o000)
        r = self._run('status', ctl=locked / 'dynamic_debug' / 'control')
        locked.chmod(0o700)
        self.assertEqual(r.returncode, 1)
        self.assertIn('run with sudo', r.stderr)

    def test_status_lists_only_print_enabled_sites_of_allowlisted_modules(self):
        r = self._run('status')
        self.assertEqual(r.returncode, 0, r.stderr)
        lines = r.stdout.splitlines()
        self.assertEqual([l.split()[0] for l in lines], ['drivers/usb/core/hub.c:100', 'drivers/usb/core/hub.c:200'])
        self.assertNotIn('[main]', r.stdout, 'a non-allowlisted module is out of scope')
        r = self._run('status', 'xhci_hcd')
        self.assertEqual(r.returncode, 0)
        self.assertEqual(r.stdout.strip(), '(no print sites enabled for xhci_hcd)', '=_ and =flmt are not print-enabled')
        r = self._run('status', 'usbcore')
        self.assertIn('=pfl', r.stdout)
        self.assertNotIn('quiesce', r.stdout)

    def test_status_refuses_a_module_outside_the_allowlist(self):
        r = self._run('status', 'main')
        self.assertEqual(r.returncode, 1)
        self.assertIn('not allowlisted: main', r.stderr)

    def test_status_reports_an_unreadable_control_file(self):
        self.ctl.chmod(0o000)
        r = self._run('status')
        self.ctl.chmod(0o600)
        self.assertEqual(r.returncode, 1)
        self.assertIn('cannot read', r.stderr)

    def test_a_read_error_is_not_reported_as_no_sites(self):
        broken = Path(self.tmp.name) / 'dir' / 'dynamic_debug' / 'control'
        broken.mkdir(parents=True)  # readable by stat, unreadable as a file
        r = self._run('status', ctl=broken)
        self.assertEqual(r.returncode, 1)
        self.assertIn('cannot read', r.stderr)
        self.assertNotIn('no print sites', r.stdout)

    def test_on_and_off_write_one_command_per_module_and_refuse_others(self):
        r = self._run('on', 'usbcore', 'dwc2')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stdout, 'dynamic debug on: usbcore\ndynamic debug on: dwc2\n')
        self.assertEqual(self.ctl.read_text(), 'module dwc2 +p\n', 'each write replaces the file: the last command')
        self._run('off', 'dwc2')
        self.assertEqual(self.ctl.read_text(), 'module dwc2 -p\n')
        r = self._run('on', 'usbcore', 'ext4')
        self.assertEqual(r.returncode, 1)
        self.assertIn('not allowlisted: ext4', r.stderr)
        self.assertEqual(self.ctl.read_text(), 'module dwc2 -p\n', 'nothing written when any module is refused')
        self.assertEqual(self._run('on').returncode, 2)


if __name__ == '__main__':
    unittest.main()
