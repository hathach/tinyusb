#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# USBTMC/USB488 protocol session against examples/device/usbtmc (cafe:401c) for one board, in a
# disposable process: pyusb calls block in usbfs ioctls on a wedged device, so
# hil_test.test_device_usbtmc runs this under hil_util.run_cmd (killpg + bounded reap, rc 124 on
# timeout). Imports stay stdlib + pyusb: nothing here may pull in the harness.
#
# Raw requests, not a VISA library: NI-VISA and the Linux usbtmc driver recover from a refused abort
# or a missing short packet on their own, which hides exactly what these cases check. Each case
# names the USBTMC 1.0 / USB488 1.0 rule it checks; the multi-packet cases need a full-speed bulk
# endpoint, as the example's 225-byte buffer fits one high-speed packet.
#
# Exit 0 when every case passes; 1 with the failures on stdout otherwise.
import argparse
import errno
import struct
import sys
import time

import usb.core
import usb.util

VID, PID = 0xCAFE, 0x401C

# bRequest
INITIATE_ABORT_BULK_OUT, CHECK_ABORT_BULK_OUT_STATUS = 1, 2
INITIATE_ABORT_BULK_IN, CHECK_ABORT_BULK_IN_STATUS = 3, 4
INITIATE_CLEAR, CHECK_CLEAR_STATUS = 5, 6
INDICATOR_PULSE, READ_STATUS_BYTE = 64, 128
# USBTMC_status
SUCCESS, PENDING, FAILED, NOT_IN_PROGRESS, INT_IN_BUSY = 0x01, 0x02, 0x80, 0x81, 0x20
# MsgID
DEV_DEP_MSG_OUT, REQUEST_DEV_DEP_MSG_IN, TRIGGER = 1, 2, 128

TMO = 3000  # ms per transfer
APP_DELAY_MS = 50  # the example answers a query after 2x this; the abort cases race it


class SkipCase(Exception):
    pass


class Dut:
    def __init__(self, uid, timeout):
        self.uid = uid
        self.find(timeout)

    def find(self, timeout):
        deadline = time.monotonic() + timeout
        def serial(dev):
            try:
                return usb.util.get_string(dev, dev.iSerialNumber)
            except (usb.core.USBError, ValueError):
                return None

        while True:
            self.dev = next((d for d in usb.core.find(find_all=True, idVendor=VID, idProduct=PID)
                             if serial(d) == self.uid), None)
            if self.dev is not None:
                break
            if time.monotonic() > deadline:
                raise RuntimeError(f'USBTMC device {VID:04x}:{PID:04x} serial {self.uid} not found')
            time.sleep(0.5)
        itf = self.dev.get_active_configuration()[(0, 0)]
        self.itf = itf.bInterfaceNumber
        if self.dev.is_kernel_driver_active(self.itf):
            self.dev.detach_kernel_driver(self.itf)  # the kernel usbtmc driver
        usb.util.claim_interface(self.dev, self.itf)
        self.ep_int = None
        for ep in itf:
            kind = usb.util.endpoint_type(ep.bmAttributes)
            is_in = usb.util.endpoint_direction(ep.bEndpointAddress) == usb.util.ENDPOINT_IN
            if kind == usb.util.ENDPOINT_TYPE_BULK and not is_in:
                self.ep_out, self.mps = ep.bEndpointAddress, ep.wMaxPacketSize
            elif kind == usb.util.ENDPOINT_TYPE_BULK:
                self.ep_in = ep.bEndpointAddress
            elif kind == usb.util.ENDPOINT_TYPE_INTR:
                self.ep_int = ep.bEndpointAddress
        self.tag = 1

    def next_tag(self):
        t = self.tag
        self.tag = 1 if self.tag == 255 else self.tag + 1
        return t

    def write(self, data, timeout=1000):
        return self.dev.write(self.ep_out, bytes(data), timeout)

    def read_pkt(self, timeout=TMO):
        return bytes(self.dev.read(self.ep_in, self.mps, timeout))

    def read_transfer(self):
        pkts = []
        while True:
            pkts.append(self.read_pkt())
            if len(pkts[-1]) < self.mps:
                return pkts

    def ep_req(self, req, wvalue, ep, length):
        return bytes(self.dev.ctrl_transfer(0xA2, req, wvalue, ep, length, TMO))

    def itf_req(self, req, wvalue, length):
        return bytes(self.dev.ctrl_transfer(0xA1, req, wvalue, self.itf, length, TMO))

    @staticmethod
    def hdr(msgid, tag, size, attr=0, termchar=0, inverse=None):
        inv = (~tag & 0xFF) if inverse is None else inverse
        return bytes([msgid, tag, inv, 0]) + struct.pack('<I', size) + bytes([attr, termchar, 0, 0])

    def send_msg(self, payload, tag=None):
        tag = self.next_tag() if tag is None else tag
        pkt = self.hdr(DEV_DEP_MSG_OUT, tag, len(payload), attr=1) + payload
        self.write(pkt + bytes(-len(pkt) % 4))
        return tag

    def request(self, size=1024, tag=None):
        tag = self.next_tag() if tag is None else tag
        self.write(self.hdr(REQUEST_DEV_DEP_MSG_IN, tag, size))
        return tag

    def query(self, payload):
        self.send_msg(payload)
        tag = self.request()
        pkts = self.read_transfer()
        raw = b''.join(pkts)
        expect(raw[0] == REQUEST_DEV_DEP_MSG_IN and raw[1] == tag and raw[2] == (~tag & 0xFF),
               f'Bulk-IN header {raw[:12].hex()} for bTag {tag}')
        return raw[12:12 + struct.unpack_from('<I', raw, 4)[0]], pkts

    def idn(self):
        data, _ = self.query(b'*IDN?\n')
        expect(data.startswith(b'TinyUSB'), f'*IDN? answered {data!r}')

    def recover(self):
        """INITIATE_CLEAR sequence, which also resets the example's query state; a port reset if that fails."""
        try:
            self.itf_req(INITIATE_CLEAR, 0, 1)
            for _ in range(50):
                rsp = self.itf_req(CHECK_CLEAR_STATUS, 0, 2)
                if rsp[0] != PENDING:
                    break
                if rsp[1] & 1:  # bmClear.D0: read Bulk-IN until a short packet
                    try:
                        self.read_pkt(200)
                    except usb.core.USBError:
                        pass
                time.sleep(0.02)
            self.dev.clear_halt(self.ep_out)
            self.dev.clear_halt(self.ep_in)
            self.idn()
            return
        except (usb.core.USBError, AssertionError, IndexError) as e:
            print(f'    recover: clear failed ({e}), resetting the port', flush=True)
        try:
            usb.util.dispose_resources(self.dev)
            self.dev.reset()
        except usb.core.USBError:
            pass
        time.sleep(1.0)
        self.find(10)
        self.itf_req(INITIATE_CLEAR, 0, 1)
        self.itf_req(CHECK_CLEAR_STATUS, 0, 2)
        self.dev.clear_halt(self.ep_out)
        self.dev.clear_halt(self.ep_in)
        self.idn()


