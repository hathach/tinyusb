# hil_report.py Module Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fold every function that produces, renders, merges or reads `hil_report.json`/`hil_report.md` into one module, `test/hil/helper/hil_report.py`, and take the two fixes that consolidation enables.

**Architecture:** A new leaf-ish module owns the report document. `hil_test.py` and `hil_health.py` both import it, which dissolves the circular-import constraint that forced `write_timeout_report` to compose its own markdown. The duplicated cell classifier (`cell_kind` in `hil_test`, `cell_state` in `hil_summary`) collapses into one. `hil_summary.py` is deleted and its CLI moves in.

**Tech Stack:** Python 3.13 stdlib only (`json`, `argparse`, `pathlib`); existing unit suites under `test/hil/test/` run with plain `unittest`.

**Spec:** `docs/superpowers/specs/2026-08-21-hil-report-module-design.md`

## Global Constraints

- **Behaviour-preserving motion.** `hil_test.py`'s CLI, arguments, output and report format stay byte-identical. The two intended exceptions are named in the spec: the `hil_summary.py` → `hil_report.py` CLI path, and `write_timeout_report` rendering instead of concatenating.
- **`hil_report.py` must work in two modes.** It is imported as `helper.hil_report` by `hil_test.py`, and run as a script by the operator (`python3 test/hil/helper/hil_report.py <config> -b BOARD`). A script run puts `test/hil/helper/` on `sys.path`, *not* `test/hil/`, so `from helper import hil_health` fails in that mode. Task 1 pins both modes with tests.
- **Containment paths must never raise.** `mark_report_abandoned` and `write_timeout_report` run while the interpreter is being torn down or on the way to `os._exit`; anything escaping hangs the process in multiprocessing's unbounded `join()`. Their existing broad handlers move with them unchanged.
- **`hil_ci.sh` stages helpers by an explicit list** (`test/hil/hil_ci.sh:222-228`). A helper module missing from it reaches the rig absent, and the run dies with `ImportError` *after* `REMOTE_DIR` has been wiped. `RemoteStaging.test_import_closure_is_staged_to_the_rig` in `test_hil_bounded.py` already enforces this from the AST import closure; Task 1 only has to add the file to the list.
- Run `python3 -m unittest discover -s test/hil/test` (~82 s) before each commit; `pre-commit run --files <changed>` before pushing.

---

### Task 1: The module, the vocabulary, one classifier, and the render half

**Files:**
- Create: `test/hil/helper/hil_report.py`
- Create: `test/hil/test/test_hil_report.py`
- Modify: `test/hil/hil_test.py:110` (`REPORT_CELL`), `:1715` (`BOUNDARY_CELL`), `:1902-1903` (`REPORT_MD`/`REPORT_JSON`), `:1921-1978` (`render_matrix`), `:1981-2003` (`render_report`), `:67` (imports)
- Modify: `test/hil/hil_ci.sh:222-228` (scp list)
- Modify: `test/hil/test/test_hil_bounded.py` (move `RenderReportIsPureFunctionOfTheDocument` out)

**Interfaces:**
- Produces: `helper.hil_report` exposing `REPORT_MD`, `REPORT_JSON`, `REPORT_CELL`, `BOUNDARY_CELL`, `LOCKED_CELL`, `cell_state(v) -> str`, `render_matrix(rows_all) -> str`, `render_report(doc) -> str`.
- `hil_test.py` re-exports nothing: call sites become `hil_report.NAME`.

- [ ] **Step 1: Write the failing tests**

Create `test/hil/test/test_hil_report.py`:

