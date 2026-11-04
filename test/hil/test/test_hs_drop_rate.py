#!/usr/bin/env python3
"""Unit tests for hs_drop_rate's wire parser, device lookup and bounded commands. No hardware
required; the lookup reads a fake sysfs tree through agentrc's usb_observe (lib/agentrc)."""
import subprocess
import tempfile
import types
import unittest
import os, sys
from pathlib import Path
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import hs_drop_rate
from hs_drop_rate import parse_setup_outcomes
from helper import hil_util, usb_observe
import usbtest_harness

SETUP, ACK, DATA0, SOF, IN = '0x2d', '0xd2', '0xc3', '0xa5', '0x69'


class TestParseSetupOutcomes(unittest.TestCase):
    def test_acked_setup_counts_as_acked(self):
        rows = [
            ('1.000000', SETUP, 'host', '1.0'),
            ('1.000001', DATA0, 'host', '1.0'),
            ('1.000002', ACK, '1.0', 'host'),
        ]
        self.assertEqual(parse_setup_outcomes(rows), (1, 0))

    def test_setup_with_no_handshake_counts_as_dropped(self):
        rows = [
            ('1.000000', SETUP, 'host', '1.0'),
            ('1.000001', DATA0, 'host', '1.0'),
            ('1.000125', SOF, 'host', 'broadcast'),
        ]
        self.assertEqual(parse_setup_outcomes(rows), (0, 1))

    def test_host_sourced_ack_does_not_count(self):
        """An ACK the HOST sends (acknowledging device IN data) is not a device handshake."""
        rows = [
            ('1.000000', SETUP, 'host', '1.0'),
            ('1.000001', DATA0, 'host', '1.0'),
            ('1.000002', ACK, 'host', '1.0'),
        ]
        self.assertEqual(parse_setup_outcomes(rows), (0, 1))

    def test_retry_pair_counts_one_drop_and_one_ack(self):
        rows = [
            ('1.000000', SETUP, 'host', '1.0'),
            ('1.000001', DATA0, 'host', '1.0'),
            ('1.000125', SETUP, 'host', '1.0'),
            ('1.000126', DATA0, 'host', '1.0'),
            ('1.000127', ACK, '1.0', 'host'),
        ]
        self.assertEqual(parse_setup_outcomes(rows), (1, 1))

    def test_ack_counted_at_enumeration_address_zero(self):
        """Before SET_ADDRESS the device answers at address 0; that ACK is still an ACK.

        one_run() deliberately captures across enumeration, so this is not a corner case: it is
        the coldest and densest part of the sample the metric exists to measure.
        """
        rows = [
            ('1.000000', SETUP, 'host', '0.0'),
            ('1.000001', DATA0, 'host', '0.0'),
            ('1.000002', ACK, '0.0', 'host'),
        ]
        self.assertEqual(parse_setup_outcomes(rows), (1, 0))

    def test_ack_counted_at_any_assigned_address(self):
        """xHCI hosts routinely assign a wire address other than 1."""
        for addr in ('5', '12'):
            rows = [
                ('1.000000', SETUP, 'host', f'{addr}.0'),
                ('1.000001', DATA0, 'host', f'{addr}.0'),
                ('1.000002', ACK, f'{addr}.0', 'host'),
            ]
            with self.subTest(address=addr):
                self.assertEqual(parse_setup_outcomes(rows), (1, 0))

    def test_unrelated_traffic_is_ignored(self):
        rows = [
            ('1.000000', SOF, 'host', 'broadcast'),
            ('1.000010', IN, 'host', '1.1'),
            ('1.000011', DATA0, '1.1', 'host'),
            ('1.000012', ACK, 'host', '1.1'),
        ]
        self.assertEqual(parse_setup_outcomes(rows), (0, 0))


