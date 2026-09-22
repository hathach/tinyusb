"""hil_remote.py against a local "rig": a fake ssh runs each remote command under sh with a
clean environment and a temp HOME, a fake rsync strips the host prefix, and a fake python3
in that HOME's ~/.local/bin stands in for hil_test.py. So the quoting, the tilde expansion,
the PATH shim and every refusal-before-wipe execute for real. Plumbing only: the real
transport is proved by a run on ci.lan."""
import importlib.util
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SCRIPT = REPO / '.claude' / 'skills' / 'hil' / 'scripts' / 'hil_remote.py'
spec = importlib.util.spec_from_file_location('hil_remote', SCRIPT)
hil_remote = importlib.util.module_from_spec(spec)
spec.loader.exec_module(hil_remote)

REAL_RSYNC = shutil.which('rsync')
needs_rsync = unittest.skipUnless(REAL_RSYNC, 'rsync not installed; the fake rsync execs the real one')

FAKE_SSH = f'''#!{sys.executable}
import json, os, subprocess, sys
host, cmd = sys.argv[1], sys.argv[2]
with open(os.environ['FAKE_LOG'], 'a') as f:
    f.write(json.dumps({{'tool': 'ssh', 'argv': sys.argv[1:]}}) + '\\n')
env = {{'HOME': os.environ['FAKE_HOME'], 'PATH': '/usr/bin:/bin'}}
env.update({{k: v for k, v in os.environ.items() if k.startswith('FAKE_')}})
sys.exit(subprocess.run(['sh', '-c', cmd], env=env, stdin=sys.stdin).returncode)
'''

FAKE_RSYNC = f'''#!{sys.executable}
import json, os, sys
with open(os.environ['FAKE_LOG'], 'a') as f:
    f.write(json.dumps({{'tool': 'rsync', 'argv': sys.argv[1:]}}) + '\\n')
args = [a.split(':', 1)[1] if a.startswith('rig:') else a for a in sys.argv[1:]]
os.execv({REAL_RSYNC!r}, [{REAL_RSYNC!r}, *args])
'''

# hil_test.py on the "rig": record argv, cwd and the HIL_* env it got; write the report pair
# and a .failed spec when asked; exit with FAKE_RC.
FAKE_PYTHON = f'''#!{sys.executable}
import json, os, sys
with open(os.environ['FAKE_RUN'], 'w') as f:
    json.dump({{'argv': sys.argv[1:], 'cwd': os.getcwd(),
               'env': {{k: v for k, v in os.environ.items() if k.startswith('HIL_')}}}}, f)
for name in os.environ.get('FAKE_WRITE', '').split():
    open(name, 'w').write('from the rig')
sys.exit(int(os.environ.get('FAKE_RC', '0')))
'''

CONFIG = {'boards': [
    {'name': 'alpha'},
    {'name': 'beta', 'variant': [{'name': 'beta_one'}, {'name': 'beta_two'}]},
]}


def fake(path, body):
    path.write_text(body)
    path.chmod(0o755)


