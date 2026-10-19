"""hil_remote.py against a local "rig": a fake ssh runs each remote command under sh with a
clean environment and a temp HOME, a fake rsync strips the host prefix, and a fake python3
in that HOME's ~/.local/bin stands in for hil_test.py. So the quoting, the tilde expansion,
the PATH shim and every refusal-before-wipe execute for real. Windows cannot exec the #!
fakes, so there only the Refusals and Units run. Plumbing only: the real transport is proved
by a run on ci.lan."""
import contextlib
import importlib.util
import io
import json
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
import types
import unittest
from pathlib import Path
from unittest import mock

REPO = Path(__file__).resolve().parents[2]
SCRIPT = REPO / '.claude' / 'skills' / 'hil-contract' / 'scripts' / 'hil_remote.py'
spec = importlib.util.spec_from_file_location('hil_remote', SCRIPT)
hil_remote = importlib.util.module_from_spec(spec)
spec.loader.exec_module(hil_remote)

REAL_RSYNC = shutil.which('rsync')
needs_rsync = unittest.skipUnless(REAL_RSYNC, 'rsync not installed; the fake rsync execs the real one')
needs_flock = unittest.skipUnless(shutil.which('flock', path='/usr/bin:/bin'), 'flock not on the fake rig PATH (/usr/bin:/bin)')
# the fake ssh, rsync and python3 are #! scripts Windows cannot exec: a run there would reach the real ones
needs_fake_rig = unittest.skipIf(os.name == 'nt', 'the fake rig is #! scripts and a POSIX shell')

FAKE_SSH = f'''#!{sys.executable}
import json, os, subprocess, sys
args = sys.argv[1:]
while args[:1] == ['-o']:
    args = args[2:]
host, cmd = args[0], args[1]
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

# hil_test.py on the "rig": record argv, cwd, the HIL_* env it got and whether another run
# could take the REMOTE_DIR lock; write the report pair and a .failed spec when asked; exit
# with FAKE_RC.
FAKE_PYTHON = f'''#!{sys.executable}
import fcntl, json, os, sys
with open(os.getcwd() + '.lock', 'a') as lk:
    try:
        fcntl.flock(lk, fcntl.LOCK_EX | fcntl.LOCK_NB)
        lock_held = False
    except BlockingIOError:
        lock_held = True
    # the run session's own shared hold: fd 9 open on the lock file across the exec
    try:
        own_hold = os.fstat(9).st_ino == os.fstat(lk.fileno()).st_ino
    except OSError:
        own_hold = False
with open(os.environ['FAKE_RUN'], 'w') as f:
    json.dump({{'argv': sys.argv[1:], 'cwd': os.getcwd(), 'lock_held': lock_held, 'own_hold': own_hold,
               'env': {{k: v for k, v in os.environ.items() if k.startswith('HIL_')}}}}, f)
for name in os.environ.get('FAKE_WRITE', '').split():
    open(name, 'w').write('from the rig')
