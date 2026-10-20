"""hil_remote.py, the adapter over agentrc's hil_remote_core, against a local "rig": a fake
ssh runs each remote command under sh with a clean environment and a temp HOME, rsync (behind a
logging shim) reaches its server through that ssh and the tree guard, the core's fetch runs the
real python3 there, and a fake python3 in that HOME's ~/.local/bin stands in for hil_test.py.
The transport itself -- the lease and its lock, the guards, the wipe, the fetch, the run
records, Windows rsync naming -- is agentrc's, tested with the core
(lib/agentrc/tests/test_hil_remote_core.py); these are the adapter's board selection, staging
list, run line, report copy-back and accumulate upload. Windows cannot exec the #! fakes, so
there only the Refusals and Units run. Plumbing only: the real transport is proved by a run on
ci.lan."""
import contextlib
import importlib.util
import io
import json
import os
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
report = hil_remote.helper('hil_report')   # this checkout's, as the fake checkouts' copy of it
RCORE = report.core()

REAL_RSYNC = shutil.which('rsync')
RIG_PATH = '/usr/bin:/bin'
needs_rsync = unittest.skipUnless(REAL_RSYNC, 'rsync not installed; the fake rsync execs the real one')
needs_flock = unittest.skipUnless(shutil.which('flock', path=RIG_PATH), f'flock not on the fake rig PATH ({RIG_PATH})')
needs_rig_python = unittest.skipUnless(shutil.which('python3', path=RIG_PATH), f'no python3 on the fake rig PATH ({RIG_PATH})')
# the fake ssh, rsync and python3 are #! scripts Windows cannot exec: a run there would reach the real ones
needs_fake_rig = unittest.skipIf(os.name == 'nt', 'the fake rig is #! scripts and a POSIX shell')

FAKE_SSH = f'''#!{sys.executable}
import json, os, subprocess, sys
args = sys.argv[1:]
while args[:1] == ['-o']:
    args = args[2:]
# like ssh, the words after the host are one remote command line: rsync passes its server's as several
host, cmd = args[0], ' '.join(args[1:])
with open(os.environ['FAKE_LOG'], 'a') as f:
    f.write(json.dumps({{'tool': 'ssh', 'argv': sys.argv[1:]}}) + '\\n')
env = {{'HOME': os.environ['FAKE_HOME'], 'PATH': {RIG_PATH!r}}}
env.update({{k: v for k, v in os.environ.items() if k.startswith('FAKE_')}})
sys.exit(subprocess.run(['sh', '-c', cmd], env=env, stdin=sys.stdin).returncode)
'''

FAKE_RSYNC = f'''#!{sys.executable}
import json, os, sys
with open(os.environ['FAKE_LOG'], 'a') as f:
    f.write(json.dumps({{'tool': 'rsync', 'argv': sys.argv[1:]}}) + '\\n')
os.execv({REAL_RSYNC!r}, [{REAL_RSYNC!r}, *sys.argv[1:]])
'''

# hil_test.py on the "rig": record argv, cwd and the HIL_* env it got and what it found in its
# tree; write FAKE_WRITE's names, and FAKE_FILES' {path: text}, where the harness writes its
# report, else FAKE_DEFAULT's, a report pair, `rig-ATTEMPT` in it a new attempt id; exit with FAKE_RC.
FAKE_PYTHON = f'''#!{sys.executable}
import json, os, sys, time
with open(os.environ['FAKE_RUN'], 'w') as f:
    json.dump({{'argv': sys.argv[1:], 'cwd': os.getcwd(), 'files': sorted(os.listdir('.')),
               'env': {{k: v for k, v in os.environ.items() if k.startswith('HIL_')}}}}, f)
files = {{n: 'from the rig' for n in os.environ.get('FAKE_WRITE', '').split()}}
files.update(json.loads(os.environ.get('FAKE_FILES', '{{}}')))
if not {{'FAKE_WRITE', 'FAKE_FILES'}} & set(os.environ):
    files = {{n: t.replace('rig-ATTEMPT', f'rig-{{time.time_ns()}}') for n, t in json.loads(os.environ['FAKE_DEFAULT']).items()}}
for name, text in files.items():
    os.makedirs(os.path.dirname(name) or '.', exist_ok=True)
    with open(name, 'w', encoding='utf-8') as f:
        f.write(text)
sys.exit(int(os.environ.get('FAKE_RC', '0')))
'''

