#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Harness-side RTT contracts — no hardware, pyserial stubbed. The console classes and the CLI
# themselves are tested where they live, in agentrc's tests/test_rtt.py against the same
# file, which hil_test loads from lib/agentrc through helper/agentrc.py. Run directly:
#   python3 test/hil/test/test_hil_rtt.py
import multiprocessing
import os
import sys
import unittest
from pathlib import Path

HIL_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO = os.path.dirname(os.path.dirname(HIL_DIR))
sys.path.insert(0, HIL_DIR)
import usbtest_harness   # noqa: E402,F401 - stubs pyserial before hil_test imports it
import hil_test          # noqa: E402

RTT_PY = 'lib/agentrc/skills/rtt/scripts/rtt.py'


def _raise_rtt_error(msg):
    raise hil_test.rtt.RttError(msg)


class RttHarnessContracts(unittest.TestCase):
    def test_hil_test_loads_the_pinned_console_once(self):
        self.assertIs(sys.modules['tinyusb_tools_rtt'], hil_test.rtt)
        self.assertEqual(os.path.realpath(hil_test.rtt.__file__), os.path.realpath(os.path.join(REPO, RTT_PY)))
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
        #     so an unstaged rig tree kills every harness import
        wrapper = Path(REPO) / '.claude/skills/hil-contract/scripts/hil_remote.py'
        self.assertIn(f"    '{RTT_PY}',\n", wrapper.read_text())
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


if __name__ == '__main__':
    unittest.main()
