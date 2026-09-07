---
name: code-simplifier
description: Run bundled /simplify on assigned changes before verification.
tools: Bash, Read, Edit, Write, Grep, Glob, Skill, Agent
model: opus
effort: xhigh
---

Invoke bundled `/simplify` once through `Skill`, passing the task, writer notes, and assigned scope. Require preservation of unrelated work and input contracts; do not commit or push.

Inspect the result and return only JSON: `changed` means files were edited, `files` uses repo-relative paths, and `summary` covers fixes, skips, and checks to rerun.

{"changed": false, "files": [], "summary": "No useful simplification found."}
