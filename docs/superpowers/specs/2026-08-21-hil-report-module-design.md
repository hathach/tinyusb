# hil_report.py: one owner for the HIL report document

**Date:** 2026-08-21
**Branch:** `hil-report` (continues the report-unification work already on it)

## Motivation

`hil_report.json` and `hil_report.md` are now one document rendered two ways, but the code that
produces, renders, merges and reads that document is spread across three modules:

| Module | Report-related content |
|---|---|
| `hil_test.py` | `REPORT_CELL`, `BOUNDARY_CELL`, `REPORT_MD`, `REPORT_JSON`, `render_matrix`, `render_report`, `write_report`, `mark_report_abandoned`, `accumulate_report` |
| `helper/hil_health.py` | `write_timeout_report` — composes its own markdown |
| `helper/hil_summary.py` | `cell_state`, `variants_of`, `summarize`, CLI |

Two concrete defects follow from that spread.

**One classifier, two copies.** `hil_test.py:1966` (`cell_kind`, keyed off `REPORT_CELL`) and
`hil_summary.py:34` (`cell_state`, with its own re-typed `FAIL_ICON, SKIP_ICON = '❌', '⚪'`)
implement the same rule. The latter's docstring says it is *"the EXACT classifier hil_test.py's own
tally uses"* — the duplication was noticed and documented as an obligation to keep in sync, rather
than removed. Change `REPORT_CELL` and the human's table and the agent's verdict silently disagree:
the markdown says ❌ where the JSON says `pass`. That is the same class of defect this branch
exists to eliminate, one layer up.

**A writer that cannot render.** `hil_test.py` imports `hil_health`, so `hil_health` cannot import
`hil_test` back. That is the only reason `write_timeout_report` composes its own markdown instead of
calling `render_report`, and the only reason the pool-guard fallback is held to a weaker promise
(same boards and caveat in both artifacts, not byte-identical) while the other four writers are
exact. The constraint is structural, not essential: a leaf module both can import dissolves it.

## Goal / non-goals

**Goal:** `test/hil/helper/hil_report.py` becomes the single owner of the report document.

**This is NOT purely code motion, and the distinction matters for review.** Measured against
`master`, `hil_test.py` contains only `render_matrix` and `accumulate_report`. Everything else in
the new module — `render_report`, `write_report`, `mark_report_abandoned`, `mark_report_no_boards`,
`_load`, `_write_stuck_over_prior_md`, `cell_state`, and the `scope`/`caveat` plumbing — is NEW
code, roughly 150 lines of it, and two rounds of review found most of their defects there. Read
those functions as new, not as relocated. `hil_test.py`'s CLI, arguments and table format do stay
unchanged.

**Deliberate user-visible changes:**
1. `hil_summary.py` is deleted; its CLI moves to `hil_report.py`. The documented command becomes
   `python3 test/hil/helper/hil_report.py <config> -b BOARD [-b BOARD…]`.
2. `write_timeout_report` re-renders from the merged sidecar instead of stapling its banner above
   the previous attempt's markdown text. Output improves — one table containing the stuck boards,
   rather than a fresh banner above a duplicate table — but it is a change (see Testing).

**Non-goals (explicit follow-ups, not this change):**
- Splitting `accumulate_report`'s `mret` folding from its merge (see "Deliberate wart").
- The flat `HIL_POOL_TIMEOUT` that does not scale with board count (`hil_test.py:225`).

## Resulting layout (`test/hil/`)

| File | ~Lines | Role |
|---|---|---|
| `hil_test.py` | 2390 (−250) | tests + orchestration + CLI |
| `helper/hil_report.py` (new) | ~400 | the report document: vocabulary, render, write, merge, fold, CLI |
| `helper/hil_health.py` | ~345 (−53) | killing wedged processes only |
| `helper/hil_summary.py` | deleted | superseded by `hil_report.py` |

Import graph: `hil_health` is a leaf; `hil_report` → `hil_health` (for `_p`, the
BrokenPipeError-safe print used on containment paths); `hil_test` → both. No cycles.

## hil_report.py

Stdlib only (`json`, `argparse`, `pathlib`) beyond that one `_p` import. Sections, in order:

**Vocabulary.** `REPORT_MD`, `REPORT_JSON`, `REPORT_CELL`, `BOUNDARY_CELL`, `LOCKED_CELL`.
`REPORT_CELL` becomes the single source of the status icons; `hil_summary.py`'s `FAIL_ICON`/
`SKIP_ICON` literals are deleted.

