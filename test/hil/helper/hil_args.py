#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""hil_test.py's command line, stdlib-only so the remote wrapper
(.claude/skills/hil/scripts/hil_remote.py) parses exactly what the rig will, without
importing hil_test.py and its serial dependency."""
import argparse


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog='hil_test.py')
    parser.add_argument('config_file', help='Configuration JSON file')
    parser.add_argument('-b', '--board', action='append', default=[], help='Boards to test, all if not specified')
    parser.add_argument('--flasher', action='append', default=[],
                        help='Only boards using these flashers, e.g. esptool '
                             '(for splitting one config across CI jobs)')
    parser.add_argument('--exclude-flasher', action='append', default=[],
                        help='Exclude boards using these flashers')
    parser.add_argument('-a', '--accumulate', action='store_true',
                        help='Merge results into the existing report instead of starting fresh '
                             '(re-runs; the .failed file starts with this)')
    parser.add_argument('-sf', '--skip-flash', action='store_true', help='Run tests without flashing firmware (use whatever is already on the board)')
    parser.add_argument('-t', '--test-only', action='append', default=[], help='Tests to run, all if not specified')
    parser.add_argument('-bt', '--board-test', action='append', default=[],
                        help='Per-board test list as BOARD:test1,test2 (overrides -t for that board); repeat for multiple boards')
    parser.add_argument('-B', '--build-dir', default='cmake-build', help='Build folder name (default: cmake-build)')
    parser.add_argument('--build', action='store_true', help='Build firmware for selected boards with cmake before running tests')
    # default 1, not 3: the pool guard is a FLAT 3600s that does not scale with max_retry,
    # and one usbtest test at default 3 can burn 1530s of it (510s outer x3) for a single
    # board. Every CI caller already pins --retry 1; the bare invocations in the hil skill
    # and its delegated runs go against the same one-slot rig and used to inherit 3.
    parser.add_argument('-r', '--retry', type=int, default=1, help='Retry count for failed tests (default: 1)')
    parser.add_argument('-v', '--verbose', action='store_true', help='Verbose output')
    return parser
