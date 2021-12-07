#!/usr/bin/env python3
import sys
import struct

def align(num, alignment):
    if num % alignment != 0:
        num += (alignment - num % alignment)
    return num


def process_file(input, output):
    with open(input, 'rb') as fin:
        content = bytearray(fin.read())

    align_value = 512
    padded_length = align(len(content), align_value)
    # pad file to actual length
    content += b'\x00' * (padded_length - len(content))

    struct_format = '<L8sLL'
    (instruction, magic, checksum, length) = struct.unpack_from(struct_format, content)

    if magic != b'eGON.BT0':
        print("Magic is invalid:", magic)
        return 2

    checksum = 0x5F0A6C39
    length = align(length, align_value)

    struct.pack_into(struct_format, content, 0, instruction, magic, checksum, length)

    checksum = 0
    for i in range(0, length, 4):
        (n, ) = struct.unpack_from('<L', content, i)
        checksum += n
        checksum %= 4294967296

    struct.pack_into(struct_format, content, 0, instruction, magic, checksum, length)

    with open(output, 'wb') as fout:
        fout.write(content)
    return 0

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: mksunxi.py input.bin output.bin")
        exit(1)
    exit(process_file(sys.argv[1], sys.argv[2]))