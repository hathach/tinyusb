# Codex Delegation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make every read-only agent role routable to Codex through one enforcing launcher, validate PR reviews on Claude and challenge every dismissal on Codex before it is posted, and codify the Herdr peer channel as a skill.

**Architecture:** A committed Python launcher owns the role allowlist and the `codex exec` invocation; a thin Markdown bridge agent only invokes it. Workflows dispatch to that bridge with a literal role. `pr-babysit` keeps review validation on Claude and inserts an unconditional Codex challenge stage between the validator's dismissals and any public reply.

**Tech Stack:** Node ESM workflow scripts driven by the Workflow tool; `node:assert/strict` tests executed via pre-commit `language: system` hooks; Python 3.11+ stdlib (`tomllib`, `argparse`, `subprocess`, `tempfile`) for the launcher; `codex exec` CLI 0.153.4.

**Spec:** `docs/superpowers/specs/2026-09-08-codex-delegation-design.md`

## Global Constraints

- Read-only role set is exactly `code-verifier`. No other role may be dispatched to Codex in this plan. Tasks 1 and 4 were written with `pr-review-validator` in the set; the Task 4 revert took it back out — see that task's banner.
- The launcher runs `codex exec` with `--sandbox read-only`, always. No sandbox parameter is exposed.
- The launcher prints nothing to stdout on any failure path, and exits non-zero.
- No fallback: a Codex failure never silently reruns the work on Claude and never fabricates JSON.
- Workflow nesting stays at one level; `validate.js` dispatches agents directly rather than calling `code-verify`.
- Commit messages use imperative subjects and add a body only for a *why* the diff cannot show. Never add AI-agent attribution or session trailers.
- Never stage `.idea/`. Scope every `git add` to named paths.
- Timeout for `codex exec` is `1800s`; a full-diff review at xhigh effort routinely passes ten minutes.

---

### Task 1: The enforcing launcher

**Files:**
- Create: `.claude/codex-agent.py`
- Create: `.claude/test/test_codex_agent.py`
- Modify: `.pre-commit-config.yaml` (add a hook after the `ci-select-test` block ending at line 102)

**Interfaces:**
- Consumes: `.codex/agents/<role>.toml` keys `model`, `model_reasoning_effort`, `developer_instructions`.
- Produces: CLI `python3 .claude/codex-agent.py --role <role> --prompt-file <path> --schema-file <path>`; prints the Codex result JSON on stdout, exit 0. Exit 2 for a disallowed role, exit 1 for any other failure. Module-level constant `READ_ONLY_ROLES` and function `resolve_adapter(root, role)` are imported by the test.

- [ ] **Step 1: Write the failing test**

Create `.claude/test/test_codex_agent.py`:

```python
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LAUNCHER = ROOT / '.claude' / 'codex-agent.py'

sys.path.insert(0, str(LAUNCHER.parent))
codex_agent = __import__('codex-agent')


def run(role, extra=None):
    with tempfile.TemporaryDirectory() as tmp:
        prompt = Path(tmp) / 'prompt.txt'
        schema = Path(tmp) / 'schema.json'
        prompt.write_text('review the diff')
        schema.write_text('{"type": "object"}')
        return subprocess.run(
            [sys.executable, str(LAUNCHER), '--role', role,
             '--prompt-file', str(prompt), '--schema-file', str(schema)],
            capture_output=True, text=True, cwd=str(ROOT), env=extra)


class AllowlistTest(unittest.TestCase):
    def test_read_only_roles_is_exactly_two(self):
        self.assertEqual(codex_agent.READ_ONLY_ROLES,
                         frozenset({'code-verifier', 'pr-review-validator'}))

    def test_write_role_is_rejected_before_reading_the_adapter(self):
        # code-writer.toml exists on disk, so a rejection here proves the
        # allowlist runs before any adapter path is opened.
        self.assertTrue((ROOT / '.codex' / 'agents' / 'code-writer.toml').is_file())
        result = run('code-writer')
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, '')
        self.assertIn('code-writer', result.stderr)

    def test_unknown_role_is_rejected(self):
        result = run('not-a-role')
        self.assertEqual(result.returncode, 2)
        self.assertEqual(result.stdout, '')

    def test_resolve_adapter_refuses_a_disallowed_role(self):
        with self.assertRaises(ValueError):
            codex_agent.resolve_adapter(ROOT, 'hil-operator')

    def test_resolve_adapter_reads_an_allowed_role(self):
        adapter = codex_agent.resolve_adapter(ROOT, 'code-verifier')
        self.assertTrue(adapter['model'])
        self.assertTrue(adapter['developer_instructions'].strip())

    def test_missing_prompt_file_exits_one_with_empty_stdout(self):
        result = subprocess.run(
            [sys.executable, str(LAUNCHER), '--role', 'code-verifier',
             '--prompt-file', '/nonexistent/p', '--schema-file', '/nonexistent/s'],
            capture_output=True, text=True, cwd=str(ROOT))
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout, '')


if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run test to verify it fails**

Run: `python3 -m unittest discover -s .claude/test -p 'test_codex_agent*.py' -v`
Expected: FAIL — `ModuleNotFoundError: No module named 'codex-agent'`

- [ ] **Step 3: Write the launcher**

Create `.claude/codex-agent.py`:

```python
#!/usr/bin/env python3
"""Run one read-only TinyUSB agent role on Codex and print its JSON result.

The role allowlist here is the enforcement boundary: the Markdown bridge that
invokes this script cannot be trusted to honour a rule stated only in prose.
"""

import argparse
import json
import subprocess
import sys
import tempfile
import tomllib
from pathlib import Path

READ_ONLY_ROLES = frozenset({'code-verifier', 'pr-review-validator'})

TIMEOUT = '1800s'


def repo_root():
    out = subprocess.run(['git', 'rev-parse', '--show-toplevel'],
                         capture_output=True, text=True, check=True)
    return Path(out.stdout.strip())


