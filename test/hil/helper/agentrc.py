#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""agentrc's shared scripts for the HIL harness, from tools/agentrc at the commit
tools/get_deps.py pins (`python3 tools/get_deps.py tools/agentrc`), loaded by file path."""
import importlib.util
import sys
from pathlib import Path

TINYUSB_ROOT = Path(__file__).resolve().parents[3]
FETCH = 'python3 tools/get_deps.py tools/agentrc'


def load(rel_path, module_name):
    """tools/agentrc/`rel_path` registered as `module_name`; ImportError unless tools/agentrc
    is at the pin and holds the file."""
    spec = importlib.util.spec_from_file_location('tinyusb_get_deps', TINYUSB_ROOT / 'tools' / 'get_deps.py')
    deps = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(deps)
    # Proves the checked-out commit, not the file contents: a locally edited file at the pin
    # still loads, and the rig's staged tree has no git objects to check it against.
    pin, head = deps.deps_optional['tools/agentrc'][1], deps.dep_head('tools/agentrc')
    if head != pin:
        raise ImportError(f"tools/agentrc must be at {pin} (found {head or 'nothing'}): {FETCH}")
    path = TINYUSB_ROOT / 'tools' / 'agentrc' / rel_path
    if not path.is_file():
        raise ImportError(f'tools/agentrc at {pin} has no {rel_path}: {FETCH}')
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
