#!/usr/bin/env python3
"""Code size of TinyUSB examples: agentrc's code-size engine, from lib/agentrc at the pin
(`python3 tools/get_deps.py lib/agentrc`), run with tools/code_size_profile.py. `--help` for
the commands; the engine's module docstring defines them and the profile.

Usage:
  python tools/code_size.py report -b raspberry_pi_pico -e device/cdc_msc --symbols
  python tools/code_size.py diff -b raspberry_pi_pico
  python tools/code_size.py diff -b raspberry_pi_pico -b raspberry_pi_pico2
  python tools/code_size.py diff -b raspberry_pi_pico -f portable/raspberrypi
  python tools/code_size.py diff -b raspberry_pi_pico -e device/cdc_msc
  python tools/code_size.py diff -b raspberry_pi_pico -e device/cdc_msc --bloaty
  python tools/code_size.py diff -b raspberry_pi_pico --engine linkermap --json
  python tools/code_size.py diff --ci                                  # CI-pinned boards, combined
  python tools/code_size.py diff -b raspberry_pi_pico -b raspberry_pi_pico2 --combined  # combine listed boards
"""
import os
import sys

TOOLS = os.path.dirname(os.path.realpath(__file__))
sys.path.insert(1, os.path.join(os.path.dirname(TOOLS), 'test', 'hil'))  # the helper package
from helper import agentrc  # noqa: E402


def load_engine():
    return agentrc.load_file('skills/code-size/scripts/code_size.py')


def main(argv=None):
    try:
        engine = load_engine()
        import code_size_profile
    except ImportError as e:
        print(f'code_size.py: refused: {e}', file=sys.stderr)
        return 2
    return engine.main(code_size_profile, argv)


if __name__ == '__main__':
    sys.exit(main())
