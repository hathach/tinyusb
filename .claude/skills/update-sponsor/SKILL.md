---
name: update-sponsor
description: Use when a GitHub sponsor joins, upgrades, cancels, or switches between public and private; when the README sponsor sections are stale or show "be the first!" despite active sponsors; or when new issues, PRs, or discussions still need sponsor, priority, or Adafruit triage labels.
---

# Update Sponsors

Rewrite `README.rst`'s sponsor blocks and backfill triage labels from live GitHub Sponsors data.

```bash
S=.claude/skills/update-sponsor/update_sponsor.py
python3 $S --rules      # tier -> README section -> labels, and the privacy rules
python3 $S --help       # flags
python3 $S --dry-run    # preview; every run previews and asks before applying
```

Run from the repo root as `hathach` — the script refuses any other account, whose sponsors are not
the ones this README lists. Hand-edited data lives in `config.json`, documented in that file.

**Agents:** the confirmation prompt needs a terminal and a tool-call shell has none, so the script
refuses to apply rather than guessing. Run `--dry-run`, show the maintainer the preview, get their
answer, then re-run with `--yes`. Never `--yes` on the first call — the preview is the point.
Applying dirties tracked files (`README.rst`, sometimes `tools/codespell/ignore-words.txt`);
leave them unstaged for the maintainer, as `make-release` does.

**`.github/workflows/labeler.yml` owns the label rules.** It applies the same labels when a ticket is
opened; this script only backfills what that workflow cannot reach — tickets older than it, and
private sponsors its `GITHUB_TOKEN` cannot see. Changing the policy means changing both.