```python
#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Unit tests for the report document: the vocabulary, the one cell classifier, rendering,
# the four writers, and the fold to per-board verdicts. Split out of test_hil_bounded.py
# and test_hil_health.py when the report code moved into helper/hil_report.py.
# Run directly:
#   python3 test/hil/test/test_hil_report.py
import json
import os
import subprocess
import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

TEST_DIR = os.path.dirname(os.path.abspath(__file__))
HIL_DIR = os.path.dirname(TEST_DIR)
sys.path.insert(0, HIL_DIR)

from helper import hil_report


class OneClassifierForBothArtifacts(unittest.TestCase):
    """The markdown tally and the agent's verdict used to classify cells with two separate
    copies of one rule -- hil_test's cell_kind against REPORT_CELL, and hil_summary's
    cell_state against its own re-typed '❌'/'⚪' literals. Change the icons and the table
    and the verdict silently disagree."""

    def test_bare_states(self):
        self.assertEqual(hil_report.cell_state('fail'), 'fail')
        self.assertEqual(hil_report.cell_state('skip'), 'skip')
        self.assertEqual(hil_report.cell_state('pass'), 'pass')

    def test_icon_prefixed_metrics_carry_their_verdict(self):
        self.assertEqual(hil_report.cell_state(f'{hil_report.REPORT_CELL["fail"]} 29/30'), 'fail')
        self.assertEqual(hil_report.cell_state(f'{hil_report.REPORT_CELL["skip"]} board wedged'),
                         'skip')

    def test_an_unprefixed_metric_is_a_pass(self):
        """Load-bearing: a passing test may return a plain metric string. Classifying
        unknown shapes as fail would publish a green table as a red verdict."""
        self.assertEqual(hil_report.cell_state('480.0 MBps'), 'pass')
        self.assertEqual(hil_report.cell_state('1103 KB/s'), 'pass')

    def test_a_non_string_cell_does_not_raise(self):
        """render_matrix's copy guarded with isinstance; hil_summary's did not, because its
        caller str()'d first. The merged one keeps the guard -- it is the safer superset."""
        self.assertEqual(hil_report.cell_state(None), 'pass')

    def test_the_icons_come_from_REPORT_CELL(self):
        """No second copy of the emoji anywhere in the module."""
        src = (Path(HIL_DIR) / 'helper' / 'hil_report.py').read_text(encoding='utf-8')
        for icon in ('❌', '⚪', '✅'):
            self.assertEqual(src.count(f"'{icon}'"), 1,
                             f'{icon} is spelled as a literal more than once')


class ModuleWorksImportedAndAsAScript(unittest.TestCase):
    """It is imported as helper.hil_report by hil_test, and run as a script by the operator
    (.claude/agents/hil-operator.md). A script run puts helper/ on sys.path, NOT test/hil,
    so a plain `from helper import hil_health` breaks the CLI and only the CLI."""

    def test_importable_as_a_package_module(self):
        r = subprocess.run(
            [sys.executable, '-c',
             f'import sys; sys.path.insert(0, {HIL_DIR!r}); '
             f'from helper import hil_report; print(hil_report.REPORT_JSON)'],
            capture_output=True, text=True, timeout=60)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('hil_report.json', r.stdout)

    def test_runnable_as_a_script(self):
        r = subprocess.run(
            [sys.executable, str(Path(HIL_DIR) / 'helper' / 'hil_report.py'), '--help'],
            capture_output=True, text=True, timeout=60)
        self.assertEqual(r.returncode, 0, r.stderr)


class HilCiStagesEveryHelperTheRunImports(unittest.TestCase):
    """hil_ci.sh copies helper modules by an EXPLICIT list. One missing module reaches the
    rig absent and the run dies with ImportError -- after REMOTE_DIR has already been
    rm -rf'd, so the previous run's report and re-run spec are gone too."""

    def test_the_scp_list_covers_what_hil_test_imports(self):
        sh = (Path(HIL_DIR) / 'hil_ci.sh').read_text(encoding='utf-8')
        staged = {line.split('helper/')[1].rstrip('" \\\n')
                  for line in sh.splitlines() if '/test/hil/helper/' in line and '.py' in line}
        imported = set()
        for mod in (Path(HIL_DIR) / 'hil_test.py', Path(HIL_DIR) / 'helper' / 'hil_report.py'):
            src = mod.read_text(encoding='utf-8')
            for raw in src.splitlines():
                line = raw.strip()          # hil_report's own import is indented in a try
                if line.startswith('from helper import '):
                    imported |= {f'{n.strip()}.py' for n in line.split('import', 1)[1].split(',')}
                elif line.startswith('from helper.'):
                    imported.add(line.split('.')[1].split(' ')[0] + '.py')
        missing = imported - staged
        self.assertEqual(missing, set(),
                         f'hil_ci.sh does not stage {missing}; a remote run will ImportError')


if __name__ == '__main__':
    unittest.main()
```

