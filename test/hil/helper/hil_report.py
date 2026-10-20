#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""The HIL report document: one owner for hil_report.json and hil_report.md.

The markdown IS a rendering of the sidecar -- every writer goes through render_report(), so
a table can never contain something the JSON does not. This module owns tinyusb's side of
that document: the cell vocabulary, the one classifier both artifacts share, rendering, the
writers, and the fold to one machine-readable verdict per board. Publication, the `_hil`
metadata beside the native fields and the per-attempt records under hil_report.attempts/ are
agentrc's hil_report_core (the `hil` skill's report document), loaded from lib/agentrc at
the pin on first use.

Dual-mode by design: imported as `helper.hil_report` by hil_test.py, and run as a script by
the operator (the HIL contract's Results section, .claude/skills/hil-contract/SKILL.md). A
script run puts test/hil/helper on sys.path rather than test/hil, so this module imports no
sibling helper at module level and loads its agentrc loader by path. The same property makes
it the home of board_variants(), the roster reading the builders (check_build.py, the CI
matrix) import by path without lib/agentrc.
"""
import argparse
import importlib.util
import json
import os
import socket
import sys
from pathlib import Path
from typing import NamedTuple
try:
    import fcntl
    msvcrt = None
except ImportError:   # Windows
    import msvcrt
    fcntl = None


REPORT_MD = 'hil_report.md'
REPORT_JSON = 'hil_report.json'
# the archived attempts the core keeps beside the pair (hil_report_core.paths)
REPORT_ATTEMPTS = 'hil_report.attempts'
SCHEMA_NAME, SCHEMA_VERSION = 'tinyusb-hil-report', 1
# The status vocabulary, shared by the code that WRITES a cell (hil_test's test runners) and
# the code that reads one back (cell_state). One dict, so the human's table and the agent's
# verdict cannot drift apart.
REPORT_CELL = {'pass': '✅', 'fail': '❌', 'skip': '⚪'}
BOUNDARY_CELL = 'same-PID boundary'
LOCKED_CELL = 'board-locked'
# the host's lock dir, file or record is unusable (hil_lock.LockUnavailable): refused before any
# flash like LOCKED_CELL, but rig-side, not contention a re-admission waits out. Its value is
# '❌ <the error>'.
LOCK_UNAVAILABLE_CELL = 'lock-unavailable'
# 'fail' when the board's usbtest battery reported the device wedged (a HUNG case its
# recovery could not clear, or a device it could no longer identify), so the rest of the
# board was skipped
WEDGED_CELL = 'board-wedged'
# A pseudo-test column, not a real one: write_timeout_report marks the boards that were
# still dispatched when the pool guard fired. accumulate_report clears it on a retry.
POOL_TIMEOUT_CELL = 'pool-timeout'
# The other way a board can fail to report: the pool did not expire, a worker RAISED. Same
# shape, different cause, and naming the cause is the whole point of the column -- a board
# marked pool-timeout by an abort that never timed out sends the reader after the guard.
RUN_ABORTED_CELL = 'run-aborted'
# a board whose openocd_esp session the provider left running: its lock went to a cleanup holder
# (hil_test.retain_board_lock), and its value is '❌ <receipt, holder and token>'
CLEANUP_PENDING_CELL = 'cleanup-pending'
# its value for a board hil_test.py --build did not test because check_build.py refused the
# build: nothing ran, so the board's earlier cells stand beside it
BUILD_REFUSED = f'{REPORT_CELL["fail"]} build refused'
# body field of an accumulating attempt's report, {'attempt_id', 'rows': {row: [cell keys]}}: the
# cells it wrote, which it is graded from -- its merged rows also hold earlier attempts'. A fresh
# attempt's cells are all its own, so its report has none.
ATTEMPT_CELLS = 'attempt_cells'


_core = None
_schema = None


def core():
    """agentrc's hil_report_core at the pin, loaded on first use: importing this module for
    board_variants() loads no provider."""
    global _core
    if _core is None:
        if __package__:
            from . import agentrc
        else:   # a script run: helper/ is on sys.path, not test/hil, so the package is not
            spec = importlib.util.spec_from_file_location('tinyusb_hil_agentrc', Path(__file__).with_name('agentrc.py'))
            agentrc = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(agentrc)
        _core = agentrc.load('skills/hil/scripts/hil_report_core.py', 'tinyusb_hil_report_core')
    return _core


def schema():
    """This document for the core: tinyusb's native {rows, scope, caveat} body, a report
    without `_hil` (every one before the core) read through the same coercion as _load."""
    global _schema
    if _schema is None:
        _schema = core().BodySchema(name=SCHEMA_NAME, version=SCHEMA_VERSION, stem=Path(REPORT_JSON).stem,
                                    evidence='firmware-validation', check=_check, empty=lambda b: not b['rows'],
                                    render=lambda b: render_report(b) + '\n', legacy=_coerce)
    return _schema


def _check(body: dict) -> None:
    if not isinstance(body.get('rows'), list) or not isinstance(body.get('scope'), str) or \
            not isinstance(body.get('caveat'), str):
        raise ValueError('rows a list, scope and caveat strings')


def _coerce(raw: dict) -> dict:
    """`raw`'s rows, scope and caveat in the canonical shape: see _load."""
    rows = []
    # isinstance, not `or []`: a sidecar with `rows: 1` iterates an int and raises outside
    # _load's parse handler.
    for r in (raw.get('rows') if isinstance(raw.get('rows'), list) else []):
        if not isinstance(r, dict) or 'board' not in r:
            continue
        cells = r.get('cells')
        dur = r.get('duration')
        # VALUES as well as keys: render_matrix does REPORT_CELL.get(v, v), which raises
        # TypeError on an unhashable value, and cell_state does v.startswith. A non-str
        # cell is corrupt, and dropping it renders blank -- "not run" -- which is the
        # honest reading. Coercing it to str would make it classify as a PASS.
        row = {'board': str(r['board']),
               'cells': {str(k): v for k, v in cells.items() if isinstance(v, str)}
                        if isinstance(cells, dict) else {},
               'duration': dur if isinstance(dur, str) else None}
        hist = r.get('history')
        hist = [{str(k): v for k, v in h.items() if isinstance(v, str)}
                for h in hist if isinstance(h, dict)] if isinstance(hist, list) else []
        if hist:
            row['history'] = hist
        # only a cell-less row can be retired: the flag must never hide a result
        if r.get('retired') is True and not row['cells']:
            row['retired'] = True
        rows.append(row)
    text = lambda k: raw[k] if isinstance(raw.get(k), str) else ''
    out = {'rows': rows, 'scope': text('scope'), 'caveat': text('caveat')}
    own = raw.get(ATTEMPT_CELLS)
    if isinstance(own, dict) and isinstance(own.get('attempt_id'), str) and isinstance(own.get('rows'), dict):
        out[ATTEMPT_CELLS] = {'attempt_id': own['attempt_id'],
                              'rows': {str(k): [c for c in v if isinstance(c, str)]
                                       for k, v in own['rows'].items() if isinstance(v, list)}}
    return out


def _load(report_dir: Path) -> tuple:
    """(doc, readable) for the sidecar, coerced to the canonical shape.

    hil_remote.py uploads a sidecar as the --accumulate merge base, so a non-conforming one is
    reachable from OUTSIDE the harness -- and every writer here runs on a path where a
    TypeError costs the whole report. Coerce once, at the boundary, instead of guarding
    each use: a null text field used to kill a fully successful run with a traceback and no
    artifact at all, and `cells: null` sent write_timeout_report down its fallback so a
    board that ate the whole pool guard was published as a pass.

    `readable` is False only when a sidecar EXISTS but could not be parsed, or is absent --
    both mean its rows are unrecoverable, which callers use to avoid destroying a markdown
    that may still hold them. A report of an unknown schema or version never gets here: the
    attempt that would merge it refuses to start (hil_report_core.start_attempt)."""
    empty = {'rows': [], 'scope': '', 'caveat': ''}
    # a withdrawal (a run that published nothing) resolves to the report it archived
    reading = core().read_current(report_dir, schema())
    if reading.outcome == 'withdrawn':   # it archived none: nothing to build on
        return empty, True
    try:
        raw = json.loads(reading.raw)
        # a withdrawal read_current could not resolve (malformed) is never a report's rows
        if not isinstance(raw, dict) or core().WITHDRAWN in raw:
            raise ValueError('sidecar is not a report object')
    except (TypeError, ValueError):   # absent, unparsable, a malformed withdrawal or a cycle
        return empty, False
    return _coerce(raw), True


def cell_state(v) -> str:
    """'pass' | 'fail' | 'skip' for one report cell.

    THE classifier -- the markdown tally and the per-board verdict both call this, so they
    cannot disagree. 'fail' or a fail-icon prefix is a failure, 'skip' or a skip-icon prefix
    is a skip, and EVERYTHING ELSE is a pass. That last arm is load-bearing: a passing test
    may return a plain metric string ('480.0 MBps') that lands in the cell unprefixed, while
    failures are guaranteed marked -- TestFail's docstring pins that its metric is
    icon-prefixed precisely so render and tally treat it as a failure. Classifying unknown
    shapes as fail here would publish a green table as a red verdict.

    isinstance-guarded: cells are usually str but a caller may hand over None or a number,
    and .startswith on those raises inside a report writer that must not raise."""
    if v == 'fail' or (isinstance(v, str) and v.startswith(REPORT_CELL['fail'])):
        return 'fail'
    if v == 'skip' or (isinstance(v, str) and v.startswith(REPORT_CELL['skip'])):
        return 'skip'
    return 'pass'


def render_matrix(rows_all: list) -> str:
    """Render rows (list of (row_label, {example: status}, duration)) as an aligned
    markdown matrix: columns = tests (bare names) centered, boards left-aligned,
    per-row duration as the trailing column."""
    seen = set()
    for _, cells, _ in rows_all:
        seen.update(cells)
    if not seen:
        return 'No tests were run.'

    # metric-bearing columns pinned first, the rest alphabetical: stable regardless of the
    # shuffled execution order
    pinned = ['usbtest', 'cdc_msc_throughput', 'msc_file_explorer', 'msc_file_explorer_freertos']

    def col_key(t):
        name = t.rsplit('/', 1)[-1]
        return (pinned.index(name) if name in pinned else len(pinned), name, t)

    columns = sorted(seen, key=col_key)
    duration = '\0duration'   # the trailing column's key: no test name holds a NUL
    # a None cell renders blank, as not run; a status shows as its symbol, a metric string
    # (e.g. speed) verbatim
    rows = [(lbl, {**{k: v for k, v in cells.items() if v is not None}, **({duration: dur} if dur else {})})
            for lbl, cells, dur in rows_all]
    table = core().matrix('Board', columns + [duration], rows, REPORT_CELL,
                          headers=[c.rsplit('/', 1)[-1] for c in columns] + ['duration'])

    # tally run cells (not-run cells are absent from the dicts). A cell is a bare status or
    # a metric string carrying its own icon ("❌ 29/30"), so classify by the leading icon --
    # through cell_state, the same call the per-board verdict makes.
    kinds = [cell_state(v) for _, cells, _ in rows_all for v in cells.values()]
    failed = kinds.count('fail')
    skipped = kinds.count('skip')
    passed = kinds.count('pass')
    summary = (f'**{REPORT_CELL["pass"]} {passed} passed · {REPORT_CELL["fail"]} {failed} failed · '
               f'{REPORT_CELL["skip"]} {skipped} skipped · blank not run**')

    return summary + '\n\n' + table


def render_report(doc: dict) -> str:
    """The markdown IS a rendering of the sidecar. Every writer goes through here, so a
    table can never contain something the JSON does not."""
    # .get throughout, not subscripts: a report dir can hold an older version's sidecar or
    # an uploaded one. Same reason summarize() below reads cells as `r.get('cells') or {}`.
    # a retired row (a variant board's emptied board-name row) is kept only for its history
    md = render_matrix([(r.get('board', '?'), r.get('cells') or {}, r.get('duration'))
                        for r in doc.get('rows') or []
                        if isinstance(r, dict) and not r.get('retired')])
    if doc.get('scope'):
        # a scoped run's small table is otherwise indistinguishable from a full one, and
        # it replaces the previous full table in the sticky PR comment
        md = f'_Scoped run: {doc["scope"]}. Boards/tests not listed were not run._\n\n' + md
    # the top of the report is where the HIL contract tells the agent to look
    if doc.get('caveat'):
        md = doc['caveat'] + '\n' + md
    return md


class _Run(NamedTuple):
    attempt: object   # hil_report_core.Attempt
    meta: dict        # its invocation, selection, host and roster, as make_meta takes them


_runs = {}   # resolved report dir -> the _Run begun there


def start_attempt(report_dir: Path, accumulate: bool, config_path, selection: list, invocation=None):
    """Begin this run's attempt on the report in report_dir, before anything replaces it
    (hil_test's fresh wipe, a no-boards notice): the core archives that report under
    REPORT_ATTEMPTS, an accumulating attempt links it as its predecessor, and every writer
    below then publishes as this attempt. Raises hil_report_core.ReportError, before
    anything changed, for a report of an unknown schema or version or a dir another writer
    holds."""
    attempt = core().start_attempt(report_dir, schema(), accumulate)
    _runs[Path(report_dir).resolve()] = _Run(attempt, {
        'invocation': [str(a) for a in (sys.argv if invocation is None else invocation)],
        'selection': list(selection), 'host': socket.gethostname(), 'roster': core().roster_identity(config_path)})
    return attempt


def _publish(report_dir: Path, doc: dict, termination: str, accumulate: bool, legacy: bool = False,
             ran: dict | None = None) -> None:
    """Publish doc as the attempt begun on report_dir. A writer called outside one is an
    attempt of its own only when it starts a fresh report: an accumulating write must join
    this run's attempt (start_attempt), whose roster and host link it to its predecessor,
    and none is inferred. `legacy` publishes the earlier markdown the attempt archived without
    a readable sidecar below the table, labelled: those rows are in no JSON, and no later run
    merges them. `ran` ({row: cell keys}) is what this write adds to an accumulating attempt's
    ATTEMPT_CELLS, which keeps doc's only when an earlier write of this attempt made it.

    The core validates the document and renders the markdown from it before replacing
    either file, then replaces the JSON and the markdown each atomically: a render failure
    changes nothing, so a sidecar saying "aborted" never sits beside a green table."""
    c = core()
    run = _runs.get(Path(report_dir).resolve())
    if run is None:
        if accumulate:
            raise c.ReportError('attempt', f'an accumulating write to {report_dir} needs the attempt start_attempt '
                                           'begins there')
        run = _Run(c.start_attempt(report_dir, schema(), False),
                   {'invocation': [str(a) for a in sys.argv], 'selection': [], 'host': socket.gethostname(),
                    'roster': {}})
    earlier = doc.get(ATTEMPT_CELLS)
    doc = {k: v for k, v in doc.items() if k != ATTEMPT_CELLS}
    if run.attempt.prior is not None:   # accumulating
        aid = run.attempt.attempt_id
        own = dict(earlier['rows']) if earlier and earlier['attempt_id'] == aid else {}
        for row, keys in (ran or {}).items():
            own[row] = sorted(set(own.get(row, ())) | set(keys))
        doc[ATTEMPT_CELLS] = {'attempt_id': aid, 'rows': own}
    try:
        meta = c.make_meta(schema(), run.attempt, termination=termination,
                           legacy_text=run.attempt.legacy_text if legacy else None, **run.meta)
        c.publish(report_dir, schema(), doc, meta)
    except Exception as e:
        # withdraw() recovers as this attempt: a one-shot one is in no _runs entry, and
        # without it the core refuses to touch the JSON it just published
        e.hil_attempt = run.attempt
        raise


def withdraw(report_dir: Path, err: Exception) -> None:
    """After this run's attempt failed to publish its report with `err`: make sure the report
    left in report_dir is never read as this run's (hil_report_core.recover_publish: the
    markdown regenerated, the report withdrawn, or the archived predecessor's files removed).
    Never raises; the run's own exit is its blocker."""
    run = _runs.get(Path(report_dir).resolve())
    attempt = getattr(err, 'hil_attempt', None) or (run.attempt if run else None)
    try:
        got = core().recover_publish(report_dir, schema(), attempt, err)
    except Exception as e:  # noqa: BLE001 - the caller's own error must still surface
        print(f'warning: cannot recover the report in {report_dir}: {e}', flush=True)
        return
    for problem in got.problems:
        print(f'warning: {problem}', flush=True)
    if got.outcome == 'withdrawn':
        print(f'warning: {REPORT_JSON} in {report_dir} withdrawn: this run published no report', flush=True)
    elif got.removed:
        print(f'warning: removed {", ".join(got.removed)} from {report_dir}: archived as attempt '
              f'{attempt.archived}, not this run\'s report', flush=True)


def write_report(report_dir: Path, doc: dict, termination: str = 'completed') -> None:
    """Write both artifacts from one document (_publish).

    RAISES on failure (hil_report_core.ReportError), deliberately: every caller is on a path
    whose own handler exists to report exactly this (write_timeout_report's warning,
    hil_test's fallback-of-the-fallback). Swallowing it made both of those dead code, so an
    unwritable or root-owned report dir produced no artifact AND no message."""
    _publish(report_dir, doc, termination, accumulate=False)


def _already_aborted(doc: dict) -> bool:
    """Whether THIS attempt already recorded how it ended."""
    return '**HIL run ab' in doc.get('caveat', '')


def mark_report_no_boards(report_dir: Path, msg: str, fresh: bool = True) -> None:
    """Record that the board filters intersected to nothing.

    `fresh` mirrors hil_test's own flag, because this runs BEFORE the fresh wipe: without
    it a fresh run whose filter emptied re-published the PREVIOUS run's green rows under
    this run's red job -- the stale-table failure it exists to prevent. An --accumulate run
    keeps them, since nothing this attempt did invalidates them."""
    _mark_report(report_dir, f'**HIL run selected no boards.** {msg}\n', 'no-boards', fresh)


def mark_report_aborted(report_dir: Path, msg: str, fresh: bool = True) -> None:
    """Record a run that stopped before any board was tested although boards were selected
    (a roster error, a failed usbtest registration): `aborted`, never no-boards, so the shared
    rules read every selected board as a blocker. `fresh` as mark_report_no_boards."""
    _mark_report(report_dir, f'**HIL run aborted: {msg}.**\n', 'aborted', fresh)


def _mark_report(report_dir: Path, caveat: str, termination: str, fresh: bool) -> None:
    try:
        doc, _ = _load(report_dir)
        if not fresh and termination == 'no-boards' and _already_aborted(doc):
            # SKILL.md gives the two notices OPPOSITE rules, and an abort outranks a
            # filter that matched nothing -- do not overwrite the record of a failed run.
            # Only while ACCUMULATING, though: this runs before the fresh wipe, so guarding
            # a fresh run would leave the previous attempt's rows AND its abort notice
            # published as this run's.
            return
        # A fresh run carries NOTHING from the prior sidecar -- rows and scope alike,
        # matching accumulate_report, which builds from an empty prior when fresh.
        # Resetting only rows republished a stale scope line under this run's notice,
        # from a leftover or uploaded sidecar.
        prior = {'rows': [], 'scope': ''} if fresh else doc
        _publish(report_dir, {**prior, 'caveat': caveat}, termination, accumulate=not fresh)
    except Exception as e:  # noqa: BLE001 - the exit code is what the job reads
        print(f'warning: cannot write {REPORT_MD} to {report_dir}: {e}', flush=True)
        withdraw(report_dir, e)


def _keep_earlier(history: list, before: dict, after: dict) -> None:
    """Append a row's earlier cells to its history when this attempt changed them."""
    if before and before != after:
        history.append(before)


def accumulate_report(mret: list, report_dir: Path, fresh: bool, scope: str = '',
                      caveat: str = '', termination: str = 'completed') -> str:
    """Merge this run's results into json in report_dir, then (re)write
    the markdown matrix to md. `fresh` (a first run, no --accumulate)
    starts a new report; otherwise a re-run accumulates so boards/tests that
    already passed are preserved while re-run cells are updated. `scope` names the
    board filter, if any, so a scoped table is not mistaken for a full one, and
    `termination` how the run ended (`_hil.termination`). Returns the md.

    `mret` is hil_test.py's worker-result shape (name, err, fts, rows, ...), so this one
    function knows something about its caller that the rest of the module does not. Folding
    mret into rows could live in hil_test and only the merge here, but that would rewrite
    the subtle parts -- stale board-locked clearing, BOUNDARY_CELL dropping, duration=None
    preservation -- for a tidier seam. Data-shape coupling, not an import cycle.

    A row this attempt changes keeps its earlier cells in `history`, oldest first, so a pass
    after a failure is not mistaken for a first-try pass; the markdown shows the latest."""
    # ONE canonical load: a sidecar reaching here may have been uploaded by hil_remote.py as
    # the merge base, so it is untrusted input. `caveat` does NOT carry forward: it records
    # how a RUN ENDED, and this attempt has not ended yet. Carrying it made a clean retry
    # publish "HIL run aborted" over a run where nothing was aborted.
    prior = {'rows': []}
    if not fresh:
        prior, _ = _load(report_dir)
    # [cells, duration, history, retired]
    acc = {r['board']: [dict(r['cells']), r['duration'], list(r.get('history', [])),
                        r.get('retired', False)]
           for r in prior['rows']}
    before = {k: dict(row[0]) for k, row in acc.items()}
    ran = {}

    # current cells override prior for boards/tests that ran; a filtered run reports
    # duration None, keeping the previous full-run value
    for name, _, _, rows, *_ in mret:
        unbuilt = any(cells.get(RUN_ABORTED_CELL) == BUILD_REFUSED for _, cells, _ in rows)
        if rows and not unbuilt and not any(LOCKED_CELL in cells or LOCK_UNAVAILABLE_CELL in cells
                                            for _, cells, _ in rows):
            # board ran for real: clear a stale lock-failure cell (its row is keyed by
            # board name; test rows may be variant names), and a stale wedge cell on every
            # row of the board -- a green re-run must not stay red for ever under last
            # time's wedge
            stale = acc.get(name)
            if stale is not None:
                stale[0].pop(LOCKED_CELL, None)
                stale[0].pop(LOCK_UNAVAILABLE_CELL, None)
                stale[0].pop(WEDGED_CELL, None)
                stale[0].pop(CLEANUP_PENDING_CELL, None)
                # and the pool-timeout mark: write_timeout_report stamps it on a board that
                # never reported, and update() below MERGES, so without this a board that
                # passed clean on the retry kept a red cell for ever.
                stale[0].pop(POOL_TIMEOUT_CELL, None)
                stale[0].pop(RUN_ABORTED_CELL, None)
                if not stale[0] and all(label != name for label, _, _ in rows):
                    # variant-keyed boards never repopulate the board-name row, so drop it
                    # or it renders as a blank ghost row -- unless it has earlier cells to
                    # keep: then it stays retired, in the sidecar only
                    if before.get(name) or stale[2]:
                        stale[3] = True
                    else:
                        del acc[name]
        for row_label, cells, dur in rows:
            ran.setdefault(row_label, set()).update(cells)
            row = acc.setdefault(row_label, [{}, None, [], False])
            row[3] = False
            if unbuilt:
                row[0].update(cells)
                continue
            # a row that ran is no longer pool-timed-out, whatever it is keyed by
            row[0].pop(POOL_TIMEOUT_CELL, None)
            row[0].pop(RUN_ABORTED_CELL, None)
            # nor wedged: this attempt's row says so or it does not
            if WEDGED_CELL not in cells:
                row[0].pop(WEDGED_CELL, None)
            # the boundary cell is only ever written on failure, so a re-run of this
            # variant that cleared the boundary must drop the previous attempt's ❌
            if BOUNDARY_CELL not in cells:
                row[0].pop(BOUNDARY_CELL, None)
            # one lock refusal per row: this attempt's replaces an earlier one of the other kind
            if LOCKED_CELL in cells or LOCK_UNAVAILABLE_CELL in cells:
                row[0].pop(LOCKED_CELL, None)
                row[0].pop(LOCK_UNAVAILABLE_CELL, None)
            row[0].update(cells)
            if dur is not None:
                row[1] = dur

    for k, (c, _, h, _) in acc.items():
        _keep_earlier(h, before.get(k), c)
    doc = {'rows': [{'board': k, 'cells': c, 'duration': d, **({'history': h} if h else {}),
                     **({'retired': True} if retired else {})}
                    for k, (c, d, h, retired) in acc.items()],
           'scope': scope, 'caveat': caveat,
           **({ATTEMPT_CELLS: prior[ATTEMPT_CELLS]} if ATTEMPT_CELLS in prior else {})}
    # through _publish, not hand-rolled: writing the JSON and only then rendering is the
    # ordering it exists to forbid -- a render failure left the sidecar ahead of the
    # markdown, which is the one invariant this module holds.
    _publish(report_dir, doc, termination, accumulate=not fresh, ran=ran)
    return render_report(doc)


def write_timeout_report(report_dir: Path, boards, caveat: str, cell: str = POOL_TIMEOUT_CELL) -> None:
    """Leave a report behind when the pool guard expires (POOL_TIMEOUT_CELL, termination
    'timeout') or a worker raises (RUN_ABORTED_CELL, 'aborted').

    Any prior attempt's rows are kept and each stuck board is marked with `cell` beside
    them. `caveat` is the notice, which an --accumulate retry does not carry."""
    try:
        # names INSIDE the try: a roster entry that is not a dict raises here, and outside
        # it that escaped and stranded the runner.
        names = [b.get('name', '?') if isinstance(b, dict) else '?' for b in boards]
        doc, readable = _load(report_dir)
        rows = doc['rows']
        by_board = {r['board']: r for r in rows}
        for name in names:
            row = by_board.get(name)
            if row is None:
                rows.append({'board': name, 'cells': {cell: 'fail'},
                             'duration': None})
            else:
                # _load guarantees `cells` is a dict, so a null-cells row from an uploaded
                # sidecar can no longer send this down the fallback and publish a board
                # that ate the whole pool guard as a pass.
                prior_cells = dict(row['cells'])
                row['cells'][cell] = 'fail'
                row.pop('retired', None)
                hist = row.get('history', [])
                _keep_earlier(hist, prior_cells, row['cells'])
                if hist:
                    row['history'] = hist
        out = {**doc, 'rows': rows, 'caveat': caveat}
        # `readable` covers ABSENT as well as torn: re-rendering from the stuck rows alone
        # destroyed the real results an intact markdown beside them still held, so that
        # markdown goes below the table as labelled legacy text
        _publish(report_dir, out, 'timeout' if cell == POOL_TIMEOUT_CELL else 'aborted', accumulate=True,
                 legacy=not readable, ran={name: [cell] for name in names})
    except Exception as e:  # noqa: BLE001
        # Deliberately broad: the caller raises the abort right after, and an escape here
        # would replace that error with this one.
        print(f'warning: cannot write {REPORT_MD} to {report_dir}: {e}', flush=True)
        withdraw(report_dir, e)


def board_variants(board: dict) -> list:
    """The builds a roster board runs as, each {'name' (its cmake-build-<name> dir and report
    row), 'defines' (cmake -D list), 'flags' (compiler flag tokens)}: its "variant" list, else
    the board as itself. A malformed entry raises ValueError."""
    name = board['name']
    variants = board.get('variant', [])
    if not isinstance(variants, list):
        raise ValueError(f'board {name} variant must be a list of variants: {variants!r}')
    variants = variants or [{'name': name}]
    out = []
    for i, v in enumerate(variants):
        if not isinstance(v, dict):
            raise ValueError(f'board {name} variant {i} must be an object with name, flags and defines: {v!r}')
        vname, defines, flags = v.get('name'), v.get('defines', []), v.get('flags', '')
        # an empty name would put the build in cmake-build-, which nothing looks in
        if not (isinstance(vname, str) and vname and isinstance(flags, str) and isinstance(defines, list)
                and all(isinstance(d, str) for d in defines)):
            raise ValueError(f'board {name} variant {vname!r} needs name a non-empty string, flags a '
                             f'string and defines a list of strings: {v}')
        out.append({'name': vname, 'defines': list(defines), 'flags': flags.split()})
    return out


def variants_of(cfg: dict, board: str) -> list:
    for b in cfg.get('boards', []):
        if b['name'] == board:
            return [v['name'] for v in board_variants(b)]
    return [board]


# cells that say why a board did not run, never a test's result
_MARKER_CELLS = (LOCKED_CELL, LOCK_UNAVAILABLE_CELL, WEDGED_CELL)


def _board_rows(report: dict) -> dict:
    # .get, not a subscript: this is the one reader an agent's verdict depends on, and a
    # row without 'board' used to kill the CLI with a traceback and no results at all --
    # the caller then sees no entry for any board.
    return {r['board']: r.get('cells') or {}
            for r in (report.get('rows') or [])
            if isinstance(r, dict) and 'board' in r and not r.get('retired')}


def _owners(cfg: dict) -> dict:
    # the raw entries, not board_variants(): a malformed board must not crash the report of
    # the others, and its well-formed names still keep their rows from being stolen
    return {v['name']: b['name'] for b in cfg.get('boards', [])
            if isinstance(b.get('variant'), list)
            for v in b['variant'] if isinstance(v, dict) and isinstance(v.get('name'), str)}


def _cells_of(cfg: dict, board: str, rows: dict, owner: dict) -> dict:
    """The report rows that belong to `board`, by row name. ValueError for a malformed entry."""
    names = variants_of(cfg, board)
    mine = {n: rows[n] for n in names if n in rows}
    # a variant name that is neither declared nor prefixed cannot be attributed; the
    # `<board>-` fallback only helps ad-hoc builds, it is not the primary path. It must
    # also never steal a row DECLARED by another board: a declared variant need not start
    # with its own board's name, so it may happen to start with this board's name plus '-'.
    mine.update({n: c for n, c in rows.items()
                 if n.startswith(f'{board}-') and n not in mine
                 and owner.get(n, board) == board})
    # the BOARD-name row too: hil_test writes lock contention and pool timeouts keyed
    # by board name, but variants_of returns only DECLARED variant names -- and
    # nanoch32v203 / ch32v307v_r1_1v0 declare none equal to their board name. Without
    # this those rows are invisible, so a lock held by concurrent CI is published as a
    # hardware FAIL that the caller never retries.
    if board in rows and board not in mine:
        mine[board] = rows[board]
    return mine


def _results(cfg: dict, boards: list, report: dict) -> list:
    """summarize()'s fold, per board: (its result, the report cells that belong to it, whether
    the report has no row for it at all)."""
    rows = _board_rows(report)
    history = {r['board']: r['history'] for r in (report.get('rows') or [])
               if isinstance(r, dict) and 'board' in r and isinstance(r.get('history'), list)
               and r['history']}
    owner = _owners(cfg)
    configured = {b['name'] for b in cfg.get('boards', [])}
    results = []
    for board in boards:
        # hil_test.py refuses a name outside the config, so no run produced a row for it;
        # without this guard variants_of() falls back to the name itself and a declared
        # variant of another board, or a stale row keyed by that name, reports it as ran.
        if board not in configured:
            results.append(({'board': board, 'ran': False, 'pass': False, 'locked': False,
                             'wedged': False, 'detail': 'not a board in the config'}, {}, False))
            continue
        try:
            mine = _cells_of(cfg, board, rows, owner)
        except ValueError as err:
            # hil_test.py refuses the whole run on this; say why rather than die with a traceback
            results.append(({'board': board, 'ran': False, 'pass': False, 'locked': False,
                             'wedged': False, 'detail': f'config error: {err}'}, {}, False))
            continue
        if not mine:
            results.append(({'board': board, 'ran': False, 'pass': False, 'locked': False,
                             'wedged': False, 'detail': 'no report row for this board'}, {}, True))
            continue
        # a wedge outranks lock contention: `locked` short-circuits `detail` below, so a
        # stale board-locked cell from an earlier attempt used to mask the pool-timeout
        # cell the retry added -- publishing a board that hung the rig as LOCKED, which
        # the caller then RE-RUNS, paying another pool guard on it. RUN_ABORTED_CELL
        # is written by the same _abort_report path for a board the guard never reached,
        # and must outrank it for the same reason.
        aborted = any(POOL_TIMEOUT_CELL in cells or RUN_ABORTED_CELL in cells
                      for cells in mine.values())
        # the per-row wedge latch -- a confirmed hang, or one the battery could not rule out
        # (sudo, an ambiguous or unreadable serial) -- distinct from `aborted` above, which
        # is this run's pool-level outcome and never proof about one board. It outranks a
        # lock cell the same way: a wedged board must never be published as LOCKED, which
        # the caller re-runs.
        board_wedged = any(cell_state(cells[WEDGED_CELL]) == 'fail'
                           for cells in mine.values() if WEDGED_CELL in cells)
        unbuilt = any(cells.get(RUN_ABORTED_CELL) == BUILD_REFUSED for cells in mine.values())
        # earlier attempts' cells, per row: a red one before a green row is a pass after a
        # failure, not a first-try pass
        earlier = {n: history[n] for n in sorted(set(mine) | {board}) if n in history}
        kept = {'history': earlier} if earlier else {}
        unusable = next((cells[LOCK_UNAVAILABLE_CELL] for cells in mine.values()
                         if LOCK_UNAVAILABLE_CELL in cells), None)
        if unusable is not None and not aborted and not board_wedged:
            results.append(({'board': board, 'ran': False, 'pass': False, 'locked': False,
                             'wedged': False, 'skipped': [], **kept, 'detail': unusable}, mine, False))
            continue
        locked = not aborted and not board_wedged and any(LOCKED_CELL in cells for cells in mine.values())
        bad = []
        skipped = []
        for vname, cells in sorted(mine.items()):
            for test, val in sorted(cells.items()):
                if test in _MARKER_CELLS:
                    continue
                state = cell_state(val)
                if state == 'fail':
                    bad.append(f'{vname} {test}: {val}')
                elif state == 'skip':
                    skipped.append(f'{vname} {test}')
        # a skipped test never ran: the caller reads the row as not-run, never as a pass
        ok = not bad and not skipped and not locked and not board_wedged
        if locked:
            detail = 'held by another holder; not flashed'
        elif board_wedged:
            detail = 'wedged (usbtest reported the device wedged); ' + '; '.join(bad)
        elif bad:
            detail = '; '.join(bad) + (f'; skipped: {", ".join(skipped)}' if skipped else '')
        else:
            cells_ok = sum(len(c) for c in mine.values()) - len(skipped)
            detail = f'{len(mine)} variant(s), {cells_ok} cell(s) ok'
            if skipped:
                detail += f', {len(skipped)} skipped: {", ".join(skipped)}'
        # a build refusal never flashed this attempt, whatever test history an --accumulate
        # re-run kept in the row
        results.append(({'board': board, 'ran': not unbuilt, 'pass': ok,
                         'locked': locked, 'wedged': board_wedged, 'skipped': skipped,
                         **kept, 'detail': detail}, mine, False))
    return results


def summarize(cfg: dict, boards: list, report: dict) -> dict:
    results = [r for r, _, _ in _results(cfg, boards, report)]
    # `caveat` too: an aborted or no-boards run says so THERE, and this JSON is all
    # an agent gets -- leaving it in the sidecar puts it back where only a human looks.
    caveat = report.get('caveat', '')
    # the verdict of THIS snapshot: every row can pass on an aborted or no-boards run, so
    # the caveat gates it. --accumulate clears an earlier attempt's caveat by design, so the
    # verdict of a retry sequence is the caller's, from every attempt's result.
    return {'pass': bool(results) and all(r['pass'] for r in results) and not caveat,
            'results': results, 'caveat': caveat}


def normalised_rows(cfg: dict, boards: list, report: dict) -> list:
    """`boards`' rows of `report` in the shared rules' shape ({board, verdict, detail, evidence},
    the `hil` skill's Verdicts), folded from summarize()'s: wedged, never heard back from
    (pool-timeout, run-aborted) or left to a cleanup holder is a blocker; locked, build
    refused, unbuilt or skipped is not-run. A configured board without a report row gets no
    row, which apply_run_rules makes a blocker. Unlike the snapshot, a board passes only with
    a passing test cell."""
    return [_normalised(r, cells) for r, cells, missing in _results(cfg, boards, report) if not missing]


def _normalised(r: dict, cells: dict) -> dict:
    tests = [v for c in cells.values() for t, v in c.items() if t not in _MARKER_CELLS]
    unreported = any(POOL_TIMEOUT_CELL in c or CLEANUP_PENDING_CELL in c
                     or c.get(RUN_ABORTED_CELL, BUILD_REFUSED) != BUILD_REFUSED for c in cells.values())
    failed = any(cell_state(v) == 'fail' for v in tests)
    exercised = any(cell_state(v) == 'pass' for v in tests)
    verdict = ('blocker' if r['wedged'] or unreported else 'pass' if r['pass'] and exercised
               else 'not-run' if r['locked'] or not r['ran'] or not failed else 'fail')
    return {'board': r['board'], 'verdict': verdict, 'detail': r['detail'],
            'evidence': [f'{n} {t}: {v}' for n, c in sorted(cells.items()) for t, v in sorted(c.items())]}


def _attempt(cfg: dict, roster: dict, reading) -> tuple:
    """(apply_run_rules over every board this attempt selected, its locked boards, the boards
    whose fail is final, the cells it is graded from per board ({board: {row: cells}}), whether
    it carries an ATTEMPT_CELLS record) for one attempt of a chain; (None, ...) for a report
    without this roster's run context (legacy, unreadable, another roster), whose metadata is
    never inferred from its rows. Every selected board, not only the queried ones: a locked or
    blocker peer is what makes the next attempt no eligible retry."""
    meta = reading.meta
    if reading.outcome not in core().READABLE or meta is None or meta['roster'].get('sha256') != roster['sha256']:
        return None, set(), frozenset(), {}, False
    boards = list(dict.fromkeys(meta['selection']))
    body = _coerce(reading.body)
    own = body.get(ATTEMPT_CELLS)
    if own is not None:
        # graded from its own cells: a row this attempt wrote nothing to is not-run, never the
        # pass or fail an earlier attempt left in it. A record of another attempt credits none.
        # _coerce built these rows, so filtering them in place leaves reading.body intact.
        own = own['rows'] if own['attempt_id'] == meta['attempt_id'] else {}
        for row in body['rows']:
            row['cells'] = {k: v for k, v in row['cells'].items() if k in own.get(row['board'], ())}
    results = _results(cfg, boards, body)
    run = core().apply_run_rules(schema(), boards, [_normalised(r, c) for r, c, missing in results if not missing],
                                 meta['termination'], body['caveat'].strip())
    cells = {r['board']: c for r, c, _ in results}
    # a usbtest battery's per-case verdicts stand (Results): its fail is never retried
    final = frozenset(r['board'] for r in run['rows'] if r['verdict'] == 'fail' and any(
        cell_state(c.get('device/usbtest', '')) == 'fail' for c in cells[r['board']].values()))
    return run, {r['board'] for r, _, _ in results if r['locked']}, final, cells, own is not None


def _unretried(before: dict, after: dict) -> set:
    """The boards with a failed cell in `before` ({board: {row: cells}}) that `after`, the next
    attempt's own cells, did not re-exercise: the same test on the same row, or for a same-PID
    boundary, which a re-run clears rather than rewrites, any test on that row."""
    out = set()
    for board, rows in before.items():
        for row, cells in rows.items():
            again = after.get(board, {}).get(row, {})
            for test, val in cells.items():
                if test in _MARKER_CELLS or cell_state(val) != 'fail':
                    continue
                if (not any(t not in _MARKER_CELLS for t in again) if test == BOUNDARY_CELL
                        else test not in again):
                    out.add(board)
    return out


def sequence(config_path, requested: list, report_dir: Path) -> dict:
    """The retry-sequence verdict of the requested boards over the report in report_dir and
    the attempts it links (hil_report_core.read_chain, sequence_verdict): each attempt of the
    roster at config_path folded by apply_run_rules over every board it selected, and an
    attempt an eligible retry of its predecessor when it accumulated onto it and that run had
    no locked board, of a board's fail only when its own cells re-ran that board's failed
    tests on the same rows. `current` is the last attempt's own run, its rows the requested boards'.
    summarize() stays the snapshot verdict."""
    c, s = core(), schema()
    cfg = json.loads(Path(config_path).read_text())
    roster = c.roster_identity(config_path)
    chain, locked_before, cells_before = [], None, {}
    for ctx in c.read_chain(report_dir, s):
        run, locked, final, cells, recorded = _attempt(cfg, roster, ctx.reading) if ctx.outcome in c.READABLE \
            else (None, set(), frozenset(), {}, False)
        before = chain[-1] if chain else None
        retry = (before is not None and before.run is not None and not locked_before
                 and ctx.previous_attempt == before.attempt_id)
        if retry and recorded:
            # a retry clears only the fails it re-exercised: any other stays final for it. A
            # predecessor's `final` is read only against the attempt after it (sequence_verdict).
            chain[-1] = before._replace(final=before.final | _unretried(cells_before, cells))
        chain.append(ctx._replace(run=run, retry=retry, final=final))
        locked_before, cells_before = locked, cells
    return c.sequence_verdict(s, requested, chain)


def _prepare(argv: list) -> int:
    ap = argparse.ArgumentParser(
        prog='hil_report.py prepare',
        description=f'Make {REPORT_MD} a rendering of {REPORT_JSON} before the pair is exposed (a copy-back, a CI '
                    f'upload): regenerated from a readable sidecar, else an explicit unavailable notice, never the '
                    f'stale table. Exit 0 once it is one or the other; 1 when it could not prepare the pair, '
                    f'{REPORT_MD} then an unavailable notice unless another writer holds the report.')
    ap.add_argument('--report-dir', default='.', help=f'where {REPORT_JSON} lives (default: cwd)')
    a = ap.parse_args(argv)
    report_dir = Path(a.report_dir)
    try:
        reading = core().prepare_pair(report_dir, schema())
    except Exception as e:  # noqa: BLE001 - the core failed to load or to prepare: the notice is ours
        if _core is not None and isinstance(e, _core.ReportError) and e.stage == 'writer':
            print(f'error: {e}; {REPORT_MD} left to that writer', file=sys.stderr)
            return 1
        print(f'error: cannot prepare {REPORT_MD}: {e}', file=sys.stderr)
        try:
            _write_notice(report_dir, f'{REPORT_MD} could not be prepared from {REPORT_JSON} ({e})')
        except OSError as e2:
            print(f'error: nor write its unavailable notice: {e2}', file=sys.stderr)
        return 1
    print(f'{REPORT_JSON}: {reading.outcome}' + (f' -- {reading.detail}' if reading.detail else ''))
    return 0


def _write_notice(report_dir: Path, why: str) -> None:
    """The unavailable notice, without the core (it may be what failed): replaced atomically,
    under the core's writer lock (`.<stem>.lock`, never waited for). Raises OSError, also
    when another writer holds the lock."""
    report_dir.mkdir(parents=True, exist_ok=True)
    fd = os.open(report_dir / f'.{Path(REPORT_JSON).stem}.lock', os.O_RDWR | os.O_CREAT, 0o666)
    try:
        if fcntl:
            fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        else:
            msvcrt.locking(fd, msvcrt.LK_NBLCK, 1)
        md, tmp = report_dir / REPORT_MD, report_dir / f'.{REPORT_MD}.{os.getpid()}.tmp'
        try:
            tmp.write_text(f'**HIL report unavailable: {" ".join(why.split())}.**\n', encoding='utf-8')
            os.replace(tmp, md)
        finally:
            tmp.unlink(missing_ok=True)
    finally:
        os.close(fd)


def _no_selection(argv: list) -> int:
    ap = argparse.ArgumentParser(
        prog='hil_report.py no-selection',
        description='Publish the report of a run its selection skipped: no board selected, nothing ran, the '
                    'reason at the top (`_hil.termination` no-boards). A fresh attempt: the report it replaces '
                    f'is archived under {REPORT_ATTEMPTS}/.')
    ap.add_argument('config_file', help='the roster the selection read')
    ap.add_argument('reason', help='the line the report leads with')
    ap.add_argument('--report-dir', default='.', help=f'where {REPORT_JSON} goes (default: cwd)')
    a = ap.parse_args(argv)
    report_dir = Path(a.report_dir)
    start_attempt(report_dir, False, a.config_file, [])
    try:
        write_report(report_dir, {'rows': [], 'scope': '', 'caveat': f'{a.reason}\n'}, 'no-boards')
    except Exception as e:
        withdraw(report_dir, e)   # never leave the report it replaced for the upload to prepare
        raise
    return 0


def main(argv: list | None = None) -> int:
    argv = sys.argv[1:] if argv is None else argv
    try:
        if argv[:1] == ['prepare']:
            return _prepare(argv[1:])
        if argv[:1] == ['no-selection']:
            return _no_selection(argv[1:])
        if argv[:1] == ['sequence']:
            return _sequence(argv[1:])
        return _summarize(argv)
    except (ImportError, OSError) as e:   # ImportError: lib/agentrc off its pin, the message names the fetch
        print(f'error: {e}', file=sys.stderr)
    except Exception as e:  # noqa: BLE001 - only the core's refusal, which names its stage
        if _core is None or not isinstance(e, _core.ReportError):
            raise
        print(f'error: {e}', file=sys.stderr)
    return 1


def _report_args(argv: list, **parser) -> tuple | None:
    """A reading command's arguments: (args, config, requested boards, the sidecar path), or
    None, with the error printed, when the sidecar is missing."""
    ap = argparse.ArgumentParser(**parser)
    ap.add_argument('config_file')
    ap.add_argument('-b', '--board', action='append', default=[],
                    help='boards to report on; default: every board in the config')
    ap.add_argument('--report-dir', default='.', help=f'where {REPORT_JSON} lives (default: cwd)')
    a = ap.parse_args(argv)
    cfg = json.loads(Path(a.config_file).read_text())
    jpath = Path(a.report_dir) / REPORT_JSON
    if not jpath.is_file():
        print(f'error: {jpath} not found -- did hil_test.py run in this directory?', file=sys.stderr)
        return None
    return a, cfg, a.board or [b['name'] for b in cfg.get('boards', [])], jpath


def _sequence(argv: list) -> int:
    got = _report_args(argv, prog='hil_report.py sequence',
                       description=f'Print the retry-sequence verdict over {REPORT_JSON} and the attempts it links '
                                   f'under {REPORT_ATTEMPTS}/ (agentrc hil_report_core.sequence_verdict): '
                                   f'{{current, sequence}}. Exit 0 once printed, a failing verdict included.')
    if got is None:
        return 1
    a, _, boards, _ = got
    json.dump(sequence(a.config_file, list(dict.fromkeys(boards)), Path(a.report_dir)), sys.stdout, indent=2)
    print()
    return 0


def _summarize(argv: list) -> int:
    got = _report_args(argv, description=__doc__.splitlines()[0])
    if got is None:
        return 1
    _, cfg, boards, jpath = got
    reading = core().read_document(jpath, schema())
    if reading.outcome == 'unsupported':
        # an unknown schema or version is never read as this one, nor as a legacy report
        print(f'error: {jpath}: {reading.detail}', file=sys.stderr)
        return 1
    # coerced like every writer's _load: feeding raw JSON to summarize left the one reader an
    # agent's verdict depends on crashing on the malformed sidecars the writers tolerate. A
    # corrupt document yields no rows, so every board reads as not run.
    doc = _coerce(reading.body) if reading.outcome in core().READABLE else _coerce({})
    json.dump(summarize(cfg, boards, doc), sys.stdout, indent=2)
    print()
    return 0


if __name__ == '__main__':
    sys.exit(main())
