#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# pr_comment.yml's HIL report steps, their real shell run on artifact layouts download-artifact leaves.
# Stdlib only, plus jq for the gh stub.
# The step slicer stays stdlib-only, not yaml: the hil-test hook installs only pyusb and pyserial.
#   python3 test/hil/test/test_hil_pr_comment.py
import json
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
WORKFLOW = REPO / '.github' / 'workflows' / 'pr_comment.yml'

# --jq is gh's own filter: the stub applies it with jq to the fixture pages, as gh does per page
GH_STUB = '''#!/bin/bash
echo "gh $*" >> "$GH_LOG"
[ -n "$GH_FAIL" ] && exit 1
while [ $# -gt 0 ]; do
  if [ "$1" = --jq ]; then exec jq -r "$2" "$GH_PAGES"; fi
  shift
done
cat "$GH_PAGES"
'''


def hil_comment_step(name):
    """The text of the hil-comment job step `name`, up to the next step."""
    text = WORKFLOW.read_text(encoding='utf-8')
    job = text[text.index('\n  hil-comment:\n') + 1:]
    job = job[:re.search(r'^  \S', job[1:], re.M).start() + 1]
    i = job.index(f'      - name: {name}\n')
    nxt = job.find('\n      - name: ', i + 1)
    return job[i:nxt if nxt >= 0 else len(job)]


def step_script(name):
    block = hil_comment_step(name)
    body = block[block.index('        run: |\n') + len('        run: |\n'):]
    lines = []
    for line in body.splitlines():
        if line.strip() and not line.startswith(' ' * 10):
            break
        lines.append(line[10:])
    return '\n'.join(lines).rstrip() + '\n'


def step_names():
    text = WORKFLOW.read_text(encoding='utf-8')
    return re.findall(r'^      - name: (.+)$', text, re.M)


