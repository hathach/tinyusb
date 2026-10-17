"""run_case.py and wedge_drill.py drive usbfs, /proc and the usbtest kernel module: on a host
that is not Linux they parse their arguments (so --help works) and then refuse through their
verdict line, exit 2 (refused before any hardware action), before importing the harness."""
import json
import subprocess
import sys
import unittest
from pathlib import Path

SCRIPTS = Path(__file__).resolve().parents[1] / 'skills' / 'usbtest' / 'scripts'
ARGS = {'run_case.py': ['--config', 'rig.json', '--board', 'x', '--tests', '1', '--after', 'leave'],
        'wedge_drill.py': ['--config', 'rig.json', '--board', 'x']}


@unittest.skipIf(sys.platform == 'linux', 'the refusal is for hosts that are not Linux')
class NotLinux(unittest.TestCase):
    def run_script(self, name, *args):
        return subprocess.run([sys.executable, str(SCRIPTS / name), *args],
                              capture_output=True, encoding='utf-8', timeout=60)

    def test_each_script_refuses_with_its_verdict_line(self):
        for name, args in ARGS.items():
            with self.subTest(name):
                r = self.run_script(name, *args)
                error = f'{name} needs a Linux host (usbfs, /proc, the usbtest kernel module)'
                self.assertEqual(r.returncode, 2, r.stderr)
                self.assertEqual(r.stderr, f'error: {error}\n')
                verdict = json.loads(r.stdout.splitlines()[-1])
                self.assertEqual((verdict['board'], verdict['boardState'], verdict['error']), ('x', 'untouched', error))

    def test_help_still_works(self):
        for name in ARGS:
            with self.subTest(name):
                r = self.run_script(name, '--help')
                self.assertEqual(r.returncode, 0, r.stderr)
                self.assertIn('--board', r.stdout)

    def test_a_bad_command_line_is_argparse_s_error(self):
        for name in ARGS:
            with self.subTest(name):
                r = self.run_script(name, '--board', 'x')
                self.assertEqual((r.returncode, r.stdout), (2, ''))
                self.assertIn('the following arguments are required: --config', r.stderr)


if __name__ == '__main__':
    unittest.main()
