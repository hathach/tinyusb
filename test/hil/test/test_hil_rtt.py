#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Harness-side RTT contracts — no hardware, stdlib only. The console classes and the CLI
# themselves are tested where they live, in agentrc's tests/test_rtt.py against the same
# file. Run directly:
#   python3 test/hil/test/test_hil_rtt.py
import os
import sys
import unittest
from pathlib import Path

# the module under test lives in the parent dir's helper/ package
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from helper import hil_util


class RttHarnessContracts(unittest.TestCase):
    def test_hil_util_reexports_the_rtt_classes(self):
        for name in ('JlinkRtt', 'OpenocdRtt', 'RttError', 'strip_banner', 'RTT_BANNER_RE'):
            self.assertTrue(hasattr(hil_util, name), name)

    def test_staging_and_banner_coupling(self):
        # tripwires for couplings no import-walk can see:
        # (a) hil_remote.py must stage tools/rtt.py -- hil_util exec_module's it, so an
        #     unstaged rig tree kills every harness import
        wrapper = Path(__file__).resolve().parents[3] / '.claude/skills/hil/scripts/hil_remote.py'
        self.assertIn("    'tools/rtt.py',\n", wrapper.read_text())
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


if __name__ == '__main__':
    unittest.main()
