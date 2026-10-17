#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Board locks for the TinyUSB HIL rig.

Board locks are kernel flocks in BOARD_LOCK_DIR arbitrating hardware access
between dev sessions and CI's hil_test.py (never stop the actions-runner). The
mechanics are agentrc's shared board-lock primitive, tools/agentrc at the commit
tools/get_deps.py pins (`python3 tools/get_deps.py tools/agentrc`); this adapter
is the module and CLI (hold/release/status) every caller uses, and adds
`hold --all [--config CONFIG.json]` over tinyusb's roster.
"""
import argparse
import importlib.util
import json
import os
import sys
from pathlib import Path

TINYUSB_ROOT = Path(__file__).resolve().parents[3]
PROVIDER = TINYUSB_ROOT / 'tools' / 'agentrc' / 'skills' / 'hil' / 'scripts' / 'hil_lock.py'
FETCH = 'python3 tools/get_deps.py tools/agentrc'


def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


# Proves the checked-out commit, not the file contents: a locally edited provider at the pin
# still loads, and the rig's staged tree has no git objects to check it against.
_deps = _load('tinyusb_get_deps', TINYUSB_ROOT / 'tools' / 'get_deps.py')
_pin, _head = _deps.deps_optional['tools/agentrc'][1], _deps.dep_head('tools/agentrc')
if _head != _pin or not PROVIDER.is_file():
    raise ImportError(f"tools/agentrc must be at {_pin} (found {_head or 'nothing'}): {FETCH}")
# A __main__ run must not load the provider as __main__ too, or its own CLI would run.
_lock = _load('hil_lock' if __name__ == '__main__' else __name__, PROVIDER)


def boards_from_config(config: str) -> list:
    """All board names, INCLUDING boards-skip: `hold --all` guards rig-wide
    operations, and parked boards can still be touched (pool_check -b names them
    explicitly), so a rig-wide hold that skipped them would leave a gap."""
    try:
        with open(config) as f:
            cfg = json.load(f)
            return [b['name'] for b in cfg['boards'] + cfg.get('boards-skip', [])]
    except (OSError, ValueError, KeyError) as e:
        print(f'ERROR: cannot read board roster {config}: {e}', file=sys.stderr)
        sys.exit(1)


def main():
    """`hold --all` expands to the roster's boards; everything else is the provider's CLI."""
    if sys.argv[1:2] == ['hold'] and '--all' in sys.argv[2:]:
        ap = argparse.ArgumentParser(usage='hil_lock.py hold --all [--config CONFIG.json] --reason TEXT')
        ap.add_argument('boards', nargs='*', help='ignored: --all takes the roster')
        ap.add_argument('--all', action='store_true')
        ap.add_argument('--config', default=str(TINYUSB_ROOT / 'test' / 'hil' / 'tinyusb.json'),
                        help='board roster JSON (default: test/hil/tinyusb.json)')
        ap.add_argument('--reason', required=True)
        a = ap.parse_args(sys.argv[2:])
        sys.argv[1:] = ['hold', *boards_from_config(a.config), '--reason', a.reason]
    _lock.main()


if __name__ == '__main__':
    main()
else:
    # Callers get the provider module itself, so a monkeypatched hil_lock.BOARD_LOCK_DIR
    # reaches every function that reads it.
    sys.modules[__name__] = _lock
