# usbtest Recovery Reserve Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the post-hang recovery reserve a derived, asserted property instead of an
accident of four independently-set constants.

**Architecture:** `hil_test` passes `--budget` and `--outer-timeout` to `usbtest.py`, which
decides at runtime whether a recovery still fits. Today the reserve survives only because
the four numbers happen to line up; nothing ties them together or fails when they stop.

**Tech Stack:** Python 3.13 stdlib.

## Global Constraints

- `usbtest.py`: `RECOVER_FLASH_TIMEOUT = 90`, `RECOVER_RESET_TIMEOUT = 30`.
- `hil_test.py`: `USBTEST_BATTERY_BUDGET = 260`, `USBTEST_RECOVERY_BUDGET = 250`,
  `USBTEST_OVERSHOOT = 120`; `outer = BATTERY_BUDGET + (RECOVERY_BUDGET if recovery else
  OVERSHOOT)`, used for both the child's `--outer-timeout` and the parent's `run_cmd` bound.
- All five are env-overridable via `hil_util.pos_int_env`, so a rig can change them.
- Tests: `cd test/hil && python3 test/test_hil_health.py` and `test_hil_bounded.py`.

## What is already established

The reserve holds at the shipped values, checked by hand:

- The battery checks its budget BEFORE dispatching a case, so it can overshoot by one
  case — worst case `260 + 60 + 5 = 325 s`.
- Recovery is gated on `_time_left() >= RECOVER_RESET_TIMEOUT`, where
  `_time_left() = outer_timeout - elapsed - 35`; with `outer = 510` that allows recovery
  until `elapsed = 445 s`, and the reflash until `385 s`.
- So ~60 s of margin survives, and recovery does fire.

**The defect is structural, not arithmetic:** lower `--outer-timeout`, raise `--timeout`, or
raise `USBTEST_BATTERY_BUDGET` via the env and the reserve silently disappears. The failure
mode is a skipped reflash that leaves the D-state holder for the next job — the exact thing
the containment exists to prevent — with no error anywhere.

**Why this is a separate PR:** it changes the timing contract between `hil_test` and
`usbtest.py`, which affects every board's run duration, so it wants its own review and a
full rig run.

## File Structure

- `test/hil/usbtest.py` — a `reserve_ok()` predicate plus a startup assertion.
- `test/hil/hil_test.py` — derive the battery budget from the outer bound rather than
  setting both independently.
- `test/hil/test/test_hil_health.py` — tests.

---

### Task 1: Assert the reserve at startup

**Files:**
- Modify: `test/hil/usbtest.py` (constants block, and `main()` after argparse)
- Test: `test/hil/test/test_hil_health.py`

**Interfaces:**
- Produces: `usbtest.reserve_ok(budget, outer, case_timeout)` returning bool.

- [ ] **Step 1: Write the failing test**

```python
class RecoveryReserveIsChecked(unittest.TestCase):
    """The battery may overshoot its budget by ONE already-started case, so the outer bound
    must leave room for that overshoot AND a bounded recovery afterwards."""

    def setUp(self):
        import usbtest
        self.u = usbtest

    def test_the_shipped_numbers_leave_room(self):
        self.assertTrue(self.u.reserve_ok(budget=260, outer=510, case_timeout=60))

    def test_a_tighter_outer_bound_is_rejected(self):
        self.assertFalse(self.u.reserve_ok(budget=260, outer=380, case_timeout=60))

    def test_a_longer_case_timeout_is_rejected(self):
        self.assertFalse(self.u.reserve_ok(budget=260, outer=510, case_timeout=200))
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd test/hil && python3 test/test_hil_health.py RecoveryReserveIsChecked -v`
Expected: FAIL — `module 'usbtest' has no attribute 'reserve_ok'`

- [ ] **Step 3: Write minimal implementation**

```python
def reserve_ok(budget: int, outer: int, case_timeout: int) -> bool:
    """Does `outer` leave room for the battery's worst case AND a bounded recovery?

    The budget is checked BEFORE dispatch, so the battery can run to
    `budget + case_timeout + 5` (the +5 is run_case's reap). _time_left() subtracts a
    further 35 s of fixed tail. A reflash needs RECOVER_FLASH_TIMEOUT beyond that.
    """
    worst_case_end = budget + case_timeout + 5
    return outer - worst_case_end - 35 >= RECOVER_FLASH_TIMEOUT
```

In `main()`, after parsing args:

```python
    if args.budget and args.outer_timeout and not reserve_ok(
            args.budget, args.outer_timeout, args.timeout):
        print(f'warning: --outer-timeout {args.outer_timeout} leaves no room for a bounded '
              f'recovery after a --budget {args.budget} battery with --timeout '
              f'{args.timeout} cases; a HUNG board will be left wedged', file=sys.stderr)
```

Warn, do not exit: a caller that deliberately runs without recovery is legitimate.

- [ ] **Step 4: Run test to verify it passes**

Run: `cd test/hil && python3 test/test_hil_health.py RecoveryReserveIsChecked -v`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add test/hil/usbtest.py test/hil/test/test_hil_health.py
git commit -m "usbtest: check the recovery reserve instead of assuming it"
```

---

### Task 2: Derive the outer bound from one place

**Files:**
- Modify: `test/hil/hil_test.py` (constants block ~line 227, and `test_device_usbtest`)
- Test: `test/hil/test/test_hil_bounded.py`

**Interfaces:**
- Consumes: `usbtest.reserve_ok` semantics (duplicate the arithmetic, do not import
  usbtest — `hil_test` must not import it).
- Produces: an assertion at module import that the shipped constants satisfy the reserve.

- [ ] **Step 1: Write the failing test**

```python
    def test_the_shipped_constants_satisfy_the_reserve(self):
        """Whatever the env overrides, the pair hil_test computes must leave recovery room:
        outer - (budget + case_timeout + 5) - 35 >= 90."""
        outer = hil_test.USBTEST_BATTERY_BUDGET + hil_test.USBTEST_RECOVERY_BUDGET
        self.assertGreaterEqual(outer - (hil_test.USBTEST_BATTERY_BUDGET + 60 + 5) - 35, 90)
```

- [ ] **Step 2: Run test to verify it fails**

Temporarily set `HIL_USBTEST_RECOVERY_BUDGET=100` and run; expect FAIL. Unset.

- [ ] **Step 3: Add the guard**

```python
# The recovery reserve is a PROPERTY of these two, not a coincidence: the battery may
# overshoot its budget by one already-started case (checked before dispatch), and a bounded
# reflash needs 90 s after a 35 s fixed tail. Env overrides make this checkable at import
# rather than discoverable when a wedge is left unrecovered.
if USBTEST_RECOVERY_BUDGET - 60 - 5 - 35 < 90:
    print(f'warning: HIL_USBTEST_RECOVERY_BUDGET={USBTEST_RECOVERY_BUDGET} leaves no room '
          f'for a bounded reflash after a one-case overshoot; HUNG boards will stay wedged',
          file=sys.stderr)
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `cd test/hil && python3 test/test_hil_bounded.py -v`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add test/hil/hil_test.py test/hil/test/test_hil_bounded.py
git commit -m "hil: warn when the timeout constants leave no recovery reserve"
```
