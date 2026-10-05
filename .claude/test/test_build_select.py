"""The selection contract: `check_build.py --select-only` prints tools/ci_select.py's
manifest v1 and stops before any dependency check or build, under the bare-runner
interpreter CI gives it; ci_select's input modes resolve their revisions once and
refuse what they cannot answer."""
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import ci_select  # noqa: E402

CHECK_BUILD = ROOT / '.claude' / 'skills' / 'build' / 'scripts' / 'check_build.py'
# runs inside an isolated interpreter: every path to a build raises there
SELECT_ONLY = f'''
import importlib.util, json, sys
spec = importlib.util.spec_from_file_location('check_build', {str(CHECK_BUILD)!r})
cb = importlib.util.module_from_spec(spec); spec.loader.exec_module(cb)
def refuse(*a, **k):
    raise SystemExit('select-only reached a build step')
cb.build_one = cb.ensure_deps = cb.boards_for = cb.missing_deps = refuse
cb.tools_build.build_boards_list = refuse
sys.exit(cb.main(sys.argv[1:]))
'''


def select_only(*args):
    return subprocess.run([sys.executable, '-I', '-S', '-c', SELECT_ONLY, '--select-only', *args],
                          capture_output=True, text=True, cwd=ROOT, timeout=300)


class SelectOnlyTest(unittest.TestCase):
    def test_prints_the_manifest_isolated_and_builds_nothing(self):
        r = select_only('--scope', 'src/class/cdc/cdc_device.c', 'docs/index.rst',
                        '--config', 'test/hil/tinyusb.json', '--config', 'test/hil/hfp.json')
        self.assertEqual(r.returncode, 0, r.stderr)
        m = json.loads(r.stdout.splitlines()[-1])
        self.assertEqual(m['version'], 1)
        self.assertEqual(m['input']['mode'], 'paths')
        b = m['build']
        self.assertIs(b['full'], False)
        self.assertIs(b['needed'], True)
        self.assertTrue(b['families'])
        for sel in b['families'].values():
            self.assertTrue(sel['examples'] == 'all' or (isinstance(sel['examples'], list) and sel['examples']))
        rules = {p['path']: (p['rule'], p['effect']) for p in b['paths']}
        self.assertEqual(rules, {'src/class/cdc/cdc_device.c': ('class', 'select'),
                                 'docs/index.rst': ('noncode', 'none')})
        h = m['hil']
        self.assertIs(h['full'], False)
        self.assertIs(h['needed'], True)
        self.assertEqual(set(h['args']), {'tinyusb.json', 'hfp.json'})

    def test_named_boards_have_nothing_to_select(self):
        r = select_only('--board', 'stm32f407disco')
        self.assertEqual(r.returncode, 2)
        self.assertIn('--select-only', r.stdout)

    def test_the_contract_entry_point_selects_everything(self):
        r = select_only('--scope', '.claude/skills/build/scripts/check_build.py', 'docs/index.rst')
        m = json.loads(r.stdout.splitlines()[-1])
        self.assertIs(m['build']['full'], True)
        self.assertIs(m['hil']['full'], True)
        self.assertEqual([p['rule'] for p in m['build']['paths']], ['selector', 'noncode'])


def git(repo, *argv):
    return subprocess.run(['git', *argv], cwd=repo, capture_output=True, text=True, check=True).stdout.strip()