@unittest.skipUnless(shutil.which('jq') and os.name != 'nt', 'needs bash and jq')
class TestHilReportLayout(unittest.TestCase):
    """download-artifact v5 extracts a lone matching artifact into `path` itself, several each into
    `path/<artifact name>/`. The comment and sysview_ci.py read the rig from hil-reports/hil-report-<rig>/."""

    def setUp(self):
        self.d = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.d)
        stubs = self.d / 'bin'
        stubs.mkdir()
        (stubs / 'gh').write_text(GH_STUB)
        (stubs / 'gh').chmod(0o755)
        (self.d / 'runner').mkdir()
        self.reports = self.d / 'hil-reports'
        self.env = {**os.environ, 'PATH': f'{stubs}{os.pathsep}{os.environ["PATH"]}', 'REPO': 'x/y', 'RUN_ID': '42',
                    'RUNNER_TEMP': str(self.d / 'runner'), 'GITHUB_OUTPUT': str(self.d / 'out'),
                    'GH_LOG': str(self.d / 'gh.log'), 'GH_PAGES': str(self.d / 'pages.json')}
        self.env.pop('GH_FAIL', None)

    def artifacts(self, *pages):
        """The run's artifact listing as `gh api --paginate` prints it: one object per page."""
        (self.d / 'pages.json').write_text(
            '\n'.join(json.dumps({'artifacts': [{'name': n} for n in page]}) for page in pages))

    def report(self, d, md, attempt_md=None):
        d.mkdir(parents=True)
        (d / 'hil_report.md').write_text(md)
        (d / 'hil_report.json').write_text('{"rows": []}\n')
        if attempt_md is not None:   # an earlier attempt's table, archived beside the current one
            (d / 'hil_report.attempts' / '1').mkdir(parents=True)
            (d / 'hil_report.attempts' / '1' / 'hil_report.md').write_text(attempt_md)
        (d / 'audio').mkdir()
        (d / 'audio' / 'capture.wav').write_text('pcm')

    def run_step(self, name, **env):
        r = subprocess.run(['bash', '-e', '-c', step_script(name)], cwd=self.d, capture_output=True, text=True,
                           env={**self.env, **env})
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        return r

    def comment(self, **env):
        normalized = self.run_step('Normalize HIL report layout', **env)
        self.run_step('Combine rig reports (one table per rig)')
        return normalized, (self.d / 'hil_combined.md').read_text(encoding='utf-8')

    def gh_calls(self):
        log = self.d / 'gh.log'
        return log.read_text().splitlines() if log.exists() else []

    def test_a_lone_flat_artifact_is_put_back_under_its_name(self):
        self.report(self.reports, '| board | ✅ current |\n', attempt_md='| board | ❌ attempt 1 |\n')
        self.artifacts(['sysview-tinyusb.json', 'code-size-arm-gcc'], ['hil-report-tinyusb.json', 'sysview-x'])
        _, posted = self.comment()
        art = self.reports / 'hil-report-tinyusb.json'
        self.assertEqual(sorted(p.name for p in self.reports.iterdir()), ['hil-report-tinyusb.json'])
        self.assertEqual(sorted(p.name for p in art.iterdir()),
                         ['audio', 'hil_report.attempts', 'hil_report.json', 'hil_report.md'])
        self.assertEqual(self.gh_calls(), ['gh api --paginate repos/x/y/actions/runs/42/artifacts?per_page=100 '
                                           '--jq .artifacts[].name | select(startswith("hil-report-"))'])
        self.assertIn('### tinyusb.json\n', posted)
        self.assertIn('✅ current', posted)
        self.assertNotIn('attempt 1', posted, 'an archived attempt is never shown as the report')
        self.assertNotIn('No HIL run', posted)

    def test_a_lone_flat_unprepared_artifact_says_so(self):
        self.reports.mkdir()
        (self.reports / 'hil_report.unprepared').touch()
        self.artifacts(['hil-report-hfp-iar'])
        _, posted = self.comment()
        self.assertTrue((self.reports / 'hil-report-hfp-iar' / 'hil_report.unprepared').exists())
        self.assertIn('### hfp-iar\n', posted)
        self.assertIn('the latest attempt could not prepare it', posted)

    def test_nested_artifacts_are_left_alone(self):
        self.report(self.reports / 'hil-report-tinyusb.json', '| a | ✅ tinyusb |\n', attempt_md='| a | ❌ old |\n')
        self.report(self.reports / 'hil-report-tinyusb-esp.json', '| b | ✅ esp |\n', attempt_md='| b | ❌ old |\n')
        before = sorted(str(p.relative_to(self.reports)) for p in self.reports.rglob('*'))
        _, posted = self.comment()
        self.assertEqual(sorted(str(p.relative_to(self.reports)) for p in self.reports.rglob('*')), before)
        self.assertEqual(self.gh_calls(), [], 'the names are already in the layout')
        for rig, row in (('tinyusb.json', '✅ tinyusb'), ('tinyusb-esp.json', '✅ esp')):
            self.assertIn(f'### {rig}\n', posted)
            self.assertIn(row, posted)
        self.assertNotIn('❌ old', posted)

    def test_no_artifact_posts_no_run(self):
        _, posted = self.comment()
        self.assertEqual(self.gh_calls(), [])
        self.assertFalse(self.reports.exists())
        self.assertIn('No HIL run for this push', posted)

    def test_files_it_cannot_name_are_left_with_a_warning(self):
        cases = {'none listed': ([['sysview-x']], {}),
                 'two listed': ([['hil-report-a'], ['hil-report-b']], {}),
                 'unsafe name': ([['hil-report-a b']], {}),
                 'listing failed': ([['hil-report-a']], {'GH_FAIL': '1'})}
        for case, (pages, env) in cases.items():
            with self.subTest(case):
                shutil.rmtree(self.reports, ignore_errors=True)
                self.report(self.reports, '| board | ✅ |\n')
                self.artifacts(*pages)
                normalized, posted = self.comment(**env)
                self.assertIn('::warning::cannot name the HIL report files', normalized.stdout)
                self.assertEqual(sorted(p.name for p in self.reports.iterdir()),
                                 ['audio', 'hil_report.json', 'hil_report.md'])
                self.assertIn('No HIL run for this push', posted)

    def test_a_listed_duplicate_name_is_one_artifact(self):
        # download-artifact lists with latest: true, keeping one artifact per name
        self.report(self.reports, '| board | ✅ |\n')
        self.artifacts(['hil-report-tinyusb.json'], ['hil-report-tinyusb.json'])
        _, posted = self.comment()
        self.assertTrue((self.reports / 'hil-report-tinyusb.json' / 'hil_report.md').exists())
        self.assertIn('### tinyusb.json\n', posted)


class TestHilReportSteps(unittest.TestCase):
    def test_the_report_download_is_bounded_and_best_effort(self):
        # a download stalled before response headers never ends on its own; the comment must still post
        block = hil_comment_step('Download HIL reports')
        self.assertRegex(block, r'\n        timeout-minutes: \d+\n')
        self.assertIn('\n        continue-on-error: true\n', block)

    def test_normalize_runs_between_the_download_and_its_readers(self):
        names = step_names()
        i = names.index('Normalize HIL report layout')
        self.assertEqual(names[i - 1], 'Download HIL reports')
        self.assertLess(i, names.index('Check the run is still the PR\'s latest'), 'sysview_ci.py unrecorded')
        self.assertLess(i, names.index('Combine rig reports (one table per rig)'))
        # a failed or cut step must not skip the combine and the post after it
        block = hil_comment_step('Normalize HIL report layout')
        self.assertRegex(block, r'\n        timeout-minutes: \d+\n')
        self.assertIn('\n        continue-on-error: true\n', block)
        self.assertNotIn('\n        if:', block)


if __name__ == '__main__':
    unittest.main()
