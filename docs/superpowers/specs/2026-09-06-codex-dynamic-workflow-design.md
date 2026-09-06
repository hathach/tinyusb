# Codex-backed Dynamic Workflow Verification

Date: 2026-09-06
Branch: `claude/codex-dynamic-workflows`

## Goal

Let Claude Code dynamic workflows run the canonical `code-verifier` role with
Codex, Claude, or both without maintaining a second workflow tree. Codex is the
default provider. Provider failures are visible and never trigger a silent
fallback.

## Boundaries

- Claude Code remains the dynamic workflow runtime and orchestrator.
- `.claude/workflows/` remains the only authored workflow tree.
- `.claude/agents/code-verifier.md` remains the only authored verifier role.
- `.codex/agents/code-verifier.toml` remains the Codex model/effort adapter. It
  currently selects `gpt-5.6-sol` at `xhigh` effort and loads the canonical
  Markdown role.
- Only `code-verifier` is routed in this change. Other roles remain
  Claude-native.

## Provider Router

Add `.claude/workflows/code-verify.js` as a nested workflow with this input:

```js
{
  prompt: string,
  schema: object,
  provider?: 'codex' | 'claude' | 'both',
  label?: string,
}
```

`provider` defaults to `codex`.

- `codex` returns the Codex result in the supplied schema.
- `claude` returns the Claude result in the supplied schema.
- `both` starts both providers independently with the same prompt and schema,
  waits for both, and returns `{ codex, claude }`. The router does not merge or
  adjudicate the results.

The router validates `prompt`, `schema`, and `provider` before dispatch. A
provider that dies, exits unsuccessfully, or returns output that does not match
the schema causes the router to throw a provider-specific error.

## Claude Provider

The Claude path invokes the existing native agent:

```js
agent(prompt, {
  agentType: 'code-verifier',
  label,
  phase: 'Verify',
  schema,
})
```

The agent's canonical frontmatter currently selects Opus at `xhigh` effort.
The router does not copy that configuration. The existing one-call `max`
override in `driver-review.js` is removed so both providers use their approved
`xhigh` verifier configurations consistently.

## Codex Provider

Dynamic workflow JavaScript cannot directly run shell commands, so the Codex
path uses one Haiku/low Claude agent as a thin process bridge. The bridge does
no review work. It:

1. Locates the repository root.
2. Reads `.codex/agents/code-verifier.toml` with Python's standard `tomllib` to
   obtain the Codex model, effort, and canonical-role loading instruction.
3. Writes the supplied JSON schema to a temporary file.
4. Runs `codex exec` from the repository root with the adapter's model and
   effort, `--sandbox read-only`, `--output-schema`, and
   `--output-last-message`.
5. Returns the parsed JSON value without interpreting its findings and removes
   its temporary files.

The task sent to Codex combines the adapter's canonical-role loading
instruction with the caller's prompt. The verifier role body is therefore read
at execution time rather than embedded in the router. Future edits to either
the role body or the Codex adapter take effect without regenerating anything.

The Codex subprocess has a bounded timeout. Timeout, missing CLI, nonzero exit,
missing output, invalid JSON, and schema mismatch are hard failures. The router
does not run Claude as a fallback.

## Callers

Replace direct `agentType: 'code-verifier'` calls in these workflows with the
nested router:

- `driver-review.js`
- `fanout-dev.js`
- `pr-babysit.js`

Those calls omit `provider` and therefore use Codex only.

Replace `validate.js`'s separate Claude and ad-hoc Codex review implementations
with one `code-verify` call using `provider: 'both'`. Preserve two independently
reported stages and apply the existing Claude and Codex gating rules to their
respective results. A failed provider remains a failed stage.

Future dynamic workflows that need this role call `workflow('code-verify',
...)`; they do not invoke `agentType: 'code-verifier'` directly or copy the
Codex command.

## Concurrency and Safety

Both providers are read-only and may run in parallel in the same checkout.
Codex always receives the read-only sandbox. The router does not expose a write
mode, approval override, model override, or provider fallback.

Each invocation owns unique temporary files and cleans them on success or
failure, so parallel verifier calls cannot overwrite each other's schema or
result.

## Verification

Add `.claude/workflows/test-code-verify.mjs` using only Node's standard
library. Execute the router with mocked workflow primitives and verify:

- omitted provider dispatches only Codex;
- `claude` dispatches only the native verifier;
- `both` starts independent Codex and Claude calls and returns both results;
- invalid inputs fail before dispatch;
- a dead or malformed Codex result fails without invoking Claude;
- no workflow other than `code-verify.js` directly names the `code-verifier`
  agent type.

Register the test as a local pre-commit hook for `.claude/workflows/` changes.
Also run:

1. `.claude/workflows/check.sh` for every workflow JavaScript file.
2. `node .claude/workflows/test-code-verify.mjs`.
3. `pre-commit run --all-files`.
4. One live read-only `code-verify` workflow call with a small schema, checking
   that Codex used the canonical role and returned valid structured output.

## Repository Changes

- Add `.claude/workflows/code-verify.js`.
- Add `.claude/workflows/test-code-verify.mjs`.
- Modify `.claude/workflows/{driver-review,fanout-dev,pr-babysit,validate}.js`.
- Modify `.pre-commit-config.yaml` to run the router self-test.
- Modify `CLAUDE.md` to describe Codex-backed verifier routing instead of
  saying workflows can only use Codex around their edges.

## Out of Scope

- A Codex-native dynamic workflow runtime or workflow mirror.
- Routing `builder`, `code-writer`, HIL, static-analysis, PR-watcher, or target
  debugging roles through Codex.
- Automatic provider selection, retries, fallback, or result merging.
- Per-call model, effort, sandbox, or write-mode controls.