Then **move** the class `RenderReportIsPureFunctionOfTheDocument` from `test/hil/test/test_hil_bounded.py` into this file verbatim, changing only `hil_test.render_report` → `hil_report.render_report` throughout.

- [ ] **Step 2: Run them to verify they fail**

Run: `python3 test/hil/test/test_hil_report.py`
Expected: FAIL — `ModuleNotFoundError: No module named 'helper.hil_report'`

- [ ] **Step 3: Create the module**

Create `test/hil/helper/hil_report.py`:

```python
#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""The HIL report document: one owner for hil_report.json and hil_report.md.

The markdown IS a rendering of the sidecar -- every writer goes through render_report(),
so a table can never contain something the JSON does not. This module owns the whole life
of that document: the cell vocabulary, the one classifier both artifacts share, rendering,
the four writers, and the fold to one machine-readable verdict per board.

Dual-mode by design: imported as `helper.hil_report` by hil_test.py, and run as a script by
the operator (see .claude/agents/hil-operator.md). A script run puts test/hil/helper on
sys.path rather than test/hil, hence the guarded hil_health import below.
"""
import argparse
import json
import sys
from pathlib import Path

try:                                    # imported as part of the helper package
    from helper.hil_health import _p
except ImportError:                     # run as a script: helper/ is sys.path[0]
    from hil_health import _p

REPORT_MD = 'hil_report.md'
REPORT_JSON = 'hil_report.json'
# The status vocabulary, shared by the code that WRITES a cell (hil_test's test runners) and
# the code that reads one back (cell_state). One dict, so the human's table and the agent's
# verdict cannot drift apart.
REPORT_CELL = {'pass': '✅', 'fail': '❌', 'skip': '⚪'}
BOUNDARY_CELL = 'same-PID boundary'
LOCKED_CELL = 'board-locked'


def cell_state(v) -> str:
    """'pass' | 'fail' | 'skip' for one report cell.

    THE classifier -- the markdown tally and the per-board verdict both call this, so they
    cannot disagree. 'fail' or a ❌ prefix is a failure, 'skip' or a ⚪ prefix is a skip, and
    EVERYTHING ELSE is a pass. That last arm is load-bearing: a passing test may return a
    plain metric string ('480.0 MBps') that lands in the cell unprefixed, while failures are
    guaranteed marked -- TestFail's docstring pins that its metric is icon-prefixed precisely
    so render and tally treat it as a failure. Classifying unknown shapes as fail here would
    publish a green table as a red verdict.

    isinstance-guarded: cells are usually str but a caller may hand over None or a number,
    and .startswith on those raises inside a report writer that must not raise."""
    if v == 'fail' or (isinstance(v, str) and v.startswith(REPORT_CELL['fail'])):
        return 'fail'
    if v == 'skip' or (isinstance(v, str) and v.startswith(REPORT_CELL['skip'])):
        return 'skip'
    return 'pass'
```

Then move, verbatim, from `hil_test.py`:
- `render_matrix` (`hil_test.py:1921-1978`) — with one change: delete its nested `cell_kind`
  definition and call the module-level `cell_state` instead. The line
  `kinds = [cell_kind(v) for _, cells, _ in rows_all for v in cells.values()]` becomes
  `kinds = [cell_state(v) for _, cells, _ in rows_all for v in cells.values()]`.
- `render_report` (`hil_test.py:1981-2003`) — unchanged.

Add a placeholder CLI so `--help` works (Task 4 fills in `summarize`):

