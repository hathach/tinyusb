#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# helper/agentrc.py, the bootstrap that picks the agentrc the harness and tools load, in a scratch
# tree against scratch agentrc checkouts made of the real closure: a staged .agentrc/ wins over
# CI's pin and the installed agentrc and never falls back when it is not its manifest's; CI
# (strictly true or false) loads the pin, fetched only there; elsewhere the installed one. It
# reads agentrc_source.py's API before running it, takes one identity per process, and loads
# only closure files whose bytes are still the identity's, through hil_load: one module per file
# whatever names load it, and a module whose exec fails stays out of sys.modules. Run directly:
#   python3 test/hil/test/test_hil_agentrc.py
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HIL_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO = os.path.dirname(os.path.dirname(HIL_DIR))
sys.path.insert(0, HIL_DIR)
from helper import agentrc   # noqa: E402

RTT_PY = 'skills/rtt/scripts/rtt.py'
ESP_SESSION = 'skills/target-debug/scripts/esp_session.py'
REAL = agentrc.identity()
# git deaf to a caller's GIT_* (a pre-commit hook's GIT_DIR would aim it at tinyusb) and to its identity
GIT_ENV = {**{k: v for k, v in os.environ.items() if not k.startswith('GIT_')},
           'GIT_AUTHOR_NAME': 't', 'GIT_AUTHOR_EMAIL': 't@t', 'GIT_COMMITTER_NAME': 't', 'GIT_COMMITTER_EMAIL': 't@t',
           'GIT_CONFIG_GLOBAL': os.devnull, 'GIT_CONFIG_NOSYSTEM': '1'}   # never signs, never reads the user's config


def git(root, *args):
    return subprocess.run(['git', '-C', str(root), *args], env=GIT_ENV, capture_output=True, text=True,
                          check=True).stdout.strip()


def scratch_agentrc(dest, changes=None, drop=()):
    """A committed agentrc checkout at dest of the closure this process loaded, `changes` ({rel: text}) applied
    and `drop` left out: an install AGENTRC_DIR can name, or a pin to clone. Other suites import it."""
    dest = Path(dest)
    for rel in REAL['files']:
        if rel not in drop:
            (dest / rel).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy(Path(REAL['root']) / rel, dest / rel)
    for rel, text in {'.gitignore': '__pycache__/\n', **(changes or {})}.items():   # as agentrc's own
        (dest / rel).parent.mkdir(parents=True, exist_ok=True)
        (dest / rel).write_text(text)
    git(dest, 'init', '-q')
    git(dest, 'add', '-A')
    git(dest, 'commit', '-qm', 'scratch agentrc')
    return dest


def scratch_env(agentrc_dir=None, **extra):
    """os.environ for a child in a scratch tree on a dev PC: no CI, and AGENTRC_DIR the agentrc this process
    loaded (its checkout, an installed agentrc) unless agentrc_dir names another."""
    env = {k: v for k, v in os.environ.items() if k not in ('CI', 'GITHUB_ACTIONS')}
    return {**env, 'AGENTRC_DIR': str(agentrc_dir or REAL['root']), **extra}


