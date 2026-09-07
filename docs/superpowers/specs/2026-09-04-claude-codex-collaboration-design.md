# Claude and Codex Collaboration — Design

Date: 2026-09-04
Branch: `claude/codex-collaboration`

## Goal

Keep Claude Code as the primary TinyUSB harness while making Codex available
for independent review, diagnosis, and bounded implementation work. Preserve
one repository-owned source for instructions, agents, skills, and dynamic
workflows, while keeping standalone Codex useful.

## Source of Truth

- `CLAUDE.md` is the only authored repository instruction file.
- `.claude/agents/`, `.claude/skills/`, and `.claude/workflows/` are the only
  authored agent, skill, and workflow trees.
- `AGENTS.md` remains a relative symlink to `CLAUDE.md` so standalone Codex
  loads the same repository instructions.
- `.agents` becomes a relative symlink to `.claude` so Codex discovers the
  canonical skills at `.agents/skills` without a copied mirror.
- The imported `.codex/` agent tree is replaced with thin TOML adapters that
  contain only each role's name, model/effort pin, and instruction to load the
  canonical `.claude/agents/<role>.md` body.

Standalone Codex therefore uses the shared project instructions, skills, and
named TinyUSB roles. JavaScript workflows remain Claude Code orchestration
surfaces.

## Claude-to-Codex Flow

The enabled `codex@openai-codex` Claude Code plugin is the bridge:

- `/codex:review` performs an independent, read-only review of local git state.
- `/codex:adversarial-review` challenges the implementation and design.
- `/codex:rescue` delegates diagnosis or an explicitly requested fix.
- `/codex:status`, `/codex:result`, and `/codex:cancel` manage background work.

Claude should offload work when a second implementation or diagnosis pass is
valuable, when it is stuck, or when a substantial bounded task can be isolated.
Simple tasks stay in the main Claude session.

When a Codex task should follow a named TinyUSB role, select its
`.codex/agents/<role>.toml` adapter. The role body remains authored once in
`.claude/agents/<role>.md`.

## Concurrency and Safety

- Reviews and research may run in parallel with Claude because they are
  read-only.
- Claude and Codex must not edit overlapping files concurrently in the same
  worktree.
- Write-capable background delegation uses a separate worktree when Claude will
  continue editing; otherwise Claude yields ownership of the current worktree
  until Codex finishes.
- Codex follows the same destructive-action, HIL, push, and hardware-lock rules
  through `AGENTS.md -> CLAUDE.md`.

## Dynamic Workflows

`.claude/workflows/*.js` stay Claude Code-native. Claude remains responsible
for workflow control flow and structured joins. Codex can be called before or
after a workflow for a second opinion or rescue task, but the workflow files
are not translated into a Codex-specific format.

This avoids an unsupported generated workflow mirror and keeps deterministic
orchestration in one place.

## Repository Changes

1. Replace the untracked imported `.agents/` directory with the tracked
   `.agents -> .claude` symlink.
2. Replace the untracked imported `.codex/` directory with thin agent adapters.
3. Add a concise collaboration section to `CLAUDE.md` covering delegation,
   review, role reuse, and the single-writer rule.
4. Keep the existing tracked `AGENTS.md -> CLAUDE.md` symlink unchanged.

## Verification

- Confirm both compatibility paths are relative symlinks with the intended
  targets.
- Confirm `.agents/skills/*/SKILL.md` resolves to the canonical files and every
  `.codex/agents/*.toml` adapter loads its matching canonical role.
- Run every `.claude/workflows/*.js` through `.claude/workflows/check.sh`.
- Run Codex non-interactively from the worktree and verify it identifies the
  shared instruction source and project skills.
- Run the Claude Code Codex companion setup check, a small rescue/read task,
  and a small review task without changing repository source.

## Out of Scope

- Translating Claude Code dynamic workflows to a second runtime.
- Maintaining Codex-native copies of the named Claude agent bodies.
- Changing Claude Code or Codex permission defaults.
- Enabling automatic stop-time reviews without explicit user direction.