sys.exit(int(os.environ.get('FAKE_RC', '0')))
'''

CONFIG = {'boards': [
    {'name': 'alpha', 'flasher': {'name': 'jlink'}},
    {'name': 'beta', 'flasher': {'name': 'openocd'}, 'variant': [{'name': 'beta_one'}, {'name': 'beta_two'}]},
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
        if os.name == 'nt':
            # only the refusals run here: a REMOTE_DIR they accept, and no real ssh or rsync on PATH
            e.update(REMOTE_DIR='/tmp/hil-remote-test', PATH=str(self.bin))
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
        r = self.hil_remote('-b', 'alpha', '--build')
        self.refused(r, '--build')
        self.assertIn('check_build.py --board alpha --shared --variants test/hil/tinyusb.json', r.stderr)
        # without -b the line names the boards the run would select
        r = self.hil_remote('--build', '--exclude-flasher', 'openocd')
        self.refused(r, '--build')
        self.assertIn('check_build.py --board alpha --shared --variants', r.stderr)

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
        self.assertIn('--board alpha --board beta --shared --variants test/hil/tinyusb.json', r.stderr)

    def test_all_boards_with_nothing_built(self):
        r = self.hil_remote()
        self.refused(r, 'nothing to test')
        self.assertIn('--board alpha --board beta --shared --variants test/hil/tinyusb.json', r.stderr)

    def test_the_build_line_quotes_the_config(self):
        (self.root / 'test/hil/my rig.json').write_text(json.dumps(CONFIG))
        r = self.hil_remote('-b', 'alpha', '--build', CONFIG=str(self.root / 'test/hil/my rig.json'))
        self.refused(r, "--variants 'test/hil/my rig.json'")

    def test_a_flasher_filter_that_leaves_no_board(self):
        self.build('alpha')
        self.refused(self.hil_remote('-b', 'alpha', '--flasher', 'openocd'), 'no board left after the flasher filter')

    def test_a_board_the_flasher_filter_drops_satisfies_nothing(self):
        self.build('beta_one')
        self.refused(self.hil_remote('--exclude-flasher', 'openocd'), 'nothing to test')

    def test_the_preset_layout_is_not_read(self):
        self.build('alpha', root='examples')
        self.refused(self.hil_remote('-b', 'alpha'), 'no build under cmake-build/ for:')

    def test_the_build_line_says_where_it_writes_under_another_build_dir(self):
        r = self.hil_remote('-b', 'alpha', '-B', 'other')
        self.refused(r, 'no build under other/ for:')
        self.assertIn('--board alpha --shared --variants test/hil/tinyusb.json\n  (it writes under cmake-build/, not other/)', r.stderr)

    def test_bad_arguments_and_build_dirs(self):
        self.build('alpha')
        self.assertNotEqual(self.hil_remote('--nope').returncode, 0)
        self.assertEqual(self.calls(), [])
        for bad in ('/abs', '../up', 'a/../b', 'a b'):
            self.refused(self.hil_remote('-b', 'alpha', '-B', bad), '-B must be')


@needs_fake_rig
@needs_rsync
@needs_flock
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
                         sorted([*hil_remote.HARNESS_FILES, 'test/hil/tinyusb.json', '.hil-remote-run', *fw]))

    def lock_dependency(self, provided=True):
        """The real hil_lock adapter, its loader, get_deps.py and, when provided, agentrc's files at the pin."""
        rels = ['test/hil/helper/hil_lock.py', 'test/hil/helper/agentrc.py', 'tools/get_deps.py']
        rels += ['lib/agentrc/.git/HEAD', 'lib/agentrc/skills/hil/scripts/hil_lock.py',
                 'lib/agentrc/skills/hil/scripts/usb_observe.py',
                 'lib/agentrc/skills/rtt/scripts/rtt.py'] if provided else []
        for rel in rels:
            shutil.copy(REPO / rel, self.root / rel)
        if not provided:
            shutil.rmtree(self.root / 'lib/agentrc')

    def test_the_staged_tree_passes_the_pin_check(self):
        self.build('alpha')
        self.lock_dependency()
        r = self.hil_remote('-b', 'alpha')
        self.assertEqual(r.returncode, 0, r.stderr)
        probe = subprocess.run([sys.executable, '-c', 'import sys; sys.path.insert(0, "test/hil"); '
                                'from helper import agentrc, hil_lock; print(hil_lock.__file__); '
                                'print(agentrc.load("skills/rtt/scripts/rtt.py", "tinyusb_tools_rtt").__file__)'],
                               cwd=self.remote, capture_output=True, text=True, timeout=30)
        self.assertEqual(probe.returncode, 0, probe.stderr)
        self.assertEqual(probe.stdout.split(), [str(self.remote / 'lib/agentrc/skills/hil/scripts/hil_lock.py'),
                                                str(self.remote / 'lib/agentrc/skills/rtt/scripts/rtt.py')])

    def test_a_missing_lock_dependency_refuses_before_the_rig(self):
        self.build('alpha')
        self.lock_dependency(provided=False)
        r = self.hil_remote('-b', 'alpha')
        self.assertEqual(r.returncode, 1)
        spec = importlib.util.spec_from_file_location('get_deps_under_test', REPO / 'tools/get_deps.py')
        get_deps = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(get_deps)
        self.assertIn(get_deps.deps_optional['lib/agentrc'][1], r.stderr)
        self.assertIn('python3 tools/get_deps.py lib/agentrc', r.stderr)
        self.assertEqual(self.calls(), [])

    def test_a_missing_variant_warns(self):
        self.build('beta_one')
        r = self.hil_remote('-b', 'beta')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('no cmake-build/cmake-build-beta_two -- its cells will be skipped or fail the '
                      'same-PID boundary on a one-test run\n', r.stderr)
        self.assertNotIn('beta_one --', r.stderr)
        r = self.hil_remote('-b', 'beta', '--skip-flash')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('no cmake-build/cmake-build-beta_two -- its cells will be skipped\n', r.stderr)

    def test_a_missing_first_variant_has_no_boundary_to_fail(self):
        self.build('beta_two')
        r = self.hil_remote('-b', 'beta')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('no cmake-build/cmake-build-beta_one -- its cells will be skipped\n', r.stderr)

    def test_a_board_the_flasher_filter_drops_needs_no_build(self):
        self.build('alpha')
        r = self.hil_remote('-b', 'alpha', '-b', 'beta', '--exclude-flasher', 'openocd')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual({f.split('/')[1] for f in self.remote_files() if f.startswith('cmake-build/')},
                         {'cmake-build-alpha'})

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


@needs_fake_rig
@needs_rsync
@needs_flock
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
        r = subprocess.run(['bash', '-c', hil_remote.SETUP_SCRIPT, 'hil-setup', '~/', 'cmake-build'],
                           stdin=subprocess.DEVNULL, capture_output=True, text=True,
                           env={'HOME': str(self.home), 'PATH': '/usr/bin:/bin'})
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('refusing', r.stderr)
        self.assertTrue((self.home / '.local/bin/python3').exists())

    def test_every_ssh_carries_the_keepalive_options(self):
        self.build('alpha')
        self.assertEqual(self.hil_remote('-b', 'alpha').returncode, 0)
        opts = list(hil_remote.SSH_OPTS)
        ssh = [c['argv'] for c in self.calls() if c['tool'] == 'ssh']
        rsync = [c['argv'] for c in self.calls() if c['tool'] == 'rsync']
        self.assertEqual([a[:len(opts) + 1] for a in ssh], [[*opts, 'rig']] * 2)
        self.assertTrue(rsync)
        self.assertTrue(all(a[:2] == ['-e', shlex.join(['ssh', *opts])] for a in rsync), rsync)
        # every remote rsync server starts under the tree guard: shared lock plus this run's token
        token = (self.remote / '.hil-remote-run').read_text().strip()
        self.assertTrue(all(a[2].startswith('--rsync-path=sh -c ') and 'flock -s' in a[2] and token in a[2]
                            for a in rsync), rsync)


@needs_fake_rig
@needs_flock
class RemoteLock(Rig):
    """A second run sharing REMOTE_DIR must not rm -rf the tree the first one is running from."""

    def lock(self):
        import fcntl
        lk = open(f'{self.remote}.lock', 'a')
        self.addCleanup(lk.close)
        fcntl.flock(lk, fcntl.LOCK_EX | fcntl.LOCK_NB)

    def test_a_held_lock_refuses_before_the_wipe(self):
        self.build('alpha')
        (self.remote / 'running').mkdir(parents=True)
        (self.remote / 'running/x').write_text('the other run')
        self.lock()
        r = self.hil_remote('-b', 'alpha')
        self.assertNotEqual(r.returncode, 0)
        self.assertIn(f'another hil_remote run holds {self.remote}.lock', r.stderr)
        self.assertTrue((self.remote / 'running/x').is_file())
        self.assertEqual([c['tool'] for c in self.calls()], ['ssh'])

    def test_a_failing_flock_is_not_reported_as_a_held_lock(self):
        rig_bin = self.tmp / 'rig-bin'
        rig_bin.mkdir()
        fake(rig_bin / 'flock', '#!/bin/sh\necho "flock: 9: No locks available" >&2\nexit 71\n')
        fake(self.bin / 'ssh', FAKE_SSH.replace("'PATH': '/usr/bin:/bin'", f"'PATH': '{rig_bin}:/usr/bin:/bin'"))
        self.build('alpha')
        (self.remote / 'running').mkdir(parents=True)
        r = self.hil_remote('-b', 'alpha')
        self.assertNotEqual(r.returncode, 0)
        self.assertIn(f'flock on {self.remote}.lock failed (exit 71)', r.stderr)
        self.assertNotIn('another hil_remote run holds', r.stderr)
        self.assertTrue((self.remote / 'running').is_dir())

    @needs_rsync
    def test_the_lock_is_held_for_the_run_and_released_after(self):
        self.build('alpha')
        r = self.hil_remote('-b', 'alpha')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertTrue(self.ran()['lock_held'])
        self.assertTrue(self.ran()['own_hold'], 'hil_test.py must hold the lock itself, not only the lease')
        self.lock()

    @needs_rsync
    def test_a_failed_staging_releases_the_lock(self):
        fake(self.bin / 'rsync', FAKE_RSYNC.replace('args = [', "if 'test/hil/hil_test.py' in sys.argv:\n    sys.exit(1)\nargs = [", 1))
        # a lease that outlives its stdin by a second: the wrapper must wait it out, not exit holding the lock
        fake(self.bin / 'ssh', FAKE_SSH.replace("['sh', '-c', cmd]", "['sh', '-c', cmd.replace('cat >/dev/null', 'cat >/dev/null; sleep 1')]"))
        self.build('alpha')
        r = self.hil_remote('-b', 'alpha')
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('could not stage the harness', r.stderr)
        self.lock()

    @needs_rsync
    def test_the_lock_is_held_for_the_copy_back(self):
        seen = self.tmp / 'copy_back_lock'
        fake(self.bin / 'rsync', FAKE_RSYNC.replace('args = [', f'''if any(a.startswith('rig:') and a.endswith('/hil_report.md') for a in sys.argv):
    import fcntl
    with open({str(self.remote) + '.lock'!r}, 'a') as lk:
        try:
            fcntl.flock(lk, fcntl.LOCK_EX | fcntl.LOCK_NB)
            open({str(seen)!r}, 'w').write('free')
        except BlockingIOError:
            open({str(seen)!r}, 'w').write('held')
