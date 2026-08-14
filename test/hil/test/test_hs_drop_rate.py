#!/usr/bin/env python3
"""Unit tests for hs_drop_rate's wire parser. No hardware required."""
import unittest
import os, sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from hs_drop_rate import parse_setup_outcomes

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


if __name__ == '__main__':
    unittest.main()