```python
def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('config_file')
    ap.add_argument('-b', '--board', action='append', default=[],
                    help='boards to report on; default: every board in the config')
    ap.add_argument('--report-dir', default='.', help=f'where {REPORT_JSON} lives (default: cwd)')
    ap.parse_args()
    raise SystemExit('hil_report: summarize() lands in Task 4')


if __name__ == '__main__':
    sys.exit(main())
```

- [ ] **Step 4: Point `hil_test.py` at the module**

In `hil_test.py:67`, extend the import:

```python
from helper import hil_health, hil_lock, hil_report, hil_util
```

Delete `REPORT_CELL` (`:110`), `BOUNDARY_CELL` (`:1715`), `REPORT_MD`/`REPORT_JSON`
(`:1902-1903`), `render_matrix` and `render_report` from `hil_test.py`. Then rewrite every
reference to the moved names as `hil_report.<name>`. Find them all with:

```bash
grep -n "REPORT_CELL\|BOUNDARY_CELL\|REPORT_MD\|REPORT_JSON\|render_matrix\|render_report" \
  test/hil/hil_test.py
```

Known sites: `:876`, `:1369`, `:1459`, `:1490`, `:1492`, `:1508`, `:1818`, `:1834`, `:2162`,
`:2191-2192`, `:2209-2210`, `:2403`, `:2592`.

- [ ] **Step 5: Stage the new module for remote runs**

In `test/hil/hil_ci.sh:222-228`, add the module to the scp list (keep alphabetical-ish order
with the rest):

```bash
scp -q "$ROOT_DIR/test/hil/helper/__init__.py" \
       "$ROOT_DIR/test/hil/helper/hil_util.py" \
       "$ROOT_DIR/test/hil/helper/hil_health.py" \
       "$ROOT_DIR/test/hil/helper/hil_lock.py" \
       "$ROOT_DIR/test/hil/helper/hil_report.py" \
       "$ROOT_DIR/test/hil/helper/hil_summary.py" \
       "$ROOT_DIR/test/hil/helper/hil_select.py" \
       "$REMOTE:$REMOTE_DIR/test/hil/helper/"
```

- [ ] **Step 6: Run the tests**

Run: `python3 test/hil/test/test_hil_report.py` → OK
Run: `python3 -m unittest discover -s test/hil/test` → 274 OK (266 + 8 new: 5 classifier,
2 dual-mode, 1 scp guard; `RenderReport…` moves rather than adds)

- [ ] **Step 7: Commit**

```bash
git add test/hil/helper/hil_report.py test/hil/hil_test.py test/hil/hil_ci.sh \
        test/hil/test/test_hil_report.py test/hil/test/test_hil_bounded.py
git commit -m "hil_report: new module for the report vocabulary, classifier and rendering

The markdown tally and the agent's verdict classified cells with two separate
copies of one rule, the second documented as 'the EXACT classifier hil_test.py's
own tally uses'. One cell_state now serves both, keyed off the one REPORT_CELL."
```

---

### Task 2: Move the three writers

**Files:**
- Modify: `test/hil/helper/hil_report.py` (add the writers)
- Modify: `test/hil/hil_test.py:2005-2036` (`write_report`, `mark_report_abandoned`), `:2149-2212` (`accumulate_report`)
- Modify: `test/hil/test/test_hil_bounded.py` (move three classes out), `test/hil/test/test_hil_report.py`

**Interfaces:**
- Consumes: `render_report`, `REPORT_MD`, `REPORT_JSON`, `BOUNDARY_CELL` from Task 1.
- Produces: `hil_report.write_report(report_dir, doc)`, `hil_report.mark_report_abandoned(report_dir, why)`, `hil_report.accumulate_report(mret, report_dir, fresh, scope='', banner='') -> str`.

- [ ] **Step 1: Move the tests**

Move these classes from `test/hil/test/test_hil_bounded.py` into `test/hil/test/test_hil_report.py`,
verbatim except `hil_test.<name>` → `hil_report.<name>` for the three moved functions:

