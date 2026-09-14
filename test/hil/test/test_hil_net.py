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


class NamespaceLifetime(unittest.TestCase):
    def setUp(self):
        self.calls = []
        self.proc = Mock(returncode=0, pid=1234)
        self.proc.stdout = io.StringIO('4321\n')
        self.proc.poll.return_value = 0
        self.proc.communicate.return_value = ('HTTP verified\n', '')

        def command(argv):
            self.calls.append(argv)
            if '-j' in argv:
                return json.dumps([{'ifname': 'lo'}, {'ifname': 'renamed0'}])
            return ''

        for name, value in [('wait_interface', Mock(return_value='usb7')),
                            ('command', Mock(side_effect=command))]:
            p = patch.object(net, name, value)
            p.start()
            self.addCleanup(p.stop)
        for p in [patch.object(net.subprocess, 'Popen', return_value=self.proc),
                  patch.object(net.select, 'select', return_value=([self.proc.stdout], [], [])),
                  patch.object(net.os, 'geteuid', return_value=1000, create=True),
                  patch.object(net.os, 'getuid', return_value=1000, create=True),
                  patch.object(net.os, 'getgid', return_value=1000, create=True),
                  patch.object(net.signal, 'SIGKILL', 9, create=True),
                  patch('sys.stdout', new_callable=io.StringIO)]:
            p.start()
            self.addCleanup(p.stop)

    def test_python_runs_as_caller_in_anonymous_namespace(self):
        net.check_device('ABC123')
        argv = net.subprocess.Popen.call_args.args[0]
        self.assertEqual(argv[:7], ['sudo', '-n', 'unshare', '--net', '--setgid', '1000', '--setuid'])
        self.assertEqual(argv[7], '1000')
        self.assertNotIn('start_new_session', net.subprocess.Popen.call_args.kwargs)
        self.assertIn(['sudo', '-n', 'ip', 'link', 'set', 'dev', 'usb7', 'netns', '4321'], self.calls)
        self.assertTrue(any('renamed0' in c for c in self.calls))
        self.assertFalse(any(c[:4] == ['sudo', '-n', 'ip', 'netns'] for c in self.calls))
        self.proc.communicate.assert_called_once_with(input='go\n', timeout=30)

    def test_interrupt_terminates_client_and_reaps_sudo(self):
        net.command.side_effect = KeyboardInterrupt()
        self.proc.poll.return_value = None
        with patch.object(net.os, 'kill') as kill:
            with self.assertRaises(KeyboardInterrupt):
                net.check_device('ABC123')
        kill.assert_called_once_with(4321, net.signal.SIGTERM)
        self.proc.communicate.assert_called_once_with(timeout=5)

    def test_setup_failure_releases_namespace_owner(self):
        net.command.side_effect = RuntimeError('move failed')
        self.proc.poll.return_value = None
        with patch.object(net.os, 'kill') as kill:
            with self.assertRaisesRegex(RuntimeError, 'move failed'):
                net.check_device('ABC123')
        kill.assert_called_once_with(4321, net.signal.SIGTERM)

    def test_stalled_client_escalates_after_cleanup_grace(self):
        self.proc.communicate.side_effect = [net.subprocess.TimeoutExpired('client', 5), ('', '')]
        with patch.object(net.os, 'kill') as kill:
            net.stop_client(self.proc, 4321)
        self.assertEqual([c.args for c in kill.call_args_list],
                         [(4321, net.signal.SIGTERM), (4321, net.signal.SIGKILL)])

    def test_http_failure_is_reported(self):
        self.proc.returncode = 1
        self.proc.communicate.return_value = ('', 'corrupt data')
        with self.assertRaisesRegex(RuntimeError, 'corrupt data'):
            net.check_device('ABC123')

    def test_readiness_timeout_cleans_up_without_reported_pid(self):
        net.select.select.return_value = ([], [], [])
        self.proc.poll.return_value = None
        with patch.object(net.hil_health, 'child_procs', return_value={1234: [(4321, 1)]}), \
                patch.object(net.os, 'kill') as kill:
            with self.assertRaisesRegex(RuntimeError, 'did not become ready'):
                net.check_device('ABC123')
        net.command.assert_not_called()
        kill.assert_called_once_with(4321, net.signal.SIGTERM)
        self.proc.send_signal.assert_called_once_with(net.signal.SIGTERM)
        self.proc.communicate.assert_called_once_with(timeout=5)

    def test_root_child_is_left_for_sudo_to_signal(self):
        with patch.object(net.hil_health, 'child_procs', return_value={1234: [(4321, 1)]}), \
                patch.object(net.os, 'kill', side_effect=PermissionError()):
            net.stop_client(self.proc, None)
        self.proc.send_signal.assert_called_once_with(net.signal.SIGTERM)
        self.proc.communicate.assert_called_once_with(timeout=5)


@unittest.skipUnless(os.name == 'posix', 'Linux process-tree cleanup')
class UnreportedClient(unittest.TestCase):
    def test_live_wrapper_and_descendant_without_pid_are_reaped(self):
        # Both ignore TERM so this also exercises the KILL pass. They inherit our
        # process group; cleanup must not signal the test runner or its siblings.
        code = '''import os, signal, subprocess, sys, time
signal.signal(signal.SIGTERM, signal.SIG_IGN)
child = subprocess.Popen([sys.executable, '-c',
    'import signal,time; signal.signal(signal.SIGTERM,signal.SIG_IGN); print("ready",flush=True); time.sleep(60)'],
    stdout=subprocess.PIPE, text=True)
child.stdout.readline()
print(child.pid, flush=True)
time.sleep(60)
'''
        proc = net.subprocess.Popen([sys.executable, '-c', code], stdout=net.subprocess.PIPE,
                                    stderr=net.subprocess.PIPE, text=True)
        child = None
        try:
            self.assertTrue(net.select.select([proc.stdout], [], [], 5)[0])
            # The test knows the descendant PID; stop_client deliberately does not.
            child = int(proc.stdout.readline())
            with patch.object(net.os, 'killpg') as killpg:
                net.stop_client(proc, None, grace=0.1)
            killpg.assert_not_called()
            self.assertIsNotNone(proc.poll())
            stat = Path('/proc') / str(child) / 'stat'
            if stat.exists():
                self.assertEqual(stat.read_text().rsplit(')', 1)[1].split()[0], 'Z')
        finally:
            if proc.poll() is None:
                proc.kill()
                proc.wait(timeout=5)
            if child is not None:
                try:
                    os.kill(child, net.signal.SIGKILL)
                except ProcessLookupError:
                    pass
            net.hil_util._close_pipes(proc)


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
