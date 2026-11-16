#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Harness-side RTT contracts — no hardware, pyserial stubbed. The console classes and the CLI
# themselves are tested where they live, in agentrc's tests/test_rtt.py against the same
# file, which hil_test loads from the agentrc helper/agentrc.py selects. Run directly:
#   python3 test/hil/test/test_hil_rtt.py
import io
import json
import multiprocessing
import os
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from unittest import mock

HIL_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO = os.path.dirname(os.path.dirname(HIL_DIR))
sys.path.insert(0, HIL_DIR)
import usbtest_harness   # noqa: E402,F401 - stubs pyserial before hil_test imports it
import hil_test          # noqa: E402
from helper import agentrc   # noqa: E402

RTT_PY = 'skills/rtt/scripts/rtt.py'


def _raise_rtt_error(msg):
    raise hil_test.rtt.RttError(msg)


class RttHarnessContracts(unittest.TestCase):
    def test_hil_test_loads_the_agentrc_console_once(self):
        self.assertIs(sys.modules['agentrc_rtt'], hil_test.rtt)
        self.assertEqual(os.path.realpath(hil_test.rtt.__file__),
                         os.path.realpath(os.path.join(agentrc.identity()['root'], RTT_PY)))
        self.assertIs(hil_test.RTT_BANNER_RE, hil_test.rtt.RTT_BANNER_RE)

    def test_rtt_error_survives_the_fork_pool(self):
        # hil_test.py's pool is fork; a worker's RttError pickles back by its registered module name
        with multiprocessing.get_context('fork').Pool(1) as pool:
            with self.assertRaises(hil_test.rtt.RttError) as cm:
                pool.apply(_raise_rtt_error, ('stall',))
        self.assertIs(type(cm.exception), hil_test.rtt.RttError)
        self.assertEqual(str(cm.exception), 'stall')

    def test_staging_and_banner_coupling(self):
        # tripwires for couplings no import-walk can see:
        # (a) hil_remote.py must stage agentrc's rtt.py -- helper/agentrc.py exec_module's it,
        #     so an unstaged rig tree kills every harness import; it stages the closure of SCRIPTS
        wrapper = Path(REPO) / '.claude/skills/hil-contract/scripts/hil_remote.py'
        self.assertIn('core.stage_tree(tree, agentrc[0])', wrapper.read_text())
        self.assertIn(RTT_PY, agentrc.SCRIPTS)
        # (b) the shared RTT banner filter must drop ALL THREE J-Link banner lines,
        #     including the middle one, which is the PROBE MODEL string and in
        #     libjlinkarm carries no 'SEGGER ' prefix (J-Link OH3, J-Trace H9...)
        banner_re = hil_test.RTT_BANNER_RE
        for line in ('SEGGER J-Link V9.66 - Real time terminal output',
                     'SEGGER J-Link LPC-Link 2 V1.0, SN=611000000',
                     'J-Link OH3 V1.0, SN=123456789',
                     'J-Trace H9 V2.0, SN=123456789002',
                     'Process: JLinkExe'):
            self.assertTrue(banner_re.match(line), f'banner line not filtered: {line!r}')
        for line in ('Hello from TinyUSB', 'USBD init on controller 0',
                     'ID 1a86:8010 SN 7FD88F0604B5', 'echo:p'):
            self.assertFalse(banner_re.match(line), f'target line wrongly filtered: {line!r}')


class _PastTheGate(Exception):
    pass


class RttLoggerGate(unittest.TestCase):
    """An "rtt" board whose variant lacks LOGGER=rtt aborts the run where it built the firmware: --build, or CI as
    agentrc.ci() reads it. Elsewhere it is a warning, a GITHUB_ACTIONS=false included."""
    BOARD = {'name': 'b', 'uid': 'X', 'tests': {'device': True}, 'logger': 'rtt',
             'flasher': {'name': 'jlink', 'uid': 'X', 'args': '-device x'}}

    def main(self, env):
        with tempfile.TemporaryDirectory() as td:
            cfg = Path(td) / 'c.json'
            cfg.write_text(json.dumps({'boards': [self.BOARD]}))
            out = io.StringIO()
            with mock.patch.dict(os.environ, {'HIL_REPORT_DIR': td, 'GITHUB_ACTIONS': '', 'CI': '', **env}), \
                    mock.patch.object(sys, 'argv', ['hil_test.py', str(cfg)]), \
                    mock.patch.object(hil_test, 'schedule_boards', side_effect=_PastTheGate), \
                    redirect_stdout(out), self.assertRaises((SystemExit, _PastTheGate)) as cm:
                hil_test.main()
        return isinstance(cm.exception, SystemExit), out.getvalue()

    def test_the_gate_reads_the_ci_predicate(self):
        msg = '"logger": "rtt" board has a variant without LOGGER=rtt'
        for env in ({'GITHUB_ACTIONS': 'true'}, {'GITHUB_ACTIONS': '1'}, {'CI': 'true'}):
            with self.subTest(env=env):
                aborted, out = self.main(env)
                self.assertTrue(aborted, out)
                self.assertIn(f'ERROR: {msg}', out)
        for env in ({}, {'GITHUB_ACTIONS': 'false'}, {'GITHUB_ACTIONS': '0'}):
            with self.subTest(env=env):
                aborted, out = self.main(env)
                self.assertFalse(aborted, out)
                self.assertIn(f'warning: {msg}', out)


if __name__ == '__main__':
    unittest.main()
