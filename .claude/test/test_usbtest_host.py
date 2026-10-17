"""run_case.py and wedge_drill.py drive usbfs, /proc and the usbtest kernel module: on a host
that is not Linux they refuse first thing, before importing the harness, with exit 2 (refused
before any hardware action)."""
import subprocess
import sys
import unittest
from pathlib import Path

SCRIPTS = Path(__file__).resolve().parents[1] / 'skills' / 'usbtest' / 'scripts'


@unittest.skipIf(sys.platform == 'linux', 'the refusal is for hosts that are not Linux')
class NotLinux(unittest.TestCase):
    def test_each_script_refuses_before_anything_else(self):
        for name in ('run_case.py', 'wedge_drill.py'):
            r = subprocess.run([sys.executable, str(SCRIPTS / name), '--board', 'x'],
                               capture_output=True, encoding='utf-8', timeout=60)
            self.assertEqual(r.returncode, 2, r.stderr)
            self.assertEqual(r.stderr, f'error: {name} needs a Linux host (usbfs, /proc, the usbtest kernel module)\n')
            self.assertEqual(r.stdout, '')


if __name__ == '__main__':
    unittest.main()
