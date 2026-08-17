# Blindness Reporting Gaps Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make a HIL worker's sysfs blindness reach the report in the two cases where it
currently does not — an untested producer, and a board that raises.

**Architecture:** A worker returns `hil_util.sysfs_blind()` as the last field of its result
tuple; `_blind_note()` turns that into a report banner. Two holes: nothing tests the
producer, and a board that raises returns no tuple at all, so its blindness is lost.

**Tech Stack:** Python 3.13 stdlib, multiprocessing Pool with `maxtasksperchild=1`.

## Global Constraints

- A blind worker answers `SYSFS_UNKNOWN` for every attribute, so its "device not found"
  means "could not tell". The report must say so or a red cell reads as a broken board.
- `maxtasksperchild=1`: one worker per board, so the flag is per-board and must not be
  smeared across boards.
- Tests: `cd test/hil && python3 test/test_hil_bounded.py`.

## What is already established

- `hil_test.test_board` returns `(..., hil_util.sysfs_blind(), stray)`; `_blind_note(mret)`
  renders the banner; wired into all three report paths.
- **The producer is provably untested**: replacing `hil_util.sysfs_blind()` with `False` in
  the return leaves all tests green. Nothing drives `test_board` — it needs a board dict, a
  real flock, a flasher and `test_example` per test.
- Blindness fired for real on ci.lan: four workers went blind in one run, and cells failed
  *because* of it (`Printer device not found ... (this worker is blind)`).

**Why this is a separate PR:** closing it means making `test_board` testable, which is a
refactor of the harness's orchestration layer — a different scope from the containment
work, and the reason the gap was accepted rather than papered over.

## File Structure

- `test/hil/hil_test.py` — extract the result-tuple assembly from `test_board` so it can be
  built and asserted without running a board; carry blindness out of the raise path.
- `test/hil/test/test_hil_bounded.py` — tests for both.

---

### Task 1: Make the result tuple assembly testable

**Files:**
- Modify: `test/hil/hil_test.py` (`test_board`, the `return (name, err_count, ...)` at the
  end of the try block)
- Test: `test/hil/test/test_hil_bounded.py`

**Interfaces:**
- Produces: `_board_result(name, err_count, failed_tests, rows, t_total, board_wide_fail)`
  returning the 7-tuple `(name, err_count, failed, rows, t_total, blind, stray)`, reading
  `hil_util.sysfs_blind()` and `hil_health.kill_own_children()` itself.

- [ ] **Step 1: Write the failing test**

```python
class BoardResultCarriesBlindness(unittest.TestCase):
    def test_a_blind_worker_reports_it(self):
        from helper import hil_util, hil_health
        self.addCleanup(setattr, hil_util, 'sysfs_blind', hil_util.sysfs_blind)
        self.addCleanup(setattr, hil_health, 'kill_own_children', hil_health.kill_own_children)
        hil_util.sysfs_blind = lambda: True
        hil_health.kill_own_children = lambda: 0
        row = hil_test._board_result('b', 0, [], [], 1.0, False)
        self.assertTrue(row[5], 'blindness did not reach the result tuple')
        self.assertIn('b', hil_test._blind_note([row]))

    def test_a_sighted_worker_does_not(self):
        from helper import hil_util, hil_health
        self.addCleanup(setattr, hil_util, 'sysfs_blind', hil_util.sysfs_blind)
        self.addCleanup(setattr, hil_health, 'kill_own_children', hil_health.kill_own_children)
        hil_util.sysfs_blind = lambda: False
        hil_health.kill_own_children = lambda: 0
        row = hil_test._board_result('b', 0, [], [], 1.0, False)
        self.assertFalse(row[5])
        self.assertEqual(hil_test._blind_note([row]), '')
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd test/hil && python3 test/test_hil_bounded.py BoardResultCarriesBlindness -v`
Expected: FAIL — `module 'hil_test' has no attribute '_board_result'`

- [ ] **Step 3: Write minimal implementation**

```python
def _board_result(name, err_count, failed_tests, rows, t_total, board_wide_fail):
    """Assemble a worker's result tuple. Separate from test_board so the two fields only
    the WORKER can answer -- its process-global blindness latch and what it could not kill
    -- are testable without running a board."""
    stray = hil_health.kill_own_children()
    return (name, err_count, [] if board_wide_fail else sorted(set(failed_tests)),
            rows, t_total, hil_util.sysfs_blind(), stray)
```

Replace the tail of `test_board` with:

```python
        return _board_result(name, err_count, failed_tests, rows, t_total, board_wide_fail)
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd test/hil && python3 test/test_hil_bounded.py -v`
Expected: PASS, and the existing `BlindWorkerReachesTheReport` tests still pass.

- [ ] **Step 5: Verify the mutation is now caught**

Replace `hil_util.sysfs_blind()` with `False` inside `_board_result` and re-run; the suite
MUST fail. Restore it.

- [ ] **Step 6: Commit**

```bash
git add test/hil/hil_test.py test/hil/test/test_hil_bounded.py
git commit -m "test/hil: make the worker result tuple testable, covering blindness"
```

---

### Task 2: Carry blindness out of the worker-raise path

**Files:**
- Modify: `test/hil/hil_test.py` (`test_board`'s except/finally, and `main`'s worker-raise
  handler that builds synthetic rows)
- Test: `test/hil/test/test_hil_bounded.py`

**Interfaces:**
- Consumes: `_board_result` from Task 1.
- Produces: a board that raises still contributes a row whose blindness field is accurate.

- [ ] **Step 1: Write the failing test**

```python
    def test_a_board_that_raises_still_reports_blindness(self):
        """The result tuple is returned inside a try whose finally only releases the lock,
        so a board that dies by exception contributed nothing -- and its blindness, the
        thing that most explains its failure, was lost with it."""
        from helper import hil_util
        self.addCleanup(setattr, hil_util, 'sysfs_blind', hil_util.sysfs_blind)
        hil_util.sysfs_blind = lambda: True
        row = hil_test._board_result_on_error('b', RuntimeError('boom'))
        self.assertTrue(row[5])
        self.assertIn('b', hil_test._blind_note([row]))
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd test/hil && python3 test/test_hil_bounded.py BoardResultCarriesBlindness -v`
Expected: FAIL — no `_board_result_on_error`

- [ ] **Step 3: Write minimal implementation**

```python
def _board_result_on_error(name, exc):
    """A row for a board that died by exception. err_count 1, no per-test detail, but the
    blindness and stray fields are still accurate -- they explain the failure more often
    than the exception text does."""
    rows = [(name, {BOUNDARY_CELL: f'{REPORT_CELL["fail"]} {type(exc).__name__}'}, None)]
    return _board_result(name, 1, [], rows, 0.0, True)
```

Wrap the body of `test_board` so the exception path returns it instead of propagating.

- [ ] **Step 4: Run test to verify it passes**

Run: `cd test/hil && python3 test/test_hil_bounded.py -v`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add test/hil/hil_test.py test/hil/test/test_hil_bounded.py
git commit -m "test/hil: keep a raising board's blindness in the report"
```

---

## Caution

`test_board`'s `finally` releases the board flock. Any restructuring MUST keep that
release on every path, including the new error path — a leaked flock locks the board until
the host reboots.
