#!/usr/bin/env python3
"""Run a pre-commit hook's steps in order, stopping at the first failure, with no shell: Windows has no `sh`.

    python3 tools/hook_steps.py 'python3 tools/family_json.py fix' '@test/unit-test ceedling test:all'

Each argument is one step, split like a POSIX shell word list. A leading `@DIR` runs it in DIR, relative to the repo
root. A step whose command is `python` or `python3` runs with this interpreter, so a `language: python` hook's steps
get its venv's packages. Any other command is found on PATH (a .bat or .cmd one too, on Windows).
"""
import os
import shlex
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def run(step):
    argv = shlex.split(step)
    cwd = ROOT
    if argv and argv[0].startswith('@'):
        cwd = os.path.join(ROOT, argv.pop(0)[1:])
    if not argv:
        sys.exit(f'hook_steps: empty step {step!r}')
    if argv[0] in ('python', 'python3'):
        argv[0] = sys.executable
    else:
        argv[0] = shutil.which(argv[0]) or argv[0]
    try:
        return subprocess.run(argv, cwd=cwd).returncode
    except OSError as e:
        print(f'hook_steps: {step}: {e}', file=sys.stderr)
        return 127


def main(steps):
    for step in steps:
        rc = run(step)
        if rc:
            return rc
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
