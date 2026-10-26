#!/usr/bin/env python3
"""Check that every example enumerates with a unique USB PID.

Each example's usb_descriptors.c hardcodes its idProduct (0x40xx). Uniqueness is what
guarantees back-to-back re-enumeration on the HIL rig and a fresh host driver match, and
it is easy to break by hand: a new example copying a neighbour's PID, or an arithmetic
PID (dynamic_configuration derives a second one from USB_PID). This collects every
`#define USB_PID 0x....`, every literal `.idProduct = 0x....`, and every `USB_PID + <n>`
derivation across examples/, and fails on any duplicate value.
"""

import re
import sys
from pathlib import Path

EXAMPLES = Path(__file__).resolve().parents[1] / 'examples'

RE_DEFINE = re.compile(r'#define\s+USB_PID\s+\(?(0x[0-9a-fA-F]+)\)?')
RE_LITERAL = re.compile(r'\.idProduct\s*=\s*(0x[0-9a-fA-F]+)')
RE_DERIVED = re.compile(r'\.idProduct\s*=\s*USB_PID\s*\+\s*(0x[0-9a-fA-F]+|\d+)')


def main() -> int:
    pids: dict[int, list[str]] = {}
    for f in sorted(EXAMPLES.glob('*/*/src/usb_descriptors.c')):
        text = f.read_text(errors='replace')
        rel = f.relative_to(EXAMPLES.parent)
        base = None
        m = RE_DEFINE.search(text)
        if m:
            base = int(m.group(1), 16)
        for m in RE_LITERAL.finditer(text):
            pids.setdefault(int(m.group(1), 16), []).append(str(rel))
        for m in RE_DERIVED.finditer(text):
            if base is None:
                print(f'{rel}: derived idProduct but no USB_PID define', file=sys.stderr)
                return 1
            pids.setdefault(base + int(m.group(1), 0), []).append(f'{rel} (USB_PID + {m.group(1)})')
        # examples whose descriptor uses .idProduct = USB_PID pick up the define itself
        if base is not None and re.search(r'\.idProduct\s*=\s*USB_PID\s*[,;]', text):
            pids.setdefault(base, []).append(str(rel))

    dups = {pid: users for pid, users in pids.items() if len(users) > 1}
    for pid, users in sorted(dups.items()):
        print(f'duplicate USB PID 0x{pid:04x}:', file=sys.stderr)
        for u in users:
            print(f'  {u}', file=sys.stderr)
    if dups:
        return 1
    print(f'{len(pids)} unique example USB PIDs')
    return 0


if __name__ == '__main__':
    sys.exit(main())