class InputModeTest(unittest.TestCase):
    """select_input over a scratch repo: which files, which get_deps blobs, which SHAs."""

    def setUp(self):
        # a pre-commit hook exports GIT_DIR/GIT_INDEX_FILE for the outer repo: git in the
        # scratch repo (ours and ci_select's) must not inherit them
        env = mock.patch.dict(os.environ, {k: v for k, v in os.environ.items() if not k.startswith('GIT_')},
                              clear=True)
        env.start()
        self.addCleanup(env.stop)
        self.tmp = tempfile.TemporaryDirectory()
        self.repo = self.tmp.name
        git(self.repo, 'init', '-q', '-b', 'main')
        git(self.repo, 'config', 'user.email', 't@t')
        git(self.repo, 'config', 'user.name', 't')
        self.write('a.c', 'a\n')
        self.write('tools/get_deps.py', 'deps = {}\n')
        git(self.repo, 'add', '.')
        git(self.repo, 'commit', '-qm', 'base')
        self.base = git(self.repo, 'rev-parse', 'HEAD')
        git(self.repo, 'checkout', '-qb', 'topic')
        self.write('b.c', 'b\n')
        git(self.repo, 'add', '.')
        git(self.repo, 'commit', '-qm', 'topic')
        self.head = git(self.repo, 'rev-parse', 'HEAD')

    def tearDown(self):
        self.tmp.cleanup()

    def write(self, rel, text):
        p = Path(self.repo, rel)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text)

    def test_base_diffs_merge_base_to_head_and_records_the_shas(self):
        files, gd, inp = ci_select.select_input(self.repo, base='main')
        self.assertEqual(files, ['b.c'])
        self.assertIsNone(gd)
        self.assertEqual(inp, {'mode': 'base', 'base': 'main', 'base_sha': self.base, 'merge_base': self.base,
                               'head': self.head, 'deps_base': None})

    def test_worktree_adds_uncommitted_and_untracked_files(self):
        self.write('a.c', 'changed\n')
        self.write('new.c', 'n\n')
        files, _, inp = ci_select.select_input(self.repo, base='main', worktree=True)
        self.assertEqual(sorted(files), ['a.c', 'b.c', 'new.c'])
        self.assertEqual(inp['mode'], 'worktree')

    def test_endpoints_diff_a_to_b_without_a_merge_base(self):
        files, _, inp = ci_select.select_input(self.repo, endpoints=f'{self.base}..HEAD')
        self.assertEqual(files, ['b.c'])
        self.assertEqual((inp['mode'], inp['base_sha'], inp['merge_base'], inp['head']),
                         ('endpoints', self.base, None, self.head))

    def test_endpoints_refuse_what_they_cannot_answer(self):
        for bad in (f'{self.base}..{self.base}', '0000000000000000000000000000000000000000..HEAD',
                    'HEAD', '..HEAD', 'HEAD...main', 'nosuchref..HEAD'):
            with self.assertRaises(ci_select.SelectError, msg=bad):
                ci_select.select_input(self.repo, endpoints=bad)

    def test_a_ref_that_names_no_commit_is_refused(self):
        with self.assertRaises(ci_select.SelectError):
            ci_select.select_input(self.repo, base='nosuchref')

    def test_paths_with_deps_base_refuse_an_unchanged_get_deps(self):
        listing = Path(self.repo, 'scope.txt')
        listing.write_text('tools/get_deps.py\n')
        with self.assertRaises(ci_select.SelectError):
            ci_select.select_input(self.repo, diff_file=str(listing), deps_base='HEAD')
        files, gd, inp = ci_select.select_input(self.repo, diff_file=str(listing))
        self.assertEqual((files, gd, inp['mode'], inp['deps_base']), (['tools/get_deps.py'], None, 'paths', None))


class ManifestTest(unittest.TestCase):
    def test_paths_carry_null_for_every_family_never_an_empty_list(self):
        m, _, _ = ci_select.manifest(['src/class/cdc/cdc_device.c', 'hw/bsp/stm32f4/family.c',
                                      'src/portable/no_vendor/no_driver/dcd_bogus.c'], str(ROOT), [], None, {})
        recs = {p['path']: p for p in m['build']['paths']}
        self.assertIsNone(recs['src/class/cdc/cdc_device.c']['families'])
        self.assertEqual(recs['hw/bsp/stm32f4/family.c']['families'], ['stm32f4'])
        bogus = recs['src/portable/no_vendor/no_driver/dcd_bogus.c']
        self.assertEqual((bogus['effect'], bogus['families'], bogus['port']), ('gap', None, 'no_vendor/no_driver'))
        self.assertEqual(m['build']['families']['stm32f4']['examples'], 'all')

    def test_full_lists_no_families_and_targets_come_from_records(self):
        m, _, _ = ci_select.manifest(['tools/membrowse_cli.py'], str(ROOT), [], None, {})
        self.assertEqual((m['build']['full'], m['build']['families'], m['build']['required_targets']),
                         (True, {}, ['examples-membrowse-upload']))
        self.assertIs(m['hil']['needed'], False)


if __name__ == '__main__':
    unittest.main()