def expect(cond, msg):
    if not cond:
        raise AssertionError(msg)


def next_write(d):
    """What the next Bulk-OUT write meets: 'stall', 'ack', or 'nak' (endpoint left unarmed)."""
    try:
        d.write(d.hdr(DEV_DEP_MSG_OUT, 77, 4, attr=1) + b'nop\n', timeout=500)
        return 'ack'
    except usb.core.USBError as e:
        if e.errno == errno.EPIPE:
            return 'stall'
        if e.errno == errno.ETIMEDOUT:
            return 'nak'
        raise


def need_full_speed(d):
    if d.mps != 64:
        raise SkipCase('needs a multi-packet reply (full-speed bulk endpoint)')


# ---------------------------------------------------------------------------------------------- cases
def t_idn_tag_sweep(d):
    """bTag 120..131 (across 127/128) and 250..255,1,2 (wrap)."""
    for start in (120, 250):
        d.tag = start
        for _ in range(12 if start == 120 else 8):
            d.idn()


def t_echo_sizes(d):
    """Multi-packet Bulk-OUT and Bulk-IN; every reply ends with a short packet (USBTMC 3.3 rule 10)."""
    for n in (1, 3, 4, 51, 52, 53, 100, 116, 180, 200, 220):
        msg = bytes((i * 7 + n) % 0x5F + 0x20 for i in range(n))
        data, pkts = d.query(msg)
        expect(data == msg, f'echo {n}: got {len(data)} bytes')
        expect(len(pkts[-1]) < d.mps, f'echo {n}: no short packet')


def t_indicator_pulse(d):
    rsp = d.itf_req(INDICATOR_PULSE, 0, 1)
    expect(rsp[0] == SUCCESS, f'INDICATOR_PULSE {rsp.hex()}')


