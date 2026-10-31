#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# hil_test.TTY_RAW, run as the throughput test runs it: a bounded child that must leave the tty
# raw with echo off, as `stty raw -echo` did, and report a failed open through its rc.
# Not waiting on the drain needs a real CDC tty (a pty's drain never blocks): checked on hardware.
# Run directly:
#   python3 test/hil/test/test_hil_tty_raw.py
import os
import sys
import termios
import unittest

TEST_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(TEST_DIR))

import usbtest_harness  # noqa: E402,F401 - stubs serial before hil_test is imported
import hil_test  # noqa: E402
from hil_test import hil_util  # noqa: E402


def run_tty_raw(path):
    return hil_util.run_cmd([sys.executable, '-c', hil_test.TTY_RAW, path], timeout=30, quiet=True)


class TestTtyRaw(unittest.TestCase):
    def test_raw_and_no_echo(self):
        master, slave = os.openpty()
        self.addCleanup(os.close, master)
        self.addCleanup(os.close, slave)
        self.assertEqual(run_tty_raw(os.ttyname(slave)).returncode, 0)
        _, _, _, lflag, _, _, cc = termios.tcgetattr(slave)
        self.assertFalse(lflag & (termios.ECHO | termios.ICANON | termios.ISIG))
        self.assertEqual(cc[termios.VMIN], 1)

    def test_missing_node_fails(self):
        r = run_tty_raw('/dev/nonexistent-tty')
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('FileNotFoundError', hil_util.cmd_stdout_text(r.stdout))


if __name__ == '__main__':
    unittest.main()
