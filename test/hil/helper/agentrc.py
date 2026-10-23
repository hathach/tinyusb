#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""agentrc's shared scripts for the HIL harness, from lib/agentrc at the commit
tools/get_deps.py pins (`python3 tools/get_deps.py lib/agentrc`), loaded by file path through
agentrc's hil_load: one module per file per process, whichever name loads it first."""
import ast
import importlib.util
import sys
from pathlib import Path

TINYUSB_ROOT = Path(__file__).resolve().parents[3]
DEP = 'lib/agentrc'
FETCH = f'python3 tools/get_deps.py {DEP}'
HIL_LOAD = 'skills/hil/scripts/hil_load.py'

_spec = importlib.util.spec_from_file_location('tinyusb_get_deps', TINYUSB_ROOT / 'tools' / 'get_deps.py')
_get_deps = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_get_deps)
PIN = _get_deps.deps_optional[DEP][1]


def files_of(rel_path) -> tuple:
    """The FILES literal DEP/`rel_path` exports, the files it loads or launches by path
    (transitively), read without running it; () when it exports none."""
    for node in ast.parse((TINYUSB_ROOT / DEP / rel_path).read_text()).body:
        if isinstance(node, ast.Assign) and [ast.unparse(t) for t in node.targets] == ['FILES']:
            return tuple(ast.literal_eval(node.value))
    return ()


def _require(rel_path):
    """ImportError naming the fetch unless DEP is at PIN and holds hil_load, the file and its FILES."""
    # Proves the checked-out commit, not the file contents: a locally edited file at the pin
    # still loads, and the rig's staged tree has no git objects to check it against.
    head = _get_deps.dep_head(DEP)
    if head != PIN:
        raise ImportError(f"{DEP} must be at {PIN} (found {head or 'nothing'}): {FETCH}")
    for rel in (HIL_LOAD, rel_path):
        if not (TINYUSB_ROOT / DEP / rel).is_file():
            raise ImportError(f'{DEP} at {PIN} has no {rel}: {FETCH}')
    for rel in files_of(rel_path):
        if not (TINYUSB_ROOT / DEP / rel).is_file():
            raise ImportError(f'{DEP} at {PIN} has no {rel}, which {rel_path} needs: {FETCH}')


def _hil_load():
    """agentrc's hil_load, registered as `agentrc_hil_load` the way its own scripts bootstrap it."""
    spec = importlib.util.spec_from_file_location('agentrc_hil_load', TINYUSB_ROOT / DEP / HIL_LOAD)
    if spec.name not in sys.modules:
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        sys.modules.setdefault(spec.name, mod)
    return sys.modules[spec.name].load(spec.name, spec.origin)


def load(rel_path, module_name):
    """DEP/`rel_path` registered as `module_name` (pickle finds its classes by it): the module
    already loaded from that file under any name, else a new one; ImportError from _require."""
    _require(rel_path)
    return _hil_load().load(module_name, TINYUSB_ROOT / DEP / rel_path)


def load_file(rel_path):
    """load() under agentrc's own name for it, `agentrc_<stem>`: the instance its scripts load."""
    _require(rel_path)
    return _hil_load().load_file(rel_path)