- `ScopeSurvivesInTheJson`
- `EveryExitPathLeavesBothArtifacts`
- `AbandonNoticeLandsInBothArtifacts`
- `CaveatSurvivesAccumulate`
- `MarkdownIsAlwaysARenderingOfTheJson`

`AbandonNoticeLandsInBothArtifacts.test_an_existing_abandon_caveat_is_not_overwritten` calls
`hil_health.write_timeout_report`; leave that call as-is — Task 3 moves it.

- [ ] **Step 2: Run them to verify they fail**

Run: `python3 test/hil/test/test_hil_report.py`
Expected: FAIL — `AttributeError: module 'helper.hil_report' has no attribute 'write_report'`

- [ ] **Step 3: Move the functions**

Cut `write_report` (`hil_test.py:2005-2014`), `mark_report_abandoned` (`:2016-2036`) and
`accumulate_report` (`:2149-2212`) from `hil_test.py` and paste them into `hil_report.py`
below `render_report`, unchanged.

Add to `accumulate_report`'s docstring, after the existing text, so the wart is recorded
where a reader meets it:

```
    `mret` is hil_test.py's worker-result shape (name, err, fts, rows, ...), so this one
    function knows something about its caller that the rest of the module does not. Folding
    mret into rows could live in hil_test and only the merge here, but that would rewrite
    the subtle parts -- stale board-locked clearing, BOUNDARY_CELL dropping, duration=None
    preservation -- for a tidier seam. Data-shape coupling, not an import cycle.
```

- [ ] **Step 4: Update the call sites**

In `hil_test.py`, the three call sites become `hil_report.*`:

```bash
grep -n "accumulate_report(\|write_report(\|mark_report_abandoned(" test/hil/hil_test.py
```

Known sites: `:2260` (inside `_abandon_exit`), `:2351` (no-boards exit), `:2486`, `:2525`,
`:2618`.

- [ ] **Step 5: Run the tests**

Run: `python3 -m unittest discover -s test/hil/test` → 274 OK (motion only, no count change)

- [ ] **Step 6: Commit**

```bash
git add test/hil/helper/hil_report.py test/hil/hil_test.py \
        test/hil/test/test_hil_report.py test/hil/test/test_hil_bounded.py
git commit -m "hil_report: move the report writers off hil_test

write_report, mark_report_abandoned and accumulate_report join the renderer they
already call. Pure motion; accumulate_report's knowledge of mret's tuple shape
moves with it and is now documented rather than implicit."
```

---

### Task 3: `write_timeout_report` renders like everyone else

**Files:**
- Modify: `test/hil/helper/hil_report.py` (receive the function)
- Modify: `test/hil/helper/hil_health.py:347-398` (remove it), `:19` (drop `import json`)
- Modify: `test/hil/hil_test.py:2498` (call site)
- Modify: `test/hil/test/test_hil_health.py` (move `WriteTimeoutReport` out), `test/hil/test/test_hil_report.py`

**Interfaces:**
- Consumes: `render_report`, `write_report` from Tasks 1-2.
- Produces: `hil_report.write_timeout_report(report_dir, boards, secs, banner='', prefix='')`. The `md_name` parameter is **gone** — the module owns `REPORT_MD`.

- [ ] **Step 1: Write the failing tests**

Move `WriteTimeoutReport` from `test/hil/test/test_hil_health.py` into
`test/hil/test/test_hil_report.py`, changing `hil_health.write_timeout_report` →
`hil_report.write_timeout_report` and dropping the `md_name` argument from every call. Two
of its tests change substantively:

