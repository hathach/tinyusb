#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Network regression coverage without USB hardware or privileged host changes."""
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import net_test as net

SCRIPT = str(Path(net.__file__).resolve())


def clean_env(tmp):
    """A fresh interpreter's env with no agentrc to find: the --http client needs none."""
    env = {k: v for k, v in os.environ.items()
           if k not in ('GITHUB_ACTIONS', 'CI') and not k.startswith('PYTHON')}
    env.update(PYTHONSAFEPATH='1', AGENTRC_DIR=tmp, HOME=tmp, USERPROFILE=tmp)
    return env


class Startup(unittest.TestCase):
    # hil_test.py starts net_test.py in a fresh interpreter, and the --http child under
    # unshare; the in-process tests above add test/hil to sys.path and would hide both.
    def test_safe_path_startup_finds_helper(self):
        with TemporaryDirectory() as tmp:
            ret = subprocess.run([sys.executable, SCRIPT, '--help'], cwd=tmp, env=clean_env(tmp),
                                 capture_output=True, text=True, timeout=30)
        self.assertNotIn('ModuleNotFoundError', ret.stderr)
        self.assertEqual(ret.returncode, 0, ret.stderr)

    @unittest.skipUnless(os.name == 'posix', 'the --http client arms SIGALRM')
    def test_http_client_does_not_load_agentrc(self):
        # runpy does not put the script's folder on sys.path: its own append must.
        code = '''import io, runpy, signal, sys
sys.argv = [sys.argv[1], '--http']
sys.stdin = io.StringIO('')
try:
    runpy.run_path(sys.argv[0], run_name='__main__')
except RuntimeError as e:
    print('stopped:', e)
signal.alarm(0)
print('loaded:', sorted(m for m in sys.modules if 'agentrc' in m or 'usb_observe' in m))
'''
        with TemporaryDirectory() as tmp:
            ret = subprocess.run([sys.executable, '-c', code, SCRIPT], cwd=tmp, env=clean_env(tmp),
                                 capture_output=True, text=True, timeout=30)
        self.assertEqual(ret.returncode, 0, ret.stderr)
        self.assertIn('stopped: network setup owner exited before starting HTTP', ret.stdout)
        self.assertIn('loaded: []', ret.stdout)