def t_read_stb(d):
    """READ_STATUS_BYTE over Interrupt-IN, INTERRUPT_IN_BUSY while that is still queued (USB488 4.3.1.2)."""
    if d.ep_int is None:
        raise SkipCase('no Interrupt-IN endpoint')
    rsp = d.itf_req(READ_STATUS_BYTE, 5, 3)
    expect(rsp[0] == SUCCESS and rsp[1] == 5 and rsp[2] == 0, f'READ_STATUS_BYTE {rsp.hex()}')
    rsp = d.itf_req(READ_STATUS_BYTE, 6, 3)
    expect(rsp[0] == INT_IN_BUSY, f'READ_STATUS_BYTE while queued: {rsp.hex()}, want INTERRUPT_IN_BUSY')
    n = bytes(d.dev.read(d.ep_int, 8, TMO))
    expect(n[0] == 0x80 | 5, f'Interrupt-IN bNotify1 {n.hex()}')


def t_trigger(d):
    """TRIGGER (USB488 3.2.1.1) is accepted and the example raises SRQ in the Status Byte."""
    if d.ep_int is None:
        raise SkipCase('no Interrupt-IN endpoint')
    d.write(d.hdr(TRIGGER, d.next_tag(), 0))
    rsp = d.itf_req(READ_STATUS_BYTE, 7, 3)
    expect(rsp[0] == SUCCESS, f'READ_STATUS_BYTE {rsp.hex()}')
    n = bytes(d.dev.read(d.ep_int, 8, TMO))
    expect(n[0] == 0x87 and n[1] & 0x40, f'Status Byte after TRIGGER {n.hex()}, want SRQ')
    d.idn()


def _abort_in_before_reply(d, tag):
    d.send_msg(b'*IDN?\n')
    d.request(tag=tag)
    rsp = d.ep_req(INITIATE_ABORT_BULK_IN, tag ^ 1, d.ep_in, 2)
    expect(rsp[0] == NOT_IN_PROGRESS, f'abort with another bTag: {rsp.hex()}, want TRANSFER_NOT_IN_PROGRESS')
    rsp = d.ep_req(INITIATE_ABORT_BULK_IN, tag, d.ep_in, 2)
    expect(rsp[0] == SUCCESS and rsp[1] == tag, f'INITIATE_ABORT_BULK_IN bTag {tag}: {rsp.hex()}')
    rsp = d.ep_req(CHECK_ABORT_BULK_IN_STATUS, 0, d.ep_in, 8)
    expect(rsp[0] == PENDING and rsp[1] & 1, f'CHECK before the short packet is read: {rsp.hex()}, want PENDING D0=1')
    expect(len(d.read_pkt()) < d.mps, 'abort did not end with a short packet')
    rsp = d.ep_req(CHECK_ABORT_BULK_IN_STATUS, 0, d.ep_in, 8)
    expect(rsp[0] == SUCCESS and not rsp[1] & 1, f'CHECK after the short packet: {rsp.hex()}, want SUCCESS D0=0')
    d.idn()


def t_abort_in_high_tag(d):
    """INITIATE_ABORT_BULK_IN matches all 8 bits of bTag (Table 24); CHECK_ABORT_BULK_IN_STATUS is PENDING with
    D0=1 until the short packet is read, then SUCCESS (Tables 28, 29); the device then takes a new command."""
    _abort_in_before_reply(d, 200)


def t_abort_in_low_tag(d):
    _abort_in_before_reply(d, 50)


def t_abort_in_mid_transfer(d):
    """Abort with a full packet in flight: it is sent, then a short packet (Table 26, 4.2.1.5 example);
    NBYTES_TXD is the data bytes the host received."""
    need_full_speed(d)
    d.send_msg(bytes(0x41 + i % 26 for i in range(200)))
    tag = d.request()
    first = d.read_pkt()
    expect(len(first) == d.mps, f'first packet {len(first)} bytes')
    rsp = d.ep_req(INITIATE_ABORT_BULK_IN, tag, d.ep_in, 2)
    expect(rsp[0] == SUCCESS, f'INITIATE_ABORT_BULK_IN {rsp.hex()}')
    rest = d.read_transfer()
    rsp = d.ep_req(CHECK_ABORT_BULK_IN_STATUS, 0, d.ep_in, 8)
    sent = len(first) - 12 + sum(len(p) for p in rest)
    expect(rsp[0] == SUCCESS, f'CHECK_ABORT_BULK_IN_STATUS {rsp.hex()}')
    expect(struct.unpack_from('<I', rsp, 4)[0] == sent, f'NBYTES_TXD {rsp.hex()}, host received {sent}')


