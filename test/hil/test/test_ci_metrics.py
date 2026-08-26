#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Unit tests for the by-example half of tools/metrics.py and the CircleCI/GitHub
# Actions selection hand-off contracts. Stdlib only; synthetic map.json fixtures, no
# builds.
#   python3 test/hil/test/test_ci_metrics.py
import json
import os
import subprocess
import sys
import tempfile
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.abspath(__file__)))))
METRICS = os.path.join(REPO, 'tools', 'metrics.py')


def fake_map(path, files):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w') as f:
        json.dump({'files': files}, f)


def entry(name, size, path_prefix='tinyusb/src'):
    return {'file': name, 'path': f'{path_prefix}/{name}', 'size': size,
            'symbols': [{'name': f'{name}_fn', 'size': size}], 'sections': {'.text': size}}


class TestByExample(unittest.TestCase):
    def build_tree(self, td):
        fake_map(os.path.join(td, 'device', 'cdc_msc', 'cdc_msc.map.json'),
                 [entry('usbd.c', 100), entry('cdc_device.c', 50)])
        fake_map(os.path.join(td, 'host', 'bare_api', 'bare_api.map.json'),
                 [entry('usbh.c', 200)])

    def test_by_example_output(self):
        with tempfile.TemporaryDirectory() as td:
            self.build_tree(td)
            out = os.path.join(td, 'metrics')
            r = subprocess.run([sys.executable, METRICS, 'combine', '-q', '-j',
                                '--by-example', '-o', out,
                                os.path.join(td, '*', '*', '*.map.json')],
                               capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stderr)
            by_ex = json.load(open(out + '_by_example.json'))
            self.assertEqual(set(by_ex), {'device/cdc_msc', 'host/bare_api'})
            self.assertEqual({f['file'] for f in by_ex['device/cdc_msc']['files']},
                             {'usbd.c', 'cdc_device.c'})
            # the plain averaged output is unchanged by the extra flag
            avg = json.load(open(out + '.json'))
            self.assertIn('files', avg)

    def test_by_example_json_roundtrips_as_combine_input(self):
        with tempfile.TemporaryDirectory() as td:
            self.build_tree(td)
            out = os.path.join(td, 'metrics')
            subprocess.run([sys.executable, METRICS, 'combine', '-q', '-j', '--by-example',
                            '-o', out, os.path.join(td, '*', '*', '*.map.json')], check=True)
            out2 = os.path.join(td, 'sub')
            r = subprocess.run([sys.executable, METRICS, 'combine', '-q', '-j',
                                '-o', out2, out + '_by_example.json'],
                               capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stderr)
            sub = json.load(open(out2 + '.json'))
            names = {f['file'] for f in sub['files']}
            # one data entry per example, not one blob: reading it as an ordinary
            # metrics.json would double-count every file
            self.assertIn('usbd.c', names)
            self.assertIn('cdc_device.c', names)
            self.assertNotIn('TOTAL', {n.upper() for n in names})

    def test_by_example_expansion_is_keyed_on_the_filename(self):
        # the '_by_example.json' suffix IS the contract (write_by_example spells it).
        # A shape-sniff would reroute any coincidentally-shaped JSON into the
        # per-example branch instead.
        with tempfile.TemporaryDirectory() as td:
            look_alike = os.path.join(td, 'metrics.json')
            with open(look_alike, 'w') as f:
                json.dump({'device/cdc_msc': {'files': [entry('usbd.c', 100)]}}, f)
            out = os.path.join(td, 'combined')
            r = subprocess.run([sys.executable, METRICS, 'combine', '-q', '-j',
                                '-o', out, look_alike],
                               capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stderr)
            combined = json.load(open(out + '.json'))
            self.assertNotIn('usbd.c', {f['file'] for f in combined.get('files', [])})


CIRCLECI = os.path.join(REPO, '.circleci')
SENTINELS = ('example-map-default', 'build-filtered-default')


