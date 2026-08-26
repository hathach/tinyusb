# Remove the Unread `build_filtered`/`BUILD_FILTERED` Plumbing

> rename to pr<NNN>-dead-build-filtered.md when the PR opens

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Delete the `build_filtered` output in `.github/workflows/build.yml`, the
`BUILD_FILTERED`/`build-filtered` pipeline-parameter sentinel plumbing in
`.circleci/config.yml`/`config2.yml`, and the test coverage that exists solely to pin that
plumbing — all of it computed but never consumed by anything downstream.

**Architecture:** Pure subtraction, mirrored across both CI backends: GHA's `set-matrix` job
stops computing/emitting a `build_filtered` output, and CircleCI's `set-matrix` job stops
computing `BUILD_FILTERED` and rewriting it into `config2.yml`'s `build-filtered` pipeline
parameter (which it currently declares but never reads). `test_ci_metrics.py`'s
`TestWorkflowSelectionHandOff` loses the assertions that exist only to pin this dead value.

**Tech Stack:** GitHub Actions YAML + bash, CircleCI YAML + bash + inline Python, Python 3
stdlib `unittest` (`test/hil/test/test_ci_metrics.py`).

**Spec:** none — split out of the rework-metrics branch's final-review fix wave, kept in place
there to keep that PR's diff minimal.

**Origin:** split out of the `rework-metrics` branch. Delete this file when its own PR lands.

## What is already established

- **`build_filtered` (GHA) is computed and emitted, but nothing reads it.**
  `.github/workflows/build.yml:55` declares the job output
  (`build_filtered: ${{ steps.set-matrix-json.outputs.build_filtered }}`); lines `169-195`
  compute it (`EXAMPLE_MAP`/`BUILD_FILTERED` derived from the PR selection file) and emit it
  (`echo "build_filtered=$BUILD_FILTERED" >> $GITHUB_OUTPUT` at line 195). Grepping the whole
  `.github/` tree for `needs.set-matrix.outputs.build_filtered` (or any other read of this
  output) turns up nothing — every other `set-matrix` output (`json`, `hil_json`,
  `example_map`, the `hil_args_*`/`hil_run_*` pairs) is consumed by a later job; this one is
  not.
- **`BUILD_FILTERED`/`build-filtered` (CircleCI) is the same dead value on the other backend.**
  `.circleci/config.yml:59-60,62-63` computes `BUILD_FILTERED` from the same selection file;
  `:77-96` (the inline Python heredoc plus its `EXAMPLE_MAP`/`BUILD_FILTERED` env hand-off and
  failure fallback) rewrites it into `.circleci/config2.yml`'s `build-filtered` pipeline
  parameter via the sentinel-comment rewrite (`# build-filtered-default: rewritten in-place by
  config.yml set-matrix`, `config2.yml:7-9`). `config2.yml` never references
  `<< pipeline.parameters.build-filtered >>` anywhere — only `<< pipeline.parameters.example-map
  >>` is read (`config2.yml:121`, the `EXAMPLE_MAP` env line for the build job).
- **Real test coverage exists for this dead value and will need to move with it.**
  `test/hil/test/test_ci_metrics.py`'s `TestWorkflowSelectionHandOff`:
  - `_run_extras_block()` (`:284-311`) extracts and executes the GHA shell block bounded by
    `EXAMPLE_MAP='{}'\n          BUILD_FILTERED='false'` through the `echo "matrix=$MATRIX_JSON"`
    line, and returns `(legs, filtered)` — `filtered` is `BUILD_FILTERED`'s value.
  - `test_an_empty_family_list_is_not_treated_as_unusable` (`:316-327`) asserts
    `filtered == 'false'` for an empty-but-valid family selection.
  - `test_a_real_family_list_stays_scoped` (`:328-332`) asserts `filtered == 'true'` for a
    real scoped selection.
  These are legitimate regression tests for a computation that used to matter (or was meant
  to feed something that was never built) — they need to be dropped or rewritten to stop
  asserting on a value that no longer exists, not just left to bit-rot against deleted code.

## What remains (not started)

### Task 1: Remove the GHA side

**Files:** `.github/workflows/build.yml`

- [ ] Delete the `build_filtered:` line from the `set-matrix` job's `outputs:` block
      (currently line 55).
