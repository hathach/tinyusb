# Revert HIL/membrowse Build De-dup + Fork-PR Tokenless Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove the general hil-build/cmake membrowse de-dup machinery (the cmake job again builds and uploads ALL CI boards on every run), keep the espressif exception, fix the fork-PR tokenless upload regression in `membrowse_report.py`, and rename the board file to `.github/ci-pinned-boards.json` with matching `--ci-pinned-boards`/`--ci-pinned-boards-only` flags (maintainer's decision — "pinned" says the set is explicitly held: membrowse history is keyed on `<board>/<example>` target names, so a pinned board cannot be casually swapped).

**Architecture:** The cmake job returns to being the single owner of membrowse uploads for arm/riscv/etc. boards — PRs and pushes, one uniform path — with `hil-build` building only for the rig. `hil-build-esp` keeps its upload + `hil-build-esp-identical` (esp-idf is the slowest toolchain and its boards are enumerated by name; that exception is small and self-contained). The ~10-board double build this re-introduces is free: TinyUSB is a public repo, GHA minutes cost nothing.

**Tech Stack:** GitHub Actions workflows, `tools/build.py` (argparse + resolver), Python `unittest` suites under `test/hil/test/`.

**Spec:** `docs/superpowers/specs/2026-08-25-rework-metrics-design.md` — the branch's original design, which never contained the de-dup (it was added later by request and is now being backed out). This plan restores the spec's upload topology; the spec text needs no edit.

**Branch state:** `rework-metrics`, 5 folded commits on `origin/master` at the time this plan was written (rebased 2026-08-27; `origin/master` was the base ref, since the local `master` ref lagged). Executed 2026-08-28; branch folded back to 5 commits, this plan file included.

## Global Constraints

