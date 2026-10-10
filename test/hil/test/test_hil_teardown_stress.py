#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Offline tests for teardown_stress: device discovery, argument limits and the verdict on each
# iteration's checks, against a scripted device. Whether a configuration change under traffic
# really leaves EP0 alive is proven on the rig.
import errno
import os
import sys
import threading
import types
import unittest
import unittest.mock

pyusb_stub = {}
try:
    import usb.core
    import usb.util
except ImportError:
    # the bare pre-commit runner lacks pyusb; no test reaches libusb, so stand in for the names
    # teardown_stress uses, with pyusb's signatures and constants, for its import only
    class USBError(IOError):
        def __init__(self, strerror, error_code=None, errno=None):
            IOError.__init__(self, errno, strerror)
            self.backend_error_code = error_code

    usb = types.ModuleType('usb')
    usb.core = types.ModuleType('usb.core')
    usb.core.USBError = USBError
    usb.core.USBTimeoutError = type('USBTimeoutError', (USBError,), {})
    usb.core.find = lambda **kw: None
    usb.util = types.ModuleType('usb.util')
    usb.util.ENDPOINT_TYPE_BULK, usb.util.ENDPOINT_IN = 0x02, 0x80
    usb.util.endpoint_type = lambda bmAttributes: bmAttributes & 0x03
    usb.util.endpoint_direction = lambda address: address & 0x80
    pyusb_stub = {'usb': usb, 'usb.core': usb.core, 'usb.util': usb.util}

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.modules.update(pyusb_stub)
import teardown_stress as ts
for name in pyusb_stub:   # other suites must still see pyusb as missing
    del sys.modules[name]


class FakeDev:
    """usbtest as teardown_stress sees it: bulk moves `read_len`/`write_len` bytes (default the
    full request) unless `bulk_ok` is false; a SET_CONFIGURATION(value) after the initial one raises
    `set_config_error[value]` if present; GET_DESCRIPTOR returns `device_desc`."""

    def __init__(self, set_config_error=None, device_desc=bytes(18), bulk_ok=True, read_len=None, write_len=None):
        self.set_config_error = set_config_error or {}
        self.device_desc = device_desc
        self.bulk_ok = bulk_ok
        self.read_len, self.write_len = read_len, write_len
        self.set_configs = 0

    def is_kernel_driver_active(self, itf):
        return False

    def set_interface_altsetting(self, itf, alt):
        pass

    def read(self, ep, size, timeout=None):
        if not self.bulk_ok:
            raise usb.core.USBTimeoutError('t', errno=errno.ETIMEDOUT)
        return bytes(size if self.read_len is None else self.read_len)

    def write(self, ep, data, timeout=None):
        if not self.bulk_ok:
            raise usb.core.USBTimeoutError('t', errno=errno.ETIMEDOUT)
        return len(data) if self.write_len is None else self.write_len

    def ctrl_transfer(self, bm, req, val, idx, data_or_len=None, timeout=None):
        if req == ts.REQ_SET_CONFIGURATION:
            self.set_configs += 1
            if self.set_configs > 1 and val in self.set_config_error:
                raise self.set_config_error[val]
        if req == ts.REQ_GET_DESCRIPTOR:
            return self.device_desc
        return 0


class OnePassStop:
    """pump's stop event, ignoring main's set(): exactly one pass runs before the thread is joined,
    so the verdict tests do not depend on the worker being scheduled before stop is set."""

    def __init__(self):
        self.checks = 0

    def is_set(self):
        self.checks += 1
        return self.checks > 1

    def set(self):
        pass


def run(dev, *argv):
    with unittest.mock.patch.object(ts, 'find', return_value=dev), \
         unittest.mock.patch.object(ts, 'bulk_eps', return_value=(0, 0x01, 0x81)), \
         unittest.mock.patch.object(ts, 'threading', types.SimpleNamespace(Event=OnePassStop, Thread=threading.Thread)), \
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


class Pump(unittest.TestCase):
    def pump(self, dev):
        counts = {'ok': 0, 'err': 0}
        ts.pump(dev, 0x01, 0x81, OnePassStop(), counts)
        return counts

    def test_full_transfers_count_ok(self):
        self.assertEqual({'ok': 2, 'err': 0}, self.pump(FakeDev()))

    def test_short_or_empty_read_counts_err(self):
        for n in (0, ts.BULK_LEN - 1):
            with self.subTest(read_len=n):
                self.assertEqual({'ok': 1, 'err': 1}, self.pump(FakeDev(read_len=n)))

    def test_short_write_counts_err(self):
        self.assertEqual({'ok': 1, 'err': 1}, self.pump(FakeDev(write_len=ts.BULK_LEN - 1)))

    def test_usb_error_counts_err(self):
        self.assertEqual({'ok': 0, 'err': 2}, self.pump(FakeDev(bulk_ok=False)))


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

    def test_only_short_bulk_traffic_fails(self):
        rc, out = run(FakeDev(read_len=0, write_len=0))
        self.assertEqual(1, rc)
        self.assertIn('no bulk traffic', out)

    def test_short_bulk_after_reconfigure_fails(self):
        for kw, wrote_read in (({'read_len': 0}, f'wrote {ts.BULK_LEN} read 0'),
                               ({'write_len': 1}, f'wrote 1 read {ts.BULK_LEN}')):
            with self.subTest(**kw):
                rc, out = run(FakeDev(**kw))
                self.assertEqual(1, rc)
                self.assertIn(f'short bulk after SET_CONFIGURATION(1): {wrote_read}', out)

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