- [ ] In the `Generate matrix json` step, delete the `BUILD_FILTERED` computation
      (currently lines 175, 178-189: the `BUILD_FILTERED='false'` init, the `jq -r` derivation,
      and the `FAMILY_COUNT`/nothing-selected special case that only exists to correct
      `BUILD_FILTERED`) and the emission line (`echo "build_filtered=$BUILD_FILTERED" >>
      $GITHUB_OUTPUT`, currently line 195). Keep `EXAMPLE_MAP` and its own emission — that one
      **is** read (`example-map` input to `build_util.yml`, `cmake` job).
- [ ] Re-read the surrounding comments (`:169-173`, `:182-186`) — they explain
      `EXAMPLE_MAP`/`BUILD_FILTERED` together; trim references to `BUILD_FILTERED` without
      breaking the remaining explanation of `EXAMPLE_MAP`'s own fall-open behavior.
- [ ] Validate: `python3 -c "import yaml; yaml.safe_load(open('.github/workflows/build.yml'))"`

### Task 2: Remove the CircleCI side

**Files:** `.circleci/config.yml`, `.circleci/config2.yml`

- [ ] In `.circleci/config.yml`'s `set-matrix` job: delete the `BUILD_FILTERED='false'` init
      (currently `:60`, adjacent to `EXAMPLE_MAP='{}'` at `:59`) and its `jq -r` derivation
      (currently `:63`, adjacent to `EXAMPLE_MAP`'s own derivation at `:62`).
  - [ ] Delete the `('BUILD_FILTERED', 'build-filtered-default')` tuple from the
      `for env, tag in (...)` sentinel-rewrite loop (currently `:84`, inside the inline Python
      heredoc starting `:77`), leaving only the `EXAMPLE_MAP`/`example-map-default` pair.
  - [ ] Drop `BUILD_FILTERED="$BUILD_FILTERED"` from the `if ! EXAMPLE_MAP=... python3 -
      <<'PYEOF'` env hand-off line (`:77`).
  - [ ] Delete `BUILD_FILTERED='false'` from the rewrite-failure fallback branch (the block
      after `then`/the failure warning, alongside where `EXAMPLE_MAP='{}'` is reset, currently
      `:92-96`).
- [ ] In `.circleci/config2.yml`: delete the `build-filtered:` parameter block (currently
      `:7-9`).
- [ ] Validate: `python3 -c "import yaml; yaml.safe_load(open('.circleci/config.yml')); yaml.safe_load(open('.circleci/config2.yml'))"`
      and re-run `test/hil/test/test_ci_metrics.py::TestCircleCiSentinelContract` (the sentinel
      tests) to confirm the remaining `example-map` sentinel contract still holds with only one
      tag pair instead of two.

### Task 3: Update the test coverage

**Files:** `test/hil/test/test_ci_metrics.py`

- [ ] Change `_run_extras_block()` to stop returning/asserting `filtered`: update the
      boundary markers (it currently locates the block via the literal string
      `"EXAMPLE_MAP='{}'\n          BUILD_FILTERED='false'"` — this must change once
      `BUILD_FILTERED` is gone from that block), stop appending `"%s" "$BUILD_FILTERED"` to the
      probe script, and change its return type from `(legs, filtered)` to `legs`.
- [ ] Update `test_an_empty_family_list_is_not_treated_as_unusable` and
      `test_a_real_family_list_stays_scoped` to drop their `filtered` assertions and the
      now-unused second return value.
- [ ] Run: `python3 test/hil/test/test_ci_metrics.py -v` — full suite must pass.

### Task 4: Full gate and commit

- [ ] `pre-commit run --all-files`
- [ ] One real PR push to confirm both `set-matrix` jobs (GHA and CircleCI) still produce a
      valid matrix and the CircleCI `continue`/config2 rewrite still succeeds with only the
      `example-map` sentinel in play.
- [ ] Commit with a message explaining `build_filtered` was write-only plumbing (cite this
      doc's grep result) — not a behavior change to what gets built.

## Why this is a separate PR

Left in place during the rework-metrics branch specifically to keep that PR's diff minimal —
it touches both CI backends' matrix-generation scripts plus their test coverage, which is
unrelated to that branch's actual goal (moving size analytics onto membrowse) and easy to
review wrong together with real behavior changes. It is a safe, mechanical removal (the grep
evidence above is exhaustive: no consumer exists on either backend) but deserves its own
before/after CI run on both GHA and CircleCI to prove neither pipeline silently depended on
the parameter merely existing (e.g. CircleCI's `/pipeline/continue` API or a cached config).