**Classifier.** One `cell_state(v) -> 'pass' | 'fail' | 'skip'`, replacing both `cell_kind` and the
old `cell_state`. Keeps the surviving docstring's warning that the `pass` arm is load-bearing: a
passing test may return an unprefixed metric string (`'480.0 MBps'`), while failures are guaranteed
icon-marked, so classifying unknown shapes as `fail` would publish a green table as a red verdict.

**Render.** `render_matrix(rows_all)`, `render_report(doc)`. Unchanged; `render_matrix`'s inline
`cell_kind` is replaced by a call to the module-level `cell_state`.

**Write.** `write_report`, `accumulate_report`, `mark_report_abandoned`, `write_timeout_report`.
Moved verbatim except `write_timeout_report`, which loses its `md_name` parameter (the module owns
`REPORT_MD`) and renders instead of concatenating.

**Fold.** `variants_of`, `summarize`, and the `main()` CLI from `hil_summary.py`.

## Deliberate wart

`accumulate_report` moves wholesale, keeping its knowledge of `mret`'s worker-result tuple shape.
The cleaner boundary would split "fold `mret` → rows" (`hil_test`'s domain) from "merge rows → doc"
(`hil_report`'s), but that rewrites subtle, well-tested logic — stale `board-locked` clearing,
`BOUNDARY_CELL` dropping, `duration=None` preservation — for a tidier seam. It is a data-shape
coupling, not an import cycle. Moving it verbatim keeps the motion reviewable as motion.

## The sharp edge

`hil_ci.sh:222-228` stages helper modules by an **explicit scp list**. A new `helper/hil_report.py`
that is not added there reaches the rig missing, and the run dies with `ImportError` *after*
`REMOTE_DIR` has already been wiped — so the previous run's report and re-run spec are gone too.

This is already guarded: `test_hil_bounded.py`'s `RemoteStaging.test_import_closure_is_staged_to_the_rig`
walks the AST import closure from `hil_test.py`, `usbtest.py` and `mtp_test.py` and requires an exact
scp entry for each file. Adding the module to the list is all this change needs; no new guard is
warranted, and an earlier draft of this document wrongly claimed none existed.

## Consumers to update

| File | Change |
|---|---|
| `test/hil/hil_ci.sh:226` | `hil_summary.py` → `hil_report.py` in the scp list |
| `.claude/agents/hil-operator.md:71` | the documented command |
| `.claude/workflows/hil-validate.js:58` | the command the operator is told to run |
| `.claude/workflows/hil-validate.js:14,17,54,67`, `test-hil-validate.mjs:7` | stale `hil_summary.py` mentions in comments |

No logic in the `.claude` files changes — the operator's return contract
(`{results, banner, wedged}`) is untouched.

## Testing

New `test/hil/test/test_hil_report.py`. The report-specific classes move there from
`test_hil_bounded.py` (`CaveatSurvivesAccumulate`, `SummaryFoldsReportToBoards`,
`ScopeSurvivesInTheJson`, `RenderReportIsPureFunctionOfTheDocument`,
`EveryExitPathLeavesBothArtifacts`, `AbandonNoticeLandsInBothArtifacts`,
`MarkdownIsAlwaysARenderingOfTheJson`) and from `test_hil_health.py` (`WriteTimeoutReport`).

Three test changes are substantive rather than mechanical:

1. `WriteTimeoutReport.test_keeps_a_previous_attempts_table` asserts the prior **markdown text**
   survives. It becomes an assertion that the prior attempt's **rows** survive — the same guarantee
   against the new representation.
2. `MarkdownIsAlwaysARenderingOfTheJson` gains a fifth case for the pool-guard fallback, which now
   satisfies the byte-identical invariant like the other four.
3. `test_the_pool_guard_fallback_agrees_even_if_it_does_not_render` — the weaker promise — is
   deleted, because the promise it encoded no longer applies.

Gate: `python3 -m unittest discover -s test/hil/test` at 275 — the current 266, minus the one
deleted test, plus the fifth invariant case, the scp-list guard, two dual-mode import tests,
five classifier tests and one pinning that the old entry point is gone — then
`pre-commit run --all-files`. Because this lands on a
branch already validated on hardware, it closes with a rig re-check: the invariant check against a
real report pair and a scoped `--accumulate` run, not the full fleet.