```python
    def test_the_prior_attempts_rows_survive(self):
        """Was: the prior MARKDOWN TEXT survives below the banner. It now re-renders from
        the merged sidecar, so the guarantee is stated against rows -- one table with the
        stuck boards in it, rather than a banner stapled above a duplicate table."""
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_report.accumulate_report(
            [('done', 0, 0, [('done', {'cdc_msc': 'OK'}, '1s')], 0)], rd, True, '', '')
        hil_report.write_timeout_report(rd, [{'name': 'stuck'}], 3600)
        doc = json.loads((rd / hil_report.REPORT_JSON).read_text())
        self.assertEqual([r['board'] for r in doc['rows']], ['done', 'stuck'])
        md = (rd / hil_report.REPORT_MD).read_text()
        self.assertIn('done', md)
        self.assertIn('stuck', md)
        self.assertIn('abandoned', md)
        self.assertLess(md.index('abandoned'), md.index('done'))
        self.assertEqual(md.count('| Board'), 1, 'the prior table was duplicated, not merged')

    def test_prefix_carries_the_preflight_diagnosis(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_report.write_timeout_report(rd, [{'name': 'b1'}], 4200,
                                        prefix='> **wedged usb_hub_wq worker.**\n')
        out = (rd / hil_report.REPORT_MD).read_text()
        self.assertTrue(out.startswith('> **wedged usb_hub_wq worker.**'))
        self.assertIn('timed out after 4200s', out)
        self.assertIn('b1', out)
```

And in `MarkdownIsAlwaysARenderingOfTheJson`, **delete**
`test_the_pool_guard_fallback_agrees_even_if_it_does_not_render` and add the fifth case in
its place:

```python
    def test_the_pool_guard_fallback(self):
        """The last writer to join the invariant: it composed its own markdown only because
        hil_health could not import the renderer."""
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_report.accumulate_report(
            [('done', 0, 0, [('done', {'cdc_msc': 'OK'}, '1s')], 0)], rd, True, '', '')
        hil_report.write_timeout_report(rd, [{'name': 'stuck'}], 3600,
                                        prefix='> **wedged usb_hub_wq worker.**\n')
        self._check(rd)
```

- [ ] **Step 2: Run them to verify they fail**

Run: `python3 test/hil/test/test_hil_report.py`
Expected: FAIL — `AttributeError: module 'helper.hil_report' has no attribute 'write_timeout_report'`

- [ ] **Step 3: Move it and make it render**

Add to `hil_report.py`, and delete `hil_health.py:347-398` plus its now-unused
`import json` at `hil_health.py:19`:

```python
def write_timeout_report(report_dir: Path, boards, secs: int,
                         banner: str = '', prefix: str = '') -> None:
    """Leave a report behind when the worker pool has to be abandoned.

    map_async is all-or-nothing, so a timeout loses every per-board result and the report
    dir would stay empty with no reason for the failure. Any prior attempt's rows are kept
    and the stuck boards are merged in beside them.

    `prefix` carries the preflight rig-health verdict: the timeout aborts before
    accumulate_report, so without it the report loses the one line saying WHY the pool never
    finished."""
    try:
        # Built INSIDE the try: a roster entry without a 'name' key raises while assembling
        # the board list, and outside the try that escaped and stranded the runner -- which
        # is exactly what the broad handler below exists to prevent.
        caveat = (prefix + '\n' if prefix else '') + (banner or (
            f'**HIL run abandoned: worker pool timed out after {secs}s.**\n\n'
            f'No per-board results could be collected for this attempt, so any rows below '
            f'are from an earlier one. Boards dispatched:\n\n'
            + '\n'.join(f'- {b.get("name", "?")}' for b in boards) + '\n'))
        # Rows MERGE rather than replace: an earlier attempt's finished boards are real
        # results and this attempt has none of its own. Own handler, because a torn sidecar
        # must not cost the stuck rows -- losing the old table is a nicety, losing the
        # caveat is the failure.
        jpath = report_dir / REPORT_JSON
        try:
            doc = json.loads(jpath.read_text()) if jpath.is_file() else {}
            rows = list(doc.get('rows', []))
        except (OSError, ValueError, TypeError, AttributeError):
            doc, rows = {}, []
        done = {r.get('board') for r in rows if isinstance(r, dict)}
        rows += [{'board': b.get('name', '?'), 'cells': {'pool-timeout': 'fail'},
                  'duration': None} for b in boards if b.get('name', '?') not in done]
        write_report(report_dir, {'rows': rows, 'banner': doc.get('banner', ''),
                                  'scope': doc.get('scope', ''), 'caveat': caveat})
    except Exception as e:  # noqa: BLE001
        # Deliberately broad: this is the first statement of the pool-abandon path, so ANY
        # escape skips kill_pool_children and os._exit and strands the runner.
        _p(f'warning: cannot write {REPORT_MD} to {report_dir}: {e}', flush=True)
```

