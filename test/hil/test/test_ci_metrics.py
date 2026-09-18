#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Unit tests for the CircleCI/GitHub Actions selection hand-off contracts.
# Stdlib only; no builds.
#   python3 test/hil/test/test_ci_metrics.py
import os
import re
import sys
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.abspath(__file__)))))
CIRCLECI = os.path.join(REPO, '.circleci')
SENTINEL = 'example-map-default'


class TestCircleCiSentinelContract(unittest.TestCase):
    """config.yml's set-matrix rewrites config2.yml's parameter defaults by matching
    a sentinel comment line — the only way past /pipeline/continue's 512-char
    parameter cap. Renaming or reformatting either side is a silent full-build
    fallback that no CI job reports, so pin the contract here."""

    def setUp(self):
        self.config = open(os.path.join(CIRCLECI, 'config.yml')).read()
        self.config2 = open(os.path.join(CIRCLECI, 'config2.yml')).read()

    def test_sentinel_appears_once_on_a_default_line(self):
        marker = f'# {SENTINEL}: rewritten in-place by config.yml set-matrix'
        hits = [l for l in self.config2.splitlines() if l.strip().endswith(marker)]
        self.assertEqual(len(hits), 1, f'{SENTINEL}: {len(hits)} sentinel lines in config2.yml')
        self.assertIn('default:', hits[0], f'{SENTINEL}: sentinel is not on a default: line')

    def test_the_selection_travels_as_a_file(self):
        # a mass-sweep selection runs to hundreds of KB: handed to ci_set_matrix as one
        # argv it E2BIGs the step before the `||` fallback can fire, while EXAMPLE_MAP
        # would remain scoped and disagree with the full build
        self.assertIn('--select-file', self.config)
        self.assertNotIn('--select "', self.config)

    def test_the_rewriter_names_the_same_sentinel(self):
        self.assertIn(f"'{SENTINEL}'", self.config,
                      f'{SENTINEL}: config.yml rewrite block does not name this sentinel')
        self.assertIn("# {tag}: rewritten in-place by config.yml set-matrix", self.config,
                      'config.yml no longer builds the sentinel comment it matches on')

    def test_the_rewrite_precedes_the_scoped_entries(self):
        # the scoping is all-or-nothing: config2's checked-in defaults are {} / false =
        # unfiltered, so a rewrite that fails AFTER the family entries were generated
        # leaves a subset of families built while the build is labeled full.
        # Rewrite first, and on failure drop the scoping (back to the full matrix).
        rewrite = self.config.index("p = '.circleci/config2.yml'")
        entries = self.config.index('gen_build_entry() {')
        self.assertLess(rewrite, entries,
                        'the sentinel rewrite must run before any build entry is generated')
        tail = self.config[rewrite:entries]
        self.assertIn('MATRIX_JSON="$FULL_MATRIX_JSON"', tail,
                      'a failed rewrite must fall back to the FULL matrix, not keep the '
                      'scoped one')
        # and that fallback must be a plain assignment: a second `python ...` here is an
        # unguarded command under CircleCI's `set -e`, inside the one branch whose whole
        # job is to keep the pipeline green
        self.assertNotIn('ci_set_matrix.py)', tail)

    def test_the_selector_gate_runs_both_suites(self):
        # test_ci_select.py owns the rules; this file owns the sentinel contract the
        # very same job rewrites. Gating on one of the two leaves the other unguarded.
        for suite in ('test_ci_select.py', 'test_ci_metrics.py'):
            self.assertIn(suite, self.config, f'{suite} does not gate the CircleCI selector')


