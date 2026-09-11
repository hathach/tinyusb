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
# The role is agentrc's user-level code-verifier, installed for both harnesses.
ADAPTER = Path.home() / '.codex' / 'agents' / 'code-verifier.toml'

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


def role(path=None):
    """Model, effort and role instructions come from the same adapter a
    standalone Codex session loads, so there is one source for both harnesses."""
    with open(path or ADAPTER, 'rb') as f:
        return tomllib.load(f)


def command(root, adapter, job, review, resume=None):
    model = ['-m', adapter['model'], '-c', f'model_reasoning_effort={adapter["model_reasoning_effort"]}']
    out = ['-o', str(job / 'result.json'), '--json', '-']
    if resume:
        return ['codex', 'exec', 'resume', resume, '-c', 'sandbox_mode=read-only', *model, *out]
    if review:
        # no --sandbox flag here; the -c override is what keeps it read-only
        return ['codex', 'exec', 'review', '-c', 'sandbox_mode=read-only', *model, *out]
    return ['codex', 'exec', '-C', str(root), '--sandbox', 'read-only',
            '--output-schema', str(job / 'schema.json'), *model, *out]


def recover(root, adapter, job, thread, data, schema):
    """The reviewer sometimes answers in its own prose format despite the
    contract; one resume turn on the same thread converts that answer. The
    review ran as a sub-thread, so the instructions must be restated."""
    (job / 'recover.txt').write_text(
        'Convert your review above into its structured form, following these instructions:\n' +
        data['prompt'].rstrip() + '\n\n' + OUTPUT_CONTRACT + schema + '\n')
    with open(job / 'recover.txt') as stdin, open(job / 'events.jsonl', 'a') as events, \
            open(job / 'stderr.txt', 'a') as stderr:
        subprocess.run(command(root, adapter, job, True, resume=thread), cwd=root,
                       stdin=stdin, stdout=events, stderr=stderr, timeout=TIMEOUT)


def envelope(job, thread, status, result=None, error=None):
    """What the bridge relays: status 'ok' carries the result; 'timeout' means
    Codex ran and never finished, which a caller must not mistake for Codex
    being unavailable. thread is null when the run died before thread.started."""
    return {'job': str(job), 'thread': thread, 'status': status, 'result': result, 'error': error}


def timed_out(job):
    return envelope(job, thread_id(job / 'events.jsonl'), 'timeout',
                    error=f'codex timed out after {TIMEOUT // 60} min; partial output is not a result')


def thread_id(events):
    with events.open() as f:
        for line in f:
            try:
                event = json.loads(line)
            except ValueError:
                continue
            if event.get('type') == 'thread.started':
                return event.get('thread_id')
    return None


def load_result(job):
    """(result, error text) from result.json."""
    try:
        return json.loads((job / 'result.json').read_text()), None
    except (OSError, ValueError) as e:
        return None, str(e)


def run(data, root, stamp=None, adapter=None):
    adapter = adapter or role()
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
        return timed_out(job)
    if proc.returncode != 0:
        tail = [l for l in (job / 'stderr.txt').read_text().splitlines() if 'rmcp' not in l][-20:]
        raise RuntimeError(f'codex exited {proc.returncode} ({job}):\n' + '\n'.join(tail))
    result, error = load_result(job)
    thread = thread_id(job / 'events.jsonl')
    turns = ''
    if result is None and review and thread:
        try:
            recover(root, adapter, job, thread, data, schema)
        except subprocess.TimeoutExpired:
            return timed_out(job)
        result, error = load_result(job)
        turns = ', even after one conversion turn'
    if result is None:
        raise RuntimeError(f'codex produced no JSON result{turns} ({job}): {error}')
    return envelope(job, thread, 'ok', result=result)


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