Update `hil_health.py`'s module docstring: its first line reads "Shutting a wedged HIL run
down: kill what the workers spawned, then report." — drop ", then report".

- [ ] **Step 4: Update the call site**

`hil_test.py:2498` becomes:

```python
                        hil_report.write_timeout_report(
                            report_dir, [b for b in config_boards
                                         if b['name'] in stuck], POOL_TIMEOUT,
                            prefix=health_banner)
```

- [ ] **Step 5: Run the tests**

Run: `python3 -m unittest discover -s test/hil/test` → 274 OK (one deleted, one added)

- [ ] **Step 6: Commit**

```bash
git add test/hil/helper/hil_report.py test/hil/helper/hil_health.py test/hil/hil_test.py \
        test/hil/test/test_hil_report.py test/hil/test/test_hil_health.py
git commit -m "hil_report: the pool-guard fallback renders like every other writer

It composed its own markdown for one reason: hil_health cannot import hil_test
back, so it could not reach render_report. With the renderer in a module both
import, that constraint is gone and all five writers are byte-identical --
MarkdownIsAlwaysARenderingOfTheJson covers the fifth, and the weaker
'agrees even if it does not render' promise is deleted.

hil_health goes back to doing one thing: killing wedged processes."
```

---

### Task 4: Fold `hil_summary.py` in and delete it

**Files:**
- Modify: `test/hil/helper/hil_report.py` (real `summarize` + CLI)
- Delete: `test/hil/helper/hil_summary.py`
- Modify: `test/hil/hil_ci.sh` (drop `hil_summary.py` from the scp list)
- Modify: `.claude/agents/hil-operator.md:71`, `.claude/workflows/hil-validate.js:14,17,54,58,67`, `.claude/workflows/test-hil-validate.mjs:7`
- Modify: `test/hil/test/test_hil_bounded.py` (move `SummaryFoldsReportToBoards` out), `test/hil/test/test_hil_report.py`

**Interfaces:**
- Consumes: `cell_state`, `LOCKED_CELL`, `REPORT_JSON` from Task 1.
- Produces: `hil_report.variants_of(cfg, board) -> list`, `hil_report.summarize(cfg, boards, report) -> dict` returning `{'results': [...], 'banner': str, 'caveat': str}`; CLI `python3 test/hil/helper/hil_report.py <config> [-b BOARD]... [--report-dir DIR]`.

- [ ] **Step 1: Move the tests**

Move `SummaryFoldsReportToBoards` from `test/hil/test/test_hil_bounded.py` into
`test/hil/test/test_hil_report.py`, changing the subprocess target from
`helper/hil_summary.py` to `helper/hil_report.py` in both places (`test_hil_bounded.py:1675`
and `:1757`). Add one test pinning that the old entry point is gone:

```python
    def test_the_old_entry_point_is_gone(self):
        """hil_summary.py's CLI moved here. A leftover file would keep working while
        drifting from the module that now owns the fold."""
        self.assertFalse((Path(HIL_DIR) / 'helper' / 'hil_summary.py').exists())
```

- [ ] **Step 2: Run them to verify they fail**

Run: `python3 test/hil/test/test_hil_report.py`
Expected: FAIL — the subprocess exits non-zero with `hil_report: summarize() lands in Task 4`

- [ ] **Step 3: Move `summarize` in and delete the old file**

