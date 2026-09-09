# Broader Codex Delegation and the Herdr Peer Channel

Date: 2026-09-08
Branch: `claude-code-more-collab`

## Goal

Make every read-only agent role routable to Codex through one generic bridge
instead of one bridge file per role, use it to move PR review validation to
Codex behind a Claude challenge stage, and codify the Herdr live-session
channel as a repeatable interactive consult.

Today `.codex/agents/` holds nine adapters but only `code-verifier` is
reachable from Claude, via the role-specific `.claude/agents/codex-code-verifier.md`.
The other eight are unreachable from any workflow.

## Boundaries

- Claude Code remains the workflow runtime and orchestrator.
- `.claude/workflows/` remains the only authored workflow tree, and
  `.claude/agents/*.md` the only authored role bodies.
- `.codex/agents/*.toml` remain thin model/effort adapters that load the
  canonical Markdown role, with the one existing exception recorded in
  `CLAUDE.md`: `code-simplifier` wraps the bundled `/simplify` in Claude and
  keeps a standalone equivalent body in TOML. No new role body is duplicated.
- The bridge is a process bridge, never a second role.
- No write-capable Codex role lands in this change. Writer delegation depends
  on worktree isolation work that is explicitly deferred.
- Herdr is an interactive channel only. Workflow jobs keep using `codex exec`
  with an output schema.

## Read-only role set

`--sandbox read-only` is the only sandbox this change grants, so a role
qualifies only if it never writes to the working tree:

