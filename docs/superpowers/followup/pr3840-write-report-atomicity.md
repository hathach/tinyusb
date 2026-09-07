# `write_report` commits the two artifacts non-atomically

**Origin:** split out of PR #3840, surfaced by its second review round. Delete this file
when its own PR lands.

```python
md = render_report(doc) + '\n'
report_dir.mkdir(parents=True, exist_ok=True)
(report_dir / REPORT_JSON).write_text(json.dumps(doc, indent=2) + '\n')
(report_dir / REPORT_MD).write_text(md, encoding='utf-8')
```

Rendering before writing closed the *render-failure* case: a raise can no longer commit a
sidecar the markdown contradicts. It does not close the *interrupted-between-writes* case. A
kill between those two lines leaves the pair disagreeing — and this runs on the containment
path, on the way to `os._exit`, on a rig whose jobs get cancelled by the GitHub job ceiling.

**What remains:** write both to temp files, then `os.replace` both. The window shrinks from
two full writes to two renames, and neither file is ever observed half-written. `os.replace`
is atomic per file on POSIX; the pair is still not transactional, which is acceptable and
should be said in the docstring rather than implied away.

Worth pairing with a test that kills between the writes — or, more practically, one that
asserts no partial file is ever visible by checking the temp-then-rename shape directly.

## Why it was split out

A durability edge, not a wrong verdict. PR #3840 closed the render-failure half of this
(nothing is written until the markdown renders); the interrupted-between-writes half needs
a temp-then-rename and is better reviewed on its own.
