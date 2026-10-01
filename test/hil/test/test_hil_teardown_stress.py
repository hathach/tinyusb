#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Offline tests for teardown_stress: device discovery, argument limits and the verdict on each
# iteration's checks, against a scripted device. Whether a configuration change under traffic
# really leaves EP0 alive is proven on the rig.
import errno
import os
import sys
import unittest
import unittest.mock

try:
    import usb.core
    import usb.util
except ImportError:
    raise unittest.SkipTest('pyusb not installed')

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import teardown_stress as ts


class FakeDev:
    """usbtest as teardown_stress sees it: bulk succeeds unless `bulk_ok` is false; a
    SET_CONFIGURATION(value) after the initial one raises `set_config_error[value]` if present;
    GET_DESCRIPTOR returns `device_desc`."""

    def __init__(self, set_config_error=None, device_desc=bytes(18), bulk_ok=True):
        self.set_config_error = set_config_error or {}
        self.device_desc = device_desc
        self.bulk_ok = bulk_ok
        self.set_configs = 0

    def is_kernel_driver_active(self, itf):
        return False

    def set_interface_altsetting(self, itf, alt):
        pass

    def read(self, ep, size, timeout=None):
        if not self.bulk_ok:
            raise usb.core.USBTimeoutError('t', errno=errno.ETIMEDOUT)
        return bytes(size)

    def write(self, ep, data, timeout=None):
        if not self.bulk_ok:
            raise usb.core.USBTimeoutError('t', errno=errno.ETIMEDOUT)
        return len(data)

    def ctrl_transfer(self, bm, req, val, idx, data_or_len=None, timeout=None):
        if req == ts.REQ_SET_CONFIGURATION:
            self.set_configs += 1
            if self.set_configs > 1 and val in self.set_config_error:
                raise self.set_config_error[val]
        if req == ts.REQ_GET_DESCRIPTOR:
            return self.device_desc
        return 0


def one_pump_pass(dev, ep_out, ep_in, stop, counts):
    """ts.pump's work for one pass, done before the thread is joined: the verdict tests must not
    depend on the worker being scheduled before stop is set."""
    for fn in (lambda: dev.read(ep_in, 512), lambda: dev.write(ep_out, bytes(512))):
        try:
            fn()
            counts['ok'] += 1
        except usb.core.USBError:
            counts['err'] += 1


def run(dev, *argv):
    with unittest.mock.patch.object(ts, 'find', return_value=dev), \
         unittest.mock.patch.object(ts, 'bulk_eps', return_value=(0, 0x01, 0x81)), \
         unittest.mock.patch.object(ts, 'pump', one_pump_pass), \
         unittest.mock.patch.object(ts.time, 'sleep'), \
         unittest.mock.patch.object(sys, 'argv', ['teardown_stress.py', '-n', '2', *argv]), \
         unittest.mock.patch('builtins.print') as out:
        rc = ts.main()
    return rc, ' '.join(str(c.args[0]) for c in out.call_args_list)


class Args(unittest.TestCase):
    def test_nonpositive_iterations_rejected_before_device_access(self):
        for n in ('0', '-1'):
            with self.subTest(n=n), \
                 unittest.mock.patch.object(ts, 'find') as find, \
                 unittest.mock.patch.object(sys, 'argv', ['teardown_stress.py', '-n', n]), \
                 unittest.mock.patch('sys.stderr'):
                with self.assertRaises(SystemExit) as e:
                    ts.main()
                self.assertEqual(2, e.exception.code)
                find.assert_not_called()

    def test_device_not_found(self):
        self.assertEqual(2, run(None)[0])


class Find(unittest.TestCase):
    def find(self, found, opened=object()):
        side = found if isinstance(found, BaseException) else None
        with unittest.mock.patch.object(ts.usbtest, 'find_device', side_effect=side, return_value=found), \
             unittest.mock.patch.object(ts.usb.core, 'find', return_value=opened) as core_find, \
             unittest.mock.patch('sys.stderr'):
            return ts.find('S'), core_find

    def test_opens_the_found_node_by_bus_and_address(self):
        dev, core_find = self.find({'node': '/dev/bus/usb/009/072'})
        self.assertIsNotNone(dev)
        core_find.assert_called_once_with(bus=9, address=72)

    def test_not_found_ambiguous_or_several_is_none(self):
        for found in (None, {'ambiguous': ['9-2.6', '9-2.7']}, SystemExit('multiple')):
            with self.subTest(found=found):
                self.assertIsNone(self.find(found)[0])

    def test_device_gone_before_open_is_none(self):
        self.assertIsNone(self.find({'node': '/dev/bus/usb/009/072'}, opened=None)[0])


class Endpoints(unittest.TestCase):
    def test_missing_bulk_endpoint_raises(self):
        ep = unittest.mock.Mock(bmAttributes=usb.util.ENDPOINT_TYPE_BULK, bEndpointAddress=0x81)
        intf = unittest.mock.MagicMock(bInterfaceNumber=0)
        intf.__iter__.return_value = [ep]
        dev = unittest.mock.Mock()
        dev.get_active_configuration.return_value = {(0, 1): intf}
        with self.assertRaises(ValueError):
            ts.bulk_eps(dev)


class Verdict(unittest.TestCase):
    def test_every_iteration_recovered_passes(self):
        rc, out = run(FakeDev())
        self.assertEqual(0, rc, out)
        self.assertIn('PASS 2 iterations', out)

    def test_unconfigure_timeout_fails(self):
        rc, out = run(FakeDev(set_config_error={0: usb.core.USBTimeoutError('t', errno=errno.ETIMEDOUT)}))
        self.assertEqual(1, rc)
        self.assertIn('SET_CONFIGURATION(0)', out)

    def test_iteration_without_bulk_traffic_fails(self):
        rc, out = run(FakeDev(bulk_ok=False))
        self.assertEqual(1, rc)
        self.assertIn('no bulk traffic', out)

    def test_reconfigure_failure_fails(self):
        rc, out = run(FakeDev(set_config_error={1: usb.core.USBError('pipe', errno=errno.EPIPE)}))
        self.assertEqual(1, rc)
        self.assertIn('reconfigure/bulk after SET_CONFIGURATION(1)', out)

    def test_ep0_dead_after_unconfigure_fails(self):
        rc, out = run(FakeDev(device_desc=b''))
        self.assertEqual(1, rc)
        self.assertIn('GET_DESCRIPTOR after SET_CONFIGURATION(0)', out)


if __name__ == '__main__':
    unittest.main()