def resolve_adapter(root, role):
    if role not in READ_ONLY_ROLES:
        raise ValueError(
            f'role {role!r} is not in the read-only set '
            f'({", ".join(sorted(READ_ONLY_ROLES))}); '
            'write-capable roles are not routable to Codex')
    with open(Path(root) / '.codex' / 'agents' / f'{role}.toml', 'rb') as f:
        return tomllib.load(f)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--role', required=True)
    parser.add_argument('--prompt-file', required=True)
    parser.add_argument('--schema-file', required=True)
    args = parser.parse_args()

    root = repo_root()
    try:
        adapter = resolve_adapter(root, args.role)
    except ValueError as e:
        print(e, file=sys.stderr)
        return 2

    prompt = Path(args.prompt_file).read_text()
    schema = Path(args.schema_file).read_text()
    json.loads(schema)  # reject a malformed schema before spending a Codex run

    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        composed = tmp / 'prompt.txt'
        composed.write_text(
            adapter['developer_instructions'].rstrip() + '\n\n' + prompt)
        schema_path = tmp / 'schema.json'
        schema_path.write_text(schema)
        result_path = tmp / 'result.json'
        with open(composed, 'rb') as stdin:
            run = subprocess.run([
                'timeout', TIMEOUT, 'codex', 'exec',
                '-C', str(root),
                '-m', adapter['model'],
                '-c', f'model_reasoning_effort={adapter["model_reasoning_effort"]}',
                '--sandbox', 'read-only',
                '--output-schema', str(schema_path),
                '--output-last-message', str(result_path),
                '-',
            ], stdin=stdin, capture_output=True, text=True)
        if run.returncode != 0:
            print(run.stderr, file=sys.stderr)
            return 1
        if not result_path.is_file():
            print('codex produced no result file', file=sys.stderr)
            return 1
        body = result_path.read_text()

    json.loads(body)  # never hand the caller something that is not JSON
    sys.stdout.write(body)
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except Exception as e:  # any failure prints nothing to stdout
        print(f'{type(e).__name__}: {e}', file=sys.stderr)
        sys.exit(1)
```

- [ ] **Step 4: Run test to verify it passes**

Run: `python3 -m unittest discover -s .claude/test -p 'test_codex_agent*.py' -v`
Expected: PASS, 6 tests

- [ ] **Step 5: Add the pre-commit hook**

In `.pre-commit-config.yaml`, immediately after the `ci-select-test` hook block (which ends with its `language: system` at line 102), insert:

```yaml
  - id: codex-agent-launcher
    name: codex-agent-launcher
    files: ^\.claude/(codex-agent\.py|test/)
    entry: python3 -m unittest discover -s .claude/test -p 'test_codex_agent*.py'
    pass_filenames: false
    language: system
```

- [ ] **Step 6: Verify the hook runs**

Run: `pre-commit run codex-agent-launcher --all-files`
Expected: `codex-agent-launcher.....Passed`

- [ ] **Step 7: Commit**

```bash
git add .claude/codex-agent.py .claude/test/test_codex_agent.py .pre-commit-config.yaml
git commit -m "agents: add an enforcing launcher for read-only Codex roles

The allowlist has to live in code that runs. A Markdown bridge can only
promise to reject a write-capable role, and a scan of the workflow tree
is defeated by a spread override or a call from outside it."
```

---

### Task 2: Thin bridge and routing

**Files:**
- Create: `.claude/agents/codex-agent.md`
- Delete: `.claude/agents/codex-code-verifier.md`
- Modify: `.claude/workflows/code-verify.js:20`
- Modify: `.claude/workflows/validate.js:173-181`
- Modify: `.claude/workflows/test/test-code-verify.mjs` (stub mapping at line 101, assertions at lines 60, 160-166)
- Modify: `.pre-commit-config.yaml:87` (the `code-verify-logic` `files:` pattern names the deleted file)
- Modify: `CLAUDE.md:29`

**Interfaces:**
- Consumes: the launcher CLI from Task 1.
- Produces: agent type `codex-agent`, taking a JSON string `{"role": string, "prompt": string, "schema": object}` and returning schema-valid JSON. Replaces agent type `codex-code-verifier` at every call site.

- [ ] **Step 1: Write the failing test**

`test-code-verify.mjs` names the old bridge in ten places. Migrate all of them, or the suite fails on a missing file rather than on the behaviour under test. Run `grep -n codex-code-verifier .claude/workflows/test/test-code-verify.mjs` and confirm you have covered every hit.

Line 23 — the provider stub mapping:

```js
      : options.agentType === 'codex-agent' ? 'codex' : 'unknown'
```

Line 58 and the `deepEqual` on line 60 — the `defaults to codex` assertions:

```js
  assert.equal(calls[0].options.agentType, 'codex-agent')
  assert.deepEqual(JSON.parse(calls[0].prompt),
    { role: 'code-verifier', prompt: 'review', schema: RESULT })