class TestCircleCiSentinelContract(unittest.TestCase):
    """config.yml's set-matrix rewrites config2.yml's parameter defaults by matching
    a sentinel comment line — the only way past /pipeline/continue's 512-char
    parameter cap. Renaming or reformatting either side is a silent full-build
    fallback that no CI job reports, so pin the contract here."""

    def setUp(self):
        self.config = open(os.path.join(CIRCLECI, 'config.yml')).read()
        self.config2 = open(os.path.join(CIRCLECI, 'config2.yml')).read()

    def test_each_sentinel_appears_once_on_a_default_line(self):
        for tag in SENTINELS:
            marker = f'# {tag}: rewritten in-place by config.yml set-matrix'
            hits = [l for l in self.config2.splitlines() if l.strip().endswith(marker)]
            self.assertEqual(len(hits), 1, f'{tag}: {len(hits)} sentinel lines in config2.yml')
            self.assertIn('default:', hits[0], f'{tag}: sentinel is not on a default: line')

    def test_the_selection_travels_as_a_file(self):
        # a mass-sweep selection runs to hundreds of KB: handed to ci_set_matrix as one
        # argv it E2BIGs the step before the `||` fallback can fire, and EXAMPLE_MAP /
        # BUILD_FILTERED (derived with jq, no argv limit) would then label a FULL build
        # scoped -- the build and its label disagreeing is worse than either alone
        self.assertIn('--select-file', self.config)
        self.assertNotIn('--select "', self.config)

    def test_the_rewriter_names_the_same_sentinels(self):
        for tag in SENTINELS:
            self.assertIn(f"'{tag}'", self.config,
                          f'{tag}: config.yml rewrite block does not name this sentinel')
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

    def _run_extras_block(self, sel):
        """Extract the build-extras shell block from build.yml and run it for real.
        Nothing else exercises it, which is why the empty/rejected conflation shipped."""
        import re as _re, shlex, subprocess, tempfile, json as _json
        repo = os.path.dirname(CIRCLECI)
        i = self.build.index("EXAMPLE_MAP='{}'\n          BUILD_FILTERED='false'")
        i = self.build.rindex('\n', 0, i) + 1
        j = self.build.index('          echo "matrix=$MATRIX_JSON"', i)
        block = _re.sub(r'^ {10}', '', self.build[i:j], flags=_re.M)
        with tempfile.TemporaryDirectory() as d:
            selp = os.path.join(d, 'sel.json')
            with open(selp, 'w') as fh:
                _json.dump(sel, fh)
            matrix = subprocess.run(
                [sys.executable, os.path.join(repo, '.github/scripts/ci_set_matrix.py'),
                 '--select-file', selp], capture_output=True, text=True, cwd=repo).stdout.strip()
            self.assertTrue(matrix, 'ci_set_matrix produced nothing')
            sh = os.path.join(d, 'probe.sh')
            with open(sh, 'w') as fh:
                # shlex.quote, not hand-rolled quoting: a TMPDIR with a space in it
                # made this fail for a reason that had nothing to do with the block
                fh.write('BUILD_SELECT_FILE=' + shlex.quote(selp) + '\n')
                fh.write('MATRIX_JSON=' + shlex.quote(matrix) + '\n')
                fh.write(block)
                # sentinel + newline separated: the block itself writes ::warning:: to
                # stdout
                fh.write('\nprintf "@@R@@\\n%s\\n%s" "$MATRIX_JSON" "$BUILD_FILTERED"\n')
            r = subprocess.run(['bash', sh], capture_output=True, text=True, cwd=repo)
            self.assertEqual(r.returncode, 0, r.stderr)
            mj, filtered = r.stdout.split('@@R@@\n', 1)[1].split('\n', 1)
            return sum(len(v) for v in _json.loads(mj).values()), filtered

    def test_an_empty_family_list_is_not_treated_as_unusable(self):
        """A legitimate nothing-selected PR (every family filtered out) must keep the
        all-empty matrix ci_set_matrix produced, not fall open to a full build.

        #3842 (docs + .gitignore) and #3840 (test/hil only) each rebuilt all 74 cmake
        legs after the selector had correctly chosen none, because an earlier version
        of this block conflated an empty families list with an unusable one."""
        legs, filtered = self._run_extras_block(
            {'build': {'full': False, 'families': [], 'family_examples': {}}})
        self.assertEqual(legs, 0, 'an empty families list must keep the all-empty matrix')
        self.assertEqual(filtered, 'false', 'nothing was built, so nothing to compare')

    def test_a_real_family_list_stays_scoped(self):
        legs, filtered = self._run_extras_block(
            {'build': {'full': False, 'families': ['stm32f4', 'rp2040'],
                       'family_examples': {}}})
        self.assertGreater(legs, 0)
        self.assertEqual(filtered, 'true')

    def test_membrowse_upload_is_scoped_by_the_pr_filter(self):
        # Unlike the pre-board-pins design, the upload now runs $EX_ARGS-filtered:
        # --board-pins/--pins-only fixes each pinned family to its explicit board list
        # (returned as-is, never --one-first's "first board that can build this -e
        # set"), so there is no more board-selection divergence for $EX_ARGS to cause -
        # scoping the upload by the PR's example filter is safe again.
        line = [l for l in self.util.splitlines()
                if '--target examples-membrowse-upload' in l][0]
        self.assertIn('$EX_ARGS', line)
        self.assertIn('--pins-only', line)


if __name__ == '__main__':
    unittest.main()
