#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""agentrc's shared scripts for the HIL harness, from lib/agentrc at the commit
tools/get_deps.py pins (`python3 tools/get_deps.py lib/agentrc`), loaded by file path."""
import importlib.util
import sys
from pathlib import Path

TINYUSB_ROOT = Path(__file__).resolve().parents[3]
DEP = 'lib/agentrc'
FETCH = f'python3 tools/get_deps.py {DEP}'

_spec = importlib.util.spec_from_file_location('tinyusb_get_deps', TINYUSB_ROOT / 'tools' / 'get_deps.py')
_get_deps = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_get_deps)
PIN = _get_deps.deps_optional[DEP][1]


def load(rel_path, module_name, requires=()):
    """DEP/`rel_path` registered as `module_name`; ImportError unless DEP is at PIN and holds
    the file and every `requires` file (ones it loads or launches by path)."""
    # Proves the checked-out commit, not the file contents: a locally edited file at the pin
    # still loads, and the rig's staged tree has no git objects to check it against.
    head = _get_deps.dep_head(DEP)
    if head != PIN:
        raise ImportError(f"{DEP} must be at {PIN} (found {head or 'nothing'}): {FETCH}")
    for rel in (rel_path, *requires):
        if not (TINYUSB_ROOT / DEP / rel).is_file():
            raise ImportError(f'{DEP} at {PIN} has no {rel}: {FETCH}')
    path = TINYUSB_ROOT / DEP / rel_path
    spec = importlib.util.spec_from_file_location(module_name, path)
    module = importlib.util.module_from_spec(spec)
    # Registered so pickle finds its classes by module name (RttError across the fork Pool).
    sys.modules[module_name] = module
    try:
        spec.loader.exec_module(module)
    except BaseException:
        del sys.modules[module_name]   # a half-run module must not be served from the cache
        raise
    return module
