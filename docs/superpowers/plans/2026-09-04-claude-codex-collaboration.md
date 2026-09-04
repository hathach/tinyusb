# Claude and Codex Collaboration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Claude Code the primary TinyUSB harness while sharing its repository instructions and skills with Codex and using the Codex Claude Code plugin for offload and review.

**Architecture:** Keep `CLAUDE.md` and `.claude/` as the canonical instruction, role, skill, and workflow sources. Expose them to standalone Codex through relative symlinks and thin TOML role adapters, and use Claude Code's existing `codex@openai-codex` bridge for offload and review.

**Tech Stack:** Markdown, Git symlinks, Claude Code plugins, Codex CLI, existing Claude Code workflow syntax checker

**Spec:** `docs/superpowers/specs/2026-09-04-claude-codex-collaboration-design.md`

## Global Constraints

- `CLAUDE.md` and `.claude/{agents,skills,workflows}` remain the canonical authored content.
- Keep `AGENTS.md -> CLAUDE.md` unchanged.
- Add `.agents -> .claude`; `.codex/agents/*.toml` may contain only adapter metadata and canonical-role loading instructions.
- Do not change Claude Code or Codex permission defaults or enable the stop-time review gate.
- Never allow Claude and Codex to edit overlapping files concurrently in one worktree.

---

### Task 1: Canonical Repository Wiring

**Files:**
- Create: `.agents` (relative symlink to `.claude`)
- Modify: `CLAUDE.md`

**Interfaces:**
- Produces: `AGENTS.md -> CLAUDE.md` for Codex instruction discovery.
- Produces: `.agents/skills -> .claude/skills` for Codex skill discovery.
- Produces: a durable collaboration policy used by Claude Code and standalone Codex.

- [x] **Step 1: Verify the compatibility link is absent**

Run:

```bash
test -L .agents && test "$(readlink .agents)" = .claude
```

Expected: FAIL because the imported directory is absent in the isolated worktree and no compatibility symlink exists yet.

- [x] **Step 2: Add the relative compatibility symlink**

Run:

```bash
ln -s .claude .agents
```

Expected: `readlink .agents` prints `.claude`, and `git status --short` reports `?? .agents` without copied skill files.

- [x] **Step 3: Document the collaboration contract**

Add a concise `Claude and Codex Collaboration` section to `CLAUDE.md` after `Behavioral Guidelines`. It must state:

```markdown
## Claude and Codex Collaboration

Claude Code is the primary harness. `CLAUDE.md` and `.claude/{agents,skills,workflows}` are canonical; `AGENTS.md -> CLAUDE.md` and `.agents -> .claude` expose the same instructions and skills to standalone Codex. `.codex/agents/*.toml` are thin adapters that pin Codex models and load the canonical Markdown roles; do not copy role bodies or maintain other Codex-specific mirrors.

- Use `/codex:review` for an independent read-only review and `/codex:adversarial-review` to challenge the implementation or design.
- Use `/codex:rescue` for substantial bounded implementation, diagnosis, or a second pass when Claude is stuck; use its `--background`, `--resume`, and `--fresh` controls when needed.
- When Codex should use a named TinyUSB role, select its `.codex/agents/<role>.toml` adapter; the adapter loads `.claude/agents/<role>.md` as the canonical role.
- Reviews and research may run beside Claude. For write-capable delegation, use a separate worktree if Claude continues editing; otherwise yield the current worktree to Codex until it finishes. Never let both edit overlapping files in one worktree.
- `.claude/workflows/*.js` remain Claude Code-native orchestration. Codex may review or rescue work around a workflow, but no Codex-specific workflow mirror is maintained.
```

- [x] **Step 4: Verify links and canonical skill resolution**

Run:

```bash
test -L AGENTS.md
test "$(readlink AGENTS.md)" = CLAUDE.md
test -L .agents
test "$(readlink .agents)" = .claude
test "$(realpath .agents/skills)" = "$(realpath .claude/skills)"
test "$(find .codex/agents -maxdepth 1 -name '*.toml' | wc -l)" -eq 8
git ls-files --error-unmatch AGENTS.md CLAUDE.md
```

Expected: all commands succeed; all eight Codex role adapters are present.

- [x] **Step 5: Verify all canonical dynamic workflows**

Run:

```bash
for workflow_file in .claude/workflows/*.js; do
  .claude/workflows/check.sh "$workflow_file"
done
```

Expected: six `OK:` lines, one for each workflow.

- [x] **Step 6: Check and commit the repository change**

Run:

```bash
git diff --check
git diff -- CLAUDE.md
git status --short
git add .agents CLAUDE.md
git commit -m "docs: share Claude harness with Codex"
```

Expected: the commit contains one symlink and the collaboration section; role adapters are added separately.

---

### Task 2: Live Claude-to-Codex Verification

**Files:**
- Verify only: `AGENTS.md`, `.agents/skills`, `.claude/agents`, `.codex/agents`, `.claude/workflows`

**Interfaces:**
- Consumes: the symlinks and collaboration policy from Task 1.
- Consumes: the enabled `codex@openai-codex` Claude Code plugin.
- Produces: evidence that standalone Codex and the Claude Code companion both use the shared repository setup.

- [x] **Step 1: Check the Claude Code Codex companion setup**

Run from the worktree, resolving the installed plugin root first:

```bash
plugin_root=$(claude plugin list --json | jq -r '.[] | select(.id == "codex@openai-codex" and .enabled == true) | .installPath' | head -1)
node "$plugin_root/scripts/codex-companion.mjs" setup --json
```

Expected: Codex CLI is installed and authenticated; the review gate remains disabled unless it was already enabled before this task.

- [x] **Step 2: Verify standalone Codex instruction and skill discovery**

Run:

```bash
codex --ask-for-approval never exec --sandbox read-only \
  "Read-only setup check. State the repository instruction file you loaded and the project skill root you discovered. Do not modify files."
```

Expected: Codex identifies `AGENTS.md` and project skills under `.agents/skills`; the command leaves `git status --short` unchanged.

- [x] **Step 3: Verify Claude-to-Codex role reuse**

Run:

```bash
codex --ask-for-approval never exec --sandbox read-only \
  "Use spawn_agent to run the builder project agent for a read-only integration check. Return only its name and whether its output contract is JSON. Do not build or modify files."
```

Expected: Codex selects `.codex/agents/builder.toml`, loads the canonical role,
and reports its JSON output contract without changing files.

- [x] **Step 4: Run an independent Codex review of the integration diff**

Run:

```bash
node "$plugin_root/scripts/codex-companion.mjs" review --wait --base HEAD~1 --scope branch
```

Expected: the review completes and reports either no major issue or actionable findings; it makes no edits.

- [x] **Step 5: Verify a clean bounded result**

Run:

```bash
git status --short
git diff --check HEAD~1..HEAD
git show --stat --oneline HEAD
```

Expected: only the implementation-plan tracking update, if any, is uncommitted; the collaboration commit is whitespace-clean and contains only `CLAUDE.md` plus `.agents`.
