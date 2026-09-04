# Neutral Skill Wording Design

## Goal

Make the `read-doc` and `pre-pr` skill instructions understandable in both
Claude Code and Codex without changing their behavior.

## Changes

- Replace slash-command wording with references to arguments or context supplied
  in the skill request.
- Replace the Claude-specific `AskUserQuestion` name with the neutral instruction
  to ask the user.
- Replace Claude-specific `Read` tool wording with capability-based document
  reading instructions.
- Remove slash-command syntax from skill headings.

## Deferred Scope

Leave the `pre-pr` invocation of the Claude Code `full-check` workflow unchanged.
Standalone Codex execution of that workflow will be designed separately.

No other skill files need changes: their instructions already use portable
Markdown, repository paths, and shell commands.

## Verification

- Search every `.claude/skills/*/SKILL.md` for slash-command headings,
  `AskUserQuestion`, and Claude-specific `Read` tool wording.
- Confirm `pre-pr` still invokes `full-check` exactly as before.
- Confirm `.agents` still resolves to `.claude`, so Codex discovers the canonical
  skill files without a generated copy.
