#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Where the HIL harness and tools load agentrc's shared scripts from (agentrc `hil` skill, Shared mechanisms):

  staged     <root>/.agentrc/ exists (hil_remote.py staged this tree): that copy, checked against its manifest
  pin        CI (GITHUB_ACTIONS or CI true): test/hil/agentrc.json's {url, commit}, fetched into lib/agentrc by
             `python3 test/hil/helper/agentrc.py fetch`
  installed  otherwise "${AGENTRC_DIR:-$HOME/.claude}", never fetched

The choice runs no agentrc code; agentrc's agentrc_source.py, whose API literal is checked first, then takes the
identity of SCRIPTS' closure once per process. Every module loads through agentrc's hil_load: one module per file
per process, whichever name loads it first, and only a closure file whose bytes are still the identity's.

  python3 test/hil/helper/agentrc.py fetch   CI only: fetch the pin into lib/agentrc
  python3 test/hil/helper/agentrc.py check   the selected agentrc's identity, as JSON
"""
import ast
import importlib.util
import json
import os
import re
import subprocess
import sys
from pathlib import Path

TINYUSB_ROOT = Path(__file__).resolve().parents[3]
PIN_FILE = TINYUSB_ROOT / 'test' / 'hil' / 'agentrc.json'
DEP = 'lib/agentrc'   # CI's fetch destination, ignored by git; never read outside CI
STAGED = '.agentrc'
FETCH = 'python3 test/hil/helper/agentrc.py fetch'
INSTALL = ('install agentrc (python3 <agentrc checkout>/install.py install), or set AGENTRC_DIR to an agentrc '
           'checkout')
HIL_LOAD = 'skills/hil/scripts/hil_load.py'
SOURCE = 'skills/hil/scripts/agentrc_source.py'
API = 1   # agentrc_source.py's API this bootstrap speaks
# every agentrc script tinyusb loads or runs; their closure is the identity a receipt binds and a stage copies
SCRIPTS = (
    'skills/code-size/scripts/code_size.py',
    'skills/hil/scripts/hil_boards.py',
    'skills/hil/scripts/hil_farm_check.py',
    'skills/hil/scripts/hil_flash.py',
    'skills/hil/scripts/hil_lock.py',
    'skills/hil/scripts/hil_openocd_esp.py',
    'skills/hil/scripts/hil_remote_core.py',
    'skills/hil/scripts/hil_report_core.py',
    'skills/hil/scripts/hil_report_notice.py',
    'skills/hil/scripts/hil_tt.py',
    'skills/hil/scripts/hil_usbtest.py',
    'skills/hil/scripts/usb_observe.py',
    'skills/rtt/scripts/rtt.py',
    'skills/target-debug/scripts/sysview_record.py',
    'skills/target-debug/scripts/sysview_report.py',
)


def ci():
    """True in CI: GITHUB_ACTIONS or CI set true; unset or empty is false, any other value refused."""
    on = False
    for var in ('GITHUB_ACTIONS', 'CI'):
        value = os.environ.get(var, '').strip().lower()
        if value in ('1', 'true', 'yes'):
            on = True
        elif value not in ('', '0', 'false', 'no'):
            raise ImportError(f'{var}={os.environ[var]!r} is neither true nor false')
    return on


def pin():
    """test/hil/agentrc.json's (url, commit)."""
    try:
        data = json.loads(PIN_FILE.read_text(encoding='utf-8'))
    except (OSError, ValueError, RecursionError) as e:
        raise ImportError(f'{PIN_FILE} is not readable JSON ({e})') from None
    url, commit = (data.get('url'), data.get('commit')) if isinstance(data, dict) else (None, None)
    if (not isinstance(url, str) or not url.strip() or url != url.strip()
            or not isinstance(commit, str) or not re.fullmatch(r'[0-9a-f]{40}', commit)):
        raise ImportError(f'{PIN_FILE} names no {{url, commit}} with a 40-hex commit ({data!r})')
    return url, commit


def selected():
    """(source, root, pinned commit or None, remedy): the agentrc this process loads, chosen without running it."""
    if os.path.lexists(TINYUSB_ROOT / STAGED):
        return 'staged', TINYUSB_ROOT / STAGED, None, 'stage the run again with hil_remote.py'
    if ci():
        return 'pin', TINYUSB_ROOT / DEP, pin()[1], FETCH
    return 'installed', Path(os.environ.get('AGENTRC_DIR') or Path.home() / '.claude').absolute(), None, INSTALL


def _git(root, *args):
    return subprocess.run(['git', '-C', str(root), *args], capture_output=True, text=True,
                          env={k: v for k, v in os.environ.items() if not k.startswith('GIT_')})