def t_abort_out(d):
    """INITIATE_ABORT_BULK_OUT reports the latest Bulk-OUT bTag, REQUEST_DEV_DEP_MSG_IN's too (Table 19), aborts
    only a matching 8-bit bTag (Tables 18, 20) and Halts bulk-OUT; CHECK reports NBYTES_RXD (Table 23).
    The abort of a transfer in progress needs full speed: a high-speed first packet holds more than the
    example accepts in one message."""
    d.send_msg(b'*IDN?\n')
    req_tag = d.request()
    d.read_transfer()
    rsp = d.ep_req(INITIATE_ABORT_BULK_OUT, 0, d.ep_out, 2)
    expect(rsp[0] == FAILED and rsp[1] == req_tag, f'abort with no transfer: {rsp.hex()}, want FAILED bTag {req_tag}')
    rsp = d.ep_req(CHECK_ABORT_BULK_OUT_STATUS, 0, d.ep_out, 8)
    expect(rsp[0] != SUCCESS, f'CHECK_ABORT_BULK_OUT_STATUS with no abort: {rsp.hex()}')
    if d.mps != 64:
        return
    d.write(d.hdr(DEV_DEP_MSG_OUT, 150, 200, attr=1) + bytes(d.mps - 12))  # first packet of 200 bytes
    rsp = d.ep_req(INITIATE_ABORT_BULK_OUT, 151, d.ep_out, 2)
    expect(rsp[0] == NOT_IN_PROGRESS and rsp[1] == 150, f'abort with another bTag: {rsp.hex()}')
    rsp = d.ep_req(INITIATE_ABORT_BULK_OUT, 150, d.ep_out, 2)
    expect(rsp[0] == SUCCESS and rsp[1] == 150, f'INITIATE_ABORT_BULK_OUT {rsp.hex()}')
    expect(next_write(d) == 'stall', 'bulk-OUT not Halted after the abort')
    rsp = d.ep_req(CHECK_ABORT_BULK_OUT_STATUS, 0, d.ep_out, 8)
    expect(rsp[0] == SUCCESS and struct.unpack_from('<I', rsp, 4)[0] == d.mps - 12,
           f'CHECK_ABORT_BULK_OUT_STATUS {rsp.hex()}')
    d.dev.clear_halt(d.ep_out)


def t_header_errors(d):
    """Bulk-OUT header errors Halt bulk-OUT (Table 7 indexes 1-3, 3.2.2.4); CLEAR_FEATURE recovers."""
    cases = [
        (d.hdr(DEV_DEP_MSG_OUT, 9, 4, inverse=0) + b'abcd', 'bTagInverse not ~bTag'),
        (d.hdr(DEV_DEP_MSG_OUT, 0, 4) + b'abcd', 'bTag 0'),
        (d.hdr(DEV_DEP_MSG_OUT, 10, 0, attr=1), 'DEV_DEP_MSG_OUT TransferSize 0'),
        (d.hdr(REQUEST_DEV_DEP_MSG_IN, 11, 0), 'REQUEST_DEV_DEP_MSG_IN TransferSize 0'),
        (d.hdr(REQUEST_DEV_DEP_MSG_IN, 12, 64, attr=2, termchar=0x0A), 'TermCharEnabled without TermChar support'),
        (bytes([DEV_DEP_MSG_OUT, 13, 0xF2, 0]), 'incomplete header'),
        (d.hdr(3, 14, 4) + b'abcd', 'reserved MsgID'),
    ]
    for pkt, what in cases:
        d.write(pkt)
        r = next_write(d)
        expect(r == 'stall', f'{what}: next write {r}, want stall')
        d.dev.clear_halt(d.ep_out)
        d.idn()


def t_excess_data(d):
    """Alignment bytes are accepted (3.2 rule 2); more than data + alignment Halts bulk-OUT (Table 7 index 6)."""
    d.write(d.hdr(DEV_DEP_MSG_OUT, d.next_tag(), 5, attr=1) + b'ABCDE' + bytes(3))
    tag = d.request()
    raw = b''.join(d.read_transfer())
    expect(raw[1] == tag and raw[12:17] == b'ABCDE', f'aligned message, reply {raw[:20].hex()}')
    d.write(d.hdr(DEV_DEP_MSG_OUT, d.next_tag(), 4, attr=1) + b'WXYZ' + bytes(8))
    r = next_write(d)
    expect(r == 'stall', f'excess data: next write {r}, want stall')
    d.dev.clear_halt(d.ep_out)