| Role                  | Routable | Why |
|-----------------------|----------|-----|
| `code-verifier`       | yes      | reads code and the diff only |
| `pr-review-validator` | no       | its whole procedure is `gh`, and `codex exec --sandbox read-only` has no network, so a Codex-hosted run answers from an empty view instead of failing (proven on PR #3902) |
| `pr-ci-watcher`       | no       | no local writes, but mutates via `gh run rerun` |
| `builder`             | no       | writes `examples/cmake-build-<board>` |
| `static-analyzer`     | no       | writes a PVS build and `compile_commands.json` |
| `code-writer`         | no       | writes source |
| `code-simplifier`     | no       | writes source |
| `hil-operator`        | no       | writes firmware and holds hardware locks |
| `target-debugger`     | no       | writes instrumentation and holds hardware locks |

`pr-ci-watcher` is excluded on the same principle as the writers: the bridge
grants no authority to mutate outside state until someone needs it.

## Generic bridge

The allowlist must be enforced by code that runs, not by an instruction a model
agrees to follow. A Markdown bridge that promises to reject a role is
unenforceable, and a source scan over `.claude/workflows/` is only a convention
check: `{ role: 'code-verifier', ...args }` defeats it, and the bridge can be
invoked from outside the workflow tree entirely.

So the bridge splits in two.

### `.claude/codex-agent.py` — the enforcing launcher

A committed, stdlib-only Python script, joining the Python already in
`.claude/skills/` and tested by a pre-commit hook in the shape of the existing
`python3 -m unittest` ones. `tomllib` parses the adapters.

```text
codex-agent.py --role <role> --prompt-file <f> --schema-file <f>
```

1. Exit non-zero if `role` is outside the read-only set, before any adapter
   path is constructed. This is the boundary.
2. Read `.codex/agents/<role>.toml` with `tomllib`, from the repository root
   located via `git rev-parse --show-toplevel`.
3. Compose `developer_instructions` followed by the prompt file into one
   temporary directory removed on exit.
4. Run:

```text
timeout 1800s codex exec -C <root> -m <adapter model>
  -c model_reasoning_effort=<adapter model_reasoning_effort>
  --sandbox read-only --output-schema <schema file>
  --output-last-message <result file> - < <prompt file>
```

5. Print the result file's JSON to stdout. On any failure, exit non-zero and
   print nothing to stdout.

### `.claude/agents/codex-agent.md` — the thin bridge

Replaces `.claude/agents/codex-code-verifier.md`, which is deleted. Input is
JSON `{ role, prompt, schema }`. The bridge writes `prompt` and `schema` to
files, invokes the launcher, and returns its stdout verbatim. If the launcher
exits non-zero, the bridge fails: it never falls back to a Claude agent, never
performs the work itself, and never fabricates JSON.

The source scan survives as a secondary check, asserting that every
`agentType: 'codex-agent'` dispatch in `.claude/workflows/` passes a literal
role. It catches drift; the launcher is what actually holds the line.

## Provider value rename: `both` to `all`

`both` names a count, not a policy, and more providers may follow. Rename the
provider enum value to `all` in `code-verify.js`, `validate.js`,
`full-check.js`, their argument-error messages and meta descriptions, and
`.claude/workflows/test/test-code-verify.mjs`. No alias is kept: these
arguments have no consumers outside this repository.

`validate.js` keeps its existing two-element routing shape. Only the name
generalizes in this change.

## Routing changes

| File | Change |
|------|--------|
| `code-verify.js` | `agentType: 'codex-agent'`; payload gains `role: 'code-verifier'` |
| `validate.js` | same, at the `codex` review stage |
| `pr-babysit.js` | validates on Claude, challenges every dismissal on Codex |
| `CLAUDE.md` | bridge filename, the read-only role set, the Herdr channel |

This shipped as a `reviewValidator` knob defaulting to `codex`, and that was
wrong: the role's whole procedure is `gh`, which the launcher's sandbox cannot
reach. The knob is gone and validation is Claude's.

## Challenge chain in `pr-babysit`

`REVIEWS.findings[].verdict` is `valid | invalid | stale`. Both `invalid` and
`stale` produce an outward reply declining to fix, so both are challenged.

The challenge always runs, always on Codex. Claude validates and Codex checks
every dismissal before it is posted: whoever validated, the model that closes a
reviewer's thread should not be the only one to have judged it. `code-verifier`
is the role that carries this, and it needs nothing the sandbox withholds — the
findings travel in the prompt and the evidence is in the checkout.

```text
reviews#<cycle>    pr-review-validator (Claude)  -> r
challenge#<cycle>  code-verifier (Codex), only when r.findings has a non-valid entry
                   input:  IN_CHECKOUT, plus the non-valid findings as
                           [{ id, commentId, file, line, claim, verdict, reason }]
                           where id is the finding's index in that submitted list
                   schema: { verdicts: [{ id, upheld: boolean, reason: string }] }
```

The challenge prompt carries the same `IN_CHECKOUT` prefix the validator gets
at `pr-babysit.js:387`. Without it, a run with `checkoutDir` set would
challenge against the session directory instead of the PR checkout.

**Findings are identified by `id`, not by `commentId`.** `REVIEWS` places no
uniqueness constraint on `commentId`, so one review comment can raise two
findings. Keying the challenge by `commentId` could not say which of them was
overturned.

**Coverage is explicit.** The challenger returns exactly one verdict for every
submitted `id`. A response missing an `id`, carrying an unknown one, or
carrying two verdicts for the same `id`, fails the cycle. Without the
completeness rule an empty list could not distinguish "all examined and upheld"
from "examined none"; without the uniqueness rule `{id: 0, upheld: true}` and
`{id: 0, upheld: false}` would both satisfy coverage and let map-construction
order decide the outcome.

Per finding with `upheld: false`:

- Set its verdict to `valid`.
- Replace its `fixHint` with the challenger's `reason`. The existing fix text
  at `pr-babysit.js:424` is built from `claim` and `fixHint`, and an overturned
  finding's `fixHint` came from the validator that wrongly dismissed it. The
  fixer must receive the evidence that overturned it, not the dismissal.

Then, per `commentId` touched by an overturn, reconcile once. A drafted reply
body covers the whole comment and was written while the validator believed
every finding on it was invalid, so it cannot be reused selectively:

- **Every non-valid finding on the comment overturned.** Drop the comment's
  entry from `r.replies` and delete its `commentId` from `repliedIds`, before
  reply processing. Nothing is owed to the reviewer; the fix is the answer.
- **Mixed — some overturned, some upheld.** Drop the reply and the
  `repliedIds` record, and add the `commentId` to a new cycle-spanning
  `deferredComments` set. Posting the drafted body would publish a refutation
  of the finding we just accepted, and `pr-babysit.js:403-415` resolves the
  thread as it posts, closing it before the fix lands. Recomposing the body
  from partial verdicts would mean inventing prose the challenger never wrote.
  Instead the comment goes unanswered this cycle: the fix for the overturned
  finding lands, the comment re-harvests next cycle, and the validator drafts
  an accurate body against fixed code.

Deferral has to survive two more paths in the same cycle.

**The post-fix stage must skip a deferred comment.** An overturned finding
joins `validFindings` and is fixed normally, but `pr-babysit.js:444` then posts
a fix note for its `commentId` and resolves the entire thread, and
`pr-babysit.js:457` re-adds the id. That closes the mixed comment anyway and
filters the upheld sibling's refutation out at `:401` next cycle. So the
findings payload built at `pr-babysit.js:447` excludes every `commentId`
deferred this cycle. The fix work grouped at `pr-babysit.js:424` is unaffected:
the finding is still fixed, only its reply is withheld.

**The obligation must outlive the cycle.** `pendingReplies` cannot carry it: it
is re-initialized at `pr-babysit.js:397`, and `pr-babysit.js:472` re-arms on a
review-lane push before the green exit is ever reached. `deferredComments`
therefore lives at workflow scope beside `repliedIds`. An id clears when a
later cycle records it in `repliedIds` — that is, when its reply actually
posted and its thread actually resolved.

The retry needs its own branch, not a wider green condition. Merely adding
`deferredComments.size === 0` to `pr-babysit.js:502` would let a green cycle
fall past the rig-side, CI-settling and `!r.done` branches into the
`unactionable` return at `pr-babysit.js:530`, stopping on the first deferral
instead of retrying it. Instead, inside the green block and after the existing
`pendingReplies` check:

```text
if (deferredComments.size > 0) {
  cycle < maxCycles -> log, back off as the !r.done branch does, return null
  otherwise         -> { pass: false, cycles: cycle, history,
                         reason: 'deferred-replies-unresolved',
                         deferred: [...deferredComments] }
}
```

The backoff matters because reaching this point means no push happened this
cycle — a review-lane push already re-arms at `pr-babysit.js:472` — so an
unchanged PR would otherwise spin through the cycle budget.

That same `:472` re-arm is why the outer loop's exhaustion return at
`pr-babysit.js:557` needs the contract too. A comment first deferred on the
final cycle has its fix pushed, re-arms there, and never reaches the green
block, so the loop would end as a bare `maxCycles reached` with the obligation
invisible. When `deferredComments` is non-empty at `:557`, it returns
`reason: 'deferred-replies-unresolved'` with `deferred: [...deferredComments]`
instead. Exhaustion reports outstanding refutations by one rule, whichever path
reached it.

The validator must also be told what is outstanding, or it may not re-harvest a
comment it considers handled. The `reviews#<cycle>` prompt lists the
outstanding `deferredComments` ids and requires a verdict for each.

Deleting from `repliedIds` matters because that set also spans cycles: a
refutation replied to and resolved in one cycle records its id at
`pr-babysit.js:413`, and a later overturn's corrective reply would otherwise be
filtered out at `:401` by the stale record. Because neither branch above posts
anything for the comment, nothing re-adds the id at `:413` in the same cycle.
`doneIds` belongs to each posting result and needs no clearing.

Implementation refined this in three ways that the rules above do not capture:

- **Only the refutation posting discharges a deferral.** The post-fix note says
  a finding is fixed, which is not the refutation that is owed, so its
  `doneIds` clears `repliedIds` alone.
- **A deferred comment stays eligible for its reply** even once it is in
  `repliedIds`, since a fix note may have put it there.
- **Reconciliation runs over every comment, not only overturned ones.** A
  comment holding both a valid and a refuted finding is deferred whether or not
  a challenge touched it, because posting its refutation resolves the thread
  over an unlanded fix. A refuted finding with no drafted reply is likewise
  deferred, unless the comment is already in `repliedIds`.
- **`doneIds` are intersected with the ids actually submitted**, and a comment
  is replied to at most once per cycle.

A comment whose findings were all upheld is replied to and resolved exactly as
today.

If the challenge agent dies or returns incomplete coverage, the cycle fails the
same way a dead validator does: set `entry.error` and return
`{ pass: false, cycles: cycle, history, reason: 'review-challenger-died' }`.
Returning `null` would re-arm and retry, and throwing would report
`cycle-threw`; neither says what happened. The existing `finally` still settles
the CI lane.

Suppressing the replies and logging is not enough: `pendingReplies` is
initialized to `0` at `pr-babysit.js:397`, so a suppressed reply leaves it zero
and the green exit at `pr-babysit.js:501` would return `pass: true` on a cycle
whose refutations were never checked.

The cycle table's Outcome column gains `codex refuted -> claude overturned`
for overturned rows.

## Herdr peer channel skill

Add `.claude/skills/peer-agent/SKILL.md` for interactive consults with a
sibling Codex session in the same worktree.

Procedure:

1. Require `HERDR_ENV=1`; otherwise say so and stop.
2. `herdr agent list`, and select the agent whose kind is `codex` and whose
   `cwd` equals the working directory. Never create a pane, workspace, or
   agent; if there is none, report that and stop.
3. `herdr agent prompt <pane> "<framed prompt>" --wait --timeout <ms>`.
4. `herdr agent read <pane> --source recent-unwrapped --lines <n>`.
5. If the reply is truncated because Codex renders on the alternate screen,
   ask it to write the full reply to a scratch file and answer with only the
   path, then read the file. Fallback only; never request file output up
   front.

Rules the skill encodes:

- Chat history is not shared. Every message carries its own context.
- The peer pane may run with full access. State "read-only, no edits"
  explicitly whenever writes are unwanted.
- `--wait` settles on agent status, not on a specific request, and can return
  on an already-settled state. Confirm the reply answers the prompt that was
  sent before trusting it.
- Never route workflow jobs through this channel. Those use `codex exec` with
  an output schema, which has real completion and failure boundaries.
- The reply is advice to verify, not a result.

## Delivery

- PR A: the launcher and thin bridge, the `both` to `all` rename, routing
  changes, the challenge chain, `CLAUDE.md`.
- PR B: the `peer-agent` skill.
- Deferred to `docs/superpowers/followup/pr<NNN>-codex-write-roles.md`: a
  write-capable sandbox behind a widened launcher allowlist, and per-writer worktree
  isolation in `fanout-dev`, which currently leaves isolation optional so file
  scopes alone do not prevent two writers from clobbering each other. That
  work unblocks `code-writer`, `code-simplifier`, `builder`, and
  `static-analyzer`.

## Verification

- A new `.claude/codex-agent.py` test, run by its own pre-commit hook
  alongside the existing `code-verify-logic` and `pr-babysit-logic` hooks,
  asserts a role outside the read-only set exits non-zero *before* any adapter
  file is opened, and that a launcher failure prints nothing to stdout.
- `.claude/workflows/test/test-code-verify.mjs` covers the renamed provider
  value and the `codex-agent` agent type, plus the source scan asserting every
  `codex-agent` dispatch passes a literal role from the read-only set.
- A new test covers the challenge chain:
  - an overturned finding becomes `valid` and carries the challenger's `reason`
    as its `fixHint`;
  - a comment whose non-valid findings were *all* overturned loses its drafted
    reply and its `repliedIds` record;
  - a comment with one overturned and one upheld finding also loses both,
    enters `deferredComments`, is excluded from the post-fix reply payload at
    `pr-babysit.js:447` while still being fixed, and re-arms a green cycle
    rather than passing or falling through to `unactionable`, until a later
    cycle actually posts and resolves its reply; at `maxCycles` it ends as
    `deferred-replies-unresolved` listing the outstanding ids;
  - a comment whose findings were all upheld still replies and resolves;
  - a dead challenger, a missing `id`, an unknown `id`, and two verdicts for
    one `id` each fail the cycle with `review-challenger-died` rather than
    reaching the green exit;
  - the reviews stage never dispatches to `codex-agent`.
- `pre-commit run --all-files` before either PR.