args = [''', 1))
        self.build('alpha')
        r = self.hil_remote('-b', 'alpha', FAKE_WRITE='hil_report.md hil_report.json')
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(seen.read_text(), 'held')

    @needs_rsync
    def test_a_noisy_rig_shell_does_not_hide_the_marker(self):
        # a login shell greeting on stdout with no trailing newline
        fake(self.bin / 'ssh', FAKE_SSH.replace("['sh', '-c', cmd]", "['sh', '-c', 'printf greeting; ' + cmd]"))
        self.build('alpha')
        r = self.hil_remote('-b', 'alpha')
        self.assertEqual(r.returncode, 0, r.stderr)

    @needs_rsync
    def test_a_chatty_setup_stderr_does_not_deadlock(self):
        fake(self.bin / 'ssh', FAKE_SSH.replace("['sh', '-c', cmd]",
                                                "['sh', '-c', 'head -c 300000 /dev/zero | tr \\\\0 x >&2; ' + cmd]"))
        self.build('alpha')
        r = self.hil_remote('-b', 'alpha')
        self.assertEqual(r.returncode, 0, r.stderr[-300:])

    @needs_rsync
    def test_a_tree_replaced_after_staging_is_refused(self):
        # the lease died and another run wiped and rebuilt the tree before this run session
        # took its hold: its token is gone, so it must not run in the replacement
        fake(self.bin / 'ssh', FAKE_SSH.replace(
            "['sh', '-c', cmd]",
            "['sh', '-c', ('rm -rf \"$0\" && mkdir -p \"$0/test/hil\"; ' if 'hil_test.py' in cmd else '') + cmd, "
            f"{str(self.remote)!r}]"))
        self.build('alpha')
        r = self.hil_remote('-b', 'alpha')
        self.assertEqual(r.returncode, hil_remote.TREE_REPLACED, r.stderr)
        self.assertIn("not this run's tree any more", r.stderr)
        self.assertFalse(self.run_file.exists(), 'hil_test.py ran in a tree that is not this run\'s')
        # and nothing was fetched from the replacement tree
        self.assertEqual([c['tool'] for c in self.calls()][-1], 'ssh', 'copy_back ran after the refusal')


@needs_fake_rig
@needs_rsync
@needs_flock
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
            p.write_text(json.dumps({'rows': [{'board': 'alpha', 'note': '\u2714'}]}, ensure_ascii=False), encoding='utf-8')
            self.assertTrue(hil_remote.sidecar_matches(p, CONFIG), 'the rig writes it as UTF-8')

    def test_staged_files_are_named_as_rsync_and_the_rig_name_them(self):
        with tempfile.TemporaryDirectory() as td, mock.patch.object(hil_remote, 'ROOT', Path(td)):
            d = Path(td) / 'cmake-build' / 'cmake-build-alpha'
            (d / 'device' / 'cdc_msc').mkdir(parents=True)
            for f in ('cdc_msc.elf', 'cdc_msc.map'):
                (d / 'device' / 'cdc_msc' / f).write_text(f)
            files = hil_remote.staged_files([d])
            self.assertEqual(list(files), ['cmake-build/cmake-build-alpha/device/cdc_msc/cdc_msc.elf'])
            self.assertEqual(hil_remote.example_firmware(d, files), {'device/cdc_msc'})

    def test_no_rsync_is_refused_before_the_lease(self):
        def popen(*a, **k):
            raise AssertionError('ssh started')
        cases = [('none', None, None)]
        if os.name == 'nt':   # subprocess cannot start a .cmd by bare name, and the cwd is not PATH
            cases += [('a .cmd on PATH', 'path', 'rsync.cmd'), ('an .exe in the cwd', 'cwd', 'rsync.exe')]
        for case, where, name in cases:
            with self.subTest(case), tempfile.TemporaryDirectory() as path, tempfile.TemporaryDirectory() as cwd:
                if name:
                    (Path(path if where == 'path' else cwd) / name).write_bytes(b'')
                old = os.getcwd()
                os.chdir(cwd)
                try:
                    with mock.patch.dict(os.environ, {'PATH': os.pathsep.join(['', path])}), \
                            mock.patch.object(hil_remote.subprocess, 'Popen', popen), \
                            contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as e:
                        with hil_remote.remote_lease('rig', '/tmp/x', 'cmake-build'):
                            pass
                finally:
                    os.chdir(old)
                self.assertIn('rsync is not on PATH', str(e.exception.code))

    def test_windows_takes_rsync_exe_only_from_an_absolute_path_dir(self):
        # the os module as Windows' rsync_program sees it, on any host
        with tempfile.TemporaryDirectory() as path, tempfile.TemporaryDirectory() as cwd:
            for d in (path, cwd):
                (Path(d) / 'rsync.exe').write_bytes(b'')
            old = os.getcwd()
            os.chdir(cwd)
            try:
                for entries, found in ((['.', 'bin', ''], None), (['.', path], os.path.join(path, 'rsync.exe'))):
                    nt = types.SimpleNamespace(name='nt', path=os.path, pathsep=os.pathsep,
                                               environ={'PATH': os.pathsep.join(entries)})
                    with self.subTest(entries), mock.patch.object(hil_remote, 'os', nt):
                        self.assertEqual(hil_remote.rsync_program(), found)
            finally:
                os.chdir(old)

    @unittest.skipUnless(os.name == 'nt', 'rsync.exe is the Windows name')
    def test_windows_runs_the_rsync_exe_found_on_path(self):
        calls = []
        with tempfile.TemporaryDirectory() as path:
            (Path(path) / 'rsync.exe').write_bytes(b'')
            with mock.patch.dict(os.environ, {'PATH': path}), \
                    mock.patch.object(hil_remote.subprocess, 'run', lambda argv, **kw: calls.append(argv) or mock.Mock(returncode=0)):
                hil_remote.rsync(('/tmp/t', 'tok'), '-q', 'rig:/tmp/t/x', hil_remote.ROOT / 'x')
        self.assertEqual(calls[0][0], os.path.join(path, 'rsync.exe'))

    def test_rsync_names_local_files_from_the_checkout_and_passes_remote_specs_as_they_are(self):
        # a one-letter ssh host must not be read as a drive letter, the checkout's own or another
        calls = []
        local = hil_remote.ROOT / 'hil_report.json'
        hosts = ('rig', hil_remote.ROOT.drive[:1].lower() or 'e', 'r')
        with mock.patch.object(hil_remote, 'rsync_program', return_value='rsync'), \
                mock.patch.object(hil_remote.subprocess, 'run', lambda argv, **kw: calls.append((argv, kw)) or mock.Mock(returncode=0)):
            for host in hosts:
                hil_remote.rsync(('/tmp/t', 'tok'), '-q', local, f'{host}:/tmp/t/')
                hil_remote.rsync(('/tmp/t', 'tok'), '-q', f'{host}:/tmp/t/hil_report.json', local)
        name = 'hil_report.json' if os.name == 'nt' else str(local)   # Windows rsync reads C: as a host
        self.assertEqual([argv[-2:] for argv, _ in calls],
                         [x for h in hosts for x in ([name, f'{h}:/tmp/t/'], [f'{h}:/tmp/t/hil_report.json', name])])
        self.assertEqual({kw['cwd'] for _, kw in calls}, {hil_remote.ROOT})

    @unittest.skipUnless(os.name == 'nt', 'drive letters are Windows paths')
    def test_a_config_rsync_cannot_name_is_refused_before_the_lease(self):
        leased = []

        @contextlib.contextmanager
        def lease(*a):
            leased.append(a)
            yield '/tmp/t', 'tok'
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            other = 'Q:' if root.drive.upper() != 'Q:' else 'R:'
            args = types.SimpleNamespace(build=False, build_dir='cmake-build', accumulate=False)
            with mock.patch.object(hil_remote, 'ROOT', root), mock.patch.object(hil_remote, 'helper'), \
                    mock.patch.object(hil_remote, 'parse_run', return_value=(args, CONFIG, Path(f'{other}/rig/tinyusb.json'))), \
                    mock.patch.object(hil_remote, 'resolve_firmware', return_value=[root / 'cmake-build' / 'cmake-build-alpha']), \
                    mock.patch.object(hil_remote, 'remote_lease', lease), \
                    mock.patch.object(hil_remote.subprocess, 'run', side_effect=AssertionError('rsync or ssh ran')), \
                    contextlib.redirect_stdout(io.StringIO()), self.assertRaises(SystemExit) as e:
                hil_remote.run([])
        self.assertIn('is not on the drive of the checkout', str(e.exception.code))
        self.assertEqual(leased, [], 'refused after the lease wiped the rig tree')

    def test_the_pin_check_passes_where_the_lock_primitive_cannot_import_fcntl(self):
        # agentrc.load() checks the pin before running agentrc's hil_lock, whose fcntl Windows lacks
        args = types.SimpleNamespace(build=True, board=['alpha'], flasher=None, exclude_flasher=None,
                                     build_dir='cmake-build')
        for error, refusal in ((ModuleNotFoundError("No module named 'fcntl'", name='fcntl'), '--build would build'),
                               (ImportError('lib/agentrc must be at the pin'), 'lib/agentrc must be at the pin')):
            with self.subTest(error), mock.patch.object(hil_remote, 'helper', side_effect=error), \
                    mock.patch.object(hil_remote, 'parse_run', return_value=(args, CONFIG, hil_remote.ROOT / 'x.json')), \
                    self.assertRaises(SystemExit) as e:
                hil_remote.run([])
            self.assertIn(refusal, str(e.exception.code))


if __name__ == '__main__':
    unittest.main()