def t_short_transfer(d):
    """A transfer ending short of TransferSize Halts bulk-OUT (Table 7 index 4)."""
    d.write(d.hdr(DEV_DEP_MSG_OUT, d.next_tag(), 100, attr=1) + bytes(20))
    r = next_write(d)
    expect(r == 'stall', f'short transfer: next write {r}, want stall')
    d.dev.clear_halt(d.ep_out)


def t_clear_with_pending_in(d):
    """INITIATE_CLEAR with a full packet queued: a short packet follows it, CHECK_CLEAR_STATUS is PENDING with
    D0=1 until the host has read it (Tables 32, 35)."""
    need_full_speed(d)
    d.send_msg(bytes(0x61 + i % 26 for i in range(200)))
    d.request()
    d.read_pkt()
    expect(d.itf_req(INITIATE_CLEAR, 0, 1)[0] == SUCCESS, 'INITIATE_CLEAR')
    rsp = d.itf_req(CHECK_CLEAR_STATUS, 0, 2)
    expect(rsp[0] == PENDING and rsp[1] & 1, f'CHECK_CLEAR_STATUS with a packet queued: {rsp.hex()}')
    d.read_transfer()
    rsp = d.itf_req(CHECK_CLEAR_STATUS, 0, 2)
    expect(rsp[0] == SUCCESS and not rsp[1] & 1, f'CHECK_CLEAR_STATUS after the short packet: {rsp.hex()}')
    d.dev.clear_halt(d.ep_out)
    d.idn()


def t_check_clear_without_initiate(d):
    """CHECK_CLEAR_STATUS with no clear in progress does not report one completed."""
    rsp = d.itf_req(CHECK_CLEAR_STATUS, 0, 2)
    expect(rsp[0] != SUCCESS, f'CHECK_CLEAR_STATUS without INITIATE_CLEAR: {rsp.hex()}')
    d.idn()


def t_clear_out_halt_mid_reply(d):
    """CLEAR_FEATURE(bulk-OUT) while a reply is sent: the reply still completes."""
    need_full_speed(d)
    msg = bytes(0x30 + i % 10 for i in range(200))
    d.send_msg(msg)
    tag = d.request()
    first = d.read_pkt()
    d.dev.clear_halt(d.ep_out)
    raw = first + b''.join(d.read_transfer())
    expect(raw[1] == tag and raw[12:212] == msg, f'reply after CLEAR_FEATURE(bulk-OUT): {len(raw)} bytes')
    d.idn()


def t_clear_in_halt_mid_reply(d):
    """CLEAR_FEATURE(bulk-IN) while a reply is sent drops it; bulk-OUT takes commands again."""
    need_full_speed(d)
    d.send_msg(bytes(0x40 + i % 20 for i in range(200)))
    d.request()
    d.read_pkt()
    d.dev.clear_halt(d.ep_in)
    r = next_write(d)
    expect(r == 'ack', f'write after CLEAR_FEATURE(bulk-IN): {r}')


CASES = [
    t_idn_tag_sweep, t_echo_sizes, t_indicator_pulse, t_read_stb, t_trigger,
    t_abort_in_high_tag, t_abort_in_low_tag, t_abort_in_mid_transfer, t_abort_out,
    t_header_errors, t_excess_data, t_short_transfer,
    t_clear_with_pending_in, t_check_clear_without_initiate,
    t_clear_out_halt_mid_reply, t_clear_in_halt_mid_reply,
]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--uid', required=True, help='the board serial the example reports')
    ap.add_argument('--timeout', type=float, default=10, help='seconds to wait for enumeration')
    ap.add_argument('cases', nargs='*', help='run only these cases (names without t_)')
    args = ap.parse_args()

    d = Dut(args.uid, args.timeout)
    print(f'usbtmc {VID:04x}:{PID:04x} mps {d.mps}', flush=True)
    d.recover()
    d.send_msg(f'delay {APP_DELAY_MS}\n'.encode())  # newline: the example atoi()s an unterminated buffer
    fails = []
    for case in CASES:
        name = case.__name__[2:]
        if args.cases and name not in args.cases:
            continue
        try:
            case(d)
            print(f'PASS  {name}', flush=True)
        except SkipCase as e:
            print(f'SKIP  {name}: {e}', flush=True)
        except (AssertionError, usb.core.USBError) as e:
            fails.append(name)
            print(f'FAIL  {name}: {e}', flush=True)
        try:
            d.recover()
        except (usb.core.USBError, AssertionError, RuntimeError) as e:
            print(f'device lost after {name}: {e}', flush=True)
            return 1
    print(f'{len(fails)} failed: {fails}' if fails else 'all passed', flush=True)
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
