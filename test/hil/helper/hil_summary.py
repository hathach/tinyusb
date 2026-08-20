#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Fold hil_report.json into one machine-readable verdict per BOARD.

A workflow driving hil_test.py through an operator agent has no filesystem access, so the
agent has to carry the results across. It must carry them, not retype them: the previous
design asked the agent to transcribe the markdown table, and every defect found in four
review rounds came from re-parsing that prose -- variant row names vs board names,
`board locked` vs `board-locked`, folding several variant rows into one verdict, rows that
matched no board. All of it is a join, and the join belongs here, where the roster is.

Report rows are named per VARIANT (hil_test.py builds them from `vname`), and a variant name
is not required to start with the board name -- nanoch32v203 produces only `-fsdev`/`-usbfs`,
ch32v307v_r1_1v0 only `-usbhs`/`-usbfs`. The config is what maps them back.

Emits, on stdout:
  {"results": [{"board", "ran", "pass", "locked", "detail"}...], "banner": str}

`locked` is a field, not a prefix to grep for. `ran` false means the board produced no row at
all, which is not the same as failing.

Usage: hil_summary.py <config.json> [-b BOARD]... [--report-dir DIR]
"""
import argparse
import json
import sys
from pathlib import Path

FAIL_ICON, SKIP_ICON = '❌', '⚪'   # a pass needs no icon: unmarked = pass
LOCKED_CELL = 'board-locked'


def cell_state(v: str) -> str:
    """'pass' | 'fail' | 'skip' -- the EXACT classifier hil_test.py's own tally uses
    (cell_kind in render_matrix): 'fail' or a ❌ prefix is a failure, 'skip' or a ⚪
    prefix is a skip, and EVERYTHING ELSE is a pass. That last arm is load-bearing: a
    passing test may return a plain metric string ('480.0 MBps') that lands in the cell
    unprefixed, while failures are guaranteed marked -- TestFail's docstring pins that its
    metric is icon-prefixed precisely so render/tally treat it as a failure. Classifying
    unknown shapes as fail here would publish a green table as a red verdict."""
    if v == 'fail' or v.startswith(FAIL_ICON):
        return 'fail'
    if v == 'skip' or v.startswith(SKIP_ICON):
        return 'skip'
    return 'pass'


def variants_of(cfg: dict, board: str) -> list:
    for b in cfg.get('boards', []):
        if b['name'] == board:
            return [v['name'] for v in (b.get('variant') or [])] or [board]
    return [board]


def summarize(cfg: dict, boards: list, report: dict) -> dict:
    rows = {r['board']: r.get('cells') or {} for r in report.get('rows', [])}
    owner = {v['name']: b['name'] for b in cfg.get('boards', [])
             for v in (b.get('variant') or [])}
    results = []
    for board in boards:
        names = variants_of(cfg, board)
        mine = {n: rows[n] for n in names if n in rows}
        # a variant name that is neither declared nor prefixed cannot be attributed; the
        # `<board>-` fallback only helps ad-hoc builds, it is not the primary path. It must
        # also never steal a row DECLARED by another board: a declared variant need not start
        # with its own board's name, so it may happen to start with this board's name plus '-'.
        mine.update({n: c for n, c in rows.items()
                     if n.startswith(f'{board}-') and n not in mine
                     and owner.get(n, board) == board})
        if not mine:
            results.append({'board': board, 'ran': False, 'pass': False, 'locked': False,
                            'detail': 'no report row for this board'})
            continue
        locked = any(LOCKED_CELL in cells for cells in mine.values())
        bad = []
        for vname, cells in sorted(mine.items()):
            for test, val in sorted(cells.items()):
                if test == LOCKED_CELL:
                    continue
                if cell_state(str(val)) == 'fail':
                    bad.append(f'{vname} {test}: {val}')
        ok = not bad and not locked
        if locked:
            detail = 'held by another holder; not flashed'
        elif bad:
            detail = '; '.join(bad)
        else:
            detail = f'{len(mine)} variant(s), {sum(len(c) for c in mine.values())} cell(s) ok'
        results.append({'board': board, 'ran': True, 'pass': ok, 'locked': locked,
                        'detail': detail})
    return {'results': results, 'banner': report.get('banner', '')}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('config_file')
    ap.add_argument('-b', '--board', action='append', default=[],
                    help='boards to report on; default: every board in the config')
    ap.add_argument('--report-dir', default='.', help='where hil_report.json lives (default: cwd)')
    a = ap.parse_args()

    cfg = json.loads(Path(a.config_file).read_text())
    boards = a.board or [b['name'] for b in cfg.get('boards', [])]
    jpath = Path(a.report_dir) / 'hil_report.json'
    if not jpath.is_file():
        print(f'error: {jpath} not found -- did hil_test.py run in this directory?',
              file=sys.stderr)
        return 1
    json.dump(summarize(cfg, boards, json.loads(jpath.read_text())), sys.stdout, indent=2)
    print()
    return 0


if __name__ == '__main__':
    sys.exit(main())