class Bootstrap(unittest.TestCase):
    def setUp(self):
        td = tempfile.TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.tmp = Path(td.name).resolve()
        self.root = self.tmp / 'tinyusb'
        for rel in ('test/hil/helper/__init__.py', 'test/hil/helper/agentrc.py', 'test/hil/helper/hil_report.py'):
            (self.root / rel).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy(os.path.join(REPO, rel), self.root / rel)
        self.install = self.checkout('install')
        self.pin(git(self.install, 'rev-parse', 'HEAD'))

    def checkout(self, name, changes=None):
        return scratch_agentrc(self.tmp / name, changes)

    def pin(self, commit, url=None):
        (self.root / 'test/hil/agentrc.json').write_text(json.dumps({'url': url or self.install.as_uri(),
                                                                      'commit': commit}))

    def env(self, ci=None, agentrc_dir=None):
        return scratch_env(agentrc_dir or self.install, **({} if ci is None else {'CI': ci}))

    def run_py(self, code, **env):
        return subprocess.run([sys.executable, '-c', 'import sys; sys.path.insert(0, "test/hil"); '
                               'from helper import agentrc\n' + code],
                              cwd=self.root, env=self.env(**env), capture_output=True, text=True, timeout=60)

    def bootstrap(self, *argv, **env):
        return subprocess.run([sys.executable, 'test/hil/helper/agentrc.py', *argv], cwd=self.root,
                              env=self.env(**env), capture_output=True, text=True, timeout=60)

    def check(self, **env):
        r = self.bootstrap('check', **env)
        self.assertEqual(r.returncode, 0, r.stderr)
        return json.loads(r.stdout)

    def refused(self, r, *texts):
        self.assertNotEqual(r.returncode, 0, r.stdout)
        for t in texts:
            self.assertIn(t, r.stderr)

    def stage(self):
        agentrc.helper().stage(REAL, self.root)

    def provide_pin(self):
        """lib/agentrc as CI's fetch leaves it: a clone of the install at the pin."""
        subprocess.run(['git', 'clone', '-q', str(self.install), str(self.root / agentrc.DEP)], env=GIT_ENV,
                       check=True)

    # --- selection ---------------------------------------------------------------------------
    def test_a_dev_pc_loads_the_installed_agentrc(self):
        ident = self.check()
        self.assertEqual((ident['source'], ident['root'], ident['commit'], ident['dirty']),
                         ('installed', str(self.install), git(self.install, 'rev-parse', 'HEAD'), False))
        self.assertEqual(ident['files'], REAL['files'])
        (self.install / RTT_PY).write_text((self.install / RTT_PY).read_text() + '\n')
        self.assertTrue(self.check()['dirty'], 'an uncommitted closure edit is a dirty identity')

    def test_ci_loads_the_pin_never_the_installed_agentrc(self):
        self.provide_pin()
        for var in ('CI', 'GITHUB_ACTIONS'):
            with self.subTest(var):
                env = self.env(agentrc_dir=self.tmp / 'nowhere')
                r = subprocess.run([sys.executable, 'test/hil/helper/agentrc.py', 'check'], cwd=self.root,
                                   env={**env, var: 'true'}, capture_output=True, text=True, timeout=60)
                self.assertEqual(r.returncode, 0, r.stderr)
                ident = json.loads(r.stdout)
                self.assertEqual((ident['source'], ident['root']), ('pin', str(self.root / agentrc.DEP)))

    def test_ci_refuses_a_missing_or_off_pin_checkout_naming_the_fetch(self):
        self.refused(self.bootstrap('check', ci='true'), f'no usable agentrc at {self.root / agentrc.DEP}',
                     agentrc.FETCH)
        self.provide_pin()
        self.pin('0' * 40)
        self.refused(self.bootstrap('check', ci='true'), f'not the pin {"0" * 40}', agentrc.FETCH)

    def test_ci_is_strictly_true_or_false(self):
        for value, source in (('false', 'installed'), ('0', 'installed'), ('', 'installed')):
            with self.subTest(value):
                self.assertEqual(self.check(ci=value)['source'], source)
        self.refused(self.bootstrap('check', ci='maybe'), "CI='maybe' is neither true nor false")

    def test_a_staged_tree_wins_over_ci_and_the_installed_agentrc(self):
        self.stage()
        for ci in (None, 'true'):
            with self.subTest(ci=ci):
                ident = self.check(ci=ci, agentrc_dir=self.tmp / 'nowhere')
                self.assertEqual((ident['source'], ident['root'], ident['commit'], ident['sha256']),
                                 ('staged', str(self.root / '.agentrc'), REAL['commit'], REAL['sha256']))

    def test_a_staged_tree_unlike_its_manifest_never_falls_back(self):
        self.stage()
        staged = self.root / '.agentrc'
        (staged / RTT_PY).write_text('edited on the rig\n')
        self.refused(self.bootstrap('check'), f'differ from their manifest: {RTT_PY}', 'stage the run again')
        (staged / RTT_PY).unlink()
        self.refused(self.bootstrap('check'), f'staged agentrc {RTT_PY} is missing')
        (staged / agentrc.HIL_LOAD).unlink()   # run before the manifest is read: still refused, not a traceback
        r = self.bootstrap('check')
        self.refused(r, f'no usable agentrc at {staged}', 'stage the run again')
        self.assertTrue(r.stderr.startswith('error: '), r.stderr)
        shutil.rmtree(staged)
        staged.mkdir()
        self.refused(self.bootstrap('check'), f'no usable agentrc at {staged}', 'stage the run again')

    # --- fetch -------------------------------------------------------------------------------
    def test_fetch_runs_in_ci_only(self):
        self.refused(self.bootstrap('fetch'), 'fetch runs in CI only')
        self.assertFalse((self.root / agentrc.DEP).exists())

    def test_fetch_checks_out_the_pin_once_and_refuses_a_changed_checkout(self):
        commit = git(self.install, 'rev-parse', 'HEAD')
        git(self.install, 'commit', '-q', '--allow-empty', '-m', 'later')   # the pin is not the tip
        for _ in range(2):
            r = self.bootstrap('fetch', ci='true')
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertEqual(git(self.root / agentrc.DEP, 'rev-parse', 'HEAD'), commit)
        self.assertEqual(self.check(ci='true')['commit'], commit)
        (self.root / agentrc.DEP / RTT_PY).write_text('edited\n')
        self.refused(self.bootstrap('fetch', ci='true'), 'is not a clean checkout', 'remove it and fetch again')

    # --- what the bootstrap refuses -------------------------------------------------------------
    def test_an_agentrc_of_another_api_is_refused_before_its_helper_runs(self):
        marker = self.tmp / 'ran'
        other = self.checkout('other', {agentrc.SOURCE: f'open({str(marker)!r}, "w").close()\nAPI = 2\n'})
        self.refused(self.bootstrap('check', agentrc_dir=other), 'speaks agentrc_source API 2, this checkout needs 1',
                     'AGENTRC_DIR')
        self.assertFalse(marker.exists())

    def test_an_agentrc_whose_api_is_no_literal_is_refused_without_a_traceback(self):
        for i, api in enumerate(("int('1')", '{[]: 1}')):
            with self.subTest(api):
                other = self.checkout(f'other{i}', {agentrc.SOURCE: f'API = {api}\n'})
                r = self.bootstrap('check', agentrc_dir=other)
                self.refused(r, 'no usable agentrc at', 'AGENTRC_DIR')
                self.assertNotIn('Traceback', r.stderr)

    def test_no_agentrc_at_all_names_the_install(self):
        self.refused(self.bootstrap('check', agentrc_dir=self.tmp / 'nowhere'), 'no usable agentrc at',
                     'install.py install')

    def test_a_file_outside_the_closure_of_scripts_is_refused(self):
        other = self.checkout('other', {'skills/extra/scripts/extra.py': 'X = 1\n'})
        r = self.run_py('agentrc.load("skills/extra/scripts/extra.py", "extra")', agentrc_dir=other)
        self.refused(r, 'skills/extra/scripts/extra.py is not in the agentrc closure this process took',
                     'install.py install')

    def test_a_closure_file_edited_after_the_identity_is_refused(self):
        r = self.run_py(f'agentrc.identity()\nopen({str(self.install / RTT_PY)!r}, "a").write("\\n")\n'
                        f'agentrc.load_file({RTT_PY!r})')
        self.refused(r, f'agentrc {RTT_PY} changed since this process loaded it', 'install.py install')

    def test_a_dependency_edited_after_the_identity_is_refused(self):
        # hil_flash loads hil_process by path itself, past the bootstrap: the whole closure is checked first
        dep = self.install / 'skills/hil/scripts/hil_process.py'
        r = self.run_py(f'agentrc.identity()\nopen({str(dep)!r}, "a").write("raise RuntimeError(\'DRIFT RAN\')\\n")\n'
                        "agentrc.load_file('skills/hil/scripts/hil_flash.py')")
        self.refused(r, 'agentrc skills/hil/scripts/hil_process.py changed since this process loaded it')
        self.assertNotIn('DRIFT RAN', r.stderr)

    def test_a_dependency_behind_a_retargeted_skill_link_never_runs(self):
        # an install of skill symlinks: hil_load's ROOT is the checkout they resolve into, so the esp_session that
        # hil_openocd_esp loads itself by path is the identity's, never one a link retargeted since now leads to
        a, b = self.checkout('a'), self.checkout('b', {ESP_SESSION: "raise RuntimeError('B RAN')\n"})
        install = self.tmp / 'linked'
        (install / 'skills').mkdir(parents=True)
        for skill in sorted({rel.split('/')[1] for rel in REAL['files'] if rel.startswith('skills/')}):
            (install / 'skills' / skill).symlink_to(a / 'skills' / skill, target_is_directory=True)
        retarget = install / 'skills/target-debug'
        r = self.run_py(f'print(agentrc.identity()["root"])\nimport os\nos.unlink({str(retarget)!r})\n'
                        f'os.symlink({str(b / "skills/target-debug")!r}, {str(retarget)!r})\n'
                        "m = agentrc.load_file('skills/hil/scripts/hil_openocd_esp.py')\n"
                        'print(m.esp_session.__file__)', agentrc_dir=install)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertNotIn('B RAN', r.stderr)
        self.assertEqual(r.stdout.split(), [str(a), str(a / ESP_SESSION)])

    def test_one_identity_per_process_whichever_bootstrap_asks(self):
        # hil_report run as a script loads agentrc.py again as a fresh module; both share agentrc_source's
        code = ('import importlib.util, json\n'
                'first = agentrc.identity()\n'
                f'open({str(self.install / RTT_PY)!r}, "a").write("\\n")\n'
                'spec = importlib.util.spec_from_file_location("hr", "test/hil/helper/hil_report.py")\n'
                'hr = importlib.util.module_from_spec(spec); spec.loader.exec_module(hr)\n'
                'hr.__package__ = None; other = hr._agentrc()\n'
                'print(other is not agentrc, other.identity() is first, other.helper() is agentrc.helper())\n')
        r = self.run_py(code)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stdout.split(), ['True', 'True', 'True'])

    # --- loading -----------------------------------------------------------------------------
    def test_a_closure_file_loads_and_registers(self):
        r = self.run_py(f'print(agentrc.load({RTT_PY!r}, "under_test").__file__)\n'
                        'print(sys.modules["under_test"].RttError.__module__)')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stdout, f'{self.install / RTT_PY}\nunder_test\n')

    def test_every_name_of_a_file_is_one_module(self):
        r = self.run_py(f'm = agentrc.load({RTT_PY!r}, "under_test")\n'
                        f'print(agentrc.load({RTT_PY!r}, "again") is m, agentrc.load_file({RTT_PY!r}) is m,'
                        ' sys.modules["agentrc_rtt"] is m)')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(r.stdout, 'True True True\n')

    def test_a_failed_exec_leaves_no_module_registered(self):
        broken = self.checkout('broken', {RTT_PY: 'raise RuntimeError("broken")\n'})
        r = self.run_py(f'try:\n    agentrc.load({RTT_PY!r}, "under_test")\n'
                        'except RuntimeError:\n    print("under_test" in sys.modules)\n', agentrc_dir=broken)
        self.assertEqual((r.returncode, r.stdout), (0, 'False\n'), r.stderr)


if __name__ == '__main__':
    unittest.main()