def fetch():
    """CI only: lib/agentrc as a clean checkout at the pin, fetched unless it already is one; its path."""
    if not ci():
        raise ImportError(f'fetch runs in CI only: a dev PC loads the installed agentrc ({INSTALL})')
    url, commit = pin()
    dest = TINYUSB_ROOT / DEP
    if not dest.exists():
        dest.mkdir(parents=True)
        for args in (('init', '-q'), ('remote', 'add', 'origin', url),
                     ('fetch', '-q', '--depth', '1', 'origin', commit), ('checkout', '-q', '--detach', 'FETCH_HEAD')):
            p = _git(dest, *args)
            if p.returncode:
                raise ImportError(f'{DEP}: git {" ".join(args)}: {p.stderr.strip()}')
    head, status = _git(dest, 'rev-parse', 'HEAD'), _git(dest, 'status', '--porcelain')
    if head.returncode or head.stdout.strip() != commit or status.returncode or status.stdout.strip():
        raise ImportError(f'{DEP} exists but is not a clean checkout at {commit} '
                          f'({(head.stdout + head.stderr + status.stdout).strip()[:200]}); remove it and fetch again')
    return dest


def _api(root, remedy):
    path = root / SOURCE
    try:
        tree = ast.parse(path.read_text(encoding='utf-8'))
        found = next((ast.literal_eval(n.value) for n in tree.body if isinstance(n, ast.Assign)
                      and any(getattr(t, 'id', None) == 'API' for t in n.targets)), None)
    # what ast.parse and literal_eval document raising on malformed input
    except (OSError, SyntaxError, ValueError, TypeError, MemoryError, RecursionError) as e:
        raise ImportError(f'no usable agentrc at {root}: {path}: {e} ({remedy})') from None
    if found != API:
        raise ImportError(f'agentrc at {root} speaks agentrc_source API {found}, this checkout needs {API}: '
                          f'use an agentrc that does ({remedy})')


def _hil_load(root):
    """agentrc's hil_load from root, registered as `agentrc_hil_load` the way its own scripts bootstrap it."""
    # resolved: hil_load's ROOT, which its load_file() reads, must be the checkout, not an install of skill symlinks
    spec = importlib.util.spec_from_file_location('agentrc_hil_load', (root / HIL_LOAD).resolve())
    if spec.name not in sys.modules:
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        sys.modules.setdefault(spec.name, mod)
    return sys.modules[spec.name].load(spec.name, spec.origin)


_LOADED = None   # (identity, agentrc_source module, remedy) of the first identity() that succeeded


def identity():
    """The selected agentrc's identity (agentrc_source.py), the same object for the whole process."""
    global _LOADED
    if _LOADED is None:
        source, root, commit, remedy = selected()
        _api(root, remedy)
        try:
            hil_load = _hil_load(root)
        except OSError as e:   # a staged tree or install without hil_load: refused like any unusable agentrc
            raise ImportError(f'no usable agentrc at {root}: {e} ({remedy})') from None
        helper = hil_load.load('agentrc_agentrc_source', root / SOURCE)
        try:
            _LOADED = helper.frozen(source, root, SCRIPTS, commit), helper, remedy
        except helper.SourceError as e:
            raise ImportError(f'{e} ({remedy})') from None
    return _LOADED[0]


def helper():
    """agentrc's agentrc_source module, loaded with the identity."""
    identity()
    return _LOADED[1]


def _require(rel_path):
    """The path of the closure file `rel_path`, ImportError unless it is in SCRIPTS' closure and the whole closure
    still has the bytes the identity took (a script loads its own FILES by path, past this check)."""
    ident = identity()
    mod, remedy = _LOADED[1:]
    try:
        return mod.require(ident, rel_path)
    except mod.SourceError as e:
        raise ImportError(f'{e} ({remedy})') from None


def load(rel_path, module_name):
    """`rel_path` registered as `module_name` (pickle finds its classes by it): the module already loaded from that
    file under any name, else a new one; ImportError from _require."""
    path = _require(rel_path)
    return _hil_load(Path(identity()['root'])).load(module_name, path)


def path_of(rel_path) -> str:
    """`rel_path` for a script run as a process; ImportError from _require."""
    return str(_require(rel_path))


def load_file(rel_path):
    """load() under agentrc's own name for it, `agentrc_<stem>`: the instance its scripts load."""
    return load(rel_path, 'agentrc_' + Path(rel_path).stem)


def main(argv):
    try:
        if argv == ['fetch']:
            print(fetch())
        elif argv == ['check']:
            print(json.dumps(identity(), indent=1, sort_keys=True))
        else:
            print(__doc__, file=sys.stderr)
            return 2
    except ImportError as e:
        print(f'error: {e}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