```

Lines 63-70 — the `bridge owns the Codex subprocess contract` check now covers both halves. Replace the whole check with:

```js
await check('the launcher owns the Codex subprocess contract', async () => {
  const launcher = readFileSync(new URL('../../codex-agent.py', import.meta.url), 'utf8')
  assert.match(launcher, /READ_ONLY_ROLES = frozenset\(\{'code-verifier', 'pr-review-validator'\}\)/)
  assert.match(launcher, /tomllib/)
  assert.match(launcher, /'timeout', TIMEOUT, 'codex', 'exec'/)
  assert.match(launcher, /'--sandbox', 'read-only'/)
  assert.match(launcher, /--output-schema/)
  // no sandbox knob may be reachable from the caller
  assert.doesNotMatch(launcher, /add_argument\(['"]--sandbox/)

  const bridge = readFileSync(new URL('../../agents/codex-agent.md', import.meta.url), 'utf8')
  assert.match(bridge, /model: haiku/)
  assert.match(bridge, /effort: low/)
  assert.match(bridge, /python3 \.claude\/codex-agent\.py/)
  assert.doesNotMatch(bridge, /codex exec/, 'the bridge must not run codex itself')
})
```

Line 161 — the `validate dispatches directly` source assertion:

```js
  assert.match(src, /agentType:\s*'codex-agent'/)
```

Line 181 — the second stub, inside `validate keeps provider results and gates separate`:

```js
    if (options.agentType === 'codex-agent') return failCodex ? null : {
```

Lines 195, 238, 248 and 252 — the expected call arrays. Replace every `'codex-code-verifier'` element with `'codex-agent'`.

Then add a repository-wide guard that the name is gone, and the drift scan. **Insert both before the final `console.log(failed ...)` line — checks appended after `process.exit` never run:**

```js
await check('the retired bridge is referenced nowhere', async () => {
  const dir = new URL('../', import.meta.url)
  for (const name of readdirSync(dir).filter(n => n.endsWith('.js'))) {
    assert.doesNotMatch(readFileSync(new URL(name, dir), 'utf8'), /codex-code-verifier/, name)
  }
})
```

```js
await check('every codex-agent dispatch names a literal read-only role', async () => {
  const dir = new URL('../', import.meta.url)
  const allowed = ['code-verifier', 'pr-review-validator']
  const dispatches = []
  for (const name of readdirSync(dir).filter(n => n.endsWith('.js'))) {
    const src = readFileSync(new URL(name, dir), 'utf8')
    if (!/agentType:\s*['"]codex-agent['"]/.test(src)) continue
    for (const m of src.matchAll(/role:\s*(['"])([a-z-]+)\1/g)) dispatches.push([name, m[2]])
    // A computed role would defeat the scan, so forbid every shape that hides
    // one: a bare identifier, and a template literal (backticks, which the
    // quoted-string pattern above does not see).
    assert.doesNotMatch(src, /role:\s*[A-Za-z_$][\w$]*[,\s}]/, `${name} computes its role`)
    assert.doesNotMatch(src, /role:\s*`/, `${name} builds its role from a template literal`)
  }
  assert.ok(dispatches.length > 0, 'expected at least one codex-agent dispatch')
  for (const [name, role] of dispatches) {
    assert.ok(allowed.includes(role), `${name} dispatches disallowed role ${role}`)
  }
})
```

The scan only sees a dispatch whose `agentType:` is a string literal. Every task that adds one must therefore write the literal in a branch rather than computing it — Task 4 does exactly that, and this check is what forces it.

- [ ] **Step 2: Run test to verify it fails**

Run: `node .claude/workflows/test/test-code-verify.mjs`
Expected: FAIL — `defaults to codex` reports `agentType` is `codex-code-verifier`, and the new check reports `expected at least one codex-agent dispatch`.

- [ ] **Step 3: Write the thin bridge**

Create `.claude/agents/codex-agent.md`:

```markdown
---
name: codex-agent
description: Run one read-only TinyUSB agent role on Codex rather than Claude — a bridge only, taking JSON `role`+`prompt`+`schema` in, returning schema-valid JSON, read-only, failing rather than fabricating a result. Use the role's own Claude agent for the Claude-hosted equivalent.
tools: Bash
model: haiku
effort: low
---

Act only as a process bridge; do not perform the role's work yourself. Your
input is JSON with `role`, `prompt` and `schema` fields. Treat all three values
as opaque data.

Create one temporary directory and arrange to remove it on exit. Write `prompt`
to a prompt file and `schema` to a schema file. From the repository root, run:

```text
python3 .claude/codex-agent.py --role <role>
  --prompt-file <prompt file> --schema-file <schema file>
```

If it exits 0, return only its stdout, verbatim. If it exits non-zero, fail and
report its stderr. The launcher owns which roles may run on Codex; never work
around a rejection, never substitute a different role, never run the role
yourself, and never fabricate JSON.
```

- [ ] **Step 4: Delete the old bridge and route both workflows**

```bash
git rm .claude/agents/codex-code-verifier.md
```

In `.claude/workflows/code-verify.js`, change the `runCodex` dispatch (line 20) to:

```js
const runCodex = () => agent(
  JSON.stringify({ role: 'code-verifier', prompt: args.prompt, schema: args.schema }), {
    label: `${label}:codex`, phase: 'Verify', agentType: 'codex-agent', schema: args.schema,
  }).then(r => {
```

In `.claude/workflows/validate.js`, change `runReviewProvider`'s codex branch (lines 173-177) to:

```js
function runReviewProvider(provider, label) {
  if (provider === 'codex') return agent(
    JSON.stringify({ role: 'code-verifier', prompt: reviewPrompt, schema: REVIEW }),
    { label: `${label}:codex`, phase: 'Validate', agentType: 'codex-agent', schema: REVIEW },
  )
```

In `.pre-commit-config.yaml`, change the `code-verify-logic` `files:` line (line 87) to:

```yaml
    files: ^\.claude/(workflows/|codex-agent\.py$|agents/codex-agent\.md$)
```

In `CLAUDE.md`, change the last clause of line 29 to name the new bridge:

```markdown
Keep workflow nesting to one level and the Codex subprocess in `.claude/codex-agent.py`, invoked by `.claude/agents/codex-agent.md`.
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `node .claude/workflows/test/test-code-verify.mjs && node .claude/workflows/test/test-pr-babysit.mjs`
Expected: every line `ok`, no `FAIL`

- [ ] **Step 6: Commit**

```bash
git add .claude/agents/codex-agent.md .claude/workflows/code-verify.js .claude/workflows/validate.js .claude/workflows/test/test-code-verify.mjs .pre-commit-config.yaml CLAUDE.md
git commit -m "workflows: route Codex roles through the generic bridge

One bridge per role does not scale past the verifier, and the role name
is the only thing that differed between them."
```

---

### Task 3: Rename the provider value `both` to `all`

**Files:**
- Modify: `.claude/workflows/code-verify.js:3,8,15,16,41`
- Modify: `.claude/workflows/validate.js:13,128,129,134,223`
- Modify: `.claude/workflows/full-check.js:9`
- Modify: `.claude/workflows/test/test-code-verify.mjs` (lines 86-88, 100, 114, 160, 192-260)

**Interfaces:**
- Consumes: nothing new.
- Produces: `provider` and `reviewProvider` accept `'codex' | 'claude' | 'all'`. `'both'` is no longer accepted anywhere; no alias is kept.

- [ ] **Step 1: Write the failing test**

Run `grep -n both .claude/workflows/test/test-code-verify.mjs` and migrate every hit — two of them are inside assertion *regexes*, not values, and are easy to miss.

Replace every provider *value* `'both'` with `'all'` (lines 88, 114, 192, 201, 209, 247, 251, 260), rename the check title on line 86 to `runs all providers independently`, and rename the local `const both = ...` on line 247 to `const all` with its use on line 248.

Change the rejection assertion on line 100 to:

```js
    /provider must be codex, claude, or all/,
```

the source assertion on line 160 to:

```js
  assert.match(src, /const reviewProvider = reviewStageNames\.length === 2 \? 'all'/)
```

and the two `reviewProvider` message regexes on lines 261 and 276 to:

```js
    /reviewProvider "all" is cancelled by skip/,
```

```js
    await assert.rejects(runValidate({ reviewProvider }), /reviewProvider must be codex, claude, or all/)
```

Add a check that the old value is gone. **Insert it before the final `console.log(failed ...)` line:**

```js
await check('the retired provider value is accepted nowhere', async () => {
  const dir = new URL('../', import.meta.url)
  for (const name of readdirSync(dir).filter(n => n.endsWith('.js'))) {
    const src = readFileSync(new URL(name, dir), 'utf8')
    assert.doesNotMatch(src, /['"]both['"]/, `${name} still offers the 'both' provider value`)
  }
  await assert.rejects(
    run({ prompt: 'review', schema: RESULT, provider: 'both' }),
    /provider must be codex, claude, or all/,
  )
})
```

- [ ] **Step 2: Run test to verify it fails**

Run: `node .claude/workflows/test/test-code-verify.mjs`
Expected: FAIL — `provider must be codex, claude, or both` does not match the expected message.

- [ ] **Step 3: Rename in the workflows**

In `.claude/workflows/code-verify.js`: line 3 description `Codex (default), Claude, or all independently`; line 8 comment `provider?: 'codex'|'claude'|'all'`; line 15 `if (!['codex', 'claude', 'all'].includes(provider))`; line 16 message `provider must be codex, claude, or all`. Line 41's `const [codex, claude] = await parallel(...)` is reached only when the provider is neither `codex` nor `claude`, so it needs no change beyond the guard above.

In `.claude/workflows/validate.js`: line 13 comment `reviewProvider?: 'codex'|'claude'|'all'`; line 128 `if (!['codex', 'claude', 'all'].includes(requestedProvider))`; line 129 message `reviewProvider must be codex, claude, or all`; line 134 `const reviewProvider = reviewStageNames.length === 2 ? 'all'`; line 223 `reviewProvider === 'all'`.

In `.claude/workflows/full-check.js`: line 9 comment `reviewProvider?: 'codex'|'claude'|'all'`.

- [ ] **Step 4: Run tests to verify they pass**

Run: `node .claude/workflows/test/test-code-verify.mjs`
Expected: every line `ok`, no `FAIL`

- [ ] **Step 5: Commit**

```bash
git add .claude/workflows/code-verify.js .claude/workflows/validate.js .claude/workflows/full-check.js .claude/workflows/test/test-code-verify.mjs
git commit -m "workflows: rename the provider value both to all

It names a count, and a third provider would make the name a lie."
```

---

### Task 4: Route pr-review-validator to Codex

> **Superseded — this task was built and then reverted.** `codex exec --sandbox
> read-only` has no network, so every `gh` call in the role failed and it
> reported an empty harvest as a settled one on PR #3902. The `reviewValidator`
> knob and the dispatch below are gone; validation stays on Claude and the role
> is off the launcher's allowlist. The reasoning is in the design doc, and
> #3903 covers what would have to change for a role like this to run on Codex.
>
> The revert reaches forward, so read the rest of the plan through it. Task 1's
> `READ_ONLY_ROLES` lost `pr-review-validator`. Every later `reviewValidator`
> reference is dead text: the `args: { reviewValidator: 'claude' }` checks and
> the `reviewValidator === 'codex'` gate in Task 5, and the CLAUDE.md bullet
> quoted in Task 6. What shipped is one architecture with no knob — reviews
> always dispatch to the native `pr-review-validator` on Claude, and Task 5's
> challenge stage always runs on Codex, so `contested` is filtered straight off
> `r.findings` and Task 5's `provider: 'claude'` reads `'codex'`.

**Files:**
- Modify: `.claude/workflows/pr-babysit.js:8-12` (args docs and shape check), `:386-390` (the reviews dispatch)
- Modify: `.claude/workflows/test/test-pr-babysit.mjs` (the stub's `reviews#` branch, plus new checks)

**Interfaces:**
- Consumes: agent type `codex-agent` from Task 2.
- Produces: `args.reviewValidator` of type `'claude' | 'codex'`, defaulting to `'codex'`. Module-level `const reviewValidator` is read by Task 5's challenge gate.

- [ ] **Step 1: Write the failing test**

Add to `.claude/workflows/test/test-pr-babysit.mjs`, **before the final `console.log(failed ...)` line** — a check after `process.exit` never runs:

```js
await check('reviews default to the codex bridge with a literal role', async () => {
  const { calls } = await run({ captureOptions: true })
  const reviews = calls.find(c => c.label.startsWith('reviews#'))
  assert.equal(reviews.agentType, 'codex-agent')
  assert.deepEqual(JSON.parse(reviews.prompt).role, 'pr-review-validator')
})

await check('reviewValidator claude keeps the native validator', async () => {
  const { calls } = await run({ captureOptions: true, args: { reviewValidator: 'claude' } })
  const reviews = calls.find(c => c.label.startsWith('reviews#'))
  assert.equal(reviews.agentType, 'pr-review-validator')
  assert.equal(reviews.prompt.includes('"role"'), false)
})

await check('an unknown reviewValidator is rejected before dispatch', async () => {
  await assert.rejects(run({ args: { reviewValidator: 'gemini' } }),
    /reviewValidator must be claude or codex/)
})
```

If `run()` does not already record agent options, extend its stub to push `{ label, prompt, agentType: options.agentType }` onto a `calls` array and return it alongside `logs`, and let `opts.args` merge into the args object the workflow is invoked with.

- [ ] **Step 2: Run test to verify it fails**

Run: `node .claude/workflows/test/test-pr-babysit.mjs`
Expected: FAIL — `agentType` is `pr-review-validator`, not `codex-agent`.

- [ ] **Step 3: Add the knob and route the dispatch**

In `.claude/workflows/pr-babysit.js`, extend the args comment at line 8 to include `reviewValidator?: 'claude'|'codex' (default 'codex')`, extend the `args must be` message at line 11 with `reviewValidator?`, and after the `maxCycles` validation (line 26) add:

```js
const reviewValidator = args.reviewValidator ?? 'codex'
if (!['claude', 'codex'].includes(reviewValidator)) {
  throw new Error('reviewValidator must be claude or codex')
}
```

Replace the reviews dispatch (lines 386-390) with the shape below. The two branches each carry a **string-literal** `agentType`: Task 2's drift scan only recognises a literal, so a ternary inside the options object would silently escape it, and the existing `entry.error` text is left alone so the check at `test-pr-babysit.mjs:236` keeps passing.

```js
    const reviewPrompt =
      `Validate the bot review findings on PR #${args.pr} per your procedure. ${IN_CHECKOUT}` +
      (deferredComments.size > 0
        ? 'These comments still owe an answer from an earlier cycle; report their findings again ' +
          `so they can be reconciled: ${JSON.stringify([...deferredComments])}. ` : '')
    const dispatch = reviewValidator === 'codex'
      ? {
        prompt: JSON.stringify({ role: 'pr-review-validator', prompt: reviewPrompt, schema: REVIEWS }),
        agentType: 'codex-agent',
      }
      : { prompt: reviewPrompt, agentType: 'pr-review-validator' }
    const r = await agent(dispatch.prompt, {
      label: `reviews#${cycle}`, phase: 'Triage', agentType: dispatch.agentType, schema: REVIEWS,
    }).catch(e => { log(`cycle ${cycle}: review validator errored — ${e && e.message}`); return null })
    if (!r) {
      entry.error = 'pr-review-validator died'
      return { pass: false, cycles: cycle, history, reason: 'review-validator-died' }
    }
```

`deferredComments` is declared in Task 5. Until that task lands, write this dispatch **without** the `deferredComments` clause — the plain `reviewPrompt` string — and add the clause as part of Task 5. Task 4's tests do not exercise it.

- [ ] **Step 4: Run tests to verify they pass**

Run: `node .claude/workflows/test/test-pr-babysit.mjs && node .claude/workflows/test/test-code-verify.mjs`
Expected: every line `ok`, no `FAIL` — including Task 2's literal-role scan, which now sees a second dispatch.

- [ ] **Step 5: Commit**

```bash
git add .claude/workflows/pr-babysit.js .claude/workflows/test/test-pr-babysit.mjs
git commit -m "pr-babysit: validate bot findings with Codex by default

An independent model is worth most on the stage that decides which
reviewer comments get publicly refuted."
```

---

### Task 5: The Claude challenge chain

**Files:**
- Modify: `.claude/workflows/pr-babysit.js` — workflow-scope state beside `repliedIds`; a challenge stage after the reviews dispatch; the `doneIds` loops at `:413` and `:457`; the post-fix payload at `:447`; the green block at `:502`; the exhaustion return at `:557`; `cycleSummary`'s Outcome column; the `reviewPrompt` clause deferred from Task 4
- Modify: `.claude/workflows/test/test-pr-babysit.mjs`

**Interfaces:**
- Consumes: `reviewValidator` from Task 4; the nested `code-verify` workflow with `provider: 'claude'`.
- Produces: workflow-scope `const deferredComments = new Set()`; cycle-scope `const suppressed = new Set()`; result field `deferred: number[]` present whenever `reason` is `'deferred-replies-unresolved'`; finding property `overturned: true`.

**Two sets, not one.** `deferredComments` is the *obligation* and spans cycles; it is only ever cleared by a `doneIds` that proves the reply posted and the thread resolved. `suppressed` is *this cycle's* silence and is rebuilt each cycle; it is what the post-fix payload filters on. Collapsing them would either clear an unposted obligation or deadlock a comment out of the very stage that could discharge it.

**The challenger goes through the router.** `pr-babysit` already reaches a verifier via `workflow('code-verify', ...)` at line 250, and `test-code-verify.mjs`'s `only validate bypasses the router` check forbids a bare `agentType: 'code-verifier'` anywhere but `code-verify.js` and `validate.js`. The challenge stage uses the same nested call with `provider: 'claude'`.

- [ ] **Step 1: Write the failing test**

First teach the `workflow` stub (line 58) to answer the challenge call **and to record it**. The existing `labels` array only captures `agent()` calls, so a challenge routed through `workflow()` would be invisible and any "no challenge happened" assertion would pass vacuously. Replace the stub with:

```js
  const workflowCalls = []
  const workflow = async (name, wargs) => {
    workflowCalls.push({
      name,
      label: (wargs && wargs.label) || '',
      prompt: (wargs && wargs.prompt) || '',
      provider: wargs && wargs.provider,
    })
    if (String((wargs && wargs.label) || '').startsWith('challenge#')) {
      if (!('challenge' in opts)) {
        // default: uphold every submitted dismissal, i.e. today's behaviour
        const ids = [...String(wargs.prompt).matchAll(/"id":(\d+)/g)].map(m => Number(m[1]))
        return { verdicts: ids.map(id => ({ id, upheld: true, reason: 'stands' })) }
      }
      return opts.challenge === null ? null : structuredClone(opts.challenge)
    }
    return structuredClone(opts.verify ?? { addresses: true, reason: 'verified' })
  }
```

and add `workflowCalls` to the object `run()` returns (line 70), keeping every field already there plus the `calls` array Task 4 added:

```js
    return { result, logs, labels, calls, napPoints, workflowCalls }
```

`'challenge' in opts` is what separates "the test did not care" from "the challenger died"; `opts.challenge ?? default` would collapse the two and make the dead-challenger case untestable.

**`run()` merges only `opts.args`** into the workflow's arguments (line 68: `{ pr: 3888, maxCycles: 1, autoPush: true, ...opts.args }`). A top-level `maxCycles:` or `autoPush:` on the options object is silently ignored, so every check below passes them inside `args: { ... }`.

Then add these checks **before the final `console.log(failed ...)` line**:

```js
const invalidFinding = (over = {}) => finding({ verdict: 'invalid', ...over })

await check('no challenge stage when Claude validates', async () => {
  const { workflowCalls } = await run({
    args: { reviewValidator: 'claude' },
    reviews: { findings: [invalidFinding()], replies: [{ commentId: 1, body: 'no' }], done: true },
  })
  assert.equal(workflowCalls.some(w => w.label.startsWith('challenge#')), false)
})

await check('the challenge routes through code-verify with the Claude provider', async () => {
  const { workflowCalls } = await run({
    reviews: { findings: [invalidFinding()], replies: [{ commentId: 1, body: 'no' }], done: true },
  })
  const ch = workflowCalls.find(w => w.label.startsWith('challenge#'))
  assert.ok(ch, 'expected a challenge stage')
  assert.equal(ch.name, 'code-verify')
  // without this the challenge would silently route back to Codex, and Codex
  // would be checking its own refutation
  assert.equal(ch.provider, 'claude')
})

await check('an upheld refutation still replies and resolves', async () => {
  const { calls } = await run({
    reviews: { findings: [invalidFinding()], replies: [{ commentId: 1, body: 'no' }], done: true },
    challenge: { verdicts: [{ id: 0, upheld: true, reason: 'stands' }] },
    args: { autoPush: true },
  })
  const posted = calls.find(c => c.label.startsWith('replies#'))
  assert.ok(posted, 'expected the refutation to be posted')
  assert.match(posted.prompt, /"commentId":1/)
})

await check('an overturned finding is fixed, replied to, and carries the challenger reason', async () => {
  const { calls } = await run({
    reviews: { findings: [invalidFinding()], replies: [{ commentId: 1, body: 'no' }], done: true },
    challenge: { verdicts: [{ id: 0, upheld: false, reason: 'the NAK path is real' }] },
    args: { autoPush: true },
  })
  assert.equal(calls.some(c => c.label.startsWith('replies#')), false, 'no refutation is posted')
  const fix = calls.find(c => c.label.startsWith('fix:'))
  assert.match(fix.prompt, /the NAK path is real/)
  // every finding on the comment was overturned, so the fix note is NOT withheld
  const resolve = calls.find(c => c.label.startsWith('resolve#'))
  assert.ok(resolve, 'expected the fix note to be posted')
  assert.match(resolve.prompt, /"commentId":1/)
})

await check('a mixed comment defers its reply and blocks the green exit', async () => {
  const { calls, result } = await run({
    reviews: {
      findings: [invalidFinding({ commentId: 7 }), invalidFinding({ commentId: 7, line: 9 })],
      replies: [{ commentId: 7, body: 'both wrong' }],
      done: true,
    },
    challenge: { verdicts: [
      { id: 0, upheld: false, reason: 'real' },
      { id: 1, upheld: true, reason: 'stands' },
    ] },
    args: { autoPush: true, maxCycles: 1 },
  })
  assert.equal(calls.some(c => c.label.startsWith('replies#')), false)
  const resolve = calls.find(c => c.label.startsWith('resolve#'))
  if (resolve) assert.doesNotMatch(resolve.prompt, /"commentId":7/)
  assert.equal(result.pass, false)
  assert.equal(result.reason, 'deferred-replies-unresolved')
  assert.deepEqual(result.deferred, [7])
})

await check('a broken challenge response fails the cycle', async () => {
  for (const challenge of [
    null,
    { verdicts: [] },                                             // missing id
    { verdicts: [{ id: 9, upheld: true, reason: 'x' }] },          // unknown id
    { verdicts: [{ id: 0, upheld: true, reason: 'x' },
                 { id: 0, upheld: false, reason: 'y' }] },         // duplicate id
  ]) {
    const { result } = await run({
      reviews: { findings: [invalidFinding()], replies: [{ commentId: 1, body: 'no' }], done: true },
      challenge, args: { autoPush: true, maxCycles: 1 },
    })
    assert.equal(result.reason, 'review-challenger-died')
  }
})

await check('a deferred obligation is cleared only by a posted reply', async () => {
  // Cycle 1 defers comment 7 (one overturned, one upheld). Cycle 2 sees only
  // the upheld one, posts it, and that posting is what clears the deferral.
  let cycle = 0
  const { result } = await run({
    args: { autoPush: true, maxCycles: 3 },
    reviewsPerCycle: () => {
      cycle++
      return cycle === 1
        ? { findings: [invalidFinding({ commentId: 7 }), invalidFinding({ commentId: 7, line: 9 })],
            replies: [{ commentId: 7, body: 'both wrong' }], done: true }
        : { findings: [invalidFinding({ commentId: 7, line: 9 })],
            replies: [{ commentId: 7, body: 'still wrong' }], done: true }
    },
    challengePerCycle: () => cycle === 1
      ? { verdicts: [{ id: 0, upheld: false, reason: 'real' }, { id: 1, upheld: true, reason: 'stands' }] }
      : { verdicts: [{ id: 0, upheld: true, reason: 'stands' }] },
  })
  assert.equal(result.pass, true, 'the posted reply discharges the deferral')
})

await check('the summary marks an overturned finding', async () => {
  const { logs } = await run({
    reviews: { findings: [invalidFinding()], replies: [{ commentId: 1, body: 'no' }], done: true },
    challenge: { verdicts: [{ id: 0, upheld: false, reason: 'real' }] },
    args: { autoPush: true, maxCycles: 1 },
  })
  assert.ok(logs.some(l => l.includes('overturned')), 'cycle table must show the overturn')
})
```

The last two checks need `run()` to accept per-cycle answers. Extend it so `opts.reviewsPerCycle` and `opts.challengePerCycle`, when present, are called instead of returning the fixed `opts.reviews` / `opts.challenge`.

- [ ] **Step 2: Run test to verify it fails**

Run: `node .claude/workflows/test/test-pr-babysit.mjs`
Expected: FAIL — no `challenge#` agent is ever dispatched, so the overturn and defer checks all report the refutation was posted.

- [ ] **Step 3: Implement the challenge stage**

In `.claude/workflows/pr-babysit.js`, beside the existing `repliedIds` declaration, add:

```js
// Comments whose refutation is owed but could not be posted this cycle: the
// challenger overturned one finding on them while another was upheld, so the
// drafted body would refute a finding we just accepted. Spans cycles - a
// per-cycle counter cannot hold it, because a review-lane push re-arms before
// the green exit is ever read.
const deferredComments = new Set()
```

Add the `CHALLENGE` schema beside `REVIEWS`:

```js
const CHALLENGE = {
  type: 'object', additionalProperties: false,
  required: ['verdicts'],
  properties: {
    verdicts: {
      type: 'array',
      items: {
        type: 'object', additionalProperties: false,
        required: ['id', 'upheld', 'reason'],
        properties: {
          id: { type: 'integer' }, upheld: { type: 'boolean' }, reason: { type: 'string' },
        },
      },
    },
  },
}
```

Immediately after the reviews dispatch's `if (!r)` guard, insert the challenge stage:

```js
    // Codex decided which reviewer comments deserve a public refutation; an
    // independent model checks those before any of them is posted.
    const suppressed = new Set() // this cycle's silence; deferredComments is the obligation
    const contested = reviewValidator === 'codex'
      ? r.findings.filter(f => f.verdict !== 'valid') : []
    if (contested.length > 0) {
      const submitted = contested.map((f, id) => ({
        id, commentId: f.commentId, file: f.file, line: f.line,
        claim: f.claim, verdict: f.verdict, reason: f.reason,
      }))
      const ch = await workflow('code-verify', {
        provider: 'claude',
        label: `challenge#${cycle}`,
        schema: CHALLENGE,
        prompt: `${IN_CHECKOUT}Another reviewer dismissed these findings on PR #${args.pr}; each ` +
          'dismissal is about to be posted publicly and will close the reviewer\'s thread. ' +
          'For every id, decide whether the dismissal holds. upheld=true means the dismissal is ' +
          'correct and the finding really is invalid or already fixed; upheld=false means the ' +
          'finding is real and must be fixed, and reason is the evidence that shows it. ' +
          'Return exactly one verdict per submitted id and no others.\n' +
          `Findings: ${JSON.stringify(submitted)}.`,
      }).catch(e => { log(`cycle ${cycle}: challenger errored — ${e && e.message}`); return null })

      const ids = new Set(submitted.map(s => s.id))
      const seen = new Set()
      const complete = ch && Array.isArray(ch.verdicts) &&
        ch.verdicts.length === submitted.length &&
        ch.verdicts.every(v => ids.has(v.id) && !seen.has(v.id) && (seen.add(v.id), true))
      if (!complete) {
        // Silence must never become a public claim that a reviewer was wrong.
        log(`cycle ${cycle}: challenge incomplete — refutations withheld`)
        entry.error = 'review challenger died'
        return { pass: false, cycles: cycle, history, reason: 'review-challenger-died' }
      }

      const verdictOfId = new Map(ch.verdicts.map(v => [v.id, v]))
      const touched = new Set()
      for (const s of submitted) {
        const v = verdictOfId.get(s.id)
        if (v.upheld) continue
        const f = contested[s.id]
        f.verdict = 'valid'
        f.overturned = true          // rendered by cycleSummary's valid arm
        f.fixHint = v.reason         // the evidence, not the dismissal it replaced
        touched.add(f.commentId)
      }
      // A drafted body covers a whole comment and was written believing every
      // finding on it invalid, so a partial overturn cannot reuse it, and
      // recomposing it would invent prose the challenger never wrote.
      for (const commentId of touched) {
        r.replies = r.replies.filter(x => x.commentId !== commentId)
        repliedIds.delete(commentId)
        if (r.findings.some(f => f.commentId === commentId && f.verdict !== 'valid')) {
          // Mixed: a sibling refutation is still owed, but the drafted body would
          // deny the finding we just accepted. Say nothing about this comment now.
          suppressed.add(commentId)
          deferredComments.add(commentId) // cleared only by a posted, resolved reply
        }
        // All overturned: nothing is owed to the reviewer, so the comment is NOT
        // suppressed - it flows to the post-fix stage and the fix note answers it.
      }
    }
```

Change the post-fix reply payload (line 447) to skip the comments silenced **this cycle** — not every outstanding obligation. Filtering on `deferredComments` here would lock a comment out of the one stage that can discharge it:

```js
        `Findings: ${JSON.stringify(validFindings
          .filter(f => !suppressed.has(f.commentId))
          .map(f => ({ commentId: f.commentId, file: f.file, line: f.line, claim: f.claim, fixHint: f.fixHint })))}. ` +
```

Replace the `doneIds` loop at line 413 with the shape below, and give line 457 the same treatment for `resolved.doneIds`. A posted-and-resolved reply is the only thing that discharges a deferral:

```js
      for (const id of (posted && posted.doneIds) || []) { repliedIds.add(id); deferredComments.delete(id) }
```

In the green block, after the existing `pendingReplies` check and before the `pass: true` return:

```js
      if (deferredComments.size > 0) {
        if (cycle < maxCycles) {
          log(`cycle ${cycle}: PR green but ${deferredComments.size} refutation(s) still owed — re-arming`)
          napMs = 60000 * cycle
          return null
        }
        return { pass: false, cycles: cycle, history,
          reason: 'deferred-replies-unresolved', deferred: [...deferredComments] }
      }
```

Change the outer exhaustion return (line 557) to:

```js
return deferredComments.size > 0
  ? { pass: false, cycles: maxCycles, history, reason: 'deferred-replies-unresolved', deferred: [...deferredComments] }
  : { pass: false, cycles: maxCycles, history, reason: 'maxCycles reached' }
```

Finally, add the deferred clause to Task 4's `reviewPrompt`, which was written without it because `deferredComments` did not exist yet:

```js
    const reviewPrompt =
      `Validate the bot review findings on PR #${args.pr} per your procedure. ${IN_CHECKOUT}` +
      (deferredComments.size > 0
        ? 'These comments still owe an answer from an earlier cycle; report their findings again ' +
          `so they can be reconciled: ${JSON.stringify([...deferredComments])}. ` : '')
```

In `cycleSummary` (lines 327-333), mark both arms. An overturned finding now has `verdict === 'valid'`, so it takes the **`valid`** arm — marking only the refuted arm would never display it:

```js
    const valid = f.verdict === 'valid'
    rows.push([
      cell(f.source, 16),
      cell(`${f.file}:${f.line} ${f.claim}`),
      cell(f.overturned ? 'overturned' : f.verdict, 8),
      valid ? cell((f.overturned ? 'codex refuted → claude overturned, ' : '') +
        fixCell(entry.reviewFixes, f.commentId, entry.reviewPush, entry.reviewPushFailed), 60)
        : cell(`${f.verdict === 'stale' ? 'already fixed' : 'refuted'}, ${
          repliedIds.has(f.commentId) ? 'replied + resolved'
            : deferredComments.has(f.commentId) ? 'deferred to next cycle' : 'reply pending'}`, 60),
      valid ? shaOf(entry.reviewPush) : '-',
    ])
```

`VERDICT_ORDER` on line 325 sorts by `f.verdict`, which is now `'valid'` for an overturned finding — that is correct, it belongs with the other findings being fixed.

- [ ] **Step 4: Run tests to verify they pass**

Run: `node .claude/workflows/test/test-pr-babysit.mjs`
Expected: every line `ok`, no `FAIL`

- [ ] **Step 5: Commit**

```bash
git add .claude/workflows/pr-babysit.js .claude/workflows/test/test-pr-babysit.mjs
git commit -m "pr-babysit: challenge Codex refutations before posting them

A dismissal is published to a human reviewer and closes their thread, so
it is the one verdict worth a second, independent model. A comment whose
findings split goes unanswered for a cycle rather than carrying a body
that refutes the finding we just accepted."
```

---

### Task 6: Document the delegation in CLAUDE.md

**Files:**
- Modify: `CLAUDE.md:22-30` (the "Claude and Codex Collaboration" section)

**Interfaces:**
- Consumes: everything Tasks 1-5 produced.
- Produces: no code.

- [ ] **Step 1: Update the collaboration section**

In `CLAUDE.md`, replace the `code-verify` bullet (line 30) with:

```markdown
- Keep orchestration in `.claude/workflows/`. Use `code-verify` with `provider: 'codex'` (default), `'claude'`, or `'all'`; `validate`/`full-check` use `reviewProvider`, `pr-babysit` uses `reviewValidator` (`'codex'` default, with a Claude challenge before any refutation is posted).
- Only `code-verifier` and `pr-review-validator` may run on Codex: `.claude/codex-agent.py` enforces that allowlist and runs `codex exec --sandbox read-only`. Write-capable roles need worktree isolation first.
```

- [ ] **Step 2: Verify the whole harness still passes**

Run: `pre-commit run --all-files`
Expected: every hook `Passed` or `Skipped`; no `Failed`

- [ ] **Step 3: Commit**

```bash
git add CLAUDE.md
git commit -m "docs: record which roles may run on Codex"
```

---

### Task 7: The peer-agent skill

**Files:**
- Create: `.claude/skills/peer-agent/SKILL.md`
- Modify: `CLAUDE.md` (one pointer line in the collaboration section)

**Interfaces:**
- Consumes: the `herdr` CLI (0.9.0), which must already be on `PATH` with `HERDR_ENV=1`.
- Produces: skill `peer-agent`. No code artifacts.

- [ ] **Step 1: Write the skill**

Create `.claude/skills/peer-agent/SKILL.md`:

```markdown
---
name: peer-agent
description: Use when you want a second, independent opinion from a Codex session running beside you in the same worktree — design critique, adversarial review of a plan or spec, or a claim you want challenged before you act on it. Interactive consults only; schema'd workflow jobs belong in `codex exec` via `.claude/codex-agent.py`. Requires HERDR_ENV=1.
---

# Pairing with a Codex session

A Herdr pane in this worktree may already hold an idle Codex session. It is a
peer, not a subagent: it has its own conversation, its own memory, and no
knowledge of anything you have not told it.

## Find the peer

```bash
test "${HERDR_ENV:-}" = 1 || { echo "not inside Herdr"; exit 1; }
herdr agent list
```

Select the agent whose `agent` is `codex` and whose `cwd` equals the working
directory. Never create a pane, tab, workspace, or agent to get one: if there
is no Codex session in this worktree, say so and carry on alone.

## Send a consult

```bash
herdr agent prompt <pane-id> "<framed prompt>" --wait --timeout 600000
```

Frame every prompt with, in order: who you are and your pane id; whether edits
are allowed; the context needed to answer; the question; and a length cap.

State "read-only, no edits, no commits" whenever you do not want writes. The
peer pane may be running with full access, and it shares your working tree.

## Read the reply

```bash
herdr agent read <pane-id> --source recent-unwrapped --lines 250
```

If the reply is truncated because Codex renders on the alternate screen, ask it
to write the full answer to a scratch file and reply with only the path, then
read that file. Fallback only — never request file output up front.

## Traps

- **`--wait` settles on agent status, not on your request.** It can return on a
  state the peer was already in, handing you the *previous* answer. Confirm the
  reply addresses the prompt you just sent before trusting it. When in doubt,
  `herdr agent wait <pane-id>` and read again.
- **No shared history.** Every message carries its own context, including file
  paths and the commit you want reviewed.
- **Not a workflow transport.** Schema'd, one-shot jobs go through
  `.claude/codex-agent.py`, which has real completion and failure boundaries.
  This channel has neither.
- **The reply is advice.** Verify each claim against the files before acting.
  A peer's finding is a lead, not a result.
- **Leave the layout alone.** Do not close or move panes you did not create.

## Converging

For a review loop, iterate: send the artifact, apply what verifies, tell the
peer what you applied and what you rejected and why, then ask again. Stop when
it reports no findings and you agree. Asking for "NO FINDINGS" as an exact
reply when it has none makes the terminating condition unambiguous.
```

- [ ] **Step 2: Add the pointer**

In `CLAUDE.md`'s collaboration section, after the `/codex:review` bullet, add:

```markdown
- A sibling Codex pane in the same worktree is a peer for interactive consults; use the `peer-agent` skill. Chat history is not shared, so every message carries its own context.
```

- [ ] **Step 3: Verify the skill is well-formed**

Run: `pre-commit run --all-files`
Expected: every hook `Passed` or `Skipped`; no `Failed`

- [ ] **Step 4: Commit**

```bash
git add .claude/skills/peer-agent/SKILL.md CLAUDE.md
git commit -m "skills: add peer-agent for peer consults over Herdr"
```

---

### Task 8: Record the deferred writer work

> **Superseded during implementation.** `master` moved to tracking deferred
> work in GitHub issues (#3900) while this branch was in flight, so the
> handoff below belongs in `gh issue create --label followup`, not in a file
> under `docs/superpowers/followup/`. Nothing is committed for this task.


**Files:**
- Create: `docs/superpowers/followup/pr<NNN>-codex-write-roles.md` where `<NNN>` is the number of the PR that lands Tasks 1-6

**Interfaces:**
- Consumes: nothing.
- Produces: no code.

- [ ] **Step 1: Write the handoff**

Create the file with: the goal (route `code-writer`, `code-simplifier`, `builder`, `static-analyzer` to Codex); the two blockers (the launcher's hardcoded `--sandbox read-only` and `READ_ONLY_ROLES`; `fanout-dev.js` leaving worktree isolation optional, so file scopes alone do not stop a Codex writer and a Claude writer clobbering each other); the evidence (`.claude/codex-agent.py` `READ_ONLY_ROLES`, `.claude/workflows/fanout-dev.js`); and why deferred (writer isolation is a larger change than routing, and the read-only roles delivered the value without it).

- [ ] **Step 2: Commit**

```bash
git add docs/superpowers/followup/
git commit -m "docs: hand off Codex writer-role delegation"
```

---

## Self-Review

**Spec coverage:** Read-only role set → Task 1. Generic bridge, both halves → Tasks 1-2. `both`→`all` → Task 3. Routing table → Tasks 2, 4, 6. Challenge chain, every rule including deferral, retry branch and exhaustion → Task 5. Herdr peer channel skill → Task 7. Delivery split → Tasks 1-6 are PR A, Task 7 is PR B, Task 8 is the deferral. Verification section → the tests inside Tasks 1-5 plus the `pre-commit run --all-files` gates in Tasks 6 and 7.

**Type consistency:** `READ_ONLY_ROLES` and `resolve_adapter` are defined in Task 1 and used by its test only. Agent type `codex-agent` is introduced in Task 2 and reused in Task 4. `reviewValidator` was defined in Task 4 and read in Task 5; the Task 4 revert removed both and left the challenge stage unconditional. `deferredComments` and the `deferred` result field are defined in Task 5 and used nowhere earlier. `CHALLENGE`'s `{id, upheld, reason}` matches the verdict handling and every test case.
