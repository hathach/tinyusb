#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# The membrowse-identical job uploads, without configuring, the target list build.py
# derives in Python (get_examples + skip_example). Configure the real thing and compare:
# a drift would leave a target with no upload, or upload a name nothing builds.
# Needs the toolchains; a case whose toolchain is missing skips.
#   python3 test/hil/test/test_membrowse_targets.py
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, os.path.join(REPO, 'tools'))
import build  # noqa: E402
import build_utils  # noqa: E402

TARGET_NAME = re.compile(r'--target-name (\S+)')


def python_targets(board, name, examples=None):
    family = build.find_family(board)
    return {f'{name}/{e.split("/", 1)[1]}' for e in build.get_examples(family)
            if (examples is None or e in examples)
            and not build_utils.skip_example(e, board, ('TOOLCHAIN=gcc',))}


def registered_targets(build_dirs):
    """The --target-name of every <example>-membrowse-upload command CMake wrote."""
    names = set()
    for d in build_dirs:
        with open(os.path.join(d, 'build.ninja')) as f:
            for line in f:
                if 'membrowse_cli.py report' in line and '--upload' in line:
                    names.update(TARGET_NAME.findall(line))
    return names


class TestMembrowseTargetMirror(unittest.TestCase):
    def setUp(self):
        self.cwd = os.getcwd()
        os.chdir(REPO)
        self.addCleanup(os.chdir, self.cwd)
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)

    def configure(self, board, *flags):
        if not (shutil.which('cmake') and shutil.which('ninja')):
            self.skipTest('cmake/ninja not installed')
        d = os.path.join(self.tmp.name, board)
        r = subprocess.run(['cmake', 'examples', '-B', d, '-GNinja', f'-DBOARD={board}',
                            '-DCMAKE_BUILD_TYPE=MinSizeRel', '-DTOOLCHAIN=gcc', *flags],
                           capture_output=True, text=True)
        if r.returncode != 0 and 'compiler' in r.stderr.lower() + r.stdout.lower():
            self.skipTest(f'{board}: no toolchain')
        self.assertEqual(r.returncode, 0, r.stderr[-2000:])
        return d

    def test_a_pinned_board(self):
        d = self.configure('stm32f407disco')
        self.assertEqual(registered_targets([d]), python_targets('stm32f407disco', 'stm32f407disco'))

    def test_rp2040_with_its_pio_usb_host_examples(self):
        d = self.configure('raspberry_pi_pico')
        self.assertEqual(registered_targets([d]), python_targets('raspberry_pi_pico', 'raspberry_pi_pico'))

    def test_a_named_variant_uploads_under_its_name(self):
        d = self.configure('stm32f407disco', '-DMEMBROWSE_BOARD=stm32f407disco-X')
        self.assertEqual(registered_targets([d]), python_targets('stm32f407disco', 'stm32f407disco-X'))

    def test_an_espressif_named_variant(self):
        # one idf tree per example: configure one, enough for the name and the mirror
        if not shutil.which('idf.py'):
            self.skipTest('ESP-IDF not exported')
        board, name, example = 'espressif_s3_devkitm', 'espressif_s3_devkitm-DMA', 'device/cdc_msc_freertos'
        d = os.path.join(self.tmp.name, 'esp')
        r = subprocess.run(['idf.py', '-C', f'examples/{example}', '-B', d, '-GNinja', f'-DBOARD={board}',
                            f'-DMEMBROWSE_BOARD={name}', 'reconfigure'], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout[-2000:] + r.stderr[-2000:])
        self.assertEqual(registered_targets([d]), python_targets(board, name, [example]))


if __name__ == '__main__':
    unittest.main()