# what a fake checkout needs real: the harness's argument parser, and the report adapter with
# the loader, the pin and the core it loads
REAL = ('test/hil/helper/hil_args.py', 'test/hil/helper/hil_report.py', 'test/hil/helper/agentrc.py',
        'tools/get_deps.py', 'lib/agentrc/.git/HEAD', 'lib/agentrc/skills/hil/scripts/hil_report_core.py')

CONFIG = {'boards': [
    {'name': 'alpha', 'flasher': {'name': 'jlink'}},
    {'name': 'beta', 'flasher': {'name': 'openocd'}, 'variant': [{'name': 'beta_one'}, {'name': 'beta_two'}]},
]}


def published(rows, attempt_id, previous=None):
    """The pair hil_test.py writes on the rig, published through the core: {path: text}."""
    with tempfile.TemporaryDirectory() as td:
        d = Path(td)
        if previous:
            (d / report.REPORT_ATTEMPTS / previous).mkdir(parents=True)
        attempt = RCORE.Attempt(attempt_id, previous, None, None, None)
        meta = RCORE.make_meta(report.schema(), attempt, termination='completed', invocation=['hil_test.py'],
                               selection=['alpha'], host='rig', roster={})
        RCORE.publish(d, report.schema(), {'rows': rows, 'scope': '', 'caveat': ''}, meta)
        return {n: (d / n).read_text(encoding='utf-8') for n in (report.REPORT_JSON, report.REPORT_MD)}


DEFAULT_REPORT = json.dumps(published([], 'rig-ATTEMPT'))


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
            shutil.copy(src, self.root / rel) if rel in REAL else (self.root / rel).write_text('#')
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
                 FAKE_HOME=str(self.home), FAKE_RUN=str(self.run_file),
                 FAKE_DEFAULT=DEFAULT_REPORT)
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

    def test_a_config_whose_re_run_spec_cannot_be_fetched(self):
        self.build('alpha')
        (self.root / 'test/hil/my rig.json').write_text(json.dumps(CONFIG))
        self.refused(self.hil_remote('-b', 'alpha', CONFIG=str(self.root / 'test/hil/my rig.json')),
                     'the re-run spec the copy-back fetches')

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
                         sorted([*hil_remote.HARNESS_FILES, 'test/hil/tinyusb.json', '.hil-remote-run', *fw,
                                 report.REPORT_JSON, report.REPORT_MD]))   # the last two the fake run's report

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


