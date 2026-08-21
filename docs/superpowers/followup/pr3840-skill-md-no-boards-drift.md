# `SKILL.md` contradicts the code on no-boards tables

**Origin:** split out of PR #3840, surfaced by its second review round. Delete this file
when its own PR lands.

`.claude/skills/hil/SKILL.md:150-151` tells the reading agent:

> `**HIL run selected no boards.**` — the filters intersected to nothing, so there is **no
> table at all**. Report that (and the filter shown), never `"pass": true`.

That was true when the no-boards exit wrote a bare notice. It no longer is. An
`--accumulate` no-boards run keeps the accumulated rows — deliberately, because wiping them
destroyed real results — so the artifact now reads:

```
**HIL run selected no boards.** filters emptied

**✅ 1 passed · ❌ 0 failed · ⚪ 0 skipped · blank not run**

| Board | t | duration |
...
```

The behaviour is correct; the documentation is wrong, and wrong in the direction that
matters. An agent is told to expect no table, sees one, and has no rule for whether those
rows are reportable. **They are not this run's** — they are a previous attempt's, carried
forward.

**What remains:** update that bullet to describe both cases — a fresh run has no table, an
`--accumulate` run shows the previous attempt's rows under the notice and they must not be
reported as this run's. Add a test asserting the fresh case renders no matrix, so the two
halves cannot drift again.

## Why it was split out

PR #3840 fixed the findings that changed a verdict. This is a documentation drift: the
behaviour is correct and the doc describing it is not, so it is better reviewed on its own
than appended to a branch already carrying a module consolidation.
