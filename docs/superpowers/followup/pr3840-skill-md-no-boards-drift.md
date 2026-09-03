# `SKILL.md` no-boards bullet: the regression test is still missing

**Origin:** split out of PR #3840, surfaced by its second review round. Delete this file
when its own PR lands.

The documentation half landed in PR #3881. `.claude/skills/hil/SKILL.md`'s
`**HIL run selected no boards.**` bullet (under "Reporting") now describes both cases: a
fresh run renders no table, and an `--accumulate` run keeps the previous attempt's rows
under the notice, which are not this run's and must not be reported as such. That matches
the code, which deliberately preserves accumulated rows on a no-boards exit because wiping
them destroyed real results.

**What remains:** a test asserting that the fresh no-boards case renders no matrix while
the `--accumulate` case keeps the prior rows under the notice, so the two halves cannot
drift again. The behaviour lives in `test/hil/helper/hil_report.py`'s no-boards exit
(`accumulate_report` with `fresh`); `test/hil/test/test_hil_report.py` is the suite.

## Why it was split out

PR #3840 fixed the findings that changed a verdict. This is a documentation drift: the
behaviour is correct and the doc describing it is not, so it is better reviewed on its own
than appended to a branch already carrying a module consolidation.