Copy `variants_of` (`hil_summary.py:47-52`) and `summarize` (`:54-92`) into `hil_report.py`
verbatim, with two changes: `cell_state(str(val))` becomes `cell_state(val)` (the merged
classifier is isinstance-guarded, so the `str()` is dead), and the module's own
`FAIL_ICON`/`SKIP_ICON`/`LOCKED_CELL`/`cell_state` definitions are NOT copied — Task 1's
already serve.

Replace the Task 1 placeholder `main()` with the real one from `hil_summary.py:94-115`,
changing `Path(a.report_dir) / 'hil_report.json'` to `Path(a.report_dir) / REPORT_JSON`.

Then:

```bash
git rm test/hil/helper/hil_summary.py
```

- [ ] **Step 4: Update the consumers**

`test/hil/hil_ci.sh` — remove the `hil_summary.py` line from the scp list added in Task 1.

`.claude/agents/hil-operator.md:71`:

```bash
python3 test/hil/helper/hil_report.py <config> -b BOARD [-b BOARD...]   # from the report dir
```

`.claude/workflows/hil-validate.js:58`:

```javascript
  `  python3 test/hil/helper/hil_report.py <the config you used> ${boards.map((b) => `-b ${b}`).join(' ')}\n` +
```

In `.claude/workflows/hil-validate.js` lines 14, 17, 54 and 67, and
`.claude/workflows/test-hil-validate.mjs` line 7, replace the prose mentions of
`hil_summary.py` with `hil_report.py`. Change nothing else in those files — the operator's
return contract (`{results, banner, wedged}`) is untouched.

- [ ] **Step 5: Run the tests**

Run: `python3 test/hil/test/test_hil_report.py` → OK
Run: `python3 -m unittest discover -s test/hil/test` → 275 OK
Run: `node .claude/workflows/test-hil-validate.mjs` → OK
Run: `grep -rn "hil_summary" . --include=*.py --include=*.sh --include=*.js --include=*.mjs --include=*.md | grep -v docs/superpowers` → no hits

- [ ] **Step 6: Commit**

```bash
git add test/hil/helper/hil_report.py test/hil/hil_ci.sh test/hil/test/ \
        .claude/agents/hil-operator.md .claude/workflows/hil-validate.js \
        .claude/workflows/test-hil-validate.mjs
git rm --cached test/hil/helper/hil_summary.py 2>/dev/null || true
git commit -m "hil_report: fold hil_summary in; one module owns the document end to end

The fold to per-board verdicts is the read half of the artifact the rest of this
module writes, and it carried the second copy of the cell classifier. The CLI
keeps its arguments; only its path changes, which the two harness docs that
invoke it by name follow."
```

---

## Validation

- [ ] **Full gate**

```bash
python3 -m unittest discover -s test/hil/test        # 275 OK
pre-commit run --all-files
```

- [ ] **Prove the motion changed no behaviour.** Re-render the real fleet report captured
  before the refactor and diff it against what the branch produces now:

```bash
python3 - <<'EOF'
import json, sys
sys.path.insert(0, 'test/hil')
from helper import hil_report
doc = json.load(open('hil_report.json'))          # the pair the rig produced pre-refactor
assert open('hil_report.md').read() == hil_report.render_report(doc) + '\n', 'render drifted'
print('render is byte-identical to the pre-refactor artifact')
EOF
```

- [ ] **Rig re-check.** `hil_report.py` must reach the rig and the CLI must run there:

```bash
bash test/hil/hil_ci.sh -b stm32f407disco -b nanoch32v203
ssh hathach@ci.lan 'cd /tmp/tinyusb-hil && python3 test/hil/helper/hil_report.py \
    test/hil/tinyusb.json -b stm32f407disco -b nanoch32v203'
```

Expect a two-board table, `md == render_report(json)`, and a `summarize` verdict naming both
boards — `nanoch32v203` proving the variant fold still works through the moved code.

## Out of scope

Each its own follow-up, unchanged from the spec:

- Splitting `accumulate_report`'s `mret` folding from its merge.
- The flat `HIL_POOL_TIMEOUT` that does not scale with board count.
- Carrying `caveat` through the operator/workflow return contract (`hil-validate.js:34`).
