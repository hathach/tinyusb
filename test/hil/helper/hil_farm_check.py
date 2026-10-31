#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HIL farm check (the `hil` skill's Farm check): agentrc's engine, from lib/agentrc at the pin,
run with tinyusb's profile (helper/farm_profile.py). `--help` for the flags; the engine's
module docstring defines the rows, the document and the exit status."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # the helper package
from helper import agentrc


def load_engine():
    return agentrc.load_file('skills/hil/scripts/hil_farm_check.py')


def main(argv=None) -> int:
    try:
        engine = load_engine()
        from helper import farm_profile, hil_lock   # each loads its agentrc modules at the pin too
    except ImportError as e:
        print(f'farm check: refused: {e}', file=sys.stderr)
        return 2
    # the harness's own lock module: one instance, the one every other tinyusb caller and test patches
    return engine.main(farm_profile, argv, lock_module=hil_lock)


if __name__ == '__main__':
    sys.exit(main())