@needs_fake_rig
@needs_rsync
@needs_flock
@needs_rig_python
class Reports(Rig):
    ROWS = [{'board': 'alpha', 'cells': {'device/cdc_msc': 'pass'}, 'duration': '1s'}]

    def setUp(self):
        super().setUp()
        self.build('alpha')

    def local(self, name):
        return self.root / name

    def rig_report(self, rows=ROWS, attempt_id='rig-1', previous=None):
        return published(rows, attempt_id, previous)

    def test_the_pair_comes_home(self):
        files = {**self.rig_report(), 'tinyusb.json.failed': '--accumulate -b alpha'}
        r = self.hil_remote('-b', 'alpha', FAKE_FILES=json.dumps(files))
        self.assertEqual(r.returncode, 0, r.stderr)
        for n, text in files.items():
            self.assertEqual(self.local(n).read_text(encoding='utf-8'), text, n)

    def test_a_stale_markdown_is_rendered_again_from_the_sidecar(self):
        files = self.rig_report()
        good = files[report.REPORT_MD]
        files[report.REPORT_MD] = 'a table of an earlier run'
        r = self.hil_remote('-b', 'alpha', FAKE_FILES=json.dumps(files))
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(self.local(report.REPORT_MD).read_text(encoding='utf-8'), good)

    def test_an_unreadable_sidecar_comes_home_as_a_notice(self):
        r = self.hil_remote('-b', 'alpha', FAKE_WRITE='hil_report.md hil_report.json')
        self.assertEqual(r.returncode, 1, 'a green rig exit without a readable report is not a pass')
        self.assertEqual(self.local('hil_report.json').read_text(), 'from the rig')
        md = self.local('hil_report.md').read_text(encoding='utf-8')
        self.assertTrue(md.startswith(RCORE.NOTICE), md)
        self.assertIn('from the rig', md, 'the rig\'s markdown is kept below the notice, labelled')
        self.assertIn('unavailable notice', r.stderr)

    def test_a_green_run_without_a_spec_is_quiet(self):
        files = self.rig_report()
        r = self.hil_remote('-b', 'alpha', FAKE_FILES=json.dumps(files))
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertNotIn('warning', r.stderr)
        self.assertTrue(self.local('hil_report.json').is_file())

    def test_a_relative_root_dir(self):
        r = subprocess.run([sys.executable, str(SCRIPT), '-b', 'alpha'], cwd=self.tmp,
                           env={**self.env(), 'ROOT_DIR': 'checkout'}, capture_output=True, text=True,
                           timeout=60)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn('cmake-build/cmake-build-alpha/device/cdc_msc/cdc_msc.elf', self.remote_files())

    def test_half_a_pair_withdraws_the_local_one_and_archives_it(self):
        for n in ('hil_report.md', 'hil_report.json', 'tinyusb.json.failed'):
            self.local(n).write_text('stale')
        r = self.hil_remote('-b', 'alpha', FAKE_WRITE='hil_report.md')
        self.assertEqual(r.returncode, 1, 'no report came home')
        self.assertIn('the rig report did not all arrive: hil_report.md ok, hil_report.json absent', r.stderr)
        self.assertFalse(self.local('tinyusb.json.failed').exists())
        self.assertEqual(RCORE.read_document(self.local('hil_report.json'), report.schema()).outcome, 'withdrawn')
        self.assertTrue(self.local('hil_report.md').read_text(encoding='utf-8').startswith(RCORE.NOTICE))
        records = RCORE.inventory(self.root, report.schema())['attempts']
        self.assertEqual([a['attempt_id'][:8] for a in records], ['corrupt-'])
        self.assertEqual(set(records[0]['files']), {'hil_report.json', 'hil_report.md'})

    def test_the_local_report_is_archived_before_the_copy_back_replaces_it(self):
        old = self.rig_report(attempt_id='local-1')
        new = self.rig_report(rows=[], attempt_id='rig-2')
        for n, text in old.items():
            self.local(n).write_text(text, encoding='utf-8')
        r = self.hil_remote('-b', 'alpha', FAKE_FILES=json.dumps(new))
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(self.local('hil_report.json').read_text(encoding='utf-8'), new['hil_report.json'])
        kept = self.root / report.REPORT_ATTEMPTS / 'local-1' / 'hil_report.json'
        self.assertEqual(kept.read_text(encoding='utf-8'), old['hil_report.json'])

    def test_the_rigs_attempt_records_come_home_create_only(self):
        attempts = report.REPORT_ATTEMPTS
        files = self.rig_report(attempt_id='rig-2', previous='rig-1')
        (self.root / attempts / 'rig-0').mkdir(parents=True)
        (self.root / attempts / 'rig-0' / 'hil_report.json').write_text('the local record')
        files.update({f'{attempts}/rig-1/hil_report.json': 'the predecessor', f'{attempts}/rig-1/hil_report.md': 'md',
                      f'{attempts}/rig-0/hil_report.json': 'a different record under a used id',
                      f'{attempts}/.rig-9.0123.tmp/hil_report.json': 'an interrupted archive'})
        r = self.hil_remote('-b', 'alpha', FAKE_FILES=json.dumps(files))
        self.assertEqual(r.returncode, 0, r.stderr)
        local = self.root / attempts
        self.assertEqual((local / 'rig-1' / 'hil_report.json').read_text(), 'the predecessor')
        self.assertEqual((local / 'rig-0' / 'hil_report.json').read_text(), 'the local record')
        self.assertIn('attempt record(s) rig-0 on the rig differ', r.stderr)
        self.assertEqual(sorted(p.name for p in local.iterdir()), ['rig-0', 'rig-1'])
        kept, = (self.root / hil_remote.RUNS_DIR).iterdir()
        self.assertEqual((kept / attempts / 'rig-0' / 'hil_report.json').read_text(), 'a different record under a used id',
                         'the rig\'s differing record is kept apart')

    def test_an_unknown_local_report_is_refused_before_the_rig(self):
        self.local('hil_report.json').write_text(json.dumps({'rows': [], '_hil': {'version': 99}}))
        r = self.hil_remote('-b', 'alpha')
        self.assertEqual(r.returncode, 1)
        self.assertIn('move it away first', r.stderr)
        self.assertEqual(self.calls(), [])

    def publish_local(self, aid, accumulate):
        attempt = RCORE.start_attempt(self.root, report.schema(), accumulate, aid)
        meta = RCORE.make_meta(report.schema(), attempt, termination='completed', invocation=['hil_test.py'],
                               selection=['alpha'], host='pc', roster={})
        RCORE.publish(self.root, report.schema(), {'rows': self.ROWS, 'scope': '', 'caveat': ''}, meta)

    def staged_records(self):
        return sorted(p.name for p in (self.remote / report.REPORT_ATTEMPTS).iterdir()) \
            if (self.remote / report.REPORT_ATTEMPTS).is_dir() else []

    def test_accumulate_uploads_a_matching_sidecar_and_the_records_it_names(self):
        self.publish_local('old', False)
        self.publish_local('a1', False)          # archives old, which a1 does not name
        self.publish_local('a2', True)           # names a1
        for spelling in ('--accumulate', '-a', '-av', '-va', '--accum', '--acc'):
            r = self.hil_remote('-b', 'alpha', spelling)
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertIn('Uploading hil_report.json', r.stdout, spelling)
            self.assertIn('hil_report.json', self.ran()['files'], spelling)
            self.assertEqual(self.staged_records(), ['a1'], spelling)
            for f in (report.REPORT_JSON, report.REPORT_MD):   # the run's report came home: put a2 back
                (self.root / f).write_bytes((self.root / report.REPORT_ATTEMPTS / 'a2' / f).read_bytes())

    def test_a_withdrawal_sidecar_uploads_the_reports_it_archived(self):
        """It names no predecessor, but the rig's merge reads its rows from the report it withdrew."""
        self.publish_local('old', False)
        self.publish_local('a1', False)
        self.publish_local('a2', True)
        RCORE.withdraw_current(self.root, report.schema(), 'a3', 'publish failed')
        RCORE.withdraw_current(self.root, report.schema(), 'a4', 'publish failed')   # archives a3's record
        self.hil_remote('-b', 'alpha', '-a', FAKE_WRITE='')   # the fake run leaves the uploaded sidecar in place
        self.assertEqual(self.staged_records(), ['a1', 'a2', 'withdrawn-a3'])
        self.assertEqual([r['board'] for r in report._load(self.remote)[0]['rows']], ['alpha'],
                         'the rig\'s merge base keeps the rows the withdrawals archived')

    def test_accumulate_without_a_usable_sidecar_warns(self):
        r = self.hil_remote('-b', 'alpha', '-a')
        self.assertIn('no local hil_report.json', r.stderr)
        self.local('hil_report.json').write_text(json.dumps({'rows': [{'board': 'elsewhere'}]}))
        r = self.hil_remote('-b', 'alpha', '-a')
        self.assertIn('not uploading it', r.stderr)
        self.assertNotIn('hil_report.json', self.ran()['files'])

    def test_no_upload_without_accumulate(self):
        self.local('hil_report.json').write_text(json.dumps({'rows': [{'board': 'alpha'}]}))
        self.hil_remote('-b', 'alpha', '-v')
        self.assertNotIn('hil_report.json', self.ran()['files'])


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
                    mock.patch.object(hil_remote.core, 'remote_lease', lease), \
                    mock.patch.object(hil_remote.core.subprocess, 'run', side_effect=AssertionError('rsync or ssh ran')), \
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