class Rig(unittest.TestCase):
    def setUp(self):
        td = tempfile.TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.tmp = Path(td.name)
        self.root = self.tmp / 'checkout'
        for rel in hil_remote.HARNESS_FILES:
            (self.root / rel).parent.mkdir(parents=True, exist_ok=True)
            src = REPO / rel
            shutil.copy(src, self.root / rel) if rel.endswith(('hil_args.py', 'hil_report.py')) else (self.root / rel).write_text('#')
        (self.root / 'test/hil/tinyusb.json').write_text(json.dumps(CONFIG))
        self.home = self.tmp / 'home'
        (self.home / '.local/bin').mkdir(parents=True)
        fake(self.home / '.local/bin/python3', FAKE_PYTHON)
        self.bin = self.tmp / 'bin'
        self.bin.mkdir()
        fake(self.bin / 'ssh', FAKE_SSH)
        fake(self.bin / 'rsync', FAKE_RSYNC)
        self.remote = self.tmp / 'remote'
        self.log = self.tmp / 'log.jsonl'
        self.run_file = self.tmp / 'run.json'

    def build(self, *variants, root='cmake-build'):
        for v in variants:
            d = self.root / root / f'cmake-build-{v}' / 'device/cdc_msc'
            d.mkdir(parents=True)
            for f in ('cdc_msc.elf', 'cdc_msc.bin', 'cdc_msc.map', 'flash_args'):
                (d / f).write_text(f)

    def env(self):
        e = {k: v for k, v in os.environ.items() if not k.startswith('HIL_')}
        e.update(ROOT_DIR=str(self.root), REMOTE='rig', REMOTE_DIR=str(self.remote),
                 PATH=f'{self.bin}:{os.environ["PATH"]}', FAKE_LOG=str(self.log),
                 FAKE_HOME=str(self.home), FAKE_RUN=str(self.run_file))
        e.pop('CONFIG', None)
        return e

    def hil_remote(self, *argv, **env):
        e = {**self.env(), **env}
        return subprocess.run([sys.executable, str(SCRIPT), *argv], env=e, capture_output=True,
                              text=True, timeout=60)

    def calls(self):
        if not self.log.exists():
            return []
        return [json.loads(ln) for ln in self.log.read_text().splitlines()]

    def ran(self):
        return json.loads(self.run_file.read_text())

    def remote_files(self):
        return sorted(str(p.relative_to(self.remote)) for p in self.remote.rglob('*') if p.is_file())


class Refusals(Rig):
    """Every refusal happens before any ssh or rsync: a wipe followed by an abort leaves the
    rig without the previous run's report and re-run spec."""

    def refused(self, r, text):
        self.assertNotEqual(r.returncode, 0)
        self.assertIn(text, r.stderr)
        self.assertEqual(self.calls(), [])

    def test_unsafe_remote_dirs(self):
        self.build('alpha')
        for bad in ('/', '~/', '~', 'rel/dir', '/tmp/x/', '/tmp/../etc', '/tmp//x', '/tmp/$(id)',
                    '/tmp/a b', '/tmp/x;rm'):
            self.refused(self.hil_remote('-b', 'alpha', REMOTE_DIR=bad), 'REMOTE_DIR')

    def test_build_on_the_rig(self):
        self.build('alpha')
        self.refused(self.hil_remote('-b', 'alpha', '--build'), '--build')

    def test_unknown_board(self):
        self.build('alpha')
        self.refused(self.hil_remote('-b', 'alpha', '-b', 'beta_one'), 'not in the config: beta_one')

    def test_glued_bt_is_a_board_the_roster_refuses(self):
        self.build('alpha')
        self.refused(self.hil_remote('-btalpha:x'), 'not in the config: talpha:x')

    def test_every_unbuilt_board_at_once(self):
        r = self.hil_remote('-b', 'alpha', '-b', 'beta')
        self.refused(r, 'no build under cmake-build/ for:')
        self.assertIn('alpha: none of cmake-build/cmake-build-alpha\n', r.stderr)
        self.assertIn('beta: none of cmake-build/cmake-build-beta_one, cmake-build/cmake-build-beta_two', r.stderr)

    def test_all_boards_with_nothing_built(self):
        self.refused(self.hil_remote(), 'nothing to test')

    def test_the_preset_layout_is_not_read(self):
        self.build('alpha', root='examples')
        self.refused(self.hil_remote('-b', 'alpha'), 'no build under cmake-build/ for:')

    def test_bad_arguments_and_build_dirs(self):
        self.build('alpha')
        self.assertNotEqual(self.hil_remote('--nope').returncode, 0)
        self.assertEqual(self.calls(), [])
        for bad in ('/abs', '../up', 'a/../b', 'a b'):
            self.refused(self.hil_remote('-b', 'alpha', '-B', bad), '-B must be')


