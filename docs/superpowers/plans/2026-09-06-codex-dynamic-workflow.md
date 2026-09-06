# Codex-backed Dynamic Workflow Verification Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Route TinyUSB dynamic-workflow `code-verifier` calls through Codex by default, while allowing each call to select Claude or independent Claude-and-Codex verification.

**Architecture:** Claude Code remains the workflow runtime. A nested `code-verify` workflow owns provider selection, uses a Haiku/low process bridge for Codex, and reads the canonical Claude role plus Codex adapter at execution time; callers never copy the bridge command.

**Tech Stack:** Claude Code dynamic workflow JavaScript, Node.js standard library, Python `tomllib`, Codex CLI

**Spec:** `docs/superpowers/specs/2026-09-06-codex-dynamic-workflow-design.md`

## Global Constraints

- `.claude/workflows/` is the only authored workflow tree.
- `.claude/agents/code-verifier.md` is the only authored verifier role.
- `.codex/agents/code-verifier.toml` supplies the Codex model and effort; currently `gpt-5.6-sol` and `xhigh`.
- Provider defaults to `codex`; accepted values are `codex`, `claude`, and `both`.
- Codex failures are hard failures, never automatic Claude fallbacks.
- Codex is always read-only.
- Only `code-verifier` is routed; no other role changes provider.
- Use only existing runtimes and standard libraries.

---

### Task 1: Provider Router

**Files:**
- Create: `.claude/workflows/code-verify.js`
- Create: `.claude/workflows/test-code-verify.mjs`
- Modify: `.pre-commit-config.yaml`

**Interfaces:**
- Consumes: `{ prompt: string, schema: object, provider?: 'codex'|'claude'|'both', label?: string }`
- Produces: the requested schema for a single provider; `{ codex, claude }` for `both`

- [ ] **Step 1: Write failing router tests**

Create `test-code-verify.mjs` with a small harness that evaluates a workflow body inside an async function with mocked `agent` and `parallel` primitives. Add checks for:

```js
await check('defaults to codex', async () => {
  const { result, calls } = await run({ prompt: 'review', schema: RESULT }, answers)
  assert.deepEqual(result, answers.codex)
  assert.deepEqual(calls.map(c => c.provider), ['codex'])
})

await check('selects claude', async () => {
  const { result, calls } = await run(
    { prompt: 'review', schema: RESULT, provider: 'claude' }, answers)
  assert.deepEqual(result, answers.claude)
  assert.deepEqual(calls.map(c => c.provider), ['claude'])
})

await check('runs both independently', async () => {
  const { result, calls } = await run(
    { prompt: 'review', schema: RESULT, provider: 'both' }, answers)
  assert.deepEqual(result, { codex: answers.codex, claude: answers.claude })
  assert.deepEqual(calls.map(c => c.provider).sort(), ['claude', 'codex'])
})

await assert.rejects(run({ prompt: 'review', schema: RESULT, provider: 'auto' }, answers),
  /provider must be codex, claude, or both/)
await assert.rejects(run({ prompt: 'review', schema: RESULT }, { ...answers, codex: null }),
  /codex verifier failed/)
```

The mock identifies the native Claude call by `agentType: 'code-verifier'` and the Codex bridge by `model: 'haiku'`.

- [ ] **Step 2: Run the test and verify RED**

Run:

```bash
node .claude/workflows/test-code-verify.mjs
```

Expected: FAIL because `.claude/workflows/code-verify.js` does not exist.

- [ ] **Step 3: Implement the minimal router**

Create `code-verify.js` with:

```js
export const meta = {
  name: 'code-verify',
  description: 'Run the canonical code-verifier with Codex (default), Claude, or both independently',
  whenToUse: 'Nested by TinyUSB workflows that need structured code verification',
  phases: [{ title: 'Verify' }],
}

if (typeof args === 'string') { try { args = JSON.parse(args) } catch { args = null } }
if (!args || typeof args.prompt !== 'string' || !args.prompt.trim() ||
    !args.schema || typeof args.schema !== 'object' || Array.isArray(args.schema)) {
  throw new Error('args must be { prompt: string, schema: object, provider?, label? }')
}
const provider = args.provider || 'codex'
if (!['codex', 'claude', 'both'].includes(provider)) {
  throw new Error('provider must be codex, claude, or both')
}
const label = args.label || 'code-verifier'
const codexPrompt = `Act only as a process bridge; do not perform verification yourself.
From the repository root, read .codex/agents/code-verifier.toml with Python's
tomllib. Create one temporary directory and arrange to remove it on exit. Write
the JSON inside <schema> to a schema file. Write the adapter's
developer_instructions followed by the text inside <task> to a prompt file.
Run Codex with a 600-second timeout from the repository root:
codex exec -C <root> -m <adapter model>
  -c model_reasoning_effort=<adapter model_reasoning_effort>
  --sandbox read-only --output-schema <schema file>
  --output-last-message <result file> - < <prompt file>