- Commit messages imperative, NO trailers of any kind (no `Co-Authored-By`, no `Claude-Session`). Verify `git log origin/master..HEAD --format=%B | grep -c 'Co-Authored-By\|Claude-Session'` prints 0 before reporting done.
- KEEP (do not touch while deleting the de-dup): `build_utils.hil_roster_boards()` (used by `tools/drivers_coverage_check.py`), the variant-leg upload guard in `build_util.yml` (protects hil-build-esp's DMA variants), the variant guard inside `hil-build-esp-identical`'s loop, the `$EX_ARGS`-less Membrowse Upload step, `hil-build-esp`'s `upload-membrowse: true` + `secrets: inherit`, the `hil-build-esp-identical` job, the board file's *content* (rename in Task 2 changes only names/paths), the rp2040 build fixes, `drivers_coverage_check.py`'s type check.
- Task 2 runs the rename FIRST among the code sweeps, so every later task reads and writes the NEW names (`.github/ci-pinned-boards.json`, `--ci-pinned-boards`, `--ci-pinned-boards-only`).
- After every task: the 8 suites green (`test_ci_boards`, `test_ci_metrics`, `test_ci_select`, `test_drivers_coverage`, `test_membrowse_compare`, `test_membrowse_report`, `test_membrowse_onboard`, `test_metrics_compare_base`, run as `python3 test/hil/test/<name>.py`), and `python3 -c "import yaml,glob;[yaml.safe_load(open(f)) for f in glob.glob('.github/workflows/*.yml')];print('YAML OK')"`.

---

### Task 1: Fork-PR tokenless upload fix in membrowse_report.py

Master's CMake-era upload passed `--api-key $ENV{MEMBROWSE_API_KEY}` expanded at configure time; on a fork PR the empty env var produced a bare `--api-key`, argparse (`nargs='?', const=''`) yielded `''`, and membrowse fell through to its GitHub tokenless auth (`membrowse/auth/strategy.py: determine_auth_strategy`), uploading full ELF data. The branch's `tools/membrowse_report.py` instead hard-exits when the env var is empty (`build_membrowse_cmd`, ~line 132), so every fork PR dies in the wrapper — silently, behind the workflow's `continue-on-error: true`. Fix: with no key, omit `--api-key` entirely and let membrowse decide (tokenless on a GHA PR event; its own clear error elsewhere).

**Files:**
- Modify: `tools/membrowse_report.py` (`build_membrowse_cmd`, ~lines 129-136)
- Test: `test/hil/test/test_membrowse_report.py` (`BuildMembrowseCmd` ~line 174, `CliKeyHandling` ~line 226)

**Interfaces:**
- Consumes: `os.environ['MEMBROWSE_API_KEY']` (may be absent/empty), `args.upload`, `args.target_name`.
- Produces: `build_membrowse_cmd(args, commands_text) -> (cmd: list, key: str | None)` — unchanged signature; `key` is `None` when no key, and then `cmd` contains `--upload --github --target-name <name>` but NOT `--api-key`.

- [ ] **Step 1: Rewrite the two key-requirement tests as tokenless tests**

In `test/hil/test/test_membrowse_report.py`, replace `test_upload_requires_key_env` (~line 174) with:

```python
    def test_upload_without_key_goes_tokenless(self):
        # Fork PRs: GHA withholds secrets, so the env var is empty. Master's
        # CMake-expanded bare `--api-key` fell through to membrowse's GitHub
        # tokenless auth; the wrapper must do the same by OMITTING --api-key,
        # never by exiting (which, behind continue-on-error, silently drops
        # every fork PR's upload).
        args = self._args(upload=True)
        env = {k: v for k, v in os.environ.items() if k != 'MEMBROWSE_API_KEY'}
        with unittest.mock.patch.dict(os.environ, env, clear=True):
            cmd, key = mr.build_membrowse_cmd(args, COMMANDS_TEXT)
        self.assertIsNone(key)
        self.assertIn('--upload', cmd)
        self.assertIn('--github', cmd)
        self.assertNotIn('--api-key', cmd)
```

(Adapt `self._args(...)`/`COMMANDS_TEXT` to the file's existing fixtures — the neighboring `test_upload_appends_key_and_target_name` shows the local naming; mirror it.) Replace `CliKeyHandling.test_missing_api_key_errors_cleanly_no_traceback` (~line 226) with an end-to-end variant asserting the CLI, run without the env var and with `--upload`, exits 0 up to the point of invoking membrowse and the logged `+ ` line contains `--github` but not `--api-key` (reuse the class's existing stub-membrowse harness the way `test_upload_passes_real_key_to_membrowse` does).

- [ ] **Step 2: Run the new tests, verify they FAIL**

Run: `python3 test/hil/test/test_membrowse_report.py`
Expected: the two new tests fail (`SystemExit`/exit-2 from the wrapper's key guard).

- [ ] **Step 3: Fix build_membrowse_cmd**

In `tools/membrowse_report.py` replace the upload block (~lines 129-134):

```python
    key = None
    if args.upload:
        cmd += ['--upload', '--github', '--target-name', args.target_name]
        # No MEMBROWSE_API_KEY (fork PRs: GHA withholds secrets) -> omit
        # --api-key entirely and let membrowse decide: on a GHA pull_request
        # event it falls through to its GitHub tokenless auth (server-side
        # re-validated), matching what master's CMake-expanded bare --api-key
        # did; anywhere else membrowse itself errors, naming the requirement.
        key = os.environ.get('MEMBROWSE_API_KEY') or None
        if key:
            cmd += ['--api-key', key]
```

- [ ] **Step 4: Run the suite, verify it passes**

Run: `python3 test/hil/test/test_membrowse_report.py` — all green, including the untouched redaction tests (`key=None` means the `shown` redaction loop is a no-op; the logged line contains no secret).

- [ ] **Step 5: Confirm membrowse_onboard.py needs no change, then commit**

`tools/membrowse_onboard.py` keeps requiring the key for `--upload` — backfill is a maintainer-local operation and tokenless only exists inside a GHA `pull_request` event, so its guard is correct. Verify `python3 test/hil/test/test_membrowse_onboard.py` still passes untouched.

```bash
git add tools/membrowse_report.py test/hil/test/test_membrowse_report.py
git commit -m "membrowse_report: restore tokenless upload for fork PRs

An empty MEMBROWSE_API_KEY made the wrapper exit before invoking
membrowse, and continue-on-error hid it: every fork PR silently lost
its upload. Master's CMake-expanded bare --api-key fell through to
membrowse's GitHub tokenless auth; omit the flag to do the same."
```

---

### Task 2: Rename to ci-pinned-boards.json and --ci-pinned-boards flags

Maintainer's decision. File: `.github/ci-boards.json` → `.github/ci-pinned-boards.json`. Flags: `--ci-boards` → `--ci-pinned-boards`, `--ci-boards-only` → `--ci-pinned-boards-only`. Function names (`resolve_ci_boards()` etc.), the JSON's internal keys (`boards`, `uncovered`), and membrowse target names are all UNCHANGED — this is a file-path + CLI-flag rename only.

**Files** (complete list, from `git grep -l 'ci-boards\.json\|--ci-boards'`):
- Rename: `.github/ci-boards.json` → `.github/ci-pinned-boards.json` (use `git mv`)
- Modify: `tools/build.py` (argparse flag names + help text + `parser.error` message), `tools/ci_select.py` (rule-2c table row ~line 29, comments ~118-145, `_CI_BOARDS_RE` pattern ~line 145), `tools/drivers_coverage_check.py`, `.github/scripts/ci_set_matrix.py` (comment), `.github/workflows/build.yml` (cmake + hil-build/hil-build-esp `build-options`, check-paths filter entry `.github/ci-boards.json`), `.github/workflows/build_util.yml` (comments/usages), `.pre-commit-config.yaml` (drivers-coverage hook `files:` regex, line ~94), `.claude/skills/membrowse/SKILL.md`
- Modify: `test/hil/test/test_ci_boards.py`, `test_ci_metrics.py`, `test_ci_select.py`, `test_drivers_coverage.py`
- Modify: `docs/superpowers/specs/2026-08-19-ci-build-family-filter-design.md` (rule-2c row, ~line 55) — **required**: `test_ci_select.py`'s `TestRuleTableIsCarbonOfTheSpec` asserts ci_select.py's rule table is byte-identical to this spec's; renaming the path in one without the other fails the suite.

**Interfaces:**
- Produces: `resolve_ci_boards(boards_path, ...)` unchanged signature; CLI accepts only the new flag spellings (no aliases — the branch is unpushed, nothing external depends on the old ones).

- [ ] **Step 1: git mv + mechanical sweep**

```bash
git mv .github/ci-boards.json .github/ci-pinned-boards.json
git grep -l 'ci-boards\.json\|--ci-boards' -- '*.py' '*.yml' '*.yaml' '*.md' | \
  xargs sed -i 's/ci-boards\.json/ci-pinned-boards.json/g; s/--ci-boards-only/--ci-pinned-boards-only/g; s/--ci-boards\b/--ci-pinned-boards/g'
```

Then hand-review `git diff` hunk by hunk: the sweep must not have touched membrowse target names, JSON keys, or `resolve_ci_boards` identifiers, and prose sentences must still read correctly (e.g. build.py's argparse `help=`, ci_select's rule table alignment — the table's column padding may need re-aligning after the longer name; pad ALL rows so pipes line up in the raw source).

- [ ] **Step 2: Verify the carbon test and the full suites**

Run: all 8 suites (Global Constraints list) plus `python3 tools/drivers_coverage_check.py` and the YAML parse one-liner. `TestRuleTableIsCarbonOfTheSpec` green proves ci_select.py and the 2026-08-19 spec were renamed in lockstep. Also `pre-commit run drivers-coverage --all-files` to prove the hook's new `files:` regex still matches the renamed JSON (run it once with a whitespace touch to the JSON staged if needed to force the hook).

- [ ] **Step 3: Commit**

```bash
git add -A
git commit -m "ci-boards: rename to ci-pinned-boards

The file pins the board that represents each family in CI: pinned
boards are always built and uploaded, and membrowse history is keyed
on their <board>/<example> target names, so the set is explicitly
held rather than derived. Flags follow: --ci-pinned-boards,
--ci-pinned-boards-only. Content, JSON keys and target names are
unchanged."
```

---

### Task 3: Remove the de-dup resolver from tools/build.py

**Files:**
- Modify: `tools/build.py` (delete `hil_plain_board_names` ~line 366, `resolve_hil_skip_boards` ~line 394; the `skip_hil_boards` param of `resolve_ci_boards` ~lines 420-455; the `--skip-hil-boards` argparse flag ~line 496 and its `main()` plumbing ~lines 529-585)
- Modify: `test/hil/test/test_ci_boards.py` (delete classes `SkipHilBoards` ~line 58 and `ResolveHilSkipBoards` ~line 101)

**Interfaces:**
- Produces: `resolve_ci_boards(boards_path, family, examples=None, boards_only=False, build_system='cmake', extra_defines=())` — no `skip_hil_boards` parameter. `build_utils.hil_roster_boards()` untouched (Task 4's checker still imports it).

- [ ] **Step 1: Delete the two test classes**

Remove `SkipHilBoards` and `ResolveHilSkipBoards` from `test/hil/test/test_ci_boards.py` entirely (they test only deleted behavior; `CIBoards`' six tests stay and pin the surviving contract, including the `-e` buildability filter).

- [ ] **Step 2: Delete the resolver machinery**

In `tools/build.py`: delete `hil_plain_board_names()` and `resolve_hil_skip_boards()` whole; remove the `skip_hil_boards=None` parameter, its docstring paragraph, and the `if skip_hil_boards:` filter from `resolve_ci_boards()`; delete the `--skip-hil-boards` `add_argument` and every `skip_hil_boards_arg`/`hil_boards` line in `main()` (the `parser.error('--skip-hil-boards requires --ci-boards')` guard included). If `build_utils.hil_roster_boards` was imported into build.py solely for the deleted code, drop that import; leave `build_utils.py` itself untouched.

- [ ] **Step 3: Run the affected suites**

Run: `python3 test/hil/test/test_ci_boards.py && python3 test/hil/test/test_drivers_coverage.py`
Expected: both green (the checker's roster reading is independent of build.py's deleted code).

- [ ] **Step 4: Commit**

```bash
git add tools/build.py test/hil/test/test_ci_boards.py
git commit -m "build: drop the --skip-hil-boards de-dup resolver"
```

---

### Task 4: Revert the workflow wiring in build.yml

**Files:**
- Modify: `.github/workflows/build.yml` (set-matrix `hil_built_boards` output + jq extraction; cmake job `build-options` + comment; hil-build `build-options`/`upload-membrowse`/`secrets`/comments; delete `hil-build-identical`)
- Modify: `test/hil/test/test_ci_metrics.py` (de-dup gate assertions ~lines 176-235)

**Interfaces:**
- Consumes: nothing new. Produces: the invariant Task 4 Step 1's test pins — exactly two jobs upload to membrowse: `cmake` (all CI boards, `--identical` on no-code-change runs because `code-changed: false` skips only the Build step) and `hil-build-esp`/`hil-build-esp-identical` (espressif).

- [ ] **Step 1: Rewrite the de-dup assertions in test_ci_metrics.py as the new invariant, verify they FAIL**

Replace the `--skip-hil-boards=` line test, the `hil_built_boards` reference test, and the `hil-build-identical` `if:` test (~lines 176-235) with assertions on the raw `build.yml` text:

```python
    def test_membrowse_upload_owners(self):
        # Exactly the cmake job and the espressif pair upload; hil-build
        # builds for the rig only. A `upload-membrowse: true` reappearing on
        # hil-build re-opens the target-name collision between its
        # raspberry_pi_pico PIO-USB variant build and cmake's plain build.
        jobs = re.split(r'\n  (?=[a-z][\w-]*:\n)', self.build)
        uploaders = sorted(j.split(':', 1)[0] for j in jobs
                           if 'upload-membrowse: true' in j)
        self.assertEqual(uploaders,
                         ['cmake', 'hil-build-esp', 'hil-build-esp-identical'])

    def test_no_skip_hil_boards_anywhere(self):
        self.assertNotIn('--skip-hil-boards', self.build)
        self.assertNotIn('hil_built_boards', self.build)
        self.assertNotIn('hil-build-identical:', self.build.replace('hil-build-esp-identical:', ''))
```

(Adapt `self.build` / imports to the file's existing fixture that already reads `build.yml`.) Run `python3 test/hil/test/test_ci_metrics.py` — the two new tests must FAIL against the current file.

- [ ] **Step 2: Edit build.yml**

All in `.github/workflows/build.yml`:
1. **set-matrix:** delete the `hil_built_boards:` output line and its comment block (~lines 55-64), and the `HIL_BUILT_BOARDS=` jq extraction block at the end of the "Generate matrix json" step (the block whose comment begins "Board names hil-build/hil-build-esp's matrix legs actually contain").
2. **cmake job:** replace the `build-options:` value and delete the whole `--skip-hil-boards` comment block above it (from `# --skip-hil-boards: a CI board that hil-build already builds` through the end of the gating rationale):

```yaml
      build-options: '--ci-pinned-boards .github/ci-pinned-boards.json'
```

3. **hil-build:** delete `upload-membrowse: true` and its comment block (from `# hil-build now owns the membrowse upload` through `code_changed.`), delete `secrets: inherit`, and delete the `build-options: '--ci-pinned-boards .github/ci-pinned-boards.json'` line together with its "only here to satisfy --ci-boards-only" comment (with no upload step consuming `--ci-pinned-boards-only`, the flag serves nothing). Keep the toolchain-bucket comment and everything else.
4. **Delete the `hil-build-identical` job** (whole block, from `  hil-build-identical:` up to the `# Hardware in the loop (HIL)` banner). Leave `hil-build-esp`, `hil-build-esp-identical` and every `hil-tinyusb*` job byte-untouched — in particular `hil-tinyusb`'s `needs: [hil-build, set-matrix]`.

- [ ] **Step 3: Verify**

Run: `python3 test/hil/test/test_ci_metrics.py` (new tests now pass) and the YAML parse one-liner from Global Constraints. Then `grep -rn 'skip-hil\|hil_built_boards' .github/ tools/ test/` — expected: no hits.

- [ ] **Step 4: Commit**

```bash
git add .github/workflows/build.yml test/hil/test/test_ci_metrics.py
git commit -m "build: cmake job owns all membrowse uploads again

Revert the hil-build de-dup: hil-build builds for the rig only, the
cmake job builds and uploads every CI board on PRs and pushes alike.
The 10-board double build is free on a public repo; the machinery to
avoid it was not. hil-build-esp keeps the espressif upload."
```

---

### Task 5: Skill doc wording

**Files:**
- Modify: `.claude/skills/membrowse/SKILL.md` (~lines 14-17)

- [ ] **Step 1: Rewrite the upload-topology sentences**

Replace the passage claiming hil-build/hil-build-esp own plain-board uploads with `--skip-hil-boards` dropping the rest, with: CI uploads come from the `cmake` job (every board in `.github/ci-pinned-boards.json`, `--identical` when code did not change) plus `hil-build-esp` for espressif boards (by name; `hil-build-esp-identical` covers no-code-change pushes). Smallest edit that makes every sentence true — do not restructure the file.

- [ ] **Step 2: Sweep for stragglers and commit**

Run: `grep -rn 'skip-hil\|hil-build-identical\|hil_plain' .claude/ docs/ CLAUDE.md` — fix any remaining stale sentence found (expected: none outside this file; `docs/superpowers/plans/2026-08-25-rework-metrics.md` and the spec predate the de-dup and need no edit).

```bash
git add .claude/skills/membrowse/SKILL.md
git commit -m "membrowse skill: cmake job owns CI uploads"
```

---

### Task 6: Full verification + re-fold

The de-dup must not appear anywhere in the final history — commit 5 currently *introduces* it and this branch's later commits revert it; fold them together so the branch reads as if it was never built.

- [ ] **Step 1: Full battery**

All 8 suites (Global Constraints), `python3 tools/drivers_coverage_check.py` (exit 0, INFO lines only), YAML parse, `pre-commit run --all-files`, `git status --porcelain` clean apart from `.idea/`.

- [ ] **Step 2: Re-fold to 5 commits**

Backup first: `git branch -f backup/rework-metrics-prefold8 HEAD`. Then soft-reset squash ALL Task 1-5 commits into commit 5 (`4844da63a`, "build: de-duplicate the CI and HIL builds, curate the board set") and rewrite its message: title becomes `ci-pinned-boards: curate the board set, esp upload via hil-build-esp, rp2040 fixes`; body keeps the board-curation, espressif, rp2040 skip.txt/static, `$EX_ARGS`, `secrets: inherit` (hil-build-esp only), drivers type-check and fork-PR tokenless paragraphs, plus a short paragraph on the rename (pinned = explicitly held, history keyed on target names; flags follow); every paragraph describing `--skip-hil-boards`, `hil_built_boards`, variant carve-out or `hil-build-identical` is deleted. Commits 1-4 keep their messages, but sweep them for de-dup references first (`git log origin/master..HEAD --format=%B | grep -in 'skip-hil\|dedup\|de-dup\|hil-build-identical'`) and reword any hit that describes the deleted general machinery (the espressif de-dup sentences in commit `e4bf87e9e` stay — that exception is kept).

- [ ] **Step 3: Verify the fold changed nothing**

```bash
git diff backup/rework-metrics-prefold8 HEAD   # must be empty
git log origin/master..HEAD --oneline                  # 5 commits
git log origin/master..HEAD --format=%B | grep -c 'Co-Authored-By\|Claude-Session'   # 0
```

Then re-run `pre-commit run --all-files` once on the folded HEAD. Report done — do NOT push.
