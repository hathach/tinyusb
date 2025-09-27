#!/usr/bin/env python3
import argparse
import random
import os
import sys
import time
import subprocess
from pathlib import Path
from multiprocessing import Pool
from weakref import finalize


def print_carray(f, payload):
    while len(payload) > 0:
        f.write('\n    ')
        f.write(', '.join('0x{:02x}'.format(x) for x in payload[0:16]))
        f.write(',')
        payload = payload[16:]
    f.write('\n')


def main():
    parser = argparse.ArgumentParser(description='Convert binary files to C array format')
    parser.add_argument('files', nargs='+', help='Binary files to convert')
    args = parser.parse_args()

    files = args.files
    for fin_name in files:
        if not os.path.isfile(fin_name):
            print(f"File {fin_name} does not exist")
            continue

        with open(fin_name, 'rb') as fin:
            contents = fin.read()
            fout_name = fin_name + '.h'
            with open(fout_name, 'w') as fout:
                print(f"Converting {fin_name} to {fout_name}")
                fout.write(f'const size_t bindata_len = {len(contents)};\n')
                fout.write(f'const uint8_t bindata[] __attribute__((aligned(16))) = {{')
                print_carray(fout, contents)
                fout.write('};\n')


if __name__ == '__main__':
    sys.exit(main())
