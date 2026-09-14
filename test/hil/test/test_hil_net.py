#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Network regression coverage without USB hardware or privileged host changes."""
import hashlib
import io
import json
import os
from pathlib import Path
import sys
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import net_test as net


class InterfaceSelection(unittest.TestCase):
    @unittest.skipUnless(os.name == 'posix', 'sysfs interface names contain colons')
    def test_interface_comes_from_usb_serial_not_shared_mac_or_name(self):
        with TemporaryDirectory() as tmp:
            target = Path(tmp) / '1-2'
            other = Path(tmp) / '1-3'
            (target / '1-2:1.0/net/usb7').mkdir(parents=True)
            (other / '1-3:1.0/net/enx0202846a9600').mkdir(parents=True)
            with patch.object(net.hil_util, 'usb_scan', return_value=[{'dir': str(target)}]) as scan:
                self.assertEqual(net.interfaces_for_uid('ABC123'), ['usb7'])
                scan.assert_called_once_with(vid='cafe', serial='ABC123')

    def test_waits_for_driver_interface(self):
        with patch.object(net, 'interfaces_for_uid', side_effect=[[], ['usb7'], ['usb7']]), \
                patch.object(net.hil_util, 'run_cmd', return_value=Mock(returncode=0, stderr='')), \
                patch.object(net.time, 'sleep'):
            self.assertEqual(net.wait_interface('ABC123'), 'usb7')

    def test_udev_rename_is_followed_by_serial(self):
        with patch.object(net, 'interfaces_for_uid',
                          side_effect=[['usb0'], ['enx123'], ['enx123'], ['enx123']]), \
                patch.object(net.hil_util, 'run_cmd', return_value=Mock(returncode=0, stderr='')), \
                patch.object(net.time, 'sleep'):
            self.assertEqual(net.wait_interface('ABC123'), 'enx123')

    def test_uninitialized_interface_is_not_moved(self):
        with patch.object(net, 'interfaces_for_uid', return_value=['usb0']), \
                patch.object(net.hil_util, 'run_cmd', return_value=Mock(returncode=1, stderr='not ready')), \
                patch.object(net.time, 'monotonic', side_effect=[0, 0, 2]), \
                patch.object(net.time, 'sleep'):
            with self.assertRaisesRegex(RuntimeError, 'not ready'):
                net.wait_interface('ABC123', timeout=1)

    def test_ambiguous_serial_never_selects_arbitrary_board(self):
        with patch.object(net, 'interfaces_for_uid', return_value=['usb0', 'usb1']), \
                patch.object(net.time, 'monotonic', side_effect=[0, 0, 2]), \
                patch.object(net.time, 'sleep'):
            with self.assertRaisesRegex(RuntimeError, 'found.*usb0.*usb1'):
                net.wait_interface('ABC123', timeout=1)


class NamespaceCleanup(unittest.TestCase):
    def setUp(self):
        self.calls = []
        self.failure = None

        def command(argv):
            self.calls.append(argv)
            if self.failure and self.failure in argv:
                raise RuntimeError('injected failure')
            if '-j' in argv:
                return json.dumps([{'ifname': 'lo'}, {'ifname': 'renamed0'}])
            return ''

        p = patch.object(net, 'command', side_effect=command)
        p.start()
        self.addCleanup(p.stop)

    def test_namespace_deleted_after_data_failure_and_rename(self):
        with self.assertRaisesRegex(AssertionError, 'bad data'):
            with net.network_namespace('usb7') as ns:
                self.assertIn(['ip', '-n', ns, 'addr', 'add', '192.168.7.2/24',
                               'dev', 'renamed0'], self.calls)
                raise AssertionError('bad data')
        self.assertEqual(self.calls[-1], ['ip', 'netns', 'delete', ns])
        self.assertIn(['ip', 'link', 'set', 'dev', 'usb7', 'netns', ns], self.calls)

    def test_namespace_deleted_if_move_fails(self):
        self.failure = 'usb7'
        with self.assertRaisesRegex(RuntimeError, 'injected'):
            with net.network_namespace('usb7'):
                self.fail('move failure must abort setup')
        self.assertEqual(self.calls[-1][:3], ['ip', 'netns', 'delete'])

    def test_failed_creation_does_not_delete_an_existing_namespace(self):
        self.failure = 'add'
        with self.assertRaises(RuntimeError):
            with net.network_namespace('usb7'):
                self.fail('creation failed')
        self.assertEqual(len(self.calls), 1)

    def test_signal_unwinds_namespace(self):
        with self.assertRaises(TimeoutError):
            with net.network_namespace('usb7'):
                net.interrupted(15, None)
        self.assertEqual(self.calls[-1][:3], ['ip', 'netns', 'delete'])


class HttpVerification(unittest.TestCase):
    def setUp(self):
        self.payload = b'\x00\xffbinary\r\nasset'
        self.response = Mock(status=200)
        self.response.read.return_value = self.payload
        self.conn = Mock()
        self.conn.getresponse.return_value = self.response
        for p in [patch.object(net, 'ASSETS', [('/fixture', len(self.payload),
                                              hashlib.sha256(self.payload).hexdigest())]),
                  patch.object(net.http.client, 'HTTPConnection', return_value=self.conn),
                  patch('sys.stdout', new_callable=io.StringIO)]:
            p.start()
            self.addCleanup(p.stop)

    def test_repeated_complete_binary_responses(self):
        net.check_http()
        self.assertEqual(self.conn.request.call_count, 3)
        self.assertEqual(self.conn.close.call_count, 3)

    def test_corruption_is_not_retried(self):
        self.response.read.return_value = b'x' * len(self.payload)
        with self.assertRaises(AssertionError):
            net.check_http()
        self.assertEqual(self.conn.request.call_count, 1)
        self.conn.close.assert_called_once()

    def test_truncated_and_excess_data_fail(self):
        for payload in (self.payload[:-1], self.payload + b'x'):
            with self.subTest(payload=payload):
                self.response.read.return_value = payload
                with self.assertRaises(AssertionError):
                    net.check_http()

    def test_http_error_with_correct_body_fails(self):
        self.response.status = 404
        with self.assertRaises(AssertionError):
            net.check_http()

    def test_initial_connect_can_wait_for_link(self):
        self.conn.connect.side_effect = [ConnectionRefusedError(), None, None, None]
        with patch.object(net.time, 'sleep'):
            net.check_http()
        self.assertEqual(self.conn.request.call_count, 3)

    def test_later_connection_failure_is_not_hidden(self):
        self.conn.connect.side_effect = [None, ConnectionRefusedError()]
        with self.assertRaises(ConnectionRefusedError):
            net.check_http()
        self.assertEqual(self.conn.request.call_count, 1)

    @unittest.skipUnless(os.name == 'posix', 'process-group timeout is Linux HIL behavior')
    def test_command_reaps_a_stalled_client(self):
        with self.assertRaisesRegex(RuntimeError, 'rc=124'):
            net.command([sys.executable, '-c', 'import time; time.sleep(60)'], timeout=0.1)


if __name__ == '__main__':
    unittest.main()
