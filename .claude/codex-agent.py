#!/usr/bin/env python3
"""Run one read-only TinyUSB code-verifier job on Codex and print its result.

stdin: {"prompt": str, "schema": object, "review": bool?}
stdout: {"job": <dir>, "thread": <codex thread id>, "result": <the JSON Codex produced>}

Review mode (`codex exec review`) ignores --output-schema, so the contract rides
in the prompt; when the reviewer still answers in prose, one resume turn on the
same thread converts it.

Each job keeps prompt, schema, the --json event log, stderr and the result under
/tmp/tinyusb-codex/<job>/: tail the log to watch, kill the codex pid to stop,
`codex exec resume <thread>` to ask a follow-up.
"""

import datetime
import json
import os
import subprocess
import sys
import tomllib
from pathlib import Path

TIMEOUT = 1800  # a full-diff review at xhigh effort routinely passes 10 min
JOBS = Path('/tmp/tinyusb-codex')

# `codex exec review` ignores --output-schema, so the contract rides in the prompt.
OUTPUT_CONTRACT = ('Your final message must be exactly one JSON object valid '
                   'against this JSON schema, with no prose and no code fence:\n')


def repo_root():
    """From this file's location, not the caller's cwd: the bridge agent may
    invoke us from anywhere."""
    return Path(__file__).resolve().parents[1]


def read_input(stream):
    data = json.load(stream)
    if not isinstance(data, dict) or not isinstance(data.get('prompt'), str) or \
            not data['prompt'].strip() or not isinstance(data.get('schema'), dict):
        raise ValueError('input must be {"prompt": str, "schema": object, "review"?: bool}')
    return data


def role(root):
    """Model, effort and role instructions come from the same adapter a
    standalone Codex session loads, so there is one source for both harnesses."""
    with open(root / '.codex' / 'agents' / 'code-verifier.toml', 'rb') as f:
        return tomllib.load(f)


def command(root, adapter, job, review):
    model = ['-m', adapter['model'], '-c', f'model_reasoning_effort={adapter["model_reasoning_effort"]}']
    out = ['-o', str(job / 'result.json'), '--json', '-']
    if review:
        # no --sandbox flag here; the -c override is what keeps it read-only
        return ['codex', 'exec', 'review', '-c', 'sandbox_mode=read-only', *model, *out]
    return ['codex', 'exec', '-C', str(root), '--sandbox', 'read-only',
            '--output-schema', str(job / 'schema.json'), *model, *out]


def recover(root, adapter, job, thread, data):
    """The reviewer sometimes answers in its own prose format despite the
    contract; one resume turn on the same thread converts that answer. The
    review ran as a sub-thread, so the instructions must be restated."""
    schema = (job / 'schema.json').read_text()
    (job / 'recover.txt').write_text(
        'Convert your review above into its structured form, following these instructions:\n' +
        data['prompt'].rstrip() + '\n\n' + OUTPUT_CONTRACT + schema + '\n')
    cmd = ['codex', 'exec', 'resume', thread, '-c', 'sandbox_mode=read-only',
           '-m', adapter['model'], '-c', f'model_reasoning_effort={adapter["model_reasoning_effort"]}',
           '-o', str(job / 'result.json'), '--json', '-']
    with open(job / 'recover.txt') as stdin, open(job / 'events.jsonl', 'a') as events, \
            open(job / 'stderr.txt', 'a') as stderr:
        subprocess.run(cmd, cwd=root, stdin=stdin, stdout=events, stderr=stderr, timeout=TIMEOUT)


def thread_id(events):
    for line in events.read_text().splitlines():
        try:
            event = json.loads(line)
        except ValueError:
            continue
        if event.get('type') == 'thread.started':
            return event.get('thread_id')
    return None


def run(data, root, stamp=None):
    adapter = role(root)
    review = bool(data.get('review'))
    stamp = stamp or datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    job = JOBS / f'{stamp}-{os.getpid()}'
    job.mkdir(parents=True)
    schema = json.dumps(data['schema'])
    (job / 'schema.json').write_text(schema)
    prompt = adapter['developer_instructions'].rstrip() + '\n\n' + data['prompt'].rstrip() + '\n'
    if review:
        prompt += '\n' + OUTPUT_CONTRACT + schema + '\n'
    (job / 'prompt.txt').write_text(prompt)

    with open(job / 'prompt.txt') as stdin, open(job / 'events.jsonl', 'w') as events, \
            open(job / 'stderr.txt', 'w') as stderr:
        try:
            proc = subprocess.run(command(root, adapter, job, review), cwd=root,
                                  stdin=stdin, stdout=events, stderr=stderr, timeout=TIMEOUT)
        except subprocess.TimeoutExpired:
            proc = None
    if proc is None:
        # Codex ran and never finished: an envelope without a result, so the
        # caller can tell this from Codex being unavailable and not fall back.
        return {'job': str(job), 'thread': thread_id(job / 'events.jsonl'), 'result': None,
                'error': f'codex timed out after {TIMEOUT // 60} min; partial output is not a result'}
    if proc.returncode != 0:
        tail = [l for l in (job / 'stderr.txt').read_text().splitlines() if 'rmcp' not in l][-20:]
        raise RuntimeError(f'codex exited {proc.returncode} ({job}):\n' + '\n'.join(tail))
    thread = thread_id(job / 'events.jsonl')
    try:
        result = json.loads((job / 'result.json').read_text())
    except (OSError, ValueError) as e:
        if not (review and thread):
            raise RuntimeError(f'codex produced no JSON result ({job}): {e}') from None
        recover(root, adapter, job, thread, data)
        try:
            result = json.loads((job / 'result.json').read_text())
        except (OSError, ValueError) as e2:
            raise RuntimeError(f'codex produced no JSON result, even after one '
                               f'conversion turn ({job}): {e2}') from None
    return {'job': str(job), 'thread': thread, 'result': result}


def main():
    data = read_input(sys.stdin)
    sys.stdout.write(json.dumps(run(data, repo_root())))
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except Exception as e:  # any failure prints nothing to stdout
        print(f'{type(e).__name__}: {e}', file=sys.stderr)
        sys.exit(1)