class InterfaceSelection(unittest.TestCase):
    @unittest.skipUnless(os.name == 'posix', 'sysfs interface names contain colons')
    def test_interface_comes_from_usb_serial_not_shared_mac_or_name(self):
        with TemporaryDirectory() as tmp:
            target = Path(tmp) / '1-2'
            other = Path(tmp) / '1-3'
            (target / '1-2:1.0/net/usb7').mkdir(parents=True)
            (other / '1-3:1.0/net/enx0202846a9600').mkdir(parents=True)
            with patch('helper.usb_observe.usb_scan', return_value=[{'dir': str(target)}]) as scan:
                self.assertEqual(net.interfaces_for_uid('ABC123'), ['usb7'])
                scan.assert_called_once_with(vid='cafe', serial='ABC123')

    def test_busport_is_the_one_device_with_the_serial(self):
        with patch('helper.usb_observe.usb_scan', return_value=[{'dir': '/sys/bus/usb/devices/1-2.3'}]) as scan:
            self.assertEqual(net.busport_for_uid('ABC123'), '1-2.3')
            scan.assert_called_once_with(vid='cafe', serial='ABC123')
        for found in ([], [{'dir': '/x/1-2'}, {'dir': '/x/1-3'}]):
            with self.subTest(found=len(found)), patch('helper.usb_observe.usb_scan', return_value=found):
                with self.assertRaisesRegex(RuntimeError, f'expected one device, found {len(found)}'):
                    net.busport_for_uid('ABC123')

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

        self.moved_ifindex = 7
        self.printed_ifindex = 7
        self.extra_links = []

        def command(argv, timeout=5):
            self.calls.append(argv)
            if argv[:2] == ['hil_root', 'netns']:
                return f'{self.printed_ifindex} usb7\n'
            if '-j' in argv:
                return json.dumps([{'ifname': 'lo', 'ifindex': 1}, *self.extra_links,
                                   {'ifname': 'renamed0', 'ifindex': self.moved_ifindex}])
            return ''

        hil_root = Mock()
        hil_root.argv.side_effect = lambda verb, *args: ['hil_root', verb, *args]
        for name, value in [('wait_interface', Mock(return_value='usb7')),
                            ('interface_index', Mock(return_value=7)),
                            ('busport_for_uid', Mock(return_value='1-2')),
                            ('hil_root', Mock(return_value=hil_root)),
                            ('command', Mock(side_effect=command))]:
            p = patch.object(net, name, value)
            p.start()
            self.addCleanup(p.stop)
        for p in [patch.object(net.subprocess, 'Popen', return_value=self.proc),
                  patch.object(net.select, 'select', return_value=([self.proc.stdout], [], [])),
                  patch.object(net.signal, 'SIGKILL', 9, create=True),
                  patch('sys.stdout', new_callable=io.StringIO)]:
            p.start()
            self.addCleanup(p.stop)

    def test_client_runs_unprivileged_and_hil_root_moves_the_interface(self):
        net.check_device('ABC123')
        argv = net.subprocess.Popen.call_args.args[0]
        self.assertEqual(argv, ['unshare', '--user', '--map-root-user', '--net', sys.executable, SCRIPT, '--http'])
        self.assertNotIn('start_new_session', net.subprocess.Popen.call_args.kwargs)
        self.assertEqual(self.calls[0], ['hil_root', 'netns', '1-2', '4321'])
        self.assertFalse(any('sudo' in c for c in self.calls[1:]))
        nsenter = ['nsenter', '--preserve-credentials', '--target=4321', '--user', '--net', 'ip']
        self.assertTrue(all(c[:6] == nsenter for c in self.calls[1:]))
        self.assertTrue(any('renamed0' in c for c in self.calls))
        self.proc.communicate.assert_called_once_with(input='go\n', timeout=30)

    def test_fallback_tunnels_next_to_the_moved_interface_are_ignored(self):
        self.extra_links = [{'ifname': 'tunl0', 'ifindex': 2}, {'ifname': 'sit0', 'ifindex': 3}]
        net.check_device('ABC123')
        self.assertIn(['set', 'dev', 'renamed0', 'up'], [c[-4:] for c in self.calls])
        self.assertFalse(any('tunl0' in c or 'sit0' in c for c in self.calls))
        self.proc.communicate.assert_called_once_with(input='go\n', timeout=30)

    def test_namespace_holding_another_ifindex_fails(self):
        self.moved_ifindex = 8
        self.proc.poll.return_value = None
        with self.assertRaisesRegex(RuntimeError, 'ifindex 7.*renamed0.*8'):
            net.check_device('ABC123')
        self.assertFalse(any('addr' in c for c in self.calls))
        self.proc.send_signal.assert_called_once_with(net.signal.SIGTERM)

    def test_hil_root_moving_another_index_fails_before_setup(self):
        self.printed_ifindex = 9
        self.proc.poll.return_value = None
        with self.assertRaisesRegex(RuntimeError, r"moved \['9', 'usb7'\], expected ifindex 7"):
            net.check_device('ABC123')
        self.assertEqual(len(self.calls), 1)
        self.proc.send_signal.assert_called_once_with(net.signal.SIGTERM)

    def test_interrupt_terminates_client(self):
        net.command.side_effect = KeyboardInterrupt()
        self.proc.poll.return_value = None
        with self.assertRaises(KeyboardInterrupt):
            net.check_device('ABC123')
        self.proc.send_signal.assert_called_once_with(net.signal.SIGTERM)
        self.proc.communicate.assert_called_once_with(timeout=5)

    def test_setup_failure_releases_namespace_owner(self):
        net.command.side_effect = RuntimeError('move failed')
        self.proc.poll.return_value = None
        with self.assertRaisesRegex(RuntimeError, 'move failed'):
            net.check_device('ABC123')
        self.proc.send_signal.assert_called_once_with(net.signal.SIGTERM)

    def test_stalled_client_escalates_after_cleanup_grace(self):
        self.proc.communicate.side_effect = [net.subprocess.TimeoutExpired('client', 5), ('', '')]
        net.stop_client(self.proc)
        self.assertEqual([c.args for c in self.proc.send_signal.call_args_list],
                         [(net.signal.SIGTERM,), (net.signal.SIGKILL,)])

    def test_http_failure_is_reported(self):
        self.proc.returncode = 1
        self.proc.communicate.return_value = ('', 'corrupt data')
        with self.assertRaisesRegex(RuntimeError, 'corrupt data'):
            net.check_device('ABC123')

    def test_readiness_timeout_stops_the_client(self):
        net.select.select.return_value = ([], [], [])
        self.proc.poll.return_value = None
        with self.assertRaisesRegex(RuntimeError, 'did not become ready'):
            net.check_device('ABC123')
        net.command.assert_not_called()
        self.proc.send_signal.assert_called_once_with(net.signal.SIGTERM)
        self.proc.communicate.assert_called_once_with(timeout=5)


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

    def test_every_asset_is_requested_each_round(self):
        # Each asset is its own fixture, one of them binary: a loop over ASSETS[:1]
        # would skip the image on hardware while a single-asset test still passed.
        payloads = {'/page.html': b'<html>ok</html>', '/img/bin.gif': b'GIF89a\x00\xff\r\n\x01'}
        requested = []

        def request(method, path, headers=None):
            requested.append(path)
            self.response.read.return_value = payloads[path]

        self.conn.request.side_effect = request
        with patch.object(net, 'ASSETS', [(path, len(data), hashlib.sha256(data).hexdigest())
                                          for path, data in payloads.items()]):
            net.check_http()
        self.assertEqual({path: requested.count(path) for path in payloads},
                         {path: 3 for path in payloads})
        self.assertEqual(len(requested), 6)

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
