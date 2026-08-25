# IAR HIL Leg Re-run Spec Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let the `hil-hfp-iar` CI leg re-run only its failed boards, as the other two HIL
legs already do.

**Architecture:** `hil_test.py` writes a `<config>.failed` spec into `HIL_REPORT_DIR`; a
workflow step reads it on the next attempt and passes the boards back as arguments. The IAR
leg passes `--retry 1` like the others but sets no `HIL_REPORT_DIR` and has no read-back
step, so its spec is written into the workspace and never read.

**Tech Stack:** GitHub Actions YAML, self-hosted runner.

## Global Constraints

- `.github/workflows/build.yml`. The two working legs are `hil-tinyusb` (matrix) — see its
  `Set HIL report dir (per run+job; persists across run attempts)` and `Get re-run spec from
  previous attempt` steps — and they are the pattern to copy.
- The report dir must be keyed by run id AND job so a matrix leg does not collide with
  another, and must survive across run attempts (that is the whole point).
- The IAR leg is the only HIL job that BUILDS inline; its `Build` step is bounded at
  `timeout-minutes: 30` under a 120-minute job ceiling. Do not disturb that.

## What is already established

- Verified by reading the workflow: `hil-hfp-iar` has neither `HIL_REPORT_DIR` nor a
  `Get re-run spec` step, while passing `--retry 1`.
- Consequence: a GitHub re-run of that job re-tests its whole matrix. **This is not a
  regression** — that leg never had the mechanism — and the unread spec costs only a file.
- The report artifact upload for that leg is named `hil-report-hfp-iar`.

**Why this is a separate PR:** it is CI plumbing with no code change, it needs a real
re-run on the self-hosted runner to prove, and it duplicates ~15 lines of workflow that
would be better factored — a decision worth making on its own.

## File Structure

- `.github/workflows/build.yml` — the `hil-hfp-iar` job only.

---

### Task 1: Give the IAR leg a persistent report dir and a re-run spec

**Files:**
- Modify: `.github/workflows/build.yml` (job `hil-hfp-iar`)

**Interfaces:**
- Consumes: `hil_test.py`'s existing `--report-dir` / `.failed` behaviour — no code change.
- Produces: `env.HIL_REPORT_DIR` for the job, and `$RERUN_ARGS` for the test step.

- [ ] **Step 1: Copy the two steps from `hil-tinyusb`, before the Build step**

```yaml
      - name: Set HIL report dir (per run+job; persists across run attempts)
        run: |
          BASE=$HOME/hil-reports
          echo "HIL_REPORT_DIR=$BASE/${GITHUB_RUN_ID}-hfp-iar" >> "$GITHUB_ENV"

      - name: Get re-run spec from previous attempt
        run: |
          SPEC="$HIL_REPORT_DIR/hfp.json.failed"
          if [ -f "$SPEC" ]; then
            echo "RERUN_ARGS=$(cat "$SPEC")" >> "$GITHUB_ENV"
            echo "re-running only: $(cat "$SPEC")"
          fi
```

Match the exact spec filename `hil_test.py` writes for this leg's config — read
`_write_failed_spec` and the `failed_fname` construction rather than assuming.

- [ ] **Step 2: Pass the spec to the test step**

```yaml
          python3 test/hil/hil_test.py --retry 1 $SEL_ARGS hfp.json $RERUN_ARGS
```

`--retry 1` stays FIRST so argparse's last-wins keeps any explicit override working.

- [ ] **Step 3: Point the artifact upload at the report dir**

```yaml
          path: ${{ env.HIL_REPORT_DIR }}/hil_report.md
```

- [ ] **Step 4: Validate the YAML**

Run: `python3 -c "import yaml,sys; d=yaml.safe_load(open('.github/workflows/build.yml')); j=d['jobs']['hil-hfp-iar']; print(j['timeout-minutes'], [s.get('name') for s in j['steps']])"`
Expected: the ceiling is still 120, the Build step still carries `timeout-minutes: 30`, and
the two new steps appear before Build.

- [ ] **Step 5: Commit**

```bash
git add .github/workflows/build.yml
git commit -m "ci: let the IAR HIL leg re-run only its failed boards"
```

---

### Task 2: Prove it on a real re-run

**Files:** none — evidence only.

- [ ] **Step 1:** Push and let `hil-hfp-iar` run to a failure (or force one).
- [ ] **Step 2:** Confirm `$HIL_REPORT_DIR/hfp.json.failed` exists on the runner after the
      job.
- [ ] **Step 3:** Use GitHub's "Re-run failed jobs" and confirm the log line
      `re-running only: ...` and that only those boards are tested.
- [ ] **Step 4:** Record the run URL in the PR body.

---

## Consider first

Three jobs would then carry the same ~15 lines. Factoring them into a composite action, or
computing the report dir inside `hil_test.py` from `GITHUB_RUN_ID`, may be the better
change — decide that before copying the block a third time.
