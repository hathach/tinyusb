#!/usr/bin/env python3
"""Write the NXP LPC "valid user code" checksum into an ELF's vector table.

The boot ROM starts a flash image only when the first 8 words of its vector table sum to zero:
word 7 holds the two's complement of the sum of words 0-6 (LPC43xx: UM10503 6.4.4.1). J-Link patches
that word only at the default boot address (OpenOCD's calc_checksum covers every bank it declares), so
an image booted from elsewhere, such as LPC43 flash bank B, carries its own to boot whatever loads it.
The address must hold the image's vector table (a Thumb reset handler just above it), else the tool
fails rather than patch whatever data sits there.

    lpc_vector_checksum.py --address 0x1B000000 firmware.elf          # patch in place
    lpc_vector_checksum.py --address 0x1B000000 --check firmware.elf  # exit 1 if invalid
"""
import argparse
import struct
import sys

SHT_PROGBITS = 1
VECTOR_WORDS = 8
RESET_WINDOW = 0x1000000  # the reset handler lies within 16 MB above its vector table


def checksum(words):
    """Word 7 that makes the first 8 vector table words sum to zero."""
    return (-sum(words[:7])) & 0xFFFFFFFF


def find_vectors(elf, address):
    """File offset of the 8 vector words at address, from the section that holds them."""
    if elf[:4] != b'\x7fELF' or elf[4] != 1 or elf[5] != 1:
        raise ValueError('not a little-endian ELF32 file')
    e_shoff, = struct.unpack_from('<I', elf, 0x20)
    e_shentsize, e_shnum = struct.unpack_from('<HH', elf, 0x2E)
    for i in range(e_shnum):
        sh = e_shoff + i * e_shentsize
        sh_type, = struct.unpack_from('<I', elf, sh + 4)
        sh_addr, sh_offset, sh_size = struct.unpack_from('<III', elf, sh + 12)
        if sh_type == SHT_PROGBITS and sh_addr <= address and address + 4 * VECTOR_WORDS <= sh_addr + sh_size:
            return sh_offset + address - sh_addr
    raise ValueError(f'no section holds a vector table at 0x{address:08X}')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('elf')
    parser.add_argument('--address', required=True, type=lambda s: int(s, 0),
                        help='vector table address, e.g. 0x1B000000')
    parser.add_argument('--check', action='store_true', help='verify only, exit 1 when invalid')
    args = parser.parse_args(argv)

    with open(args.elf, 'rb') as f:
        elf = bytearray(f.read())
    try:
        pos = find_vectors(elf, args.address)
    except ValueError as e:
        print(f'{args.elf}: {e}', file=sys.stderr)
        return 2

    words = list(struct.unpack_from(f'<{VECTOR_WORDS}I', elf, pos))
    # a vector table opens with the stack top and a Thumb reset handler in the same flash bank;
    # anything else at the address means the image did not put its vectors there
    if not (words[1] & 1 and args.address <= words[1] < args.address + RESET_WINDOW):
        print(f'{args.elf}: no vector table at 0x{args.address:08X} (reset vector 0x{words[1]:08X})',
              file=sys.stderr)
        return 2
    want = checksum(words)
    if words[7] == want:
        return 0
    if args.check:
        print(f'{args.elf}: vector checksum at 0x{args.address + 28:08X} is 0x{words[7]:08X}, '
              f'want 0x{want:08X}', file=sys.stderr)
        return 1
    struct.pack_into('<I', elf, pos + 28, want)
    with open(args.elf, 'r+b') as f:
        f.write(elf)
    return 0


if __name__ == '__main__':
    sys.exit(main())
