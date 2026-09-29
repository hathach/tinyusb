#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""The HIL report document: one owner for hil_report.json and hil_report.md.

The markdown IS a rendering of the sidecar -- every writer goes through render_report(), so
a table can never contain something the JSON does not. This module owns the whole life of
that document: the cell vocabulary, the one classifier both artifacts share, rendering, the
writers, and the fold to one machine-readable verdict per board.

Dual-mode by design: imported as `helper.hil_report` by hil_test.py, and run as a script by
the operator (the HIL contract's Reporting section, .claude/skills/hil/SKILL.md). A script run puts test/hil/helper on
sys.path rather than test/hil, so this module imports no sibling helper at all --
the width helpers below are defined locally for that reason. The same property makes it
the home of board_variants(), the roster reading the builders (check_build.py, the CI matrix)
import by path.
"""
import argparse
import json
import sys
import unicodedata
from pathlib import Path


def _w(s: str) -> int:
    """Terminal COLUMNS, not characters. Every status mark in REPORT_CELL is one Python
    character and TWO columns wide, so len() pads a cell holding one a column short and
    the pipes drift out of line with the header rule for the whole table.

    Local: this module is also run as a script, and
    under PYTHONSAFEPATH=1 a sibling import dies before argparse runs. hil_util carries the
    same pair for callers that can import it.
    """
    return sum(2 if unicodedata.east_asian_width(c) in 'WF' else 1 for c in s)


def _pad(s: str, width: int, center: bool = False) -> str:
    """str.ljust/center, measured in display columns. See _w."""
    room = max(0, width - _w(s))
    if not center:
        return s + ' ' * room
    left = room // 2
    return ' ' * left + s + ' ' * (room - left)


REPORT_MD = 'hil_report.md'
REPORT_JSON = 'hil_report.json'
# The status vocabulary, shared by the code that WRITES a cell (hil_test's test runners) and
# the code that reads one back (cell_state). One dict, so the human's table and the agent's
# verdict cannot drift apart.
REPORT_CELL = {'pass': '✅', 'fail': '❌', 'skip': '⚪'}
BOUNDARY_CELL = 'same-PID boundary'
LOCKED_CELL = 'board-locked'
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
# its value for a board hil_test.py --build did not test because check_build.py refused the
# build: nothing ran, so the board's earlier cells stand beside it
BUILD_REFUSED = f'{REPORT_CELL["fail"]} build refused'


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
    that may still hold them."""
    jpath = report_dir / REPORT_JSON
    if not jpath.is_file():
        return {'rows': [], 'scope': '', 'caveat': ''}, False
    try:
        raw = json.loads(jpath.read_text())
        if not isinstance(raw, dict):
            raise ValueError('sidecar is not an object')
    except (OSError, ValueError, TypeError):
        return {'rows': [], 'scope': '', 'caveat': ''}, False
    rows = []
    # isinstance, not `or []`: a sidecar with `rows: 1` iterates an int and raises outside
    # the parse handler above.
    for r in (raw.get('rows') if isinstance(raw.get('rows'), list) else []):
        if not isinstance(r, dict) or 'board' not in r:
            continue
        cells = r.get('cells')
        dur = r.get('duration')
        # VALUES as well as keys: render_matrix does REPORT_CELL.get(v, v), which raises
        # TypeError on an unhashable value, and cell_state does v.startswith. A non-str
        # cell is corrupt, and dropping it renders blank -- "not run" -- which is the
        # honest reading. Coercing it to str would make it classify as a PASS.
        rows.append({'board': str(r['board']),
                     'cells': {str(k): v for k, v in cells.items() if isinstance(v, str)}
                              if isinstance(cells, dict) else {},
                     'duration': dur if isinstance(dur, str) else None})
    text = lambda k: raw[k] if isinstance(raw.get(k), str) else ''
    return {'rows': rows, 'scope': text('scope'), 'caveat': text('caveat')}, True


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
    headers = [c.rsplit('/', 1)[-1] for c in columns] + ['duration']  # bare example names

    def cell(cells, col):
        v = cells.get(col)
        if v is None:
            return ''
        return REPORT_CELL.get(v, v)  # status symbol, or a metric string (e.g. speed) verbatim

    rows_vals = [(lbl, [cell(cells, c) for c in columns] + [dur or ''])
                 for lbl, cells, dur in rows_all]
    board_hdr = 'Board'
    # display_width, not len(): the ✅/❌/⚪ marks are one character and two columns
    board_w = max([_w(board_hdr)] + [_w(lbl) for lbl, _ in rows_vals])
    col_w = [max([_w(h)] + [_w(vals[i]) for _, vals in rows_vals])
             for i, h in enumerate(headers)]

    def line(label, values):
        padded = [_pad(label, board_w)] + [_pad(v, w, center=True)
                                           for v, w in zip(values, col_w)]
        return '| ' + ' | '.join(padded) + ' |'

    header = line(board_hdr, headers)
    sep = '| ' + '-' * board_w + ' | ' + ' | '.join(':' + '-' * (w - 2) + ':' for w in col_w) + ' |'
    body = [line(lbl, vals) for lbl, vals in rows_vals]

    # tally run cells (not-run cells are absent from the dicts). A cell is a bare status or
    # a metric string carrying its own icon ("❌ 29/30"), so classify by the leading icon --
    # through cell_state, the same call the per-board verdict makes.
    kinds = [cell_state(v) for _, cells, _ in rows_all for v in cells.values()]
    failed = kinds.count('fail')
    skipped = kinds.count('skip')
    passed = kinds.count('pass')
    summary = (f'**{REPORT_CELL["pass"]} {passed} passed · {REPORT_CELL["fail"]} {failed} failed · '
               f'{REPORT_CELL["skip"]} {skipped} skipped · blank not run**')

    return summary + '\n\n' + '\n'.join([header, sep] + body)


def render_report(doc: dict) -> str:
    """The markdown IS a rendering of the sidecar. Every writer goes through here, so a
    table can never contain something the JSON does not."""
    # .get throughout, not subscripts: a report dir can hold an older version's sidecar or
    # an uploaded one. Same reason summarize() below reads cells as `r.get('cells') or {}`.
    md = render_matrix([(r.get('board', '?'), r.get('cells') or {}, r.get('duration'))
                        for r in doc.get('rows') or [] if isinstance(r, dict)])
    if doc.get('scope'):
        # a scoped run's small table is otherwise indistinguishable from a full one, and
        # it replaces the previous full table in the sticky PR comment
        md = f'_Scoped run: {doc["scope"]}. Boards/tests not listed were not run._\n\n' + md
    # the top of the report is where hil/SKILL.md tells the agent to look
    if doc.get('caveat'):
        md = doc['caveat'] + '\n' + md
    return md


def write_report(report_dir: Path, doc: dict) -> None:
    """Write both artifacts from one document.

    RAISES on failure, deliberately: every caller is on a path whose own handler exists to
    report exactly this (write_timeout_report's warning, hil_test's fallback-of-the-
    fallback). Swallowing OSError here made both of those dead code, so an unwritable or
    root-owned report dir produced no artifact AND no message.

    Renders BEFORE writing anything: committing the JSON first and then raising in
    render_report left a sidecar saying "aborted" beside a markdown still reading as a
    clean green table -- the one invariant this module exists to hold."""
    md = render_report(doc) + '\n'
    report_dir.mkdir(parents=True, exist_ok=True)
    (report_dir / REPORT_JSON).write_text(json.dumps(doc, indent=2) + '\n')
    (report_dir / REPORT_MD).write_text(md, encoding='utf-8')


def _already_aborted(doc: dict) -> bool:
    """Whether THIS attempt already recorded how it ended."""
    return '**HIL run ab' in doc.get('caveat', '')


def mark_report_no_boards(report_dir: Path, msg: str, fresh: bool = True) -> None:
    """Record that the board filters intersected to nothing.

    `fresh` mirrors hil_test's own flag, because this runs BEFORE the fresh wipe: without
    it a fresh run whose filter emptied re-published the PREVIOUS run's green rows under
    this run's red job -- the stale-table failure it exists to prevent. An --accumulate run
    keeps them, since nothing this attempt did invalidates them."""
    try:
        doc, _ = _load(report_dir)
        if not fresh and _already_aborted(doc):
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
        write_report(report_dir, {'rows': prior['rows'], 'scope': prior['scope'],
                                  'caveat': f'**HIL run selected no boards.** {msg}\n'})
    except (OSError, ValueError, TypeError, AttributeError):
        pass          # loud on stdout already; the exit code is what the job reads


def accumulate_report(mret: list, report_dir: Path, fresh: bool, scope: str = '',
                      caveat: str = '') -> str:
    """Merge this run's results into json in report_dir, then (re)write
    the markdown matrix to md. `fresh` (a first run, no --accumulate)
    starts a new report; otherwise a re-run accumulates so boards/tests that
    already passed are preserved while re-run cells are updated. `scope` names the
    board filter, if any, so a scoped table is not mistaken for a full one.
    Returns the md.

    `mret` is hil_test.py's worker-result shape (name, err, fts, rows, ...), so this one
    function knows something about its caller that the rest of the module does not. Folding
    mret into rows could live in hil_test and only the merge here, but that would rewrite
    the subtle parts -- stale board-locked clearing, BOUNDARY_CELL dropping, duration=None
    preservation -- for a tidier seam. Data-shape coupling, not an import cycle."""
    # ONE canonical load: a sidecar reaching here may have been uploaded by hil_remote.py as
    # the merge base, so it is untrusted input. `caveat` does NOT carry forward: it records
    # how a RUN ENDED, and this attempt has not ended yet. Carrying it made a clean retry
    # publish "HIL run aborted" over a run where nothing was aborted.
    prior = {'rows': []}
    if not fresh:
        prior, _ = _load(report_dir)
    acc = {r['board']: [dict(r['cells']), r['duration']] for r in prior['rows']}

    # current cells override prior for boards/tests that ran; a filtered run reports
    # duration None, keeping the previous full-run value
    for name, _, _, rows, *_ in mret:
        unbuilt = any(cells.get(RUN_ABORTED_CELL) == BUILD_REFUSED for _, cells, _ in rows)
        if rows and not unbuilt and not any(LOCKED_CELL in cells for _, cells, _ in rows):
            # board ran for real: clear a stale lock-failure cell (its row is keyed by
            # board name; test rows may be variant names), and a stale wedge cell on every
            # row of the board -- a green re-run must not stay red for ever under last
            # time's wedge
            stale = acc.get(name)
            if stale is not None:
                stale[0].pop(LOCKED_CELL, None)
                stale[0].pop(WEDGED_CELL, None)
                # and the pool-timeout mark: write_timeout_report stamps it on a board that
                # never reported, and update() below MERGES, so without this a board that
                # passed clean on the retry kept a red cell for ever.
                stale[0].pop(POOL_TIMEOUT_CELL, None)
                stale[0].pop(RUN_ABORTED_CELL, None)
                if not stale[0]:
                    # variant-keyed boards never repopulate the board-name row, so drop it
                    # or it renders as a blank ghost row
                    del acc[name]
        for row_label, cells, dur in rows:
            row = acc.setdefault(row_label, [{}, None])
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
            row[0].update(cells)
            if dur is not None:
                row[1] = dur

    report_dir.mkdir(parents=True, exist_ok=True)
    doc = {'rows': [{'board': k, 'cells': c, 'duration': d} for k, (c, d) in acc.items()],
           'scope': scope, 'caveat': caveat}
    # through write_report, not hand-rolled: writing the JSON and only then rendering is
    # the ordering write_report exists to forbid -- a render failure left the sidecar ahead
    # of the markdown, which is the one invariant this module holds.
    write_report(report_dir, doc)
    return render_report(doc)


def _write_stuck_over_prior_md(report_dir: Path, doc: dict) -> None:
    """Sidecar unrecoverable: rebuild it from the stuck rows alone, but leave the
    markdown's existing table beneath the caveat rather than throwing real results away.

    The one place the md-is-a-rendering-of-the-json invariant is deliberately suspended,
    because there is no readable json left for it to be a rendering of."""
    try:
        prior = (report_dir / REPORT_MD).read_text(encoding='utf-8')
    except (OSError, ValueError):
        prior = ''
    # Say so explicitly: those rows exist only as rendered text, so no later --accumulate
    # can merge them back. Claiming the sidecar represents them would be false.
    note = ('_The table below is a previous attempt\'s rendered output. The sidecar could '
            'not be read, so those rows are NOT in it and will not survive another run._\n')
    head = doc['caveat'] + '\n' + note
    body = prior if prior.strip() else render_matrix(
        [(r['board'], r['cells'], r['duration']) for r in doc['rows']])
    report_dir.mkdir(parents=True, exist_ok=True)
    (report_dir / REPORT_JSON).write_text(json.dumps(doc, indent=2) + '\n')
    (report_dir / REPORT_MD).write_text(head + '\n' + body, encoding='utf-8')


def write_timeout_report(report_dir: Path, boards, caveat: str,
                         cell: str = POOL_TIMEOUT_CELL) -> None:
    """Leave a report behind when the pool guard expires or a worker raises.

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
                row['cells'][cell] = 'fail'
        out = {'rows': rows, 'scope': doc['scope'], 'caveat': caveat}
        if not readable and (report_dir / REPORT_MD).is_file():
            # `readable` covers ABSENT as well as torn: an absent sidecar beside an intact
            # markdown used to re-render from the stuck row alone and destroy real results.
            _write_stuck_over_prior_md(report_dir, out)
            return
        write_report(report_dir, out)
    except Exception as e:  # noqa: BLE001
        # Deliberately broad: the caller raises the abort right after, and an escape here
        # would replace that error with this one.
        print(f'warning: cannot write {REPORT_MD} to {report_dir}: {e}', flush=True)
        try:
            try:
                prior = (report_dir / REPORT_MD).read_text(encoding='utf-8')
            except (OSError, ValueError):
                prior = ''
            report_dir.mkdir(parents=True, exist_ok=True)
            (report_dir / REPORT_MD).write_text(
                caveat + (f'\n{prior}' if prior else ''), encoding='utf-8')
        except Exception as e2:  # noqa: BLE001
            print(f'warning: fallback {REPORT_MD} write failed too: {e2}', flush=True)


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


def _not_run(board: str, detail: str) -> dict:
    return {'board': board, 'ran': False, 'pass': False, 'locked': False, 'wedged': False,
            'detail': detail}


def summarize(cfg: dict, boards: list, report: dict) -> dict:
    # .get, not a subscript: this is the one reader an agent's verdict depends on, and a
    # row without 'board' used to kill the CLI with a traceback and no results at all --
    # the caller then sees no entry for any board.
    rows = {r['board']: r.get('cells') or {}
            for r in (report.get('rows') or [])
            if isinstance(r, dict) and 'board' in r}
    # the raw entries, not board_variants(): a malformed board must not crash the report of
    # the others, and its well-formed names still keep their rows from being stolen
    owner = {v['name']: b['name'] for b in cfg.get('boards', [])
             if isinstance(b.get('variant'), list)
             for v in b['variant'] if isinstance(v, dict) and isinstance(v.get('name'), str)}
    configured = {b['name'] for b in cfg.get('boards', [])}
    results = []
    for board in boards:
        # hil_test.py refuses a name outside the config, so no run produced a row for it;
        # without this guard variants_of() falls back to the name itself and a declared
        # variant of another board, or a stale row keyed by that name, reports it as ran.
        if board not in configured:
            results.append(_not_run(board, 'not a board in the config'))
            continue
        try:
            names = variants_of(cfg, board)
        except ValueError as err:
            # hil_test.py refuses the whole run on this; say why rather than die with a traceback
            results.append(_not_run(board, f'config error: {err}'))
            continue
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
        if not mine:
            results.append(_not_run(board, 'no report row for this board'))
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
        locked = not aborted and not board_wedged and any(LOCKED_CELL in cells for cells in mine.values())
        bad = []
        for vname, cells in sorted(mine.items()):
            for test, val in sorted(cells.items()):
                if test in (LOCKED_CELL, WEDGED_CELL):
                    continue
                if cell_state(val) == 'fail':
                    bad.append(f'{vname} {test}: {val}')
        ok = not bad and not locked and not board_wedged
        if locked:
            detail = 'held by another holder; not flashed'
        elif board_wedged:
            detail = 'wedged (usbtest reported the device wedged); ' + '; '.join(bad)
        elif bad:
            detail = '; '.join(bad)
        else:
            detail = f'{len(mine)} variant(s), {sum(len(c) for c in mine.values())} cell(s) ok'
        # a build refusal never flashed this attempt, whatever test history an --accumulate
        # re-run kept in the row
        results.append({'board': board, 'ran': not unbuilt, 'pass': ok,
                        'locked': locked, 'wedged': board_wedged, 'detail': detail})
    # `caveat` too: an aborted or no-boards run says so THERE, and this JSON is all
    # an agent gets -- leaving it in the sidecar puts it back where only a human looks.
    caveat = report.get('caveat', '')
    # the verdict of THIS snapshot: every row can pass on an aborted or no-boards run, so
    # the caveat gates it. --accumulate clears an earlier attempt's caveat by design, so the
    # verdict of a retry sequence is the caller's, from every attempt's result.
    return {'pass': bool(results) and all(r['pass'] for r in results) and not caveat,
            'results': results, 'caveat': caveat}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('config_file')
    ap.add_argument('-b', '--board', action='append', default=[],
                    help='boards to report on; default: every board in the config')
    ap.add_argument('--report-dir', default='.', help=f'where {REPORT_JSON} lives (default: cwd)')
    a = ap.parse_args()

    cfg = json.loads(Path(a.config_file).read_text())
    boards = a.board or [b['name'] for b in cfg.get('boards', [])]
    jpath = Path(a.report_dir) / REPORT_JSON
    if not jpath.is_file():
        print(f'error: {jpath} not found -- did hil_test.py run in this directory?',
              file=sys.stderr)
        return 1
    # through _load, like every writer: feeding raw JSON to summarize left the one reader an
    # agent's verdict depends on crashing on the malformed sidecars the writers tolerate.
    doc, _ = _load(Path(a.report_dir))
    json.dump(summarize(cfg, boards, doc), sys.stdout, indent=2)
    print()
    return 0

if __name__ == '__main__':
    sys.exit(main())
