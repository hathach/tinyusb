#!/usr/bin/env python3
"""Run one read-only TinyUSB agent role on Codex and print its JSON result.

The role allowlist here is the enforcement boundary: the Markdown bridge that
invokes this script cannot be trusted to honour a rule stated only in prose.
"""

import argparse
import json
import subprocess
import sys
import tempfile
import tomllib
from pathlib import Path

# A role belongs here only if it can do its whole job from the local checkout.
# `codex exec --sandbox read-only` has no network, so a role built on `gh` does
# not fail there - it answers from an empty view, which is worse.
READ_ONLY_ROLES = frozenset({'code-verifier'})

TIMEOUT = '1800s'  # a full-diff review at xhigh effort routinely passes 10 min


def failure_detail(returncode, stderr):
    if returncode == 124:  # `timeout` killed the child
        return (f'codex exec timed out after {TIMEOUT}; its partial output is not a '
                'result. Narrow the prompt or raise TIMEOUT.')
    return stderr


def repo_root():
    """From this file's location, not the caller's cwd: the bridge agent may
    invoke us from anywhere."""
    return Path(__file__).resolve().parents[1]


def resolve_adapter(root, role):
    if role not in READ_ONLY_ROLES:
        raise ValueError(
            f'role {role!r} is not in the read-only set '
            f'({", ".join(sorted(READ_ONLY_ROLES))}); '
            'write-capable roles are not routable to Codex')
    with open(Path(root) / '.codex' / 'agents' / f'{role}.toml', 'rb') as f:
        return tomllib.load(f)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--role', required=True)
    parser.add_argument('--prompt-file', required=True)
    parser.add_argument('--schema-file', required=True)
    args = parser.parse_args()

    root = repo_root()
    try:
        adapter = resolve_adapter(root, args.role)
    except ValueError as e:
        print(e, file=sys.stderr)
        return 2

    prompt = Path(args.prompt_file).read_text()
    json.loads(Path(args.schema_file).read_text())  # reject a bad schema before spending a run

    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        composed = tmp / 'prompt.txt'
        composed.write_text(
            adapter['developer_instructions'].rstrip() + '\n\n' + prompt)
        result_path = tmp / 'result.json'
        with open(composed, 'rb') as stdin:
            run = subprocess.run([
                'timeout', TIMEOUT, 'codex', 'exec',
                '-C', str(root),
                '-m', adapter['model'],
                '-c', f'model_reasoning_effort={adapter["model_reasoning_effort"]}',
                '--sandbox', 'read-only',
                '--output-schema', args.schema_file,
                '--output-last-message', str(result_path),
                '-',
            ], stdin=stdin, capture_output=True, text=True)
        if run.returncode != 0:
            print(failure_detail(run.returncode, run.stderr), file=sys.stderr)
            return 1
        if not result_path.is_file():
            print('codex produced no result file', file=sys.stderr)
            return 1
        body = result_path.read_text()

    json.loads(body)  # never hand the caller something that is not JSON
    sys.stdout.write(body)
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except Exception as e:  # any failure prints nothing to stdout
        print(f'{type(e).__name__}: {e}', file=sys.stderr)
        sys.exit(1)
