# SPDX-License-Identifier: MIT
# Raw PTP/MTP over pyusb: the recovery paths libmtp cannot drive -- cancel, device reset,
# Get Device Status, halt clearing, malformed containers, oversized and multi-packet
# datasets. Runs inside mtp_test.py's disposable process after the libmtp session
# disconnects. Imports stay stdlib + pyusb (lazily, so the offline tests and rigs without
# pyusb still import this module); nothing here may pull in the harness.
import errno
import struct
import sys
import time

HDR = 12
TYPE_COMMAND, TYPE_DATA, TYPE_RESPONSE = 1, 2, 3

OP_GET_DEVICE_INFO = 0x1001
OP_OPEN_SESSION = 0x1002
OP_CLOSE_SESSION = 0x1003
OP_GET_OBJECT_HANDLES = 0x1007
OP_GET_OBJECT_INFO = 0x1008
OP_GET_OBJECT = 0x1009
OP_DELETE_OBJECT = 0x100B
OP_SEND_OBJECT_INFO = 0x100C
OP_SEND_OBJECT = 0x100D

RESP_OK = 0x2001
RESP_SESSION_NOT_OPEN = 0x2003
RESP_INVALID_OBJECT_HANDLE = 0x2009
RESP_SESSION_ALREADY_OPEN = 0x201E
RESP_TRANSACTION_CANCELLED = 0x201F

REQ_CANCEL, REQ_DEVICE_RESET, REQ_GET_DEVICE_STATUS = 0x64, 0x66, 0x67

STORAGE_ID = 0x00010001  # the example's one store
PARENT_ROOT = 0xFFFFFFFF
FORMAT_TEXT = 0x3004
OBJECT_INFO_HDR = 52     # mtp_object_info_header_t, before the filename string
NAME_MAX_CHARS = 15      # FS_MAX_FILENAME_LEN - 1 in the example

XFER_TIMEOUT_MS = 1000
STATUS_POLLS = 20        # x 50 ms for a Cancel to settle


class RawError(Exception):
    pass


class Stall(RawError):
    """The endpoint answered STALL (EPIPE): expected by the recovery cases."""


def container(ctype, code, tid, payload=b''):
    return struct.pack('<IHHI', HDR + len(payload), ctype, code, tid) + payload


def parse(buf):
    """(len, type, code, tid, payload) of a container; raises on a runt."""
    if len(buf) < HDR:
        raise RawError(f'runt container: {len(buf)} bytes')
    length, ctype, code, tid = struct.unpack_from('<IHHI', buf)
    return length, ctype, code, tid, buf[HDR:]


