#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Board locks for the TinyUSB HIL rig.

Board locks are kernel flocks in BOARD_LOCK_DIR arbitrating hardware access
between dev sessions and CI's hil_test.py (never stop the actions-runner). The
mechanics are agentrc's shared board-lock primitive, from the agentrc helper/agentrc.py
selects (the installed one; CI's pin, test/hil/agentrc.json); this adapter
is the module and CLI (hold/release/status) every caller uses, and adds
`hold --all [--config CONFIG.json]` over this host's roster. Imported, callers get the provider
module itself (this name is registered as it), so a monkeypatched hil_lock.BOARD_LOCK_DIR
reaches every function that reads it.
"""
import argparse
import json
import socket
import sys
from pathlib import Path

if __package__:
    from . import agentrc, hil_util
else:  # run as a script: helper/ is sys.path[0], unless -P or PYTHONSAFEPATH dropped it
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import agentrc
    import hil_util

# a __main__ run keeps this module, whose main() wraps the provider's CLI
_lock = agentrc.load_file('skills/hil/scripts/hil_lock.py')
if __name__ != '__main__':
    sys.modules[__name__] = _lock


def boards_from_config(config: str) -> list:
    """All board lock names (agentrc's registry names for registered boards), INCLUDING boards-skip: `hold --all` guards rig-wide
    operations, and parked boards can still be touched (a farm check -b names them
    explicitly), so a rig-wide hold that skipped them would leave a gap."""
    try:
        with open(config) as f:
            cfg = json.load(f)
        entries = cfg['boards'] + cfg.get('boards-skip', [])
    except (OSError, ValueError, KeyError, TypeError) as e:
        print(f'ERROR: cannot read board roster {config}: {e}', file=sys.stderr)
        sys.exit(1)
    lock_name = agentrc.load_file('skills/hil/scripts/hil_boards.py').lock_name
    try:
        return [lock_name(b) for b in entries]
    except (ValueError, KeyError, TypeError) as e:   # agentrc's registry refused an entry, or one is not a named object
        print(f'ERROR: cannot name the board locks of {config}: {e}', file=sys.stderr)
        sys.exit(1)


def main():
    """`hold --all` expands to the roster's boards; everything else is the provider's CLI."""
    if sys.argv[1:2] == ['hold'] and '--all' in sys.argv[2:]:
        ap = argparse.ArgumentParser(usage='hil_lock.py hold --all [--config CONFIG.json] --reason TEXT')
        ap.add_argument('boards', nargs='*', help='ignored: --all takes the roster')
        ap.add_argument('--all', action='store_true')
        ap.add_argument('--config', help="board roster JSON (default: this host's, hil_util.CONFIG_BY_HOST, "
                        "else test/hil/local.json)")
        ap.add_argument('--reason', required=True)
        a = ap.parse_args(sys.argv[2:])
        try:
            config = hil_util.roster_path(socket.gethostname(), a.config, local_fallback=True)
        except LookupError as e:
            ap.error(str(e))
        sys.argv[1:] = ['hold', *boards_from_config(config), '--reason', a.reason]
    _lock.main()


if __name__ == '__main__':
    main()
