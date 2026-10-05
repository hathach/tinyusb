#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Unit tests for the CircleCI/GitHub Actions selection hand-off contracts.
# Stdlib only; no builds.
#   python3 test/hil/test/test_ci_metrics.py
import json
import os
import re
import sys
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.abspath(__file__)))))
CIRCLECI = os.path.join(REPO, '.circleci')
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from test_ci_select import mf  # noqa: E402  a manifest v1 fixture
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
        # Exactly the cmake job, hil-build-esp and the identical job upload; hil-build
        # builds for the rig only. A `upload-membrowse: true` reappearing on
        # hil-build re-opens the target-name collision between its
        # raspberry_pi_pico PIO-USB variant build and cmake's plain build.
        # membrowse-identical is not a build_util.yml caller, so it is caught instead by
        # its direct MEMBROWSE_API_KEY env reference.
        uploaders = sorted(name for name, j in self.jobs.items()
                           if 'upload-membrowse: true' in j or 'MEMBROWSE_API_KEY' in j)
        self.assertEqual(uploaders, ['cmake', 'hil-build-esp', 'membrowse-identical'])

    def test_membrowse_identical_needs_no_toolchain_and_follows_the_gate(self):
        job = self.jobs['membrowse-identical']
        for heavy in ('setup_toolchain', 'docker', 'get_deps'):
            self.assertNotIn(heavy, job)
        self.assertIn('fetch-depth: 0', job)  # membrowse reads the PR head history
        # the gate builds nothing => no producer ran => every leg is identical
        self.assertIn("LEGS: ${{ needs.check-paths.outputs.code_changed == 'true' && "
                      "needs.set-matrix.outputs.membrowse_identical || needs.set-matrix.outputs.membrowse_all }}", job)
        # and the producers it complements share that gate
        for producer in ('cmake', 'hil-build-esp'):
            self.assertIn("needs.check-paths.outputs.code_changed == 'true'", self.jobs[producer])

    def test_membrowse_identical_uploads_every_leg_past_a_failure(self):
        import subprocess, tempfile
        i = self.build.index('run: |\n', self.build.index('- name: Membrowse identical upload')) + len('run: |\n')
        block = re.sub(r'^ {10}', '', self.build[i:self.build.index('exit $rc', i) + len('exit $rc')], flags=re.M)
        with tempfile.TemporaryDirectory() as d:
            with open(os.path.join(d, 'python3'), 'w') as f:
                f.write('#!/bin/sh\necho "$*" >> log\ncase "$*" in *bad*) exit 1 ;; esac\n')
            os.chmod(os.path.join(d, 'python3'), 0o755)
            r = subprocess.run(['bash', '-e', '-o', 'pipefail', '-c', block], cwd=d, capture_output=True, text=True,
                               env={**os.environ, 'PATH': d + os.pathsep + os.environ['PATH'],
                                    'LEGS': json.dumps(['-b bad', '-b x --build-name x-DMA --cflag=-DY=1'])})
            self.assertEqual(r.returncode, 1)
            with open(os.path.join(d, 'log')) as f:
                self.assertEqual(f.read().splitlines(), [
                    'tools/build.py -s cmake --target examples-membrowse-upload -j 1 -b bad',
                    'tools/build.py -s cmake --target examples-membrowse-upload -j 1 -b x --build-name x-DMA '
                    '--cflag=-DY=1'])

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
                r = subprocess.run([sys.executable, os.path.join(repo, '.claude/skills/build/scripts/check_build.py'),
                                    '--select-only', '--scope', path,
                                    '--config', 'test/hil/tinyusb.json', '--config', 'test/hil/hfp.json'],
                                   capture_output=True, text=True, cwd=repo)
                self.assertEqual(r.returncode, 0, r.stderr)
                m = json.loads(r.stdout)
                s = m['hil']
                for flasher, a in s['args_flasher']['tinyusb.json'].items():
                    self.assertTrue(ok(classes['args'], a),
                                    f'{path}/{flasher}: the args guard rejects {a!r}')
                hfp = s['args']['hfp.json']
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
                for fam, v in m['build']['families'].items():
                    exs = [] if v['examples'] == 'all' else v['examples']
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

    def test_the_toolchain_lists_match_ci_set_matrix(self):
        # cmake-required dedupes against the boards the cmake job builds, so both must
        # name the same toolchains as the workflow
        sys.path.insert(0, os.path.join(REPO, '.github', 'scripts'))
        import ci_set_matrix
        m = re.search(r"echo 'cmake_toolchains=(\[.*\])' >> \$GITHUB_OUTPUT", self.build)
        self.assertEqual(json.loads(m.group(1)), ci_set_matrix.CMAKE_JOB_TOOLCHAINS)
        m = re.search(r"^        toolchain: (\[.*\])$", self.jobs['cmake-required'], re.M)
        self.assertEqual(json.loads(m.group(1).replace("'", '"')), ci_set_matrix.REQUIRED_TOOLCHAINS)
        self.assertIn('needs.set-matrix.outputs.required_json', self.jobs['cmake-required'])

    def test_the_code_size_scope_lists_every_cmake_leg_and_esp_leg(self):
        # pr_comment.yml's compare expects a snapshot artifact per listed leg: the list
        # must be the cmake job's matrix, from the one toolchain list both read, plus every
        # hil-build-esp leg (a -DMA variant is a board of its own) under that job's owner gate
        import subprocess, tempfile
        self.assertIn('fromJSON(needs.set-matrix.outputs.cmake_toolchains)', self.jobs['cmake'])
        self.assertIn("github.repository_owner == 'hathach'", self.jobs['hil-build-esp'])
        m = re.search(r"echo 'cmake_toolchains=(\[.*\])' >> \$GITHUB_OUTPUT", self.build)
        toolchains = json.loads(m.group(1))
        self.assertNotIn('esp-idf', toolchains)
        i = self.build.index('mkdir -p code-size-scope')
        i = self.build.rindex('\n', 0, i) + 1
        j = self.build.index('cat code-size-scope/scope.json', i)
        block = re.sub(r'^ {10}', '', self.build[i:j], flags=re.M)
        pinned = {'arm-gcc': ['stm32f4', 'imxrt'], 'riscv-gcc': ['fomu'], 'esp-idf': ['espressif']}
        hil = {'arm-gcc': ['-b x'], 'esp-idf': [
            '-b espressif_s3_devkitm -e device/cdc_msc_freertos',
            '-b espressif_s3_devkitm --build-name espressif_s3_devkitm-DMA --cflag=-DCFG_TUD_DWC2_DMA_ENABLE=1']}

        def scope(changed, owner='hathach'):
            with tempfile.TemporaryDirectory() as d:
                subprocess.run(['bash', '-e', '-o', 'pipefail', '-c', block], cwd=d, capture_output=True, check=True,
                               env={**os.environ, 'TOOLCHAINS': m.group(1), 'PINNED_JSON': json.dumps(pinned),
                                    'EXAMPLE_MAP': '{"stm32f4": ["device/cdc_msc"]}', 'CODE_CHANGED': changed,
                                    'HIL_JSON': json.dumps(hil), 'OWNER': owner})
                with open(os.path.join(d, 'code-size-scope', 'scope.json')) as fh:
                    return json.load(fh)
        cmake = [{'toolchain': 'arm-gcc', 'arg': 'stm32f4'}, {'toolchain': 'arm-gcc', 'arg': 'imxrt'},
                 {'toolchain': 'riscv-gcc', 'arg': 'fomu'}]
        s = scope('true')
        self.assertEqual(s['legs'], cmake + [{'toolchain': 'esp-idf', 'arg': a} for a in hil['esp-idf']])
        self.assertEqual((s['code_changed'], s['family_examples']), (True, {'stm32f4': ['device/cdc_msc']}))
        self.assertEqual(scope('true', owner='fork')['legs'], cmake)  # hil-build-esp does not run there
        self.assertEqual(scope('false')['legs'], [])  # no code change: nothing to measure

    def test_the_esp_snapshot_runs_in_the_idf_image_with_the_leg_as_data(self):
        import subprocess, tempfile
        with open(os.path.join(REPO, '.github', 'workflows', 'build_util.yml')) as f:
            text = f.read()
        def step_script(name):
            i = text.index('run: |\n', text.index(f'- name: {name}\n')) + len('run: |\n')
            return re.sub(r'^ {10}', '', text[i:text.index('        shell: bash', i)], flags=re.M)
        block = step_script('Code size snapshot')
        with tempfile.TemporaryDirectory() as d:
            # docker runs its inner command here with the env it was told to pass; git, pip
            # and python only record their argv
            stubs = {'docker': 'echo "docker ${*:1:$#-1}" >> "$LOG"; exec bash -c "${!#}"',
                     'git': 'echo "git $*" >> "$LOG"', 'pip': 'echo "pip $*" >> "$LOG"',
                     'python': '{ printf "python"; printf " [%s]" "$@"; echo; } >> "$LOG"'}
            for name, body in stubs.items():
                with open(os.path.join(d, name), 'w') as f:
                    f.write(f'#!/bin/bash\n{body}\n')
                os.chmod(os.path.join(d, name), 0o755)
            env = {**os.environ, 'PATH': d + os.pathsep + os.environ['PATH'], 'LOG': os.path.join(d, 'log'),
                   'TOOLCHAIN': 'esp-idf', 'BUILD_OUTCOME': 'success', 'EX_ARGS': '',
                   'GITHUB_EVENT_NAME': 'pull_request'}

            def run(arg, toolchain='esp-idf'):
                open(env['LOG'], 'w').close()
                r = subprocess.run(['bash', '-e', '-o', 'pipefail', '-c', block], cwd=d, capture_output=True,
                                   text=True, env={**env, 'ARG': arg, 'TOOLCHAIN': toolchain})
                self.assertEqual(r.returncode, 0, r.stderr)
                with open(env['LOG']) as f:
                    return f.read().splitlines()
            log = run('-b espressif_s3_devkitm $(touch pwned) ;id')
            self.assertFalse(os.path.exists(os.path.join(d, 'pwned')))
            self.assertTrue(log[0].startswith('docker run --rm -e ARG -e EX_ARGS -e BUILD_OUTCOME -e GITHUB_EVENT_NAME '
                                              f'-v {d}:/project -w /project espressif/idf:tinyusb bash -c'))
            self.assertEqual(log[1:3], ['git config --global --add safe.directory /project',
                                        'pip install --only-binary :all: membrowse==1.2.12'])
            self.assertEqual(log[3], 'python [tools/code_size.py] [snapshot] [--symbols] [--build-outcome] [success] '
                                     '[-o] [code-size] [-b] [espressif_s3_devkitm] [$(touch] [pwned)] [;id]')
            self.assertEqual(run('stm32f4', toolchain='arm-gcc'),  # every other toolchain sizes on the runner
                             ['python [tools/code_size.py] [snapshot] [--symbols] [--build-outcome] [success] '
                              '[-o] [code-size] [stm32f4]'])
            # a variant leg is sized too, as the board its --build-name names
            self.assertEqual(run('-b espressif_s3_devkitm --build-name x-DMA --cflag=-DY=1')[-1],
                             'python [tools/code_size.py] [snapshot] [--symbols] [--build-outcome] [success] '
                             '[-o] [code-size] [-b] [espressif_s3_devkitm] [--build-name] [x-DMA] [--cflag=-DY=1]')

    def test_the_code_size_comment_is_written_even_without_usable_snapshots(self):
        # a run without usable snapshots, and a baseline lookup that fails, must still
        # leave a comment.md, so the post replaces an earlier push's table
        import subprocess, tempfile
        with open(os.path.join(REPO, '.github', 'workflows', 'pr_comment.yml')) as f:
            text = f.read()

        def step_script(name):
            i = text.index('run: |\n', text.index(f'- name: {name}\n')) + len('run: |\n')
            j = re.search(r'^\s*$', text[i:], re.M).start() + i  # the block ends at the first blank line
            return re.sub(r'^ {10}', '', text[i:j], flags=re.M)
        with tempfile.TemporaryDirectory() as d:
            env = {**os.environ, 'RUNNER_TEMP': d, 'REPO': 'x/y', 'RUN_URL': 'https://example/run',
                   'GITHUB_STEP_SUMMARY': os.path.join(d, 'summary.md'), 'GITHUB_OUTPUT': os.path.join(d, 'out'),
                   'PATH': d + os.pathsep + os.environ['PATH']}
            with open(os.path.join(d, 'gh'), 'w') as f:  # any API call fails
                f.write('#!/bin/sh\nexit 1\n')
            os.chmod(os.path.join(d, 'gh'), 0o755)
            leg = os.path.join(d, 'code-size', 'current', 'code-size-arm-gcc-x')
            os.makedirs(leg)
            with open(os.path.join(leg, 'leg.json'), 'w') as f:
                json.dump({'schema': 1, 'boards': [], 'examples': None, 'build_outcome': 'success',
                           'sha': 'a' * 40, 'base_sha': 'a' * 40, 'head_sha': 'a' * 40}, f)
            for name in ('Find the baseline', 'Compare against the baseline'):
                r = subprocess.run(['bash', '-e', '-o', 'pipefail', '-c', step_script(name)], cwd=REPO,
                                   capture_output=True, text=True, env=env)
                self.assertEqual(r.returncode, 0, f'{name}: {r.stderr}')
            with open(os.path.join(d, 'out')) as f:
                self.assertEqual(f.read(), 'run_id=\n')  # no baseline download
            with open(os.path.join(d, 'code-size', 'out', 'comment.md')) as f:
                comment = f.read()
        self.assertTrue(comment.startswith('## Code size'))
        self.assertIn('Baseline: unavailable - the baseline lookup failed', comment)
        self.assertIn('no usable scope manifest', comment)
        self.assertTrue(comment.endswith('_[Full report](https://example/run)_\n'))

    def _run_block(self, block, sel, setup=None):
        """Run a dedented build.yml step block with `sel` as ci_select_out.json in a temp
        dir; `setup(dir)` may prepare the dir and return extra env. Returns the block's
        $GITHUB_OUTPUT as a dict."""
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            return self._run_block_in(d, block, sel, setup)

    def _run_block_in(self, d, block, sel, setup=None):
        import subprocess
        with open(os.path.join(d, 'ci_select_out.json'), 'w') as fh:
            json.dump(sel, fh)
        out = os.path.join(d, 'gh_output')
        open(out, 'w').close()
        env = {**os.environ, **((setup and setup(d)) or {}), 'GITHUB_OUTPUT': out}
        r = subprocess.run(['bash', '-e', '-o', 'pipefail', '-c', block], cwd=d,
                           capture_output=True, text=True, env=env)
        self.assertEqual(r.returncode, 0, r.stderr)
        with open(out) as fh:
            return dict(l.split('=', 1) for l in fh.read().splitlines() if '=' in l)

    def _run_matrix_step(self, sel, fail_pinned=False):
        """Run the whole 'Generate matrix json' step for real, optionally with the
        SCOPED --pinned invocation failing (the unscoped fallback still works, as a
        broken script would not). Returns the step's $GITHUB_OUTPUT as a dict."""
        import shlex
        repo = os.path.dirname(CIRCLECI)
        i = self.build.index('SELECT_FILE=ci_select_out.json')
        i = self.build.rindex('\n', 0, i) + 1
        j = self.build.index('# HIL matrix', i)
        block = re.sub(r'^ {10}', '', self.build[i:j], flags=re.M)

        def setup(d):
            # ci_set_matrix resolves the repo from its own path and reads hw/bsp for
            # the pinned families, so the fake tree needs both
            for name in ('.github', 'hw', 'test', 'tools'):
                os.symlink(os.path.join(repo, name), os.path.join(d, name))
            bin_dir = os.path.join(d, 'bin')
            os.mkdir(bin_dir)
            with open(os.path.join(bin_dir, 'python'), 'w') as fh:
                fh.write('#!/bin/sh\n')
                if fail_pinned:
                    fh.write('case " $* " in *" --pinned "*--select-file*) exit 1 ;; esac\n')
                fh.write(f'exec {shlex.quote(sys.executable)} "$@"\n')
            os.chmod(os.path.join(bin_dir, 'python'), 0o755)
            return {'PATH': bin_dir + os.pathsep + os.environ['PATH']}
        return self._run_block(block, sel, setup)

    def test_the_pinned_matrix_is_scoped_with_the_example_map(self):
        sel = mf(['stm32f4', 'rp2040'])
        sel['build']['families']['stm32f4']['examples'] = ['device/cdc_msc']
        got = self._run_matrix_step(sel)
        self.assertEqual(json.loads(got['pinned_matrix'])['arm-gcc'], ['rp2040', 'stm32f4'])
        # rp2040 builds every example: no map entry
        self.assertEqual(json.loads(got['example_map']), {'stm32f4': ['device/cdc_msc']})

    def test_the_required_legs_come_from_the_selection(self):
        got = self._run_matrix_step(mf(['stm32f4'], required=['stm32f407disco', 'stm32u083cdk']))
        self.assertEqual(json.loads(got['required_json'])['arm-gcc'], ['-b stm32u083cdk'])
        # a build matrix fallen open to full still builds no unpinned board: keep them
        got = self._run_matrix_step(mf(['stm32f4'], required=['stm32u083cdk']), fail_pinned=True)
        self.assertEqual(json.loads(got['required_json'])['arm-gcc'], ['-b stm32u083cdk'])

    def test_a_failed_pinned_matrix_drops_the_example_map_too(self):
        # the pinned matrix falls open on its own failure; leaving the example map
        # scoped would filter the examples of a full build and upload those sizes
        sel = mf(['stm32f4'])
        sel['build']['families']['stm32f4']['examples'] = ['device/cdc_msc']
        import subprocess
        got = self._run_matrix_step(sel, fail_pinned=True)
        self.assertEqual(json.loads(got['example_map']), {})
        repo = os.path.dirname(CIRCLECI)
        full = subprocess.run([sys.executable, os.path.join(repo, '.github/scripts/ci_set_matrix.py'),
                               '--pinned'], capture_output=True, text=True, cwd=repo).stdout
        self.assertEqual(json.loads(got['pinned_matrix']), json.loads(full))

    def _run_rig_outputs(self, sel):
        """Run the args_*/run_* emitter of the 'CI selection (PR only)' step for real
        on `sel`. Returns its $GITHUB_OUTPUT as a dict."""
        i = self.build.index("          OUT=''\n")
        j = self.build.index('echo "$OUT" >> $GITHUB_OUTPUT', i)
        j = self.build.index('\n', j) + 1
        def setup(d):
            for name in ('test', 'tools'):  # ci_select, and the roster helpers it imports
                os.symlink(os.path.join(REPO, name), os.path.join(d, name))
        return self._run_block(re.sub(r'^ {10}', '', self.build[i:j], flags=re.M), sel, setup)

    def _run_gate(self, event, before='', sel=None, suites_rc=0):
        """Run check-paths' gate step with the suites and check_build.py stubbed:
        check_build prints `sel` (fails when None). Returns (code, check_build argv)."""
        import shlex, tempfile
        i = self.build.index('run: |\n', self.build.index('- name: Selection gate')) + len('run: |\n')
        j = self.build.index('\n\n', i)
        block = re.sub(r'^ {10}', '', self.build[i:j], flags=re.M)

        def setup(d):
            for name in ('test', 'tools'):
                os.symlink(os.path.join(REPO, name), os.path.join(d, name))
            os.mkdir(os.path.join(d, 'bin'))
            with open(os.path.join(d, 'bin', 'python3'), 'w') as fh:
                fh.write('#!/bin/sh\n'
                         f'case "$*" in *test_ci_select.py*|*test_build_select.py*) exit {suites_rc} ;;\n'
                         '  *check_build.py*) echo "$*" > argv; exec cat ci_select_out.json ;; esac\n'
                         f'exec {shlex.quote(sys.executable)} "$@"\n')
            os.chmod(os.path.join(d, 'bin', 'python3'), 0o755)
            if sel is None:
                os.unlink(os.path.join(d, 'ci_select_out.json'))
            return {'PATH': os.path.join(d, 'bin') + os.pathsep + os.environ['PATH'],
                    'EVENT': event, 'BASE_REF': 'master', 'BEFORE': before}
        with tempfile.TemporaryDirectory() as d:
            code = self._run_block_in(d, block, sel, setup)['code']
            argv = os.path.join(d, 'argv')
            return code, (open(argv).read().split() if os.path.exists(argv) else None)

    def test_the_gate_skips_only_what_the_selector_clears(self):
        doc = {'path': 'README.rst', 'effect': 'none'}
        gap = {'path': 'src/class/bth/bth_device.c', 'effect': 'gap'}
        code, argv = self._run_gate('pull_request', sel=mf(paths=[doc]))
        self.assertEqual(code, 'false')
        self.assertIn('--base origin/master', ' '.join(argv))
        self.assertIn('--config test/hil/tinyusb.json --config test/hil/hfp.json', ' '.join(argv))
        self.assertEqual(self._run_gate('pull_request', sel=mf(paths=[doc, gap]))[0], 'true')
        self.assertEqual(self._run_gate('pull_request', sel=mf(['stm32f4']))[0], 'true')
        self.assertEqual(self._run_gate('pull_request', sel=mf(boards={'x': 'all'}))[0], 'true')
        code, argv = self._run_gate('push', before='a1b2c3', sel=mf(paths=[doc]))
        self.assertEqual(code, 'false')
        self.assertIn('--endpoints a1b2c3..HEAD', ' '.join(argv))

    def test_the_gate_runs_everything_without_a_usable_selection(self):
        doc = mf(paths=[{'path': 'README.rst', 'effect': 'none'}])
        for name, kw in {'new branch push': dict(event='push', before='0' * 40, sel=doc),
                         'push without before': dict(event='push', sel=doc),
                         'dispatch': dict(event='workflow_dispatch', sel=doc),
                         'release': dict(event='release', sel=doc),
                         'suites fail': dict(event='pull_request', sel=doc, suites_rc=1),
                         'selector fails': dict(event='pull_request', sel=None),
                         'legacy selection': dict(event='pull_request', sel={'full': False, 'build': {}})}.items():
            with self.subTest(name):
                code, argv = self._run_gate(**kw)
                self.assertEqual(code, 'true')
                if name in ('new branch push', 'push without before', 'dispatch', 'release', 'suites fail'):
                    self.assertIsNone(argv)

    ALL_RUN = {'run_tinyusb': 'true', 'run_tinyusb_esp': 'true', 'run_hfp': 'true'}

    def test_rig_outputs_skip_an_unselected_rig(self):
        sel = mf(boards={'raspberry_pi_pico': 'all'})
        sel['hil'].update(args={'tinyusb.json': '-b raspberry_pi_pico', 'hfp.json': ''},
                          args_flasher={'tinyusb.json': {'openocd': '-b raspberry_pi_pico'}, 'hfp.json': {}})
        got = self._run_rig_outputs(sel)
        self.assertEqual((got['run_tinyusb'], got['run_tinyusb_esp'], got['run_hfp']),
                         ('true', 'false', 'false'))

    def test_rig_outputs_full_runs_every_rig(self):
        sel = mf(hil_full=True)
        sel['hil'].update(args={'tinyusb.json': '', 'hfp.json': ''},
                          args_flasher={'tinyusb.json': {}, 'hfp.json': {}})
        got = self._run_rig_outputs(sel)
        self.assertEqual({k: got[k] for k in self.ALL_RUN}, self.ALL_RUN)

    def test_rig_outputs_fall_open_on_a_malformed_selection(self):
        # run_hfp=false now skips hil-hfp-iar before its own selection can fall back,
        # so a selection missing a rig's entry must run every rig, not skip that one
        def bad(**hil):
            m = mf()
            m['hil'].update(args={'tinyusb.json': '', 'hfp.json': ''},
                            args_flasher={'tinyusb.json': {}, 'hfp.json': {}})
            m['hil'].update(hil)
            return m
        no_boards = bad()
        del no_boards['hil']['boards']
        bad = {
            'no hfp args': bad(args={'tinyusb.json': ''}),
            'no tinyusb flasher map': bad(args_flasher={'hfp.json': {}}),
            'full not a bool': bad(full='false'),
            # hil_ci_set_matrix falls open to the whole roster here: run_*=false instead
            # would build everything and still skip the rigs
            'no boards map': no_boards,
            'needed contradicts boards': bad(needed=True),
            'legacy shape': {'full': False, 'boards': {}, 'args': {'tinyusb.json': '', 'hfp.json': ''},
                             'args_flasher': {'tinyusb.json': {}, 'hfp.json': {}}},
            'bad characters': bad(args={'tinyusb.json': '', 'hfp.json': '-b x\nrun_hfp=false'}),
        }
        for name, sel in bad.items():
            with self.subTest(name):
                got = self._run_rig_outputs(sel)
                self.assertEqual({k: got[k] for k in self.ALL_RUN}, self.ALL_RUN)
                self.assertEqual(got['args_hfp'], '')

    def test_hfp_iar_skips_on_set_matrix_selection_before_taking_the_runner(self):
        job = self.jobs['hil-hfp-iar']
        needs = re.search(r'^    needs: \[(.*)\]$', job, re.M).group(1)
        self.assertEqual({n.strip() for n in needs.split(',')}, {'check-paths', 'set-matrix'})
        cond = re.search(r'^    if: \|\n((?:      .*\n)+)', job, re.M).group(1)
        # whole, not fragments: an added success() anywhere (or the implicit one, without
        # !cancelled()) skips the job when set-matrix fails, and a set-matrix that failed
        # after writing run_hfp=false must leave the job to its own selection
        self.assertEqual(' '.join(cond.split()),
                         "!cancelled() && needs.check-paths.result == 'success' && "
                         "needs.check-paths.outputs.code_changed == 'true' && "
                         "github.repository_owner == 'hathach' && "
                         "!(github.event_name == 'pull_request' && "
                         "github.event.pull_request.head.repo.fork == true) && "
                         "(needs.set-matrix.result != 'success' || "
                         "needs.set-matrix.outputs.hil_run_hfp != 'false')")
        # it still narrows (or falls back to full) by itself
        self.assertIn('check_build.py --select-only --base "origin/$BASE_REF"', job)
        self.assertIn('--config test/hil/hfp.json > ci_select.json', job)
        self.assertIn('ci_select.check_manifest(json.load(open("ci_select.json")))', job)

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
        flat = re.sub(r"['\"]\s*\n\s*f?['\"]", '', matrix)
        hits = [m.start() for m in re.finditer('emitting the full ', flat)]
        self.assertGreaterEqual(len(hits), 2, 'fall-open messages not found')
        for i in hits:
            self.assertIn('UNSCOPED', flat[max(0, i - 200):i],
                          'a fall-open path without the marker build.yml greps for')

    def _matrix_legs(self, sel):
        """Families across every toolchain leg of the step's matrix, for `sel`."""
        return sum(len(v) for v in
                   json.loads(self._run_matrix_step(sel)['pinned_matrix']).values())

    def test_an_empty_family_list_is_not_treated_as_unusable(self):
        """A legitimate nothing-selected PR (every family filtered out) must keep the
        all-empty matrix ci_set_matrix produced, not fall open to a full build.

        #3842 (docs + .gitignore) and #3840 (test/hil only) each rebuilt all 74 cmake
        legs after the selector had correctly chosen none, because an earlier version
        of this block conflated an empty families list with an unusable one."""
        legs = self._matrix_legs(mf())
        self.assertEqual(legs, 0, 'an empty families list must keep the all-empty matrix')

    def test_a_real_family_list_stays_scoped(self):
        legs = self._matrix_legs(mf(['stm32f4', 'rp2040']))
        self.assertGreater(legs, 0)
        self.assertLess(legs, self._matrix_legs(mf(full=True)))

    def test_membrowse_upload_is_not_scoped_by_the_pr_filter(self):
        # $EX_ARGS must reach build.py so the upload resolves the same board the Build
        # step fell back to; --ci-pinned-boards-only keeps it from scoping the examples
        # (an out-of-selection example still gets its --identical row)
        line = [l for l in self.util.splitlines()
                if '--target examples-membrowse-upload' in l][0]
        self.assertIn('$EX_ARGS', line)
        self.assertIn('--ci-pinned-boards-only', line)
        # M3: what the Build step should have built fails, never goes identical
        self.assertIn('--expect-built', line)
        step = self.util[self.util.index('- name: Membrowse Upload'):]
        self.assertIn("if: ${{ !cancelled() && inputs.upload-membrowse }}", step.split('run: |')[0])

    def test_the_build_step_stays_scoped_by_the_pr_filter(self):
        # the fix above only touches the Membrowse Upload step - the Build step
        # must keep compiling just the PR-selected examples
        line = [l for l in self.util.splitlines()
                if 'python tools/build.py $BUILD_PY_ARGS ${{ matrix.arg }} $EX_ARGS' in l]
        self.assertTrue(line, '$EX_ARGS missing from the Build step invocation')


if __name__ == '__main__':
    unittest.main()
