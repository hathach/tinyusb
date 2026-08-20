# One Source of Truth for the HIL Report Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `hil_report.md` a rendering of `hil_report.json` rather than a second, independently written artifact, so no run can produce a table whose contents are not in the JSON.

**Architecture:** `hil_report.json` gains the two fields the markdown carries but the JSON does not (`scope`, and a `caveat` for text prepended after the fact). A single `render_report(doc) -> str` turns that document into the markdown, and every writer — the normal path, the pool-guard fallback, the no-boards exit, and `_abandon_exit` — goes through `write_report(report_dir, doc)`, which writes both files from the same dict. `_abandon_exit` stops doing a text-prepend on a file it did not write and instead sets `doc['caveat']`.

**Tech Stack:** Python 3.13 stdlib only (`json`, `pathlib`); existing unit suites under `test/hil/test/` run with plain `unittest`.

**Spec:** none — this is a follow-up split out of the `claude/hil-doc-audit` branch. The evidence it argues from is inline below.

**Origin:** split out of PR #3836 (the HIL one-run rework + `.claude` instruction audit). Delete this file when its own PR lands.

## Global Constraints

- **No behaviour change to the containment paths' ordering or exit codes.** `_abandon_exit` runs while the interpreter is being torn down; its own comments record that anything raising between the pool's `finally` and `os._exit` hangs the process in multiprocessing's unbounded `join()` (reproduced at rc=124/25s with SIGTERM-ignoring workers). Serialisation added there must stay inside the existing `try`/`except` and must never raise past it.
- **The markdown stays the human artifact.** `.github/workflows/build.yml:487` uploads `hil_report.md`, `test/hil/hil_ci.sh:293` copies only it back, and `.claude/skills/hil/SKILL.md` tells the operator to paste that table verbatim. It becomes generated output, not a dropped file.
- **Banner outranks the scope note outranks the table.** Preserve the existing order (`hil_test.py:2153-2161`): the caveat is outermost because that is where `hil/SKILL.md` tells an agent to look.
- **`--accumulate` merges from the JSON** (`hil_test.py:2102-2115`), including carrying the prior banner forward. Adding fields must not break that merge for a sidecar written by an older version.
- Run `python3 -m unittest discover -s test/hil/test` (115 tests, ~78 s) before each commit; `pre-commit run --files <changed>` before pushing.

## Why this is worth doing

Four writers produce `hil_report.md`, and three of them write no JSON at all:

| Writer | JSON? | Line |
|---|---|---|
| `accumulate_report` — the normal path | yes | `hil_test.py:2149`, `:2162` |
| `**HIL run selected no boards.**` | **no** | `hil_test.py:2317` |
| pool-guard fallback → `hil_health.write_timeout_report(...)` | **no** | `hil_test.py:2469`, `hil_health.py:346` |
| `_abandon_exit` — prepends to whatever `.md` exists | **no** | `hil_test.py:2603` |

Those three are exactly the paths where the run died, so they are the cases where the artifact matters most and where a JSON consumer sees nothing. `test/hil/helper/hil_summary.py` (added on the origin branch) reads the JSON to build the per-board verdicts an agent hands back — on any of those three paths it finds no file and reports "no report row for this board" for the whole fleet, while a human reading the markdown sees the real story.