def mtp_string(text):
    """PTP string: 1-byte count of UTF-16 units including the NUL, then the units."""
    if not text:
        return b'\x00'
    units = text.encode('utf-16-le') + b'\x00\x00'
    return bytes([len(units) // 2]) + units


def object_info(size, name, storage=STORAGE_ID, parent=PARENT_ROOT):
    fixed = struct.pack('<IHHIHIIIIIIIHII', storage, FORMAT_TEXT, 0, size, 0, 0, 0, 0, 0, 0, 0,
                        parent, 0, 0, 0)
    assert len(fixed) == OBJECT_INFO_HDR
    return fixed + mtp_string(name) + mtp_string('') + mtp_string('') + mtp_string('')


def object_info_name(dataset):
    """The filename of a GetObjectInfo dataset, without its NUL."""
    n = dataset[OBJECT_INFO_HDR]
    units = dataset[OBJECT_INFO_HDR + 1:OBJECT_INFO_HDR + 1 + 2 * n]
    return units.decode('utf-16-le').rstrip('\x00')


class UsbLink:
    """pyusb transport for one interface. `find` is deferred so this module imports
    without pyusb; the offline tests substitute a fake with the same methods."""

    def __init__(self, dev, itf, ep_in, ep_out, mps):
        self.dev, self.itf, self.ep_in, self.ep_out, self.mps = dev, itf, ep_in, ep_out, mps

    @classmethod
    def open(cls, busnum, devnum):
        import usb.core
        import usb.util
        dev = usb.core.find(bus=busnum, address=devnum)
        if dev is None:
            raise RawError(f'no usb device at bus {busnum} address {devnum}')
        for itf in dev.get_active_configuration().interfaces():
            if (itf.bInterfaceClass, itf.bInterfaceSubClass, itf.bInterfaceProtocol) == (6, 1, 1):
                break
        else:
            raise RawError('no still-image (MTP) interface')
        ep_in = ep_out = mps = None
        for ep in itf.endpoints():
            if usb.util.endpoint_type(ep.bmAttributes) != usb.util.ENDPOINT_TYPE_BULK:
                continue
            if usb.util.endpoint_direction(ep.bEndpointAddress) == usb.util.ENDPOINT_IN:
                ep_in = ep.bEndpointAddress
            else:
                ep_out = ep.bEndpointAddress
            mps = ep.wMaxPacketSize
        if ep_in is None or ep_out is None:
            raise RawError('MTP interface lacks a bulk endpoint pair')
        usb.util.claim_interface(dev, itf.bInterfaceNumber)
        return cls(dev, itf.bInterfaceNumber, ep_in, ep_out, mps)

    def close(self):
        import usb.util
        try:
            usb.util.release_interface(self.dev, self.itf)
        finally:
            usb.util.dispose_resources(self.dev)

    @staticmethod
    def _wrap(exc):
        import usb.core
        if isinstance(exc, usb.core.USBTimeoutError):
            return RawError(f'transfer timed out: {exc}')
        if isinstance(exc, usb.core.USBError) and exc.errno == errno.EPIPE:
            return Stall(str(exc))
        return RawError(f'{type(exc).__name__}: {exc}')

    def _io(self, fn, *args):
        import usb.core
        try:
            return fn(*args)
        except usb.core.USBError as e:
            raise self._wrap(e) from None

    def write(self, data):
        n = self._io(self.dev.write, self.ep_out, data, XFER_TIMEOUT_MS)
        if n != len(data):
            raise RawError(f'short write: {n} of {len(data)} bytes')

    def read(self, size):
        return bytes(self._io(self.dev.read, self.ep_in, size, XFER_TIMEOUT_MS))

    def ctrl(self, bm_request_type, b_request, w_value, w_index, data_or_length):
        r = self._io(self.dev.ctrl_transfer, bm_request_type, b_request, w_value, w_index, data_or_length,
                     XFER_TIMEOUT_MS)
        return bytes(r) if not isinstance(r, int) else r

    def clear_halt(self, ep):
        self._io(self.dev.clear_halt, ep)


class RawMtp:
    def __init__(self, link):
        self.link = link
        self.tid = 0

    # -- bulk ------------------------------------------------------------------------------
    def send_container(self, ctype, code, payload=b'', tid=None):
        """Write one container; a ZLP follows when the length is a packet multiple, as the
        device's read only completes on a short packet."""
        if tid is None:
            self.tid += 1
            tid = self.tid
        data = container(ctype, code, tid, payload)
        self.link.write(data)
        if len(data) % self.link.mps == 0:
            self.link.write(b'')
        return tid

    def command(self, code, *params):
        if code == OP_OPEN_SESSION:
            self.tid = 0  # outside a session the transaction id is 0; the session counts from 1
        return self.send_container(TYPE_COMMAND, code, struct.pack('<%dI' % len(params), *params),
                                   tid=0 if code == OP_OPEN_SESSION else None)

    def send_data(self, code, tid, payload):
        return self.send_container(TYPE_DATA, code, payload, tid)

    def read_container(self, size=64 * 1024):
        """One read large enough for a whole phase: the device ends it with a short packet or
        a ZLP, so a data phase arrives complete."""
        return parse(self.link.read(size))

    def read_data(self, tid):
        length, ctype, _, rtid, payload = self.read_container()
        if ctype != TYPE_DATA or rtid != tid:
            raise RawError(f'expected data block for tid {tid}, got type {ctype} tid {rtid}')
        if length != HDR + len(payload):
            raise RawError(f'data block declares {length} bytes, received {HDR + len(payload)}')
        return payload

    def read_response(self, tid):
        length, ctype, code, rtid, payload = self.read_container(512)
        if ctype != TYPE_RESPONSE or rtid != tid:
            raise RawError(f'expected response for tid {tid}, got type {ctype} tid {rtid}')
        if length != HDR + len(payload) or len(payload) % 4:
            raise RawError(f'response declares {length} bytes, received {HDR + len(payload)}')
        return code, struct.unpack('<%dI' % (len(payload) // 4), payload)

    def transaction(self, code, *params, data_out=None, data_in=False):
        """A whole transaction: (response code, params, data received or None)."""
        tid = self.command(code, *params)
        if data_out is not None:
            self.send_data(code, tid, data_out)
        data = self.read_data(tid) if data_in else None
        resp, rparams = self.read_response(tid)
        return resp, rparams, data

    def expect(self, want, code, *params, **kw):
        resp, rparams, data = self.transaction(code, *params, **kw)
        if resp != want:
            raise RawError(f'op {code:#06x}: expected response {want:#06x}, got {resp:#06x}')
        return rparams, data

    # -- class control requests -------------------------------------------------------------
    def device_status(self):
        """(status code, [halted endpoints]) from Get Device Status."""
        buf = self.link.ctrl(0xA1, REQ_GET_DEVICE_STATUS, 0, self.link.itf, 16)
        if len(buf) < 4:
            raise RawError(f'Get Device Status returned {len(buf)} bytes')
        length, code = struct.unpack_from('<HH', buf)
        if length != len(buf):
            raise RawError(f'Get Device Status declares {length} bytes, sent {len(buf)}')
        eps = [struct.unpack_from('<H', buf, i)[0] & 0xFF for i in range(4, len(buf), 2)]
        return code, eps

    def cancel(self, tid):
        self.link.ctrl(0x21, REQ_CANCEL, 0, self.link.itf, struct.pack('<HI', 0x4001, tid))

    def device_reset(self):
        self.link.ctrl(0x21, REQ_DEVICE_RESET, 0, self.link.itf, b'')

    def ep_halted(self, ep):
        st = self.link.ctrl(0x82, 0, 0, ep, 2)  # standard GET_STATUS(endpoint)
        return bool(st[0] & 1)

    def wait_status_ok(self):
        for _ in range(STATUS_POLLS):
            code, _ = self.device_status()
            if code == RESP_OK:
                return
            time.sleep(0.05)
        raise RawError(f'device status stayed {code:#06x}')

    # -- objects ---------------------------------------------------------------------------
    def create(self, size, name='t.txt', storage=STORAGE_ID):
        """SendObjectInfo; returns the new handle."""
        (_, _, handle), _ = self.expect(RESP_OK, OP_SEND_OBJECT_INFO, storage, PARENT_ROOT,
                                        data_out=object_info(size, name, storage))
        return handle

    def delete(self, handle):
        self.expect(RESP_OK, OP_DELETE_OBJECT, handle)

    def open_session(self):
        resp, _, _ = self.transaction(OP_OPEN_SESSION, 1)
        if resp not in (RESP_OK, RESP_SESSION_ALREADY_OPEN):  # libmtp's session was not closed
            raise RawError(f'OpenSession: {resp:#06x}')

    def list_handles(self):
        """Every object handle in the root of the example's storage, as the device sees it."""
        _, data = self.expect(RESP_OK, OP_GET_OBJECT_HANDLES, STORAGE_ID, 0, PARENT_ROOT, data_in=True)
        n = struct.unpack_from('<I', data)[0]
        if len(data) != 4 + 4 * n:
            raise RawError(f'GetObjectHandles declares {n} handles in {len(data)} bytes')
        return set(struct.unpack_from('<%dI' % n, data, 4))

    def upload_readback(self, size, name='t.txt'):
        data = bytes((i * 7 + 3) & 0xFF for i in range(size))
        handle = self.create(size, name)
        self.expect(RESP_OK, OP_SEND_OBJECT, data_out=data)
        _, back = self.expect(RESP_OK, OP_GET_OBJECT, handle, data_in=True)
        if back != data:
            raise RawError(f'{size}-byte readback differs')
        self.delete(handle)


def expect_stall(fn):
    try:
        fn()
    except Stall:
        return
    raise RawError('expected a STALL')


def expect_halted_both(m):
    code, eps = m.device_status()
    if code != RESP_TRANSACTION_CANCELLED or sorted(eps) != sorted([m.link.ep_in, m.link.ep_out]):
        raise RawError(f'expected TransactionCancelled with both endpoints, got {code:#06x} {eps}')


def clear_both(m, first, second):
    """Clear the halts in the given order; the device is ready only after the second."""
    m.link.clear_halt(first)
    code, _ = m.device_status()
    if code != RESP_TRANSACTION_CANCELLED:
        raise RawError(f'ready after clearing one endpoint: {code:#06x}')
    if not m.ep_halted(second):
        raise RawError(f'endpoint {second:#04x} not halted after clearing {first:#04x}')
    m.link.clear_halt(second)
    code, _ = m.device_status()
    if code != RESP_OK:
        raise RawError(f'not ready after clearing both endpoints: {code:#06x}')


# -- cases: each starts and ends idle with the session open ----------------------------------
def case_open_session(m):
    m.open_session()
    m.expect(RESP_OK, OP_GET_DEVICE_INFO, data_in=True)


def case_boundaries(m):
    # exactly 1 and 3 device buffers (need the terminating ZLP), one packet, a short one;
    # a set, as at mps 512 one packet is one buffer
    for size in sorted({512 - HDR, 3 * 512 - HDR, m.link.mps - HDR, 100, 2 * m.link.mps - HDR}):
        m.upload_readback(size)


def case_long_name(m):
    name = ''.join(chr(ord('a') + i % 26) for i in range(254))  # count 255 on the wire
    handle = m.create(100, name)
    _, info = m.expect(RESP_OK, OP_GET_OBJECT_INFO, handle, data_in=True)
    got = object_info_name(info)
    if got != name[:NAME_MAX_CHARS]:
        raise RawError(f'long name stored as {got!r}')
    m.delete(handle)


def case_oversized_send_object(m):
    handle = m.create(100)
    data = bytes(range(256)) * 4  # 1024 bytes for a 100-byte object
    m.expect(RESP_OK, OP_SEND_OBJECT, data_out=data)
    _, back = m.expect(RESP_OK, OP_GET_OBJECT, handle, data_in=True)
    if back != data[:100]:
        raise RawError('oversized SendObject: stored bytes differ from the declared prefix')
    m.delete(handle)


def case_no_session(m):
    m.expect(RESP_OK, OP_CLOSE_SESSION)
    m.expect(RESP_SESSION_NOT_OPEN, OP_SEND_OBJECT)
    m.expect(RESP_OK, OP_OPEN_SESSION, 1)


def case_runt_command(m):
    m.link.write(b'\x08\x00\x00\x00\x01\x00\x01\x10')
    expect_stall(lambda: m.link.read(512))
    expect_halted_both(m)
    clear_both(m, m.link.ep_out, m.link.ep_in)
    m.expect(RESP_OK, OP_GET_DEVICE_INFO, data_in=True)


def case_early_rejection(m):
    # invalid storage in a dataset that spans two device buffers: the example answers from
    # the first packet while the host still owes data -> device-initiated cancel
    name = 'x' * 254
    dataset = object_info(100, name, storage=0x00020001)
    tid = m.command(OP_SEND_OBJECT_INFO, STORAGE_ID, PARENT_ROOT)
    data = container(TYPE_DATA, OP_SEND_OBJECT_INFO, tid, dataset)

    def send_rejected():
        m.link.write(data[:512])
        m.link.write(data[512:])
        m.link.read(512)
    expect_stall(send_rejected)
    expect_halted_both(m)
    clear_both(m, m.link.ep_in, m.link.ep_out)
    m.expect(RESP_OK, OP_GET_DEVICE_INFO, data_in=True)


def _start_partial_send_object(m):
    """SendObjectInfo for 100 bytes, then a SendObject the host abandons after one buffer."""
    handle = m.create(100)
    tid = m.command(OP_SEND_OBJECT)
    data = container(TYPE_DATA, OP_SEND_OBJECT, tid, bytes(1012))  # 1024 declared
    m.link.write(data[:512])
    return handle, tid


def case_cancel_mid_send_object(m):
    handle, tid = _start_partial_send_object(m)
    m.cancel(tid)
    m.wait_status_ok()
    m.expect(RESP_INVALID_OBJECT_HANDLE, OP_SEND_OBJECT)  # the staged handle was dropped
    m.delete(handle)
    m.expect(RESP_OK, OP_GET_DEVICE_INFO, data_in=True)


def case_device_reset_mid_send_object(m):
    handle, _ = _start_partial_send_object(m)
    m.device_reset()
    m.wait_status_ok()
    m.expect(RESP_SESSION_NOT_OPEN, OP_SEND_OBJECT)
    m.expect(RESP_OK, OP_OPEN_SESSION, 1)
    m.expect(RESP_INVALID_OBJECT_HANDLE, OP_SEND_OBJECT)
    m.delete(handle)
    m.upload_readback(100)


def case_device_reset_undrained_in(m):
    m.command(OP_GET_OBJECT, 1)  # readme: its data IN is never read
    time.sleep(0.05)
    m.device_reset()
    m.wait_status_ok()
    m.link.clear_halt(m.link.ep_in)   # resync the host's toggles with the aborted endpoints
    m.link.clear_halt(m.link.ep_out)
    m.expect(RESP_OK, OP_OPEN_SESSION, 1)
    _, data = m.expect(RESP_OK, OP_GET_OBJECT, 1, data_in=True)
    if not data:
        raise RawError('GetObject after reset returned no data')


CASES = [
    ('open_session', case_open_session),
    ('boundaries', case_boundaries),
    ('long_name', case_long_name),
    ('oversized_send_object', case_oversized_send_object),
    ('no_session', case_no_session),
    ('runt_command', case_runt_command),
    ('early_rejection', case_early_rejection),
    ('cancel_mid_send_object', case_cancel_mid_send_object),
    ('device_reset_mid_send_object', case_device_reset_mid_send_object),
    ('device_reset_undrained_in', case_device_reset_undrained_in),
]


def recover(m, baseline):
    """Bring a failed case's device back to idle with the session open and no object beyond
    the baseline; False when it would not come back, so the remaining cases must not run."""
    try:
        m.device_reset()
        m.link.clear_halt(m.link.ep_in)
        m.link.clear_halt(m.link.ep_out)
        m.wait_status_ok()
        m.open_session()
        for handle in m.list_handles() - baseline:
            m.delete(handle)
        return True
    except Exception:
        return False


def run_cases(m, baseline, log=print):
    """Run every case; returns the failure messages. Any exception is the case's failure (a
    malformed reply is the device's fault, not the harness's); afterwards the device is
    recovered, and if it will not recover the rest is skipped rather than risk a wedge."""
    failures = []
    for name, case in CASES:
        try:
            case(m)
            code, _ = m.device_status()
            if code != RESP_OK:
                raise RawError(f'left device status {code:#06x}')
            left = m.list_handles() - baseline
            if left:
                raise RawError(f'left objects {sorted(left)}')
            log(f'raw {name}: ok')
        except Exception as e:
            failures.append(f'{name}: {type(e).__name__}: {e}')
            log(f'raw {name}: FAIL {type(e).__name__}: {e}')
            if not recover(m, baseline):
                failures.append('device did not recover: remaining raw cases skipped')
                log('raw: device did not recover, skipping the rest')
                break
    return failures


def run_raw_session(busnum, devnum, log=print):
    """The objects present before the first case are the example's built-in files; anything
    else at the end of a case is that case's leak, whether or not it saw the creating response."""
    link = UsbLink.open(busnum, devnum)
    try:
        m = RawMtp(link)
        m.open_session()
        baseline = m.list_handles()
        return run_cases(m, baseline, log)
    finally:
        link.close()


if __name__ == '__main__':
    # manual use: python3 mtp_raw.py BUS DEV
    fails = run_raw_session(int(sys.argv[1]), int(sys.argv[2]))
    sys.exit(1 if fails else 0)