If every command succeeds, return only the result file's JSON as your final
answer. If anything fails, do not perform the verification or fabricate JSON.
<schema>${JSON.stringify(args.schema)}</schema>
<task>${args.prompt}</task>`

const runCodex = () => agent(codexPrompt, {
  label: `${label}:codex`, phase: 'Verify', model: 'haiku', effort: 'low', schema: args.schema,
}).then(r => {
  if (!r) throw new Error('codex verifier failed')
  return r
})

const runClaude = () => agent(args.prompt, {
  label: `${label}:claude`, phase: 'Verify', agentType: 'code-verifier', schema: args.schema,
}).then(r => {
  if (!r) throw new Error('claude verifier failed')
  return r
})

if (provider === 'codex') return runCodex()
if (provider === 'claude') return runClaude()
const [codex, claude] = await parallel([runCodex, runClaude])
return { codex, claude }
```

The complete `codexPrompt` must instruct the bridge to:

```text
find the repo root with git rev-parse --show-toplevel
read .codex/agents/code-verifier.toml with python3 tomllib
create one mktemp directory and trap its removal
write the supplied schema and adapter developer_instructions plus task to files
run timeout 600s codex exec -C <root> -m <adapter model>
  -c model_reasoning_effort=<adapter effort> --sandbox read-only
  --output-schema <schema file> --output-last-message <result file> - < <prompt file>
return the result file's JSON verbatim; if any command fails, do not fabricate a result
```

- [ ] **Step 4: Run router tests and syntax check**

Run:

```bash
node .claude/workflows/test-code-verify.mjs
.claude/workflows/check.sh .claude/workflows/code-verify.js
```

Expected: all router checks pass and the syntax checker prints `OK`.

- [ ] **Step 5: Register the focused pre-commit hook**

Add under `repo: local`:

```yaml
  - id: code-verify-logic
    name: code-verify-logic
    files: ^\.claude/workflows/
    entry: node .claude/workflows/test-code-verify.mjs
    pass_filenames: false
    language: system
```

Run:

```bash
pre-commit run code-verify-logic --all-files
```

Expected: PASS.

- [ ] **Step 6: Commit the router**

```bash
git add .claude/workflows/code-verify.js \
  .claude/workflows/test-code-verify.mjs .pre-commit-config.yaml
git commit -m "workflows: add Codex verifier routing"
```

---

### Task 2: Route Canonical Verifier Calls

**Files:**
- Modify: `.claude/workflows/test-code-verify.mjs`
- Modify: `.claude/workflows/driver-review.js`
- Modify: `.claude/workflows/fanout-dev.js`
- Modify: `.claude/workflows/pr-babysit.js`

**Interfaces:**
- Consumes: `workflow('code-verify', { prompt, schema, label })`
- Produces: the same structured result each former native verifier call consumed

- [ ] **Step 1: Add the failing no-bypass test**

Scan every `.claude/workflows/*.js` except `code-verify.js` and fail if it matches:

```js
/agentType:\s*['"]code-verifier['"]/
```

- [ ] **Step 2: Run the test and verify RED**

Run:

```bash
node .claude/workflows/test-code-verify.mjs
```

Expected: FAIL naming direct calls in `driver-review.js`, `fanout-dev.js`, and `pr-babysit.js`.

- [ ] **Step 3: Replace each direct call**

Use this shape without a `provider`, so Codex is selected:

```js
workflow('code-verify', {
  prompt: `Review the uncommitted change in ${item} (inspect with: git diff -- ${item}) against this task:\n${args.task}\n` +
    'Dimension: does the diff correctly and completely implement the task with no unintended side effects? Coverage-first findings.',
  label: `review:${short(item)}`,
  schema: FINDINGS,
})
```

Keep every caller's existing null/result handling. Remove the `max` effort override from the adversarial `driver-review` call; provider adapters own the approved `xhigh` effort.

- [ ] **Step 4: Verify callers and commit**

Run:

```bash
node .claude/workflows/test-code-verify.mjs
for f in .claude/workflows/*.js; do .claude/workflows/check.sh "$f"; done
```

Expected: no bypasses and every workflow syntax check passes.

```bash
git add .claude/workflows/test-code-verify.mjs \
  .claude/workflows/driver-review.js .claude/workflows/fanout-dev.js \
  .claude/workflows/pr-babysit.js
git commit -m "workflows: route code verification through Codex"
```

---

### Task 3: Consolidate Validate's Dual Review

