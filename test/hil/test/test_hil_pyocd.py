#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""The pyocd flasher, its VID/PID-filtering launcher (test/hil/pyocd/run_pyocd.py) and its user script.

The launcher cases run it against fake usb/libusb_package/pyocd packages whose find() applies
keyword filters before custom_match, as pyusb's does. They prove the plumbing only; that the
real pyusb and pyocd open nothing else is the strace evidence from the rig.
"""
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

HIL_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(HIL_DIR))

import hil_flash    # noqa: E402

LAUNCHER = HIL_DIR / 'pyocd' / 'run_pyocd.py'
USER_SCRIPT = HIL_DIR / 'pyocd' / 'user_script.py'
PROBE = [0x1fc9, 0x0090]
OTHER = [0x1366, 0x1024]

FAKES = {
    'usb/__init__.py': '',
    'usb/core.py': '''
import json, os
class Device:
    def __init__(self, vid, pid):
        self.idVendor, self.idProduct = vid, pid
def find(find_all=False, backend=None, custom_match=None, **kwargs):
    devs = [Device(*v) for v in json.loads(os.environ['FAKE_USB_DEVICES'])]
    found = [d for d in devs if all(getattr(d, k) == v for k, v in kwargs.items())
             and (custom_match is None or custom_match(d))]
    return found if find_all else (found[0] if found else None)
''',
    'pyocd/__init__.py': '',
    'pyocd/probe/__init__.py': '',
    'pyocd/probe/aggregator.py': "PROBE_CLASSES = {'cmsisdap': 1, 'jlink': 2, 'stlink': 3}\n",
    'pyocd/probe/pydapaccess/__init__.py': '',
    'pyocd/probe/pydapaccess/interface/__init__.py': '',
    'pyocd/__main__.py': '''
import json, os, sys
def main():
    from pyocd.probe.aggregator import PROBE_CLASSES
    from pyocd.probe.pydapaccess.interface import pyusb_backend, pyusb_v2_backend
    plugin = next((a.split('=', 1)[1].split(':')[0] for a in sys.argv if a.startswith('--probe=')), 'cmsisdap')
    if plugin not in PROBE_CLASSES:
        sys.exit(f"unknown debug probe type '{plugin}'")
    touched = []
    for mod in (pyusb_backend, pyusb_v2_backend):
        mod.usb_find(find_all=True, custom_match=lambda d: touched.append([d.idVendor, d.idProduct]))
    print(json.dumps({'touched': touched, 'classes': sorted(PROBE_CLASSES),
                      'backend': os.environ.get('PYOCD_USB_BACKEND'), 'argv': sys.argv[1:]}))
    sys.exit(0)
''',
}
BACKEND = '''
import os
if os.environ.get('FAKE_BIND_ELSEWHERE'):
    usb_find = lambda *a, **k: []
else:
    try:
        from libusb_package import find as usb_find
    except ImportError:
        from usb.core import find as usb_find
'''
LIBUSB_PACKAGE = '''
def find(*args, **kwargs):
    import usb.core
    return usb.core.find(*args, **kwargs)
'''


@unittest.skipUnless(sys.platform.startswith('linux'), 'the launcher refuses other platforms')
class Launcher(unittest.TestCase):
    def setUp(self):
        td = tempfile.TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.root = Path(td.name)
        files = {**FAKES, 'pyocd/probe/pydapaccess/interface/pyusb_backend.py': BACKEND,
                 'pyocd/probe/pydapaccess/interface/pyusb_v2_backend.py': BACKEND}
        for rel, body in files.items():
            (self.root / 'fakes' / rel).parent.mkdir(parents=True, exist_ok=True)
            (self.root / 'fakes' / rel).write_text(body)
        # an interpreter may have the real libusb_package; the shadow makes it absent
        for where, body in (('libusb', LIBUSB_PACKAGE), ('nolibusb', "raise ImportError('absent')\n")):
            (self.root / where / 'libusb_package').mkdir(parents=True)
            (self.root / where / 'libusb_package' / '__init__.py').write_text(body)

    def run_launcher(self, *args, libusb_package=True, **env):
        path = [str(self.root / 'fakes'), str(self.root / ('libusb' if libusb_package else 'nolibusb'))]
        env = {**os.environ, 'PYTHONPATH': os.pathsep.join(path), 'PYOCD_USB_BACKEND': 'hidapiusb',
               'FAKE_USB_DEVICES': json.dumps([OTHER, PROBE, [0x0483, 0x3752]]), **env}
        return subprocess.run([sys.executable, str(LAUNCHER), *args], env=env,
                              capture_output=True, text=True, timeout=30)

    def test_only_the_filtered_probe_reaches_pyocds_matcher(self):
        for libusb_package in (True, False):
            with self.subTest(libusb_package=libusb_package):
                r = self.run_launcher('0x1fc9', '0x0090', 'flash', '-u', 'cmsisdap:X', '--script', 'roster.py',
                                      'fw.elf', libusb_package=libusb_package)
                self.assertEqual(r.returncode, 0, r.stderr)
                self.assertEqual(json.loads(r.stdout), {
                    'touched': [PROBE, PROBE],            # once per backend, nothing else
                    'classes': ['cmsisdap'],
                    'backend': 'pyusb',                   # hidapi would bypass the filter
                    # argparse keeps the last --script
                    'argv': ['flash', '-u', 'cmsisdap:X', '--script', 'roster.py', 'fw.elf',
                             '--no-config', '--script', str(USER_SCRIPT)]})

    def test_a_selector_naming_another_plugin_fails(self):
        r = self.run_launcher('0x1fc9', '0x0090', 'reset', '-u', 'cmsisdap:X', '--probe=jlink:Y')
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("unknown debug probe type 'jlink'", r.stderr)

    def test_a_backend_that_did_not_bind_the_filtered_find_stops_before_discovery(self):
        r = self.run_launcher('0x1fc9', '0x0090', 'flash', 'fw.elf', FAKE_BIND_ELSEWHERE='1')
        self.assertEqual((r.returncode, r.stdout), (2, ''))
        self.assertIn('is not the filtered find', r.stderr)

    def test_a_malformed_vid_pid_stops_before_pyocd(self):
        for args in (('flash', 'fw.elf'), ('0x1fc9',), ()):
            r = self.run_launcher(*args)
            self.assertEqual((r.returncode, r.stdout), (2, ''), args)

    def test_a_caller_asking_for_another_vid_is_refused(self):
        import importlib.util
        spec = importlib.util.spec_from_file_location('run_pyocd', LAUNCHER)
        run_pyocd = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(run_pyocd)
        find = run_pyocd._filter_vid_pid(lambda **kw: kw, 0x1fc9, 0x0090)
        self.assertEqual(find(find_all=True), {'find_all': True, 'idVendor': 0x1fc9, 'idProduct': 0x0090})
        with self.assertRaises(RuntimeError):
            find(idVendor=0x1366)


class Flasher(unittest.TestCase):
    FLASHER = {'name': 'pyocd', 'uid': 'GSA0CQEQ', 'vid_pid': '0x1fc9 0x0090',
               'args': '-t lpc55s28'}

    def capture(self, fn, *args, **kw):
        seen = {}

        def fake(cmd, **k):
            seen['cmd'], seen['kw'] = cmd, k
            return subprocess.CompletedProcess(cmd, 0, b'', b'')
        with mock.patch.object(hil_flash.hil_util, 'run_cmd', fake), \
             mock.patch.object(hil_flash, '_pyocd_python', return_value='/py'):
            fn({'name': 'b', 'flasher': self.FLASHER}, *args, **kw)
        return seen['cmd'], seen['kw']

    def test_flash_and_reset_run_the_launcher(self):
        head = ['/py', str(hil_flash.hil_util.TINYUSB_ROOT / 'test/hil/pyocd/run_pyocd.py'), '0x1fc9', '0x0090']
        tail = ['-W', '-u', 'cmsisdap:GSA0CQEQ', '-t', 'lpc55s28']
        for fn, fw, verb in ((hil_flash.flash_pyocd, ('/tmp/a b/fw.elf',), 'flash'),
                             (hil_flash.reset_pyocd, (), 'reset')):
            cmd, kw = self.capture(fn, *fw, timeout=7)
            self.assertEqual(cmd, [*head, verb, *tail, *fw])
            self.assertEqual(kw, {'timeout': 7})

    def test_an_unfiltered_entry_is_refused(self):
        for bad in ({'vid_pid': None}, {'vid_pid': '0x1fc9 0x0090 0x1fc9 0x0143'},
                    {'vid_pid': '1fc9:0090'}, {'uid': ''}):
            flasher = {**self.FLASHER, **bad}
            self.assertIsNone(hil_flash.pyocd_vid_pid(flasher), bad)
            self.assertFalse(hil_flash.convoy_safe(flasher), bad)
            with self.assertRaises(ValueError):
                hil_flash._pyocd_argv(flasher, 'reset')
        self.assertTrue(hil_flash.convoy_safe(self.FLASHER))

    def test_the_interpreter_comes_from_pyocds_shebang(self):
        with tempfile.TemporaryDirectory() as td:
            exe = Path(td) / 'pyocd'
            for shebang, want in ((f'#!{sys.executable}', sys.executable),
                                  ('#!/usr/bin/env python3', None), ('', None)):
                exe.write_text(f'{shebang}\nimport sys\n')
                exe.chmod(0o755)
                with mock.patch.dict(os.environ, {'PATH': td}):
                    if want:
                        self.assertEqual(hil_flash._pyocd_python(), want)
                    else:
                        self.assertRaises(RuntimeError, hil_flash._pyocd_python)
            exe.unlink()
            with mock.patch.dict(os.environ, {'PATH': td}):
                self.assertRaises(RuntimeError, hil_flash._pyocd_python)

    def test_hil_test_refuses_an_unfiltered_entry_before_touching_a_board(self):
        with tempfile.TemporaryDirectory() as td:
            cfg = Path(td) / 'c.json'
            cfg.write_text(json.dumps({'boards': [{'name': 'b', 'uid': 'X', 'tests': {'device': True},
                                                   'flasher': {'name': 'pyocd', 'uid': 'S'}}]}))
            r = subprocess.run([sys.executable, str(HIL_DIR / 'hil_test.py'), str(cfg)],
                               env={**os.environ, 'HIL_REPORT_DIR': td},
                               capture_output=True, text=True, timeout=60)
        self.assertEqual(r.returncode, 1)
        self.assertIn('a pyocd flasher needs a uid and one "vid_pid" pair', r.stdout)



class UserScript(unittest.TestCase):
    """set_reset_catch as pyocd calls it: the script's globals hold `target`, the delegate's
    falsy return keeps pyocd's own reset catch."""
    def load(self):
        lpc5500 = mock.MagicMock()
        lpc5500.DM_AP = 2
        lpc5500.CortexM_LPC5500 = type('CortexM_LPC5500', (), {})
        target = mock.MagicMock()
        target.aps = {0: 'ahb', 2: 'dm'}
        ns = {'target': target}
        with mock.patch.dict(sys.modules, {'pyocd': mock.MagicMock(), 'pyocd.target': mock.MagicMock(),
                                           'pyocd.target.family': mock.MagicMock(),
                                           'pyocd.target.family.target_lpc5500': lpc5500}):
            exec(compile(USER_SCRIPT.read_text(), str(USER_SCRIPT), 'exec'), ns)
        return ns, target, lpc5500.CortexM_LPC5500

    def test_an_lpc55_core_is_unlocked_through_the_debug_mailbox_first(self):
        ns, target, lpc55 = self.load()
        self.assertFalse(ns['set_reset_catch'](lpc55(), 'sw'))
        target.unlock.assert_called_once_with('dm')

    def test_any_other_core_is_left_to_pyocd(self):
        ns, target, _ = self.load()
        self.assertFalse(ns['set_reset_catch'](object(), 'sw'))
        target.unlock.assert_not_called()


if __name__ == '__main__':
    unittest.main()