@needs_rsync
class Staging(Rig):
    def test_board_spellings_stage_their_firmware(self):
        self.build('alpha', 'beta_one', 'beta_two')
        for argv in (['-b', 'alpha'], ['--board', 'alpha'], ['--board=alpha'], ['-balpha']):
            r = self.hil_remote(*argv)
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertIn('cmake-build/cmake-build-alpha/device/cdc_msc/cdc_msc.elf', self.remote_files())
            self.assertNotIn('cmake-build/cmake-build-beta_one/device/cdc_msc/cdc_msc.elf', self.remote_files())

    def test_every_requested_board_is_staged_and_forwarded(self):
        self.build('alpha', 'beta_one')
        r = self.hil_remote('-b', 'alpha', '-b', 'beta')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual({f.split('/')[1] for f in self.remote_files() if f.startswith('cmake-build/')},
                         {'cmake-build-alpha', 'cmake-build-beta_one'})
        self.assertEqual(self.ran()['argv'][4:8], ['-b', 'alpha', '-b', 'beta'])

    def test_what_reaches_the_rig(self):
        self.build('beta_one', 'beta_two')
        r = self.hil_remote('-b', 'beta')
        self.assertEqual(r.returncode, 0, r.stderr)
        fw = [f'cmake-build/cmake-build-beta_{v}/device/cdc_msc/cdc_msc.{x}'
              for v in ('one', 'two') for x in ('bin', 'elf')] + \
             [f'cmake-build/cmake-build-beta_{v}/device/cdc_msc/flash_args' for v in ('one', 'two')]
        self.assertEqual(self.remote_files(),
                         sorted([*hil_remote.HARNESS_FILES, 'test/hil/tinyusb.json', *fw]))

    def test_a_missing_variant_warns(self):
        self.build('beta_one')
        r = self.hil_remote('-b', 'beta')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('no cmake-build/cmake-build-beta_two -- its cells will be skipped', r.stderr)
        self.assertNotIn('beta_one --', r.stderr)

    def test_all_boards_stage_whatever_is_built(self):
        self.build('alpha', 'beta_two')
        r = self.hil_remote()
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual({f.split('/')[1] for f in self.remote_files() if f.startswith('cmake-build/')},
                         {'cmake-build-alpha', 'cmake-build-beta_two'})

    def test_a_named_build_dir_is_staged_and_forwarded(self):
        self.build('alpha', root='out/hil')
        r = self.hil_remote('-b', 'alpha', '-B', 'out/hil')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('out/hil/cmake-build-alpha/device/cdc_msc/cdc_msc.elf', self.remote_files())
        self.assertEqual(self.ran()['argv'][-3:], ['-B', 'out/hil', 'test/hil/tinyusb.json'])

    def test_the_previous_tree_is_wiped(self):
        self.build('alpha')
        (self.remote / 'stale').mkdir(parents=True)
        (self.remote / 'stale/x').write_text('old')
        self.assertEqual(self.hil_remote('-b', 'alpha').returncode, 0)
        self.assertFalse((self.remote / 'stale').exists())


@needs_rsync
class TheRun(Rig):
    def test_args_and_env_survive_transport(self):
        self.build('alpha')
        r = self.hil_remote('-b', 'alpha', '-t', "host/cdc msc", '-r', '3',
                            HIL_POOL_TIMEOUT='60', HIL_ODD="a b'c $HOME", HIL_REPORT_DIR='/nope')
        self.assertEqual(r.returncode, 0, r.stderr)
        run = self.ran()
        self.assertEqual(run['argv'], ['-u', 'test/hil/hil_test.py', '--retry', '1', '-b', 'alpha',
                                       '-t', 'host/cdc msc', '-r', '3', 'test/hil/tinyusb.json'])
        self.assertEqual(run['env'], {'HIL_POOL_TIMEOUT': '60', 'HIL_ODD': "a b'c $HOME"})
        self.assertEqual(Path(run['cwd']), self.remote)

    def test_the_exit_code_passes_through(self):
        self.build('alpha')
        self.assertEqual(self.hil_remote('-b', 'alpha', FAKE_RC='3').returncode, 3)

    def test_a_tilde_remote_dir_lands_in_home(self):
        self.build('alpha')
        r = self.hil_remote('-b', 'alpha', REMOTE_DIR='~/hil-run')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(Path(self.ran()['cwd']), self.home / 'hil-run')
        self.assertTrue((self.home / 'hil-run/test/hil/hil_test.py').is_file())
        self.assertFalse(any(p.name == '~' for p in self.tmp.rglob('~')))
        rsync_dests = [c['argv'][-1] for c in self.calls() if c['tool'] == 'rsync']
        self.assertTrue(all(d.startswith(f'rig:{self.home}/hil-run') or not d.startswith('rig:')
                            for d in rsync_dests), rsync_dests)

    def test_the_remote_gate_refuses_home(self):
        r = subprocess.run(['bash', '-s', '--', '~/', 'cmake-build'], input=hil_remote.SETUP_SCRIPT,
                           capture_output=True, text=True, env={'HOME': str(self.home), 'PATH': '/usr/bin:/bin'})
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('refusing', r.stderr)
        self.assertTrue((self.home / '.local/bin/python3').exists())