**Files:**
- Modify: `.claude/workflows/validate.js`
- Modify: `.claude/workflows/test-code-verify.mjs`

**Interfaces:**
- Consumes: `workflow('code-verify', { provider: 'both', prompt, schema, label })`
- Produces: separate `review` and `codex` stage rows from one independent dual-provider dispatch

- [ ] **Step 1: Add a failing validate wiring check**

Assert that `validate.js` contains one `workflow('code-verify'` call with `provider: 'both'`, and contains neither `codex review --base` nor a direct Opus review agent.

- [ ] **Step 2: Run the test and verify RED**

Run:

```bash
node .claude/workflows/test-code-verify.mjs
```

Expected: FAIL because `validate.js` still owns separate Claude and Codex review agents.

- [ ] **Step 3: Normalize the shared review prompt**

Keep the existing `REVIEW` schema but require `severity` to include both independent gate signals:

```text
severity = "CONFIRMED P0 correctness", "CONFIRMED P1 safety",
"PLAUSIBLE P2 quality", or the same format with P0-P3 and one of
correctness, safety, security, quality, simplification, or style
```

Use one prompt for both providers. Keep gate computation in JavaScript:

```js
const confirmedReview = f =>
  /^confirmed/i.test(f.severity) && !/quality|simplification|style/i.test(f.severity)
const codexBlocking = f =>
  /^confirmed/i.test(f.severity) && /\bP[01]\b/i.test(f.severity)
```

- [ ] **Step 4: Dispatch and split both results**

Schedule one internal `reviews` thunk when either review stage is enabled. Select `both` when neither `review` nor `codex` is skipped; otherwise select the remaining provider. Normalize its return into the existing stage rows:

```js
const rows = []
if (result.claude) rows.push({
  stage: 'review', pass: result.claude.pass &&
    !result.claude.findings.some(confirmedReview),
  findings: result.claude.findings, detail: result.claude.detail,
})
if (result.codex) rows.push({
  stage: 'codex', pass: result.codex.pass &&
    !result.codex.findings.some(codexBlocking),
  findings: result.codex.findings, detail: result.codex.detail,
})
return rows
```

Flatten stage-thunk results before updating `latest`. When building the next `toRun` set, map failed `review` or `codex` rows back to the `reviews` scheduler name. Preserve the existing `skip`, fixer evidence, retry, restart-required, and final-report contracts.

- [ ] **Step 5: Verify validate and commit**

Run:

```bash
node .claude/workflows/test-code-verify.mjs
.claude/workflows/check.sh .claude/workflows/validate.js
```

Expected: PASS.

```bash
git add .claude/workflows/validate.js .claude/workflows/test-code-verify.mjs
git commit -m "validate: share dual review routing"
```

---

### Task 4: Document and Verify the Integration

**Files:**
- Modify: `CLAUDE.md`
- Modify: `docs/superpowers/plans/2026-09-06-codex-dynamic-workflow.md`

**Interfaces:**
- Consumes: the implemented router and migrated callers
- Produces: repository guidance and fresh end-to-end evidence

- [ ] **Step 1: Update the collaboration contract**

Replace the claim that Codex only works around workflow edges with concise guidance:

```markdown
- `.claude/workflows/*.js` remain the canonical Claude Code orchestration.
  Their `code-verifier` work routes through the nested `code-verify` workflow:
  Codex by default, or Claude/both when a caller selects it. Do not invoke the
  verifier agent directly or copy the Codex bridge command.
```

- [ ] **Step 2: Run static and repository checks**

Run:

```bash
node .claude/workflows/test-code-verify.mjs
for f in .claude/workflows/*.js; do .claude/workflows/check.sh "$f"; done
git diff --check origin/master...HEAD
pre-commit run --all-files
```

Expected: all checks pass.

- [ ] **Step 3: Run one live read-only smoke test**

From Claude Code, invoke `code-verify` with the default provider, a prompt that asks which canonical role file it loaded, and this schema:

```js
{
  type: 'object', additionalProperties: false,
  required: ['role', 'readOnly'],
  properties: {
    role: { type: 'string' },
    readOnly: { type: 'boolean' },
  },
}
```

Expected: `role` names `.claude/agents/code-verifier.md`, `readOnly` is true, the worktree stays clean, and the run reports Sol/xhigh from `.codex/agents/code-verifier.toml`.

- [ ] **Step 4: Mark the plan complete and commit documentation**

Mark completed checkboxes in this file, then run:

```bash
git add CLAUDE.md docs/superpowers/plans/2026-09-06-codex-dynamic-workflow.md
git commit -m "docs: describe Codex verifier workflows"
git status --short
```

Expected: clean worktree with four implementation commits after the design commit.
