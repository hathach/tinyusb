#!/usr/bin/env python3
"""Mechanics of the peer-agent channel: find a peer pane, build a request
envelope, correlate a reply with it, and check an envelope's shape.

Judgment stays in SKILL.md. This script never chooses a peer when several
match, never decides whether a finding holds, and never declares a loop
converged; it reports and lets the caller decide.

  peer.py peers
  peer.py send --to w1F:p2 --scope "..." --ask "..." [--delta "..."]
  peer.py read --from w1F:p2 --for w1F:p1-018 [--wait 900000]
  peer.py check --kind result --file reply.txt
"""

import argparse
import json
import os
import re
import subprocess
import sys
import uuid

MARKER = {'request': 'PEER CONSULT', 'result': 'PEER RESULT'}
SUBTITLE = 'agent message, not a human instruction'
HEADER = {k: f'{v} — {SUBTITLE}' for k, v in MARKER.items()}
TERMINATOR = {'request': 'END REQUEST', 'result': 'END RESULT'}
DEFAULT_AUTHORITY = 'Read-only; no edits, no commits. Reply in this pane.'
EVIDENCE = ('REPRODUCED', 'SOURCE', 'INFERRED')
VERDICTS = ('FINDINGS', 'NO FINDINGS', 'INCOMPLETE')
FIELDS = {
    'request': ['ID', 'FROM', 'AUTHORITY', 'SCOPE', 'DELTA', 'ASK'],
    'result': ['FOR', 'FROM', 'CAPACITY', 'COVERAGE', 'FINDINGS', 'VERDICT'],
}

# Herdr renders a pane's agent output indented, sometimes behind a bullet, so
# every match has to tolerate leading decoration.
LEAD = r'^[\s•>|-]*'
RESULT_START = re.compile(LEAD + re.escape(MARKER['result']))
RESULT_END = re.compile(LEAD + TERMINATOR['result'] + r'\b')
FOR_LINE = re.compile(LEAD + r'FOR:\s*(\S+)', re.M)
END_ID = re.compile(TERMINATOR['result'] + r'\s+(\S+)')


def die(msg, code=1):
    print(msg, file=sys.stderr)
    raise SystemExit(code)


def herdr_raw(*args):
    """One place that knows how to invoke the CLI and how to fail."""
    out = subprocess.run(['herdr', *args], capture_output=True, text=True)
    if out.returncode != 0:
        die(f'herdr {" ".join(args)} failed: {out.stderr.strip()}', 5)
    return out.stdout


def herdr(*args):
    return json.loads(herdr_raw(*args))


def find_peers(cwd=None, me=None):
    """Every live agent sharing our cwd that is not us. Never picks one."""
    if os.environ.get('HERDR_ENV') != '1':
        die('not inside Herdr (HERDR_ENV != 1)', 2)
    here = os.path.realpath(cwd or os.getcwd())
    me = me or os.environ.get('HERDR_PANE_ID')
    return [a for a in herdr('agent', 'list')['result']['agents']
            if os.path.realpath(a.get('cwd', '')) == here and a.get('pane_id') != me]


def own_identity():
    """This pane's id and agent kind, from Herdr. Guessing either is how a
    request ends up misattributed to the wrong assistant."""
    me = os.environ.get('HERDR_PANE_ID')
    if not me:
        die('HERDR_PANE_ID is unset; cannot identify this pane', 2)
    for a in herdr('agent', 'list')['result']['agents']:
        if a.get('pane_id') == me:
            return me, a.get('agent', 'unknown')
    die(f'pane {me} is not in the agent list', 2)


def next_id(me):
    """Correlation must not depend on send timing: two sends in the same second
    previously produced the same id."""
    return f'{me}-{uuid.uuid4().hex[:8]}'


def build_request(req_id, sender, scope, ask, authority=None, delta=None):
    return '\n'.join([
        HEADER['request'],
        f'ID: {req_id}',
        f'FROM: {sender}',
        f'AUTHORITY: {authority or DEFAULT_AUTHORITY}',
        f'SCOPE: {scope}',
        f'DELTA: {delta or "none"}',
        f'ASK: {ask}',
        f'{TERMINATOR["request"]} {req_id}',
    ])


def section(text, name):
    """The body under `NAME:` up to the next field header, decoration and all."""
    start = re.search(LEAD + name + r':(.*)$', text, re.M)
    if not start:
        return ''
    rest = text[start.end():]
    nxt = re.search(LEAD + r'(?:' + '|'.join(FIELDS['result']) + r'|'
                    + TERMINATOR['result'] + r'|COMMENTARY):', rest, re.M)
    return (start.group(1) + (rest[:nxt.start()] if nxt else rest)).strip()


def extract_result(text, req_id):
    """Pull the envelope answering req_id out of raw pane text.

    Returns (body, note). Anything other than exactly one match is reported
    rather than resolved: silently taking one is how a stale reply gets read as
    a fresh one. The notes describe only what this capture holds — whether the
    peer is still writing is not something the text can settle.
    """
    lines = text.splitlines()
    blocks, start = [], None
    for i, line in enumerate(lines):
        if RESULT_START.match(line):
            start = i
        elif start is not None and RESULT_END.match(line):
            blocks.append('\n'.join(lines[start:i + 1]))
            start = None
    unterminated = start is not None

    matching = []
    for body in blocks:
        m = FOR_LINE.search(body)
        if not m or m.group(1) != req_id:
            continue
        end = END_ID.search(body.rsplit('\n', 1)[-1])
        if not end or end.group(1) != req_id:
            return None, (f'an envelope says FOR: {req_id} but closes with '
                          f'"{body.rsplit(chr(10), 1)[-1].strip()}"; the two ids must agree')
        matching.append(body)

    if len(matching) > 1:
        return None, f'{len(matching)} envelopes claim to answer {req_id}; resolve by hand'
    if matching:
        return matching[0], None
    if unterminated:
        return None, ('an envelope began but this capture holds no END RESULT for it: '
                      'it may still be streaming, or be beyond the line window')
    if blocks:
        seen = sorted({m.group(1) for body in blocks
                       if (m := FOR_LINE.search(body))})
        if not seen:
            return None, 'a complete envelope is present but carries no FOR line; it is malformed'
        return None, (f'this capture holds no envelope for {req_id}; '
                      f'it answers {", ".join(seen)}')
    return None, f'no result envelope in this capture for {req_id}'