@needs_rsync
class Reports(Rig):
    def setUp(self):
        super().setUp()
        self.build('alpha')

    def local(self, name):
        return self.root / name

    def test_the_pair_comes_home(self):
        r = self.hil_remote('-b', 'alpha', FAKE_WRITE='hil_report.md hil_report.json tinyusb.json.failed')
        self.assertEqual(r.returncode, 0, r.stderr)
        for n in ('hil_report.md', 'hil_report.json', 'tinyusb.json.failed'):
            self.assertEqual(self.local(n).read_text(), 'from the rig', n)

    def test_a_green_run_without_a_spec_is_quiet(self):
        r = self.hil_remote('-b', 'alpha', FAKE_WRITE='hil_report.md hil_report.json')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertNotIn('rsync', r.stderr)
        self.assertTrue(self.local('hil_report.json').is_file())

    def test_a_relative_root_dir(self):
        r = subprocess.run([sys.executable, str(SCRIPT), '-b', 'alpha'], cwd=self.tmp,
                           env={**self.env(), 'ROOT_DIR': 'checkout'}, capture_output=True, text=True,
                           timeout=60)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('cmake-build/cmake-build-alpha/device/cdc_msc/cdc_msc.elf', self.remote_files())

    def test_half_a_pair_leaves_none(self):
        for n in ('hil_report.md', 'hil_report.json', 'tinyusb.json.failed'):
            self.local(n).write_text('stale')
        r = self.hil_remote('-b', 'alpha', FAKE_WRITE='hil_report.md')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('copy-back incomplete', r.stderr)
        for n in ('hil_report.md', 'hil_report.json', 'hil_report.md.tmp', 'tinyusb.json.failed'):
            self.assertFalse(self.local(n).exists(), n)

    def test_accumulate_uploads_a_matching_sidecar(self):
        for spelling in ('--accumulate', '-a', '-av', '-va', '--accum', '--acc'):
            self.local('hil_report.json').write_text(json.dumps({'rows': [{'board': 'beta_two'}]}))
            r = self.hil_remote('-b', 'alpha', spelling)
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertIn('Uploading hil_report.json', r.stdout, spelling)
            self.assertTrue((self.remote / 'hil_report.json').is_file(), spelling)

    def test_accumulate_without_a_usable_sidecar_warns(self):
        r = self.hil_remote('-b', 'alpha', '-a')
        self.assertIn('no local hil_report.json', r.stderr)
        self.local('hil_report.json').write_text(json.dumps({'rows': [{'board': 'elsewhere'}]}))
        r = self.hil_remote('-b', 'alpha', '-a')
        self.assertIn('not uploading it', r.stderr)
        self.assertFalse((self.remote / 'hil_report.json').exists())

    def test_no_upload_without_accumulate(self):
        self.local('hil_report.json').write_text(json.dumps({'rows': [{'board': 'alpha'}]}))
        self.hil_remote('-b', 'alpha', '-v')
        self.assertFalse((self.remote / 'hil_report.json').exists())


class Units(unittest.TestCase):
    def test_sidecar_provenance(self):
        with tempfile.TemporaryDirectory() as td:
            p = Path(td) / 's.json'
            for body, ok in (({'rows': []}, True), ({'rows': [{'board': 'beta_one'}]}, True),
                             ({'rows': [{'board': 'zeta'}]}, False), ([], False)):
                p.write_text(json.dumps(body))
                self.assertEqual(hil_remote.sidecar_matches(p, CONFIG), ok, body)
            p.write_text('{')
            self.assertFalse(hil_remote.sidecar_matches(p, CONFIG))


if __name__ == '__main__':
    unittest.main()
