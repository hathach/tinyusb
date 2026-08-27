# Give the HIL worker result a name

**Origin:** split out of PR #3840 (making `hil_report.md` a rendering of `hil_report.json`).
Delete this file when its own PR lands.

> **SUPERSEDED IN PART (2026-08-26).** Written against a 7-field tuple whose index 5 was
> `blind`. The sysfs blindness subsystem is gone: `test_board` now returns **6** fields with
> `stray` at index 5, and its board-locked early return is 5 wide. The problem described
> below is unchanged and still worth fixing — three producers, three widths, and
> `len(r) > 5 and r[5]` reads a WRONG SLOT rather than raising. But drop the `blind` field
> from the proposed NamedTuple and re-derive every index from `hil_test.test_board` before
> executing, or `_stray_note` starts reading a duration as a stray count.
> `StrayNoteSurvivesTheTupleWidth` pins the current shape.

## What is established

`test_board()` returns a bare tuple that three producers build and fourteen call sites read
positionally. It has grown 5 → 6 → 7 fields, and the code already works around its own
shape:

```python
hil_test.py:1992   dirty = [(r[0], r[6]) for r in mret if len(r) > 6 and r[6]]
hil_test.py:2014   blind = [r[0] for r in mret if len(r) > 5 and r[5]]
hil_test.py:2386   for name, _, _, _, dur, *_ in mret:
hil_report.py:306  for name, _, _, rows, *_ in mret:
```

Two facts make this worth closing rather than tolerating:

- **The declared type is already wrong.** `hil_test.py:1711` says
  `tuple[str, int, list[str], list, float]` — five fields — while the main return at `:1872`
  yields seven (`+ sysfs_blind(), stray`).
- **A wrong slot is a wrong verdict, not a crash.** Field 5 is `blind`, which decides whether
  a board's red cells are reported as broken hardware or as "could not tell". Inserting a
  field mid-tuple makes `r[5]` read the wrong slot and keep running.

It has bitten once already: `test_hil_bounded.py`'s
`test_both_row_widths_survive_the_report_writers` exists because the blindness flag widened
the tuple to 6 while the pool-timeout path still synthesised 5-field rows, and *"a
fixed-width unpack in either one raises INSIDE the containment path, which is where a raise
costs every board's results."* That is why the unpacks end in `*_`.

## What remains

A `NamedTuple` with defaults. Verified to pickle across the pool boundary and to stay
fully tuple-compatible — existing `r[0]`, `e[1]`, `for name, _, _, rows, *_` and `len(r)`
all keep working, so it lands without touching the fourteen consumers:

```python
class BoardResult(NamedTuple):
    """What one worker returns. Field ORDER is load-bearing: it is unpacked positionally
    in a dozen places, and the pool-timeout path synthesises one by hand."""
    name: str
    err_count: int
    failed_tests: list[str]
    rows: list | None          # None from the pool-timeout synthesis, never []
    duration: float
    blind: bool = False        # defaults, so a synthesised result is full-width
    stray: int = 0
```

Then a second, smaller step removes the coupling itself: `accumulate_report` takes
`[(name, rows)]` pairs instead of `mret`, and `hil_test` does the extraction because it owns
the shape. One line at each end; the subtle merge logic — stale lock clearing,
`BOUNDARY_CELL`, `duration=None` preservation — is untouched.

## Sizing

| | Sites |
|---|---|
| Producers to convert | 4 (`hil_test.py:1724`, `:1872`, `:2283`, `:2327`) |
| Arity guards deleted | 2 (`:1992`, `:2014`) |
| Wrong annotation fixed | 1 (`:1711`) |
| `hil_report`'s coupled line | 1 (`:306`) |
| Positional consumers (optional migration) | 14 |
| **Test fixtures building tuples by hand** | **34** |

Production code is roughly ten changed lines. **The work is dominated by the test
fixtures**, which is also the risk.

## Do this first, or the refactor is unverifiable

`test_hil_report.py` (27 sites) and `test_hil_bounded.py` (7) construct plain tuples by
hand — `('boardA', 0, [], [], 1.0, True)`. A producer that forgot to switch to
`BoardResult`, or a pickling regression, **passes the entire 310-test suite** and surfaces
only on the rig. Convert the fixtures to build `BoardResult` as task 1, before touching any
producer. This ordering is not optional.

Second trap: `rows` is `None` on the pool-timeout path (`hil_test.py:2283`), never `[]`, and
`accumulate_report` guards with `if rows and ...`. A well-meaning `rows: list = []` default
silently changes that path. Pin it with a test before the conversion.

## Why it was split out

PR #3840 touches the report document. This touches `test_board`'s return and the containment
paths, where a raise costs every board's results rather than one board's — a different blast
radius, needing its own review and its own rig run. #3840 is twice-reviewed and dogfooded
ten times on hardware; folding this in would reset that surface for a latent-trap cleanup
that is not causing bugs today.