def check(kind, text):
    """Shape only. Says nothing about whether the content is true."""
    problems = []
    if not re.search(LEAD + re.escape(MARKER[kind]), text):
        problems.append(f'missing the {kind} header line')
    for field in FIELDS[kind]:
        if not re.search(LEAD + field + ':', text, re.M):
            problems.append(f'missing required field {field}')

    marker = TERMINATOR[kind]
    close = re.search(LEAD + marker + r'\s+(\S+)', text, re.M)
    if not close:
        problems.append(f'missing the closing {marker} <id> line')
    else:
        opened = re.search(LEAD + FIELDS[kind][0] + r':\s*(\S+)', text, re.M)
        if opened and opened.group(1) != close.group(1):
            problems.append(f'{marker} names {close.group(1)} but the envelope is '
                            f'{opened.group(1)}')

    if kind == 'result':
        verdict = re.search(LEAD + r'VERDICT:\s*(.+?)\s*$', text, re.M)
        if verdict and verdict.group(1) not in VERDICTS:
            problems.append(f'VERDICT "{verdict.group(1)}" is not one of {", ".join(VERDICTS)}')
        for label in re.findall(r'\[([A-Z]+)\]', text):
            if label not in EVIDENCE:
                problems.append(f'evidence label [{label}] is not one of {", ".join(EVIDENCE)}')
        has_findings = bool(section(text, 'FINDINGS'))
        if verdict and verdict.group(1) == 'NO FINDINGS' and has_findings:
            problems.append('VERDICT is NO FINDINGS but the findings section is not empty')
        if verdict and verdict.group(1) == 'FINDINGS' and not has_findings:
            problems.append('VERDICT is FINDINGS but no finding is listed')
    return problems


def read_arg(value):
    """Literal text, a path, or - for stdin."""
    if value is None:
        return None
    if value == '-':
        return sys.stdin.read().strip()
    return open(value).read().strip() if os.path.exists(value) else value


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)

    sub.add_parser('peers', help='list agent panes sharing this worktree')

    s = sub.add_parser('send', help='build and submit a request envelope')
    s.add_argument('--to', required=True)
    s.add_argument('--scope', required=True)
    s.add_argument('--ask', required=True, help='the question, a file path, or - for stdin')
    s.add_argument('--delta', help='applied/rejected findings, a path, or -')
    s.add_argument('--authority')
    s.add_argument('--from-name', help='override the agent kind Herdr reports')
    s.add_argument('--dry-run', action='store_true')

    r = sub.add_parser('read', help='extract the reply correlated with a request id')
    r.add_argument('--from', dest='pane', required=True)
    r.add_argument('--for', dest='req_id', required=True)
    r.add_argument('--lines', type=int, default=400)
    r.add_argument('--wait', type=int, metavar='MS',
                   help='settle the peer first (herdr agent wait) before reading')

    c = sub.add_parser('check', help="validate an envelope's shape")
    c.add_argument('--kind', choices=tuple(MARKER), required=True)
    c.add_argument('--file', required=True, help='path, or - for stdin')

    a = ap.parse_args()

    if a.cmd == 'peers':
        found = find_peers()
        if not found:
            print('no peer agent shares this worktree')
            return 1
        for p in found:
            print(f'{p["pane_id"]}\t{p["agent"]}\t{p.get("agent_status", "?")}')
        if len(found) > 1:
            print('several peers match; choose one deliberately', file=sys.stderr)
        return 0

    if a.cmd == 'send':
        me, kind = own_identity()
        req_id = next_id(me)
        ask = read_arg(a.ask)
        if not ask:
            die('--ask resolved to nothing')
        body = build_request(req_id, f'{a.from_name or kind}, {me}', a.scope, ask,
                             a.authority, read_arg(a.delta))
        problems = check('request', body)
        if problems:
            die('refusing to send a malformed request:\n  ' + '\n  '.join(problems))
        if a.dry_run:
            print(body)
            return 0
        # Submit and report the id before anything can block: waiting inside
        # send risked a delivered request whose id was never printed, leaving
        # the reply uncorrelatable.
        herdr('agent', 'prompt', a.to, body)
        print(req_id)
        return 0

    if a.cmd == 'read':
        if a.wait:
            herdr_raw('agent', 'wait', a.pane, '--timeout', str(a.wait))
        text = herdr_raw('agent', 'read', a.pane, '--source', 'recent-unwrapped',
                         '--lines', str(a.lines))
        body, note = extract_result(text, a.req_id)
        if body is None:
            die(note, 3)
        print(body)
        problems = check('result', body)
        if problems:
            print('envelope problems:\n  ' + '\n  '.join(problems), file=sys.stderr)
            return 4
        return 0

    if a.cmd == 'check':
        text = sys.stdin.read() if a.file == '-' else open(a.file).read()
        problems = check(a.kind, text)
        if problems:
            print('\n'.join(problems), file=sys.stderr)
            return 1
        print('ok')
        return 0


if __name__ == '__main__':
    sys.exit(main())