Separately, `scope` exists only in the markdown (`hil_test.py:2154`, from `accumulate_report`'s `scope: str = ''` parameter at `:2092`). A PR-scoped three-board table and a full-fleet run that lost 24 boards are indistinguishable in the JSON.

---

### Task 1: Put `scope` in the JSON

**Files:**
- Modify: `test/hil/hil_test.py:2092-2163` (`accumulate_report`)
- Test: `test/hil/test/test_hil_bounded.py` (new class beside `CaveatSurvivesAccumulate`)

**Interfaces:**
- Produces: `hil_report.json` gains a top-level `"scope": str` (empty string when unscoped). Existing keys `rows` and `banner` are unchanged.

- [ ] **Step 1: Write the failing test**

```python
class ScopeSurvivesInTheJson(unittest.TestCase):
    """A scoped run's small table is indistinguishable from a full run that lost boards.
    The markdown says so; the JSON did not, so any JSON consumer could not tell."""

    def _rows(self, board, cell):
        return [(board, 0, 0, [(board, {cell: 'OK'}, '1s')], 0)]

    def test_scope_is_recorded_in_the_sidecar(self):
        import json
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_test.accumulate_report(self._rows('boardA', 'cdc_msc'), rd, True,
                                   '-b boardA', '')
        doc = json.loads((rd / 'hil_report.json').read_text())
        self.assertEqual(doc['scope'], '-b boardA')

    def test_an_unscoped_run_records_an_empty_scope(self):
        import json
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_test.accumulate_report(self._rows('boardA', 'cdc_msc'), rd, True, '', '')
        self.assertEqual(json.loads((rd / 'hil_report.json').read_text())['scope'], '')
```

- [ ] **Step 2: Run it to verify it fails**

Run: `python3 test/hil/test/test_hil_bounded.py ScopeSurvivesInTheJson`
Expected: FAIL — `KeyError: 'scope'`

- [ ] **Step 3: Add the field**

In `accumulate_report`, change the `jpath.write_text(...)` call at `hil_test.py:2149`:

```python
    jpath.write_text(json.dumps({'rows': [{'board': k, 'cells': c, 'duration': d}
                                          for k, (c, d) in acc.items()],
                                 'banner': banner,
                                 'scope': scope}, indent=2) + '\n')
```

- [ ] **Step 4: Run the tests**

Run: `python3 test/hil/test/test_hil_bounded.py ScopeSurvivesInTheJson` → PASS
Run: `python3 -m unittest discover -s test/hil/test` → 117 tests OK (the merge at `:2102` reads only `rows` and `banner`, so an older sidecar without `scope` still loads).

- [ ] **Step 5: Commit**

```bash
git add test/hil/hil_test.py test/hil/test/test_hil_bounded.py
git commit -m "hil_test: record the run's scope in hil_report.json

The markdown says a scoped table is scoped; the JSON did not, so a consumer
could not tell a three-board PR run from a full run that lost 24 boards."
```

---

### Task 2: Render the markdown from the document

**Files:**
- Modify: `test/hil/hil_test.py:1921` (`render_matrix`), `:2149-2163` (`accumulate_report`'s tail)
- Test: `test/hil/test/test_hil_bounded.py`

**Interfaces:**
- Consumes: the `scope` key from Task 1.
- Produces: `render_report(doc: dict) -> str`, where `doc` is `{'rows': [{'board','cells','duration'}], 'banner': str, 'scope': str, 'caveat': str}`. `caveat` is optional and empty by default (Task 4 sets it). Order is caveat, banner, scope note, table.

- [ ] **Step 1: Write the failing test**

```python
class RenderReportIsPureFunctionOfTheDocument(unittest.TestCase):
    def _doc(self, **kw):
        d = {'rows': [{'board': 'boardA', 'cells': {'cdc_msc': 'pass'}, 'duration': '1s'}],
             'banner': '', 'scope': '', 'caveat': ''}
        d.update(kw)
        return d

    def test_table_comes_from_rows(self):
        md = hil_test.render_report(self._doc())
        self.assertIn('boardA', md)
        self.assertIn('cdc_msc', md)

    def test_scope_note_appears_above_the_table(self):
        md = hil_test.render_report(self._doc(scope='-b boardA'))
        self.assertLess(md.index('Scoped run'), md.index('boardA'))

    def test_banner_outranks_the_scope_note(self):
        md = hil_test.render_report(self._doc(scope='-b boardA',
                                              banner='> **Rig dirty.** x\n'))
        self.assertLess(md.index('Rig dirty'), md.index('Scoped run'))

    def test_caveat_is_outermost(self):
        md = hil_test.render_report(self._doc(banner='> **Rig dirty.** x\n',
                                              caveat='**HIL run abandoned.**\n'))
        self.assertLess(md.index('abandoned'), md.index('Rig dirty'))

    def test_a_document_with_no_rows_still_renders(self):
        md = hil_test.render_report(self._doc(rows=[]))
        self.assertIn('No tests were run.', md)
```

- [ ] **Step 2: Run it to verify it fails**

Run: `python3 test/hil/test/test_hil_bounded.py RenderReportIsPureFunctionOfTheDocument`
Expected: FAIL — `AttributeError: module 'hil_test' has no attribute 'render_report'`

- [ ] **Step 3: Add `render_report` and route `accumulate_report` through it**

Add beside `render_matrix` (after `hil_test.py:1919`):

```python
def render_report(doc: dict) -> str:
    """The markdown IS a rendering of the sidecar. Every writer goes through here, so a
    table can never contain something the JSON does not."""
    md = render_matrix([(r['board'], r['cells'], r.get('duration'))
                        for r in doc.get('rows', [])])
    if doc.get('scope'):
        # a scoped run's small table is otherwise indistinguishable from a full one, and
        # it replaces the previous full table in the sticky PR comment
        md = f'_Scoped run: {doc["scope"]}. Boards/tests not listed were not run._\n\n' + md
    # banner, then caveat: a rig-health caveat outranks the table AND the scope note, and an
    # abandon notice outranks even that -- the top of the report is where hil/SKILL.md tells
    # the agent to look
    if doc.get('banner'):
        md = doc['banner'] + '\n' + md
    if doc.get('caveat'):
        md = doc['caveat'] + '\n' + md
    return md
```

Then replace `accumulate_report`'s tail (`hil_test.py:2153-2163`) with:

```python
    doc = {'rows': [{'board': k, 'cells': c, 'duration': d} for k, (c, d) in acc.items()],
           'banner': banner, 'scope': scope, 'caveat': ''}
    jpath.write_text(json.dumps(doc, indent=2) + '\n')
    md = render_report(doc)
    (report_dir / REPORT_MD).write_text(md + '\n', encoding='utf-8')
    return md
```

- [ ] **Step 4: Run the tests**

Run: `python3 -m unittest discover -s test/hil/test`
Expected: 122 OK. `CaveatSurvivesAccumulate` must still pass — it asserts the banner survives a rerun, which is now the `banner` key round-tripping through the document.

- [ ] **Step 5: Commit**

```bash
git add test/hil/hil_test.py test/hil/test/test_hil_bounded.py
git commit -m "hil_test: render the markdown from the report document

One function turns the sidecar into the table, so the markdown cannot carry
anything the JSON lacks. Ordering (caveat > banner > scope > table) is pinned
by tests rather than by the order of three string concatenations."
```

---

### Task 3: Give the two early-exit paths a document

**Files:**
- Modify: `test/hil/hil_test.py:2313-2320` (no-boards exit), `test/hil/helper/hil_health.py:346` (`write_timeout_report`)
- Test: `test/hil/test/test_hil_health.py` (beside `WriteTimeoutReport`), `test/hil/test/test_hil_bounded.py`

**Interfaces:**
- Consumes: `render_report(doc)` from Task 2.
- Produces: `write_report(report_dir: Path, doc: dict) -> None`, which writes `hil_report.json` and `hil_report.md` from one dict. Both early-exit paths call it.

- [ ] **Step 1: Write the failing test**

```python
class EveryExitPathLeavesBothArtifacts(unittest.TestCase):
    """hil_summary.py builds an agent's verdicts from the JSON. A path that writes only
    markdown reports the whole fleet as 'no report row' while a human sees the real story."""

    def test_the_no_boards_exit_writes_json_too(self):
        import json
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_test.write_report(rd, {'rows': [], 'banner': '', 'scope': '',
                                   'caveat': '**HIL run selected no boards.** why\n'})
        self.assertIn('selected no boards', (rd / 'hil_report.md').read_text())
        doc = json.loads((rd / 'hil_report.json').read_text())
        self.assertEqual(doc['rows'], [])
        self.assertIn('selected no boards', doc['caveat'])
```

and, in `test_hil_health.py`:

```python
    def test_timeout_report_writes_the_sidecar(self):
        import json
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_health.write_timeout_report(rd, [{'name': 'boardA'}], 3600, 'hil_report.md')
        self.assertTrue((rd / 'hil_report.json').is_file())
        self.assertIn('boardA', (rd / 'hil_report.json').read_text())
```

- [ ] **Step 2: Run them to verify they fail**

Run: `python3 test/hil/test/test_hil_bounded.py EveryExitPathLeavesBothArtifacts`
Expected: FAIL — `AttributeError: module 'hil_test' has no attribute 'write_report'`
Run: `python3 test/hil/test/test_hil_health.py WriteTimeoutReport`
Expected: FAIL — `hil_report.json` is not a file

- [ ] **Step 3: Add `write_report` and use it in both paths**

Beside `render_report`:

```python
def write_report(report_dir: Path, doc: dict) -> None:
    """Write both artifacts from one document. Best-effort by design: every caller is on a
    failure path where an OSError must not replace the failure being reported."""
    try:
        report_dir.mkdir(parents=True, exist_ok=True)
        (report_dir / REPORT_JSON).write_text(json.dumps(doc, indent=2) + '\n')
        (report_dir / REPORT_MD).write_text(render_report(doc) + '\n', encoding='utf-8')
    except OSError:
        pass
```

Replace the no-boards block at `hil_test.py:2315-2320` with:

```python
            rd = Path(os.environ.get('HIL_REPORT_DIR', '.'))
            write_report(rd, {'rows': [], 'banner': '', 'scope': '',
                              'caveat': f'**HIL run selected no boards.** {msg}\n'})
```

In `hil_health.write_timeout_report`, after the markdown is composed, write the sidecar next to it with a row per stuck board:

```python
        json_path = report_dir / 'hil_report.json'
        json_path.write_text(json.dumps(
            {'rows': [{'board': b['name'], 'cells': {'pool-timeout': 'fail'},
                       'duration': None} for b in boards],
             'banner': banner, 'scope': '', 'caveat': prefix}, indent=2) + '\n')
```

Keep it inside the function's existing broad `try` — a roster entry without `name` must not escape, which is what that handler exists to prevent.

- [ ] **Step 4: Run the tests**

Run: `python3 -m unittest discover -s test/hil/test`
Expected: 124 OK.

- [ ] **Step 5: Commit**

```bash
git add test/hil/hil_test.py test/hil/helper/hil_health.py test/hil/test/
git commit -m "hil_test, hil_health: write the sidecar on the early-exit paths too

The no-boards exit and the pool-guard fallback wrote markdown only, so a JSON
consumer saw nothing on exactly the runs that failed. hil_summary.py reported
the whole fleet as 'no report row' while the markdown told the real story."
```

---

### Task 4: Make `_abandon_exit` set a field instead of prepending text

**Files:**
- Modify: `test/hil/hil_test.py` (`_abandon_exit`, the `if report is not None:` block near `:2622`), and its call site at `:2603`
- Test: `test/hil/test/test_hil_bounded.py`

**Interfaces:**
- Consumes: `write_report`/`render_report` from Tasks 2–3.
- Produces: `_abandon_exit(pool, mgr, abandoned, err_count, report_dir: Path | None = None)` — the parameter becomes the **directory**, not the markdown path.

- [ ] **Step 1: Write the failing test**

```python
class AbandonNoticeLandsInBothArtifacts(unittest.TestCase):
    def test_abandon_sets_the_caveat_not_just_the_markdown(self):
        import json
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        rd = Path(td.name)
        hil_test.accumulate_report(
            [('boardA', 0, 0, [('boardA', {'cdc_msc': 'OK'}, '1s')], 0)], rd, True, '', '')
        hil_test.mark_report_abandoned(rd, 'the worker pool would not shut down.')
        doc = json.loads((rd / 'hil_report.json').read_text())
        self.assertIn('abandoned', doc['caveat'])
        self.assertEqual(len(doc['rows']), 1, 'the finished board must survive')
        md = (rd / 'hil_report.md').read_text()
        self.assertLess(md.index('abandoned'), md.index('boardA'))

    def test_marking_a_missing_report_is_a_no_op(self):
        td = TemporaryDirectory()
        self.addCleanup(td.cleanup)
        hil_test.mark_report_abandoned(Path(td.name), 'x')   # must not raise
```

- [ ] **Step 2: Run it to verify it fails**

Run: `python3 test/hil/test/test_hil_bounded.py AbandonNoticeLandsInBothArtifacts`
Expected: FAIL — `AttributeError: module 'hil_test' has no attribute 'mark_report_abandoned'`

- [ ] **Step 3: Implement it**

```python
def mark_report_abandoned(report_dir: Path, why: str) -> None:
    """Stamp an existing report as abandoned, in BOTH artifacts.

    Best-effort and silent: this runs while the interpreter is being torn down, and an
    exception here hangs the process in multiprocessing's unbounded join()."""
    try:
        jpath = report_dir / REPORT_JSON
        doc = json.loads(jpath.read_text()) if jpath.is_file() else None
        if doc is None:
            return
        doc['caveat'] = (f'**HIL run abandoned: {why}** The table below is this run\'s '
                         f'partial result.\n')
        write_report(report_dir, doc)
    except (OSError, ValueError, TypeError):
        pass
```

Then in `_abandon_exit`, replace the read-modify-write of the markdown with `mark_report_abandoned(report, ...)` and change the call site at `:2603` from `report_dir / REPORT_MD` to `report_dir`.

- [ ] **Step 4: Run the tests**

Run: `python3 -m unittest discover -s test/hil/test`
Expected: 126 OK.

- [ ] **Step 5: Commit**

```bash
git add test/hil/hil_test.py test/hil/test/test_hil_bounded.py
git commit -m "hil_test: stamp abandonment into the document, not onto the markdown

_abandon_exit did a text prepend on a file it had not written, so the caveat
never reached the JSON and an agent reading the sidecar saw a clean partial
report under a red job. Still best-effort and still silent: it runs while the
interpreter is being torn down."
```

---

### Task 5: Prove the two artifacts cannot disagree

**Files:**
- Test: `test/hil/test/test_hil_bounded.py`

- [ ] **Step 1: Write the test**

```python
class MarkdownIsAlwaysARenderingOfTheJson(unittest.TestCase):
    """The property this whole change buys: whatever wrote the report, re-rendering the
    sidecar reproduces the markdown byte for byte."""

    def _check(self, rd):
        import json
        doc = json.loads((rd / 'hil_report.json').read_text())
        self.assertEqual((rd / 'hil_report.md').read_text(),
                         hil_test.render_report(doc) + '\n')

    def test_normal_path(self):
        td = TemporaryDirectory(); self.addCleanup(td.cleanup); rd = Path(td.name)
        hil_test.accumulate_report(
            [('boardA', 0, 0, [('boardA', {'cdc_msc': 'OK'}, '1s')], 0)], rd, True,
            '-b boardA', '> **Rig note.** x\n')
        self._check(rd)

    def test_after_an_accumulate_rerun(self):
        td = TemporaryDirectory(); self.addCleanup(td.cleanup); rd = Path(td.name)
        hil_test.accumulate_report(
            [('boardA', 0, 0, [('boardA', {'cdc_msc': 'OK'}, '1s')], 0)], rd, True, '', '')
        hil_test.accumulate_report(
            [('boardB', 0, 0, [('boardB', {'cdc_msc': 'OK'}, '1s')], 0)], rd, False, '', '')
        self._check(rd)

    def test_after_abandonment(self):
        td = TemporaryDirectory(); self.addCleanup(td.cleanup); rd = Path(td.name)
        hil_test.accumulate_report(
            [('boardA', 0, 0, [('boardA', {'cdc_msc': 'OK'}, '1s')], 0)], rd, True, '', '')
        hil_test.mark_report_abandoned(rd, 'the worker pool would not shut down.')
        self._check(rd)

    def test_no_boards_exit(self):
        td = TemporaryDirectory(); self.addCleanup(td.cleanup); rd = Path(td.name)
        hil_test.write_report(rd, {'rows': [], 'banner': '', 'scope': '',
                                   'caveat': '**HIL run selected no boards.** why\n'})
        self._check(rd)
```

- [ ] **Step 2: Run it**

Run: `python3 test/hil/test/test_hil_bounded.py MarkdownIsAlwaysARenderingOfTheJson`
Expected: PASS on all four. A failure here means a writer still bypasses `render_report`.

- [ ] **Step 3: Full gate and commit**

```bash
python3 -m unittest discover -s test/hil/test        # 130 OK
pre-commit run --files test/hil/hil_test.py test/hil/helper/hil_health.py \
                       test/hil/test/test_hil_bounded.py test/hil/test/test_hil_health.py
git add test/hil/test/test_hil_bounded.py
git commit -m "test/hil: pin that the markdown is always a rendering of the sidecar

Four writers, one renderer. This is the invariant the change exists to create,
so it is asserted directly rather than inferred from the writers."
```

---

## Out of scope

Deliberately not included, each its own follow-up:

- **The flat `HIL_POOL_TIMEOUT`.** `hil_test.py:225` is a per-process 3600 s guard that does not scale with board count. It was per board when runs were serial; the origin branch made one run cover the fleet, so a 27-board run shares one budget. Real, and a scheduling change rather than a reporting one.
- **`hil_ci.sh` accumulate in remote mode.** `hil_ci.sh:183` `rm -rf`s `REMOTE_DIR` every run and the copies are one-way, so a remote `--accumulate` retry has no merge base and its one-row report overwrites the local full-fleet one. Fixing that means uploading `hil_report.json` and `<config>.failed` before the run, or keeping `REMOTE_DIR` when `--accumulate` is present.
- **Dropping `hil_report.md` entirely.** Not proposed. It is the PR artifact and what the `hil` skill tells operators to paste; this plan makes it generated, not redundant.
