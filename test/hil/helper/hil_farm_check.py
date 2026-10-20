#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HIL farm check (the `farm-check` skill): agentrc's engine, from lib/agentrc at the pin,
run with tinyusb's profile (helper/farm_profile.py). `--help` for the flags; the engine's
module docstring defines the rows, the document and the exit status."""
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # the helper package
from helper import agentrc


def load_engine():
    # literal paths: the engine loads hil_lock, hil_process and hil_report_core beside it, and
    # launches itself (cleanup holder) and hil_process (run_tracked's supervisor)
    return agentrc.load('skills/hil/scripts/hil_farm_check.py', 'tinyusb_hil_farm_check',
                        requires=('skills/hil/scripts/hil_lock.py',
                                  'skills/hil/scripts/hil_process.py',
                                  'skills/hil/scripts/hil_report_core.py'))


def main(argv=None) -> int:
    # toolchain/flasher CLIs live in the user bin dirs, which non-login shells may lack --
    # the same PATH shim hil_remote.py applies on the remote side
    for d in (Path.home() / 'bin', Path.home() / '.local' / 'bin'):
        if d.is_dir() and str(d) not in os.environ.get('PATH', '').split(os.pathsep):
            os.environ['PATH'] = f'{d}{os.pathsep}{os.environ.get("PATH", "")}'
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