class TestWorkflowSelectionHandOff(unittest.TestCase):
    """build.yml's counterpart of the CircleCI contract above: same E2BIG limit, same
    consequence (the scoping silently turns itself off on exactly the PRs where it
    saves most), plus the GITHUB_ENV lines that carry PR-derived values."""

    def setUp(self):
        wf = os.path.join(os.path.dirname(CIRCLECI), '.github', 'workflows')
        self.build = open(os.path.join(wf, 'build.yml')).read()
        self.util = open(os.path.join(wf, 'build_util.yml')).read()
        self.jobs = {j.split(':', 1)[0]: j for j in re.split(r'\n  (?=[a-z][\w-]*:\n)', self.build)}

    def test_no_step_execs_with_the_selection_in_its_environment(self):
        # SELECT_JSON="$SELECT_JSON" python3 -c ... E2BIGs at ~128KiB: measured 261KB
        # for a `git ls-files hw/bsp/**` sweep. Every reader takes the file instead.
        self.assertNotIn('SELECT_JSON="$SELECT_JSON"', self.build)
        self.assertIn('json.load(open("ci_select_out.json"))', self.build)

    def test_the_file_is_written_before_its_first_reader(self):
        self.assertLess(self.build.index("printf '%s' \"$SELECT_JSON\" > ci_select_out.json"),
                        self.build.index('json.load(open("ci_select_out.json"))'),
                        'the selection file must exist before the step that reads it')

    def test_pr_derived_env_values_are_character_guarded(self):
        # values reach GITHUB_ENV/GITHUB_OUTPUT as bare NAME=VALUE lines; a newline in
        # one (git allows it in a path, and both the example map and the roster are
        # PR-editable) writes extra variables into every later step of a job that runs
        # with secrets - and for run_*, flips which rig jobs execute
        for name in ('EX_ARGS', 'ARTIFACT_TAG'):
            self.assertIn(f'echo "{name}=', self.util)
        # per guard, not a sum: `count(a) + count(b) == 2` stays green when one guard is
        # deleted and the other duplicated
        for guard in ('case "$EX_ARGS" in', 'case "$TAG" in'):
            self.assertEqual(self.util.count(guard), 1,
                             f'{guard}: each GITHUB_ENV write screens its value exactly once')
        # CircleCI builds from the same PR-derived map and uses $EX_ARGS unquoted
        cci = open(os.path.join(CIRCLECI, 'config2.yml')).read()
        self.assertIn('case "$EX_ARGS" in', cci,
                      'the CircleCI copy of the example filter needs the same screen')
        self.assertIn('case "$BUILD_ARGS" in', self.build)
        self.assertIn('unexpected characters in the " + key', self.build,
                      'the args_*/run_* emitter must screen each board filter')

    def test_membrowse_upload_owners(self):
        # Exactly the cmake job and the espressif pair upload; hil-build
        # builds for the rig only. A `upload-membrowse: true` reappearing on
        # hil-build re-opens the target-name collision between its
        # raspberry_pi_pico PIO-USB variant build and cmake's plain build.
        # hil-build-esp-identical is not a build_util.yml caller (it has no
        # elf to build - see its own comment), so it never carries the
        # `upload-membrowse: true` input; it is caught instead by its direct
        # MEMBROWSE_API_KEY env reference, the only one in this file.
        uploaders = sorted(name for name, j in self.jobs.items()
                           if 'upload-membrowse: true' in j or 'MEMBROWSE_API_KEY' in j)
        self.assertEqual(uploaders,
                         ['cmake', 'hil-build-esp', 'hil-build-esp-identical'])

    def test_esp_identical_upload_uses_full_roster(self):
        job = self.jobs['hil-build-esp-identical']
        self.assertIn('hil_ci_set_matrix.py test/hil/tinyusb.json', job)
        self.assertNotIn('needs.set-matrix.outputs.hil_json', job)

    def test_esp_identical_upload_fetches_pr_head_history(self):
        self.assertIn('fetch-depth: 0', self.jobs['hil-build-esp-identical'])

    def test_the_guards_accept_what_the_selector_actually_emits(self):
        """A guard that rejects a NORMAL value is worse than no guard: build.yml throws
        the whole selection away, warns, and both axes fall back to full - silently
        turning the feature off. So run the real character classes over real selections
        rather than only asserting that the guard text is present.

        The one that got away: `[-A-Za-z0-9_/ .=+]` has no ':' or ',', and every partial
        board filter is `-bt <board>:<test>,<test>`."""
        import re, subprocess, sys, tempfile, json
        repo = os.path.dirname(CIRCLECI)
        # the character classes, lifted from the three places they are written
        classes = {}
        m = re.search(r're\.fullmatch\(r"\[([^"]+)\]\*"', self.build)
        self.assertTrue(m, 'args_*/run_* guard not found in build.yml')
        classes['args'] = m.group(1)
        for name, text in (('BUILD_ARGS', self.build), ('EX_ARGS', self.util),
                           ('TAG', self.util)):
            m = re.search(r'case "\$%s" in\s*\n\s*\*\[!([^\]]+)\]\*\)' % name, text)
            self.assertTrue(m, f'{name} guard not found')
            classes[name] = m.group(1).replace('\\', '')

        def ok(cls, value):
            return re.fullmatch('[%s]*' % cls.replace('!', ''), value) is not None

        with tempfile.TemporaryDirectory() as d:
            for path in ('src/class/cdc/cdc_device.c', 'src/device/usbd.c',
                         'src/portable/synopsys/dwc2/dcd_dwc2.c',
                         'examples/device/cdc_msc/src/main.c',
                         'hw/bsp/stm32f4/family.cmake'):
                f = os.path.join(d, 'diff.txt')
                with open(f, 'w') as fh:
                    fh.write(path + '\n')
                r = subprocess.run([sys.executable, os.path.join(repo, 'tools/ci_select.py'),
                                    '--diff-file', f,
                                    os.path.join(repo, 'test/hil/tinyusb.json'),
                                    os.path.join(repo, 'test/hil/hfp.json')],
                                   capture_output=True, text=True, cwd=repo)
                self.assertEqual(r.returncode, 0, r.stderr)
                s = json.loads(r.stdout)
                for flasher, a in s.get('args_flasher', {}).get('tinyusb.json', {}).items():
                    self.assertTrue(ok(classes['args'], a),
                                    f'{path}/{flasher}: the args guard rejects {a!r}')
                hfp = s.get('args', {}).get('hfp.json', '')
                self.assertTrue(ok(classes['args'], hfp), f'{path}: hfp {hfp!r}')
                # BUILD_ARGS is the hfp job's `-b <board> [-e ...]` list, not the -bt
                # test filter above - screen the value that step actually builds
                with open(os.path.join(d, 'sel.json'), 'w') as fh:
                    fh.write(r.stdout)
                hm = subprocess.run(
                    [sys.executable, os.path.join(repo, '.github/scripts/hil_ci_set_matrix.py'),
                     '--select-file', os.path.join(d, 'sel.json'),
                     os.path.join(repo, 'test/hil/hfp.json')],
                    capture_output=True, text=True, cwd=repo)
                self.assertEqual(hm.returncode, 0, hm.stderr)
                build_args = ' '.join(json.loads(hm.stdout)['arm-gcc'])
                self.assertTrue(ok(classes['BUILD_ARGS'], build_args),
                                f'{path}: the BUILD_ARGS guard rejects {build_args!r}')
                for entry in json.loads(hm.stdout)['arm-gcc']:
                    tag = re.sub(r' -e [^ ]+', '', entry)
                    self.assertTrue(ok(classes['TAG'], tag),
                                    f'{path}: the artifact-name guard rejects {tag!r}')
                for fam, exs in (s.get('build', {}).get('family_examples') or {}).items():
                    ex_args = ' '.join('-e ' + e for e in exs)
                    self.assertTrue(ok(classes['EX_ARGS'], ex_args),
                                    f'{path}/{fam}: the EX_ARGS guard rejects {ex_args!r}')

    def test_the_cmake_job_builds_the_pinned_matrix(self):
        # the boards this job exists for: CircleCI builds every board of every family,
        # so an unpinned family here would only repeat one of its legs
        self.assertIn('pinned_matrix', self.build)
        self.assertIn('needs.set-matrix.outputs.pinned_json', self.jobs['cmake'])
        self.assertNotIn('needs.set-matrix.outputs.json)', self.jobs['cmake'])
        # the Build step keeps its -e filter: --ci-pinned-boards-only would null it
        # (tools/build.py sets build_examples=None) and compile every example
        self.assertNotIn('--ci-pinned-boards-only', self.jobs['cmake'])

    def _run_matrix_step(self, sel, fail_pinned=False):
        """Run the whole 'Generate matrix json' step for real, optionally with the
        SCOPED --pinned invocation failing (the unscoped fallback still works, as a
        broken script would not). Returns the step's $GITHUB_OUTPUT as a dict."""
        import re as _re, shlex, subprocess, tempfile, json as _json
        repo = os.path.dirname(CIRCLECI)
        i = self.build.index('SELECT_FILE=ci_select_out.json')
        i = self.build.rindex('\n', 0, i) + 1
        j = self.build.index('# HIL matrix', i)
        block = _re.sub(r'^ {10}', '', self.build[i:j], flags=_re.M)
        with tempfile.TemporaryDirectory() as d:
            # ci_set_matrix resolves the repo from its own path and reads hw/bsp for
            # the pinned families, so the fake tree needs both
            for name in ('.github', 'hw'):
                os.symlink(os.path.join(repo, name), os.path.join(d, name))
            with open(os.path.join(d, 'ci_select_out.json'), 'w') as fh:
                _json.dump(sel, fh)
            bin_dir = os.path.join(d, 'bin')
            os.mkdir(bin_dir)
            with open(os.path.join(bin_dir, 'python'), 'w') as fh:
                fh.write('#!/bin/sh\n')
                if fail_pinned:
                    fh.write('case " $* " in *" --pinned "*--select-file*) exit 1 ;; esac\n')
                fh.write(f'exec {shlex.quote(sys.executable)} "$@"\n')
            os.chmod(os.path.join(bin_dir, 'python'), 0o755)
            out = os.path.join(d, 'gh_output')
            open(out, 'w').close()
            r = subprocess.run(['bash', '-e', '-o', 'pipefail', '-c', block], cwd=d,
                               capture_output=True, text=True,
                               env={**os.environ, 'PATH': bin_dir + os.pathsep + os.environ['PATH'],
                                    'GITHUB_OUTPUT': out})
            self.assertEqual(r.returncode, 0, r.stderr)
            with open(out) as fh:
                return dict(l.split('=', 1) for l in fh.read().splitlines() if '=' in l)

    def test_the_pinned_matrix_is_scoped_with_the_example_map(self):
        sel = {'build': {'full': False, 'families': ['stm32f4'],
                         'family_examples': {'stm32f4': ['device/cdc_msc']}}}
        import json as _json
        got = self._run_matrix_step(sel)
        self.assertEqual(_json.loads(got['pinned_matrix'])['arm-gcc'], ['stm32f4'])
        self.assertEqual(_json.loads(got['example_map']), sel['build']['family_examples'])

    def test_a_failed_pinned_matrix_drops_the_example_map_too(self):
        # the pinned matrix falls open on its own failure; leaving the example map
        # scoped would filter the examples of a full build and upload those sizes
        sel = {'build': {'full': False, 'families': ['stm32f4'],
                         'family_examples': {'stm32f4': ['device/cdc_msc']}}}
        import json as _json, subprocess
        got = self._run_matrix_step(sel, fail_pinned=True)
        self.assertEqual(_json.loads(got['example_map']), {})
        repo = os.path.dirname(CIRCLECI)
        full = subprocess.run([sys.executable, os.path.join(repo, '.github/scripts/ci_set_matrix.py'),
                               '--pinned'], capture_output=True, text=True, cwd=repo).stdout
        self.assertEqual(_json.loads(got['pinned_matrix']), _json.loads(full))

    def test_an_unusable_selection_is_unusable_for_both_matrices(self):
        # hil_ci_set_matrix reads "full false with no boards map" as unusable and falls
        # open to the whole roster; if this emitter instead computed run_*=false, the
        # rig jobs would skip while all 37 build legs ran - a full build and still zero
        # hardware coverage, which is the outcome the guard exists to prevent
        self.assertIn('isinstance(s.get("boards"), dict)', self.build)

    def test_the_build_extras_drop_when_the_matrix_falls_open(self):
        # ci_set_matrix falls open with rc 0, so the example map and family regex must
        # follow it or a nominally full build is filtered and labelled as a scoped one
        self.assertIn("grep -q 'ci_set_matrix: UNSCOPED'", self.build)
        self.assertIn('BUILD_SELECT_FILE', self.build)
        scripts = os.path.join(os.path.dirname(CIRCLECI), '.github', 'scripts')
        matrix = open(os.path.join(scripts, 'ci_set_matrix.py')).read()
        # count-independent: pin the INVARIANT, not the number of fall-open paths -
        # every message that emits the full matrix must carry the marker, and a purely
        # informational note (a partial family miss) must not claim to have done so.
        # Adjacent string literals are joined first, since these messages wrap.
        import re as _re
        flat = _re.sub(r"['\"]\s*\n\s*f?['\"]", '', matrix)
        hits = [m.start() for m in _re.finditer('emitting the full ', flat)]
        self.assertGreaterEqual(len(hits), 2, 'fall-open messages not found')
        for i in hits:
            self.assertIn('UNSCOPED', flat[max(0, i - 200):i],
                          'a fall-open path without the marker build.yml greps for')

    def _matrix_legs(self, sel):
        """Families across every toolchain leg of the step's matrix, for `sel`."""
        import json as _json
        return sum(len(v) for v in
                   _json.loads(self._run_matrix_step(sel)['pinned_matrix']).values())

    def test_an_empty_family_list_is_not_treated_as_unusable(self):
        """A legitimate nothing-selected PR (every family filtered out) must keep the
        all-empty matrix ci_set_matrix produced, not fall open to a full build.

        #3842 (docs + .gitignore) and #3840 (test/hil only) each rebuilt all 74 cmake
        legs after the selector had correctly chosen none, because an earlier version
        of this block conflated an empty families list with an unusable one."""
        legs = self._matrix_legs(
            {'build': {'full': False, 'families': [], 'family_examples': {}}})
        self.assertEqual(legs, 0, 'an empty families list must keep the all-empty matrix')

    def test_a_real_family_list_stays_scoped(self):
        legs = self._matrix_legs(
            {'build': {'full': False, 'families': ['stm32f4', 'rp2040'],
                       'family_examples': {}}})
        self.assertGreater(legs, 0)
        self.assertLess(legs, self._matrix_legs(
            {'build': {'full': True, 'families': [], 'family_examples': {}}}))

    def test_membrowse_upload_is_not_scoped_by_the_pr_filter(self):
        # $EX_ARGS must reach build.py so the upload resolves the same board the Build
        # step fell back to; --ci-pinned-boards-only keeps it from scoping the examples
        # (an out-of-selection example still gets its --identical row)
        line = [l for l in self.util.splitlines()
                if '--target examples-membrowse-upload' in l][0]
        self.assertIn('$EX_ARGS', line)
        self.assertIn('--ci-pinned-boards-only', line)

    def test_the_build_step_stays_scoped_by_the_pr_filter(self):
        # the fix above only touches the Membrowse Upload step - the Build step
        # must keep compiling just the PR-selected examples
        line = [l for l in self.util.splitlines()
                if 'python tools/build.py $BUILD_PY_ARGS ${{ matrix.arg }} $EX_ARGS' in l]
        self.assertTrue(line, '$EX_ARGS missing from the Build step invocation')


if __name__ == '__main__':
    unittest.main()