class TestFindDevice(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        usbtest_harness.patch(self, usb_observe, 'SYS_USB_DEVICES', str(self.root))

    def add(self, name, serial, busnum='3', devnum='7'):
        d = self.root / name
        d.mkdir()
        if serial is not None:
            (d / 'serial').write_text(serial + '\n')
        (d / 'busnum').write_text(busnum + '\n')
        (d / 'devnum').write_text(devnum + '\n')

    def test_matching_serial_returns_bus_and_dev(self):
        self.add('1-1', 'OTHER', '1', '2')
        self.add('3-2', 'DUT', '3', '17')
        self.assertEqual(hs_drop_rate.find_device('DUT'), '003:017')

    def test_no_match_returns_none(self):
        self.add('1-1', 'OTHER')
        self.add('1-2', None)   # no serial attribute
        self.assertIsNone(hs_drop_rate.find_device('DUT'))

    def test_unanswered_serial_is_skipped(self):
        self.add('1-1', 'DUT')
        usbtest_harness.patch(self, usb_observe, 'read_sysfs', lambda path, *a: None)
        self.assertIsNone(hs_drop_rate.find_device('DUT'))


def completed(rc, stdout='', stderr=''):
    return subprocess.CompletedProcess(args=[], returncode=rc, stdout=stdout, stderr=stderr)


class TestBoundedCommands(unittest.TestCase):
    def test_tshark_runs_bounded_and_parses_rows(self):
        calls = []
        usbtest_harness.patch(self, hil_util, 'run_cmd', lambda cmd, **kw: calls.append((cmd, kw))
                              or completed(0, f'1.0\t{SETUP}\thost\t0.0\n2.0\t\t\t\n'))
        self.assertEqual(hs_drop_rate.tshark_rows('x.pcapng'), [('1.0', SETUP, 'host', '0.0')])
        self.assertEqual(calls[0][1]['timeout'], hs_drop_rate.TSHARK_TIMEOUT)

    def test_tshark_timeout_fails_the_run(self):
        usbtest_harness.patch(self, hil_util, 'run_cmd', lambda cmd, **kw: completed(124))
        with self.assertRaises(SystemExit) as cm:
            hs_drop_rate.tshark_rows('x.pcapng')
        self.assertIn('returned 124', str(cm.exception.code))

    def run_hammer(self, run_cmd):
        """one_run() up to the lsusb hammer, with the sniffer, flasher and sleeps stubbed."""
        sniffer = types.SimpleNamespace(terminate=lambda: None, wait=lambda timeout: 0, kill=lambda: None)
        usbtest_harness.patch(self, hs_drop_rate.subprocess, 'Popen', lambda *a, **kw: sniffer)
        usbtest_harness.patch(self, hs_drop_rate.subprocess, 'run', lambda *a, **kw: completed(0))
        usbtest_harness.patch(self, hs_drop_rate.time, 'sleep', lambda s: None)
        usbtest_harness.patch(self, hs_drop_rate.os, 'access', lambda path, mode: True)
        usbtest_harness.patch(self, hs_drop_rate, 'find_device', lambda serial: '003:017')
        usbtest_harness.patch(self, hs_drop_rate, 'tshark_rows', lambda pcap: [])
        usbtest_harness.patch(self, hil_util, 'run_cmd', run_cmd)
        with tempfile.TemporaryDirectory() as tmp:
            fw = Path(tmp) / 'fw.bin'
            fw.write_bytes(b'')
            args = types.SimpleNamespace(firmware=str(fw), flasher_uid='U', serial='DUT', hammer=3)
            return hs_drop_rate.one_run(args, Path(tmp))

    def test_hammer_runs_lsusb_bounded(self):
        calls = []
        self.assertEqual(self.run_hammer(lambda cmd, **kw: calls.append((cmd, kw)) or completed(0)), (0, 0))
        self.assertEqual([c[0] for c in calls], [['lsusb', '-v', '-s', '003:017']] * 3)
        self.assertTrue(all(c[1]['timeout'] == hs_drop_rate.LSUSB_TIMEOUT for c in calls))

    def test_hammer_timeout_fails_the_run(self):
        with self.assertRaises(SystemExit) as cm:
            self.run_hammer(lambda cmd, **kw: completed(124))
        self.assertIn('hammer iteration 1 failed', str(cm.exception.code))
        self.assertIn('returned 124', str(cm.exception.code))


if __name__ == '__main__':
    unittest.main()
