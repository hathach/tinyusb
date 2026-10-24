#!/usr/bin/env python3
"""Unit tests for tools/code_size.py, the launcher of agentrc's code-size engine at the pin, and
tools/code_size_profile.py, TinyUSB's profile. The engine's own tests are agentrc's
(tests/test_code_size.py); these cover what the profile supplies: builds through tools/build.py,
ESP-IDF, board and example checks, the CI-pinned boards, membrowse's link inputs and the base
worktree's dependencies. Builds and tools are stubbed.
"""
import contextlib
import io
import os
import subprocess
import sys
import tempfile
import types
import unittest
from unittest import mock

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..'))
sys.path.insert(0, os.path.join(REPO, 'tools'))
import code_size  # noqa: E402
import code_size_profile as prof  # noqa: E402

sd = code_size.load_engine()
prof.engine = sd  # as the engine's main() does
no_esp_on_windows = unittest.skipIf(os.name == 'nt', 'code_size.py refuses ESP-IDF on Windows')


@contextlib.contextmanager
def bound(out_dir=None):
    """The engine's module state as main() sets it for TinyUSB, `out_dir` its OUT_DIR."""
    saved = sd.profile, sd.ROOT, sd.OUT_DIR
    sd.profile, sd.ROOT, sd.OUT_DIR = prof, prof.ROOT, out_dir or prof.OUT_DIR
    try:
        yield
    finally:
        sd.profile, sd.ROOT, sd.OUT_DIR = saved


def _touch(root, *rels):
    for rel in rels:
        os.makedirs(os.path.dirname(os.path.join(root, rel)), exist_ok=True)
        open(os.path.join(root, rel), 'w').close()


class Launcher(unittest.TestCase):
    def test_the_engine_is_agentrcs_at_the_pin(self):
        self.assertEqual(os.path.realpath(sd.__file__),
                         os.path.realpath(os.path.join(REPO, 'lib', 'agentrc', 'skills', 'code-size', 'scripts',
                                                       'code_size.py')))

    def test_the_engine_runs_with_the_profile(self):
        with mock.patch.object(sd, 'main', return_value=0) as main:
            self.assertEqual(code_size.main(['report', '-b', 'b']), 0)
        main.assert_called_once_with(prof, ['report', '-b', 'b'])

    def test_an_engine_off_its_pin_refuses_before_anything_runs(self):
        with mock.patch.object(code_size, 'load_engine', side_effect=ImportError('lib/agentrc must be at x')), \
             contextlib.redirect_stderr(io.StringIO()) as err:
            self.assertEqual(code_size.main(['report', '-b', 'b']), 2)
        self.assertIn('refused: lib/agentrc must be at x', err.getvalue())

    def test_the_profile_gives_every_hook_the_engine_uses(self):
        for name in sd.REQUIRED + ('link_inputs', 'linkermap_path', 'ci_boards', 'BASELINE_REPO', 'baseline_run',
                                   'snapshot_boards', 'skip_example', 'first_error'):
            self.assertTrue(hasattr(prof, name), name)

    def test_the_filter_is_the_checkouts_own_src(self):
        self.assertEqual(prof.filters(REPO), [sd.filter_arg(os.path.realpath(os.path.join(REPO, 'src')) + os.sep)])


class LinkInputs(unittest.TestCase):
    def test_uses_the_elfs_link_and_runs_from_the_build_dir(self):
        with tempfile.TemporaryDirectory() as build:
            open(os.path.join(build, 'build.ninja'), 'w').close()
            elf = os.path.join(build, 'device', 'x', 'x.elf')
            os.makedirs(os.path.dirname(elf))
            commands = 'cc -Wl,--script=/sdk/memmap.ld -Wl,--defsym=X=1 -o device/x/x.elf\n'
            with mock.patch.object(prof, 'link_command', return_value=commands) as link:
                cwd, inputs = prof.link_inputs(elf)
            link.assert_called_once_with('ninja', build, elf)
            self.assertEqual(cwd, build)
            self.assertEqual(inputs[:2], [elf, '/sdk/memmap.ld'])
            self.assertEqual(inputs[inputs.index('--def') + 1], 'X=1')

    @no_esp_on_windows
    def test_an_esp_idf_build_uses_its_generated_scripts(self):
        with tempfile.TemporaryDirectory() as build:
            open(os.path.join(build, 'build.ninja'), 'w').close()
            ld = os.path.join(build, 'esp-idf', 'esp_system', 'ld')
            os.makedirs(ld)
            commands = 'cc -T memory.ld -T sections.ld -T esp32s3.rom.ld -Wl,--defsym=X=1 -o x.elf\n'
            open(os.path.join(ld, 'memory.ld'), 'w').close()
            elf = os.path.join(build, 'x.elf')
            with mock.patch.object(prof, 'link_command', return_value=commands), \
                 self.assertRaisesRegex(RuntimeError, 'lacks one of'):
                prof.link_inputs(elf)
            open(os.path.join(ld, 'sections.ld'), 'w').close()
            with mock.patch.object(prof, 'link_command', return_value=commands):
                self.assertEqual(prof.link_inputs(elf),
                                 (build, [elf, f'{ld}/memory.ld {ld}/sections.ld', '--def', 'X=1']))

    def test_no_build_graph_or_linker_script_is_an_error_not_default_regions(self):
        with tempfile.TemporaryDirectory() as build:
            with self.assertRaisesRegex(RuntimeError, 'no build.ninja found'):
                prof.link_inputs(os.path.join(build, 'x.elf'))
            open(os.path.join(build, 'build.ninja'), 'w').close()
            with mock.patch.object(prof, 'link_command', return_value='cc -o x.elf\n'), \
                 self.assertRaisesRegex(RuntimeError, 'no linker script found'):
                prof.link_inputs(os.path.join(build, 'x.elf'))


class Build(unittest.TestCase):
    @staticmethod
    @contextlib.contextmanager
    def _cmake_tree(tmp, skip=False, registered=('ex',)):
        """`tmp` a tree with examples/device/ex, which skip.txt/only.txt `skip` and whose
        configure creates the `registered` targets."""
        import build
        os.makedirs(os.path.join(tmp, 'examples', 'device', 'ex'), exist_ok=True)
        with mock.patch.object(prof.build_utils, 'skip_example', return_value=skip), \
             mock.patch.object(build, 'cmake_registered_targets', return_value=registered and set(registered)):
            yield

    def _build(self, *args):
        with bound(), contextlib.redirect_stdout(io.StringIO()) as out:
            return sd.build_board(*args), out.getvalue()

    def test_a_failed_build_returns_the_first_error_through_build_utils(self):
        failed = subprocess.CompletedProcess([], 1, 'FAILED: bad.c.obj\nbad.c:1: error: x undeclared\n', '')
        ok = subprocess.CompletedProcess([], 0, '', '')
        with tempfile.TemporaryDirectory() as tmp, self._cmake_tree(tmp), \
             mock.patch.object(sd, 'run', side_effect=[ok, failed]):
            error, out = self._build(tmp, os.path.join(tmp, 'b'), 'b', 'device/ex', 'build master')
        self.assertEqual(error, 'bad.c:1: error: x undeclared')
        self.assertRegex(out, r'^  build master… FAILED after \d+\.\ds\n'
                              r'    FAILED: bad.c.obj\n    bad.c:1: error: x undeclared\n$')

    def test_the_build_step_runs_ninja_for_the_example_with_a_timeout(self):
        ok = subprocess.CompletedProcess([], 0, '', '')
        with tempfile.TemporaryDirectory() as tmp, self._cmake_tree(tmp), \
             mock.patch.object(sd, 'run', return_value=ok) as run:
            self._build(tmp, os.path.join(tmp, 'b'), 'b', 'device/ex', 'build')
        self.assertEqual(run.call_args, mock.call(['ninja', '-C', os.path.join(tmp, 'b'), 'ex'], timeout=600))

    def test_an_example_the_board_skips_is_skipped_before_any_configure(self):
        with tempfile.TemporaryDirectory() as tmp, self._cmake_tree(tmp, skip=True), \
             mock.patch.object(sd, 'run') as run:
            error, out = self._build(tmp, os.path.join(tmp, 'b'), 'b', 'device/ex', 'build')
        self.assertIsInstance(error, sd.Skipped)
        self.assertEqual(error, 'b does not build device/ex')
        self.assertEqual(out, '  build… skipped: b does not build device/ex\n')
        run.assert_not_called()

    def test_an_example_absent_from_the_tree_is_skipped(self):
        with tempfile.TemporaryDirectory() as tmp, mock.patch.object(sd, 'run') as run:
            error, _out = self._build(tmp, os.path.join(tmp, 'b'), 'b', 'device/ex', 'build')
        self.assertEqual((type(error), error), (sd.Absent, 'device/ex is not in this tree'))
        run.assert_not_called()

    def test_an_example_the_configure_does_not_register_is_skipped_without_ninja(self):
        ok = subprocess.CompletedProcess([], 0, '', '')
        for registered, skipped in ((('other',), True), (None, False)):
            with self.subTest(registered=registered), tempfile.TemporaryDirectory() as tmp, \
                 self._cmake_tree(tmp, registered=registered), \
                 mock.patch.object(sd, 'run', return_value=ok) as run:
                error, _out = self._build(tmp, os.path.join(tmp, 'b'), 'b', 'device/ex', 'build')
            # None: the target list is unreadable, so ninja decides
            self.assertEqual(error, 'b has no ex target' if skipped else None)
            self.assertEqual(run.call_count, 1 if skipped else 2)

    def test_the_configure_is_build_pys_with_its_default_toolchain(self):
        import build
        ok = subprocess.CompletedProcess([], 0, '', '')
        with tempfile.TemporaryDirectory() as tmp, mock.patch.object(sd, 'run', return_value=ok) as run:
            build_dir = os.path.join(tmp, 'b')
            self._build(tmp, build_dir, 'b', None, 'build')
        self.assertEqual(run.call_args_list[0],
                         mock.call(build.cmake_configure_cmd('b', build_dir, ['-DTOOLCHAIN=gcc'],
                                                             os.path.join(tmp, 'examples'))))

    def test_elfs_are_the_examples_own_not_helpers_or_fetched_tools(self):
        with tempfile.TemporaryDirectory() as tmp:
            _touch(tmp, 'device/a/a.elf', 'device/a/sub/bs2_default.elf', '_deps/picotool/x/enc.elf',
                   'host/b/b.elf')
            self.assertEqual(sorted(os.path.relpath(e, tmp) for e in prof.elfs(tmp, None)),
                             ['device/a/a.elf', 'host/b/b.elf'])
            self.assertEqual([os.path.relpath(e, tmp) for e in prof.elfs(tmp, 'device/a')], ['device/a/a.elf'])


class Espressif(unittest.TestCase):
    def _esp_src(self, tmp):
        """A checkout with one espressif board, one example and a dependency linked in two
        hops, as a base worktree's: src -> checkout -> main checkout."""
        src = os.path.join(tmp, 'src')
        os.makedirs(os.path.join(src, 'hw', 'bsp', 'espressif', 'boards', 'esp'))
        # an ESP-IDF component, which get_examples('espressif') requires
        _touch(src, 'examples/device/a_freertos/src/CMakeLists.txt')
        _touch(src, 'examples/device/no_idf/CMakeLists.txt')
        os.makedirs(os.path.join(src, 'tools'))
        os.makedirs(os.path.join(src, 'lib'))
        os.makedirs(os.path.join(tmp, 'main', 'lib', 'dep'))
        os.makedirs(os.path.join(tmp, 'checkout', 'lib'))
        os.symlink(os.path.join(tmp, 'main', 'lib', 'dep'), os.path.join(tmp, 'checkout', 'lib', 'dep'))
        os.symlink(os.path.join(tmp, 'checkout', 'lib', 'dep'), os.path.join(src, 'lib', 'dep'))
        with open(os.path.join(src, 'tools', 'get_deps.py'), 'w') as f:
            f.write("deps_all = {'lib/dep': None, 'lib/absent': None}\n")
        return src

    def _build_esp(self, tmp, example='device/a_freertos', which=('idf.py',), inspect=0, rc=0, runs=None):
        """(build_board's error, its run() calls but the image check, also into `runs`); `rc`
        an exception class raises it from the build."""
        runs = [] if runs is None else runs

        def run(cmd, timeout=None):
            if cmd[:3] == ['docker', 'image', 'inspect']:
                return subprocess.CompletedProcess([], inspect, '', '')
            runs.append((cmd, timeout))
            if isinstance(rc, type) and 'app' in cmd:
                raise rc
            return subprocess.CompletedProcess([], rc, '', 'x')
        with bound(), mock.patch.object(sd, 'run', run), mock.patch.object(prof, 'OUT_DIR', tmp), \
             mock.patch.object(prof.shutil, 'which', lambda name: f'/bin/{name}' if name in which else None), \
             mock.patch.object(prof.build_utils, 'skip_example', return_value=False), \
             contextlib.redirect_stdout(io.StringIO()):
            error = sd.build_board(self._esp_src(tmp), os.path.join(tmp, 'b'), 'esp', example, 'build')
        return error, runs

    @no_esp_on_windows
    def test_an_espressif_board_builds_each_examples_app_as_an_idf_project(self):
        with tempfile.TemporaryDirectory() as tmp:
            error, runs = self._build_esp(tmp)
        self.assertIsNone(error)
        self.assertEqual(runs, [(['idf.py', '-C', os.path.join(tmp, 'src', 'examples', 'device', 'a_freertos'),
                                  '-B', os.path.join(tmp, 'b', 'device', 'a_freertos'), '-GNinja', '-DBOARD=esp',
                                  'app'], 600)])

    @no_esp_on_windows
    def test_without_an_exported_idf_it_builds_in_cis_image_as_this_user(self):
        with tempfile.TemporaryDirectory() as tmp:
            error, runs = self._build_esp(tmp, which=('docker',))
            dep, cache = os.path.realpath(os.path.join(tmp, 'main', 'lib', 'dep')), os.path.join(tmp, '_ccache')
            mounts = [f'{os.path.realpath(tmp)}/_ccache:{cache}', f'{os.path.realpath(tmp)}/b:{tmp}/b',
                      f'{dep}:{tmp}/checkout/lib/dep', f'{dep}:{tmp}/main/lib/dep',
                      f'{os.path.realpath(tmp)}/src:{tmp}/src']
            self.assertTrue(os.path.isdir(cache))
        self.assertIsNone(error)
        cmd = runs[0][0]
        self.assertEqual(cmd[:11], ['docker', 'run', '--rm', '--name', f'tinyusb-code-size-{os.getpid()}',
                                    '--user', f'{os.getuid()}:{os.getgid()}', '-e', 'HOME=/tmp',
                                    '-e', f'CCACHE_DIR={cache}'])
        self.assertEqual([cmd[i + 1] for i, a in enumerate(cmd) if a == '-v'], mounts)
        self.assertEqual(cmd[cmd.index(prof.ESP_IDF_IMAGE):][:3], [prof.ESP_IDF_IMAGE, 'idf.py', '-C'])

    def test_without_idf_or_its_image_the_espressif_build_fails_saying_how_to_get_one(self):
        for which, inspect in (((), 0), (('docker',), 1)):
            with tempfile.TemporaryDirectory() as tmp:
                error, runs = self._build_esp(tmp, which=which, inspect=inspect)
            self.assertEqual(runs, [])
            self.assertIn('esp needs ESP-IDF: source $IDF_PATH/export.sh', error)

    @mock.patch.object(sd, 'WINDOWS', new=False)
    def test_without_idf_report_and_diff_refuse_an_espressif_board_before_building(self):
        pinned = ['stm32f407disco', 'espressif_s3_devkitm']
        # diff names its base: CI's checkout has no master, the default --base-branch
        for argv in (['report', '-b', pinned[0], '-b', pinned[1]],
                     ['diff', '-b', pinned[0], '-b', pinned[1], '--base-branch', 'HEAD'],
                     ['diff', '--ci', '--base-branch', 'HEAD']):
            err = io.StringIO()
            with mock.patch.object(prof, 'ci_boards', return_value=pinned), \
                 mock.patch.object(sd, 'engine_missing', return_value=False), \
                 mock.patch.object(prof.shutil, 'which', return_value=None), \
                 mock.patch.object(sd, 'build_board') as build, contextlib.redirect_stderr(err):
                with self.assertRaises(SystemExit):
                    code_size.main(argv)
            build.assert_not_called()
            self.assertIn('espressif_s3_devkitm need ESP-IDF: source $IDF_PATH/export.sh', err.getvalue())

    def test_an_example_the_tree_does_not_build_for_espressif_is_skipped(self):
        # device/no_idf has no ESP-IDF component; device/board_test is absent from this tree
        for example, kind, why in (('device/no_idf', sd.Skipped, 'esp does not build device/no_idf'),
                                   ('device/board_test', sd.Absent, 'device/board_test is not in this tree')):
            with tempfile.TemporaryDirectory() as tmp:
                error, runs = self._build_esp(tmp, example=example)
            self.assertEqual((type(error), error, runs), (kind, why, []))

    def test_an_espressif_board_building_no_example_at_all_fails(self):
        with tempfile.TemporaryDirectory() as tmp, \
             mock.patch('build.select_examples', return_value=([], {})):
            error, runs = self._build_esp(tmp, example=None)
        self.assertEqual((type(error), error, runs), (str, 'esp builds no example', []))

    @no_esp_on_windows
    def test_a_timed_out_or_interrupted_docker_build_removes_its_container(self):
        rm = ['docker', 'rm', '-f', f'tinyusb-code-size-{os.getpid()}']
        for which, rc, removed in ((('docker',), 124, True), (('docker',), 2, False), (('idf.py',), 124, False)):
            with tempfile.TemporaryDirectory() as tmp:
                _error, runs = self._build_esp(tmp, which=which, rc=rc)
            self.assertEqual(runs[-1][0] == rm, removed, (which, rc))
        runs = []
        with tempfile.TemporaryDirectory() as tmp, self.assertRaises(KeyboardInterrupt):
            self._build_esp(tmp, which=('docker',), rc=KeyboardInterrupt, runs=runs)
        self.assertEqual(runs[-1][0], rm)

    def test_espressif_boards_are_refused_on_windows(self):
        with mock.patch.object(sd, 'WINDOWS', new=True):
            self.assertRegex(prof.esp_without_idf(['espressif_s3_devkitc', 'stm32f407disco']),
                             r'^espressif_s3_devkitc need ESP-IDF, .* not support on Windows')
            self.assertIsNone(prof.esp_without_idf(['stm32f407disco']))
            # an espressif board that skips every example asked for builds nothing
            self.assertIsNone(prof.esp_without_idf(['espressif_s3_devkitm'], ['device/cdc_msc']))
            self.assertRegex(prof.esp_without_idf(['espressif_s3_devkitm'],
                                                  ['device/cdc_msc', 'device/cdc_msc_freertos']),
                             'espressif_s3_devkitm need ESP-IDF')
            # an example absent here needs ESP-IDF only for a base built locally, which may have it
            self.assertIsNone(prof.esp_without_idf(['espressif_s3_devkitm'], ['device/gone']))
            self.assertRegex(prof.esp_without_idf(['espressif_s3_devkitm'], ['device/gone'], base_built=True),
                             'espressif_s3_devkitm need ESP-IDF')


# main() tests stub the builds, so need no engine tool, and use made-up boards and examples
@mock.patch.object(sd, 'engine_missing', new=lambda _engine: False)
@mock.patch.object(prof, 'unknown_boards', new=lambda _boards: [])
class CiBoardSet(unittest.TestCase):
    def _boards_built(self, tmp, pinned_json):
        """Boards main() builds for `--ci -b extra -b b1 -b extra`, with `pinned_json` as the pinned file."""
        pinned = os.path.join(tmp, 'ci-pinned-boards.json')
        with open(pinned, 'w') as f:
            f.write(pinned_json)
        built = []

        def build(_src, _build_dir, board, *_args, **_kwargs):
            built.append(board)
            os.makedirs(os.path.join(tmp, board), exist_ok=True)
            return 'boom'  # stop at the base build: only the board set matters
        ok = subprocess.CompletedProcess([], 0, '', '')
        with mock.patch.object(prof, 'OUT_DIR', tmp), mock.patch.object(prof, 'CI_PINNED_BOARDS', pinned), \
             mock.patch.object(sd, 'run', return_value=ok), mock.patch.object(prof, 'prepare_base'), \
             mock.patch.object(sd, 'build_board', side_effect=build), \
             contextlib.redirect_stdout(io.StringIO()):
            code_size.main(['diff', '--ci', '-b', 'extra', '-b', 'b1', '-b', 'extra', '--base-source', 'local'])
        return built

    def test_ci_adds_the_pinned_boards_after_the_named_ones_once_each(self):
        with tempfile.TemporaryDirectory() as tmp:
            built = self._boards_built(tmp, '{"boards": [{"board": "b1"}, {"board": "b2"}], '
                                            '"uncovered": []}')
            self.assertEqual(built, ['extra', 'b1', 'b2'])

    @mock.patch.object(sd, 'WINDOWS', new=False)
    def test_ci_builds_pinned_espressif_boards_too(self):
        with tempfile.TemporaryDirectory() as tmp:
            built = self._boards_built(tmp, '{"boards": [{"board": "b1"}, '
                                            '{"board": "espressif_s3_devkitm"}], "uncovered": []}')
        self.assertEqual(built, ['extra', 'b1', 'espressif_s3_devkitm'])


class SnapshotBoards(unittest.TestCase):
    def test_boards_are_the_pinned_ones_of_the_leg(self):
        with tempfile.TemporaryDirectory() as tmp:
            pinned = os.path.join(tmp, 'pinned.json')
            with open(pinned, 'w') as f:
                f.write('{"boards": [{"board": "p1"}, {"board": "p2"}]}')
            import build
            with mock.patch.object(prof, 'CI_PINNED_BOARDS', pinned), \
                 mock.patch.object(build, 'builds_any', return_value=True), \
                 mock.patch.object(build, 'resolve_ci_boards', return_value=['p2']) as resolve:
                boards = prof.snapshot_boards(['fam'], ['p1', 'notpinned', 'p2'], ['device/a'])
        self.assertEqual(boards, ['p1', 'p2'])
        resolve.assert_called_once_with(pinned, 'fam', True, ['device/a'], extra_defines=())

    def test_a_pinned_board_that_builds_none_of_the_examples_is_not_expected(self):
        # build.py skips it before configuring: no build dir, and no failure either
        with tempfile.TemporaryDirectory() as tmp:
            pinned = os.path.join(tmp, 'pinned.json')
            with open(pinned, 'w') as f:
                f.write('{"boards": [{"board": "p1"}, {"board": "p2"}]}')
            import build
            with mock.patch.object(prof, 'CI_PINNED_BOARDS', pinned), \
                 mock.patch.object(build, 'builds_any', side_effect=lambda b, _e, _d: b == 'p2'):
                self.assertEqual(prof.snapshot_boards([], ['p1', 'p2'], ['host/device_info']), ['p2'])

    def test_a_variant_takes_build_py_defines(self):
        argv = ['-b', 'b1', '--build-name', 'b1-X', '-DX=1', '--define-symbol', 'Y=2', '--cflag=-DZ=3']
        with mock.patch.object(sd, 'engine_missing', return_value=False), \
             mock.patch.object(sd, 'run_snapshot', return_value=0) as run:
            self.assertEqual(code_size.main(['snapshot', '-o', 'out'] + argv), 0)
        self.assertEqual((run.call_args[0][0].define_symbol, run.call_args[0][0].cflag), (['X=1', 'Y=2'], ['-DZ=3']))


class PrepareBase(unittest.TestCase):
    def test_links_each_dep_path_of_the_worktrees_own_manifest(self):
        deps = {'hw/mcu/broadcom': [], 'hw/mcu/raspberry_pi/Pico-PIO-USB': [],
                'hw/mcu/wch/ch58x': [], 'lib/lwip': [], 'lib/not_fetched': []}
        with tempfile.TemporaryDirectory() as main, tempfile.TemporaryDirectory() as wt:
            for rel in ('hw/mcu/broadcom/broadcom', 'hw/mcu/raspberry_pi/Pico-PIO-USB',
                        'hw/mcu/wch/ch58x', 'hw/mcu/wch/ch583', 'lib/lwip',
                        'hw/mcu/raspberry_pi/tracked'):
                os.makedirs(os.path.join(main, rel))
            open(os.path.join(main, 'hw/mcu/broadcom/core_ca72.h'), 'w').close()
            os.makedirs(os.path.join(wt, 'lib/lwip'))  # already present: left alone
            os.makedirs(os.path.join(wt, 'tools'))
            with open(os.path.join(wt, 'tools', 'get_deps.py'), 'w') as f:
                f.write(f'deps_all = {deps!r}\n')
            prof.prepare_base(main, wt)
            # a whole-vendor-dep keeps its root files
            self.assertTrue(os.path.isfile(os.path.join(wt, 'hw/mcu/broadcom/core_ca72.h')))
            self.assertTrue(os.path.islink(os.path.join(wt, 'hw/mcu/broadcom')))
            self.assertTrue(os.path.islink(os.path.join(wt, 'hw/mcu/raspberry_pi/Pico-PIO-USB')))
            # the base's path, not the current checkout's rename
            self.assertTrue(os.path.islink(os.path.join(wt, 'hw/mcu/wch/ch58x')))
            self.assertFalse(os.path.lexists(os.path.join(wt, 'hw/mcu/wch/ch583')))
            self.assertFalse(os.path.islink(os.path.join(wt, 'lib/lwip')))
            self.assertFalse(os.path.lexists(os.path.join(wt, 'lib/not_fetched')))
            self.assertFalse(os.path.lexists(os.path.join(wt, 'hw/mcu/raspberry_pi/tracked')))

    def test_a_symlink_without_the_privilege_names_developer_mode(self):
        denied = OSError(22, 'A required privilege is not held by the client')
        denied.winerror = 1314
        with tempfile.TemporaryDirectory() as main, tempfile.TemporaryDirectory() as wt:
            os.makedirs(os.path.join(main, 'lib/lwip'))
            os.makedirs(os.path.join(wt, 'tools'))
            with open(os.path.join(wt, 'tools', 'get_deps.py'), 'w') as f:
                f.write("deps_all = {'lib/lwip': []}\n")
            with mock.patch('os.symlink', side_effect=denied), \
                    self.assertRaisesRegex(SystemExit, 'Developer Mode, or use --base-source ci'):
                prof.prepare_base(main, wt)


class BaselineRun(unittest.TestCase):
    def test_the_baseline_is_code_size_cis_pick_whose_later_changes_are_yours(self):
        ci = types.SimpleNamespace(lookup_sha=mock.Mock(return_value={'sha': 'b' * 40}))
        with mock.patch('importlib.util.module_from_spec', return_value=ci), \
             mock.patch('importlib.util.spec_from_file_location') as spec:
            self.assertEqual(prof.baseline_run('o/r', 'a' * 40), {'sha': 'b' * 40})
        self.assertEqual(spec.call_args.args[1], os.path.join(REPO, '.github', 'scripts', 'code_size_ci.py'))
        ci.lookup_sha.assert_called_once_with('o/r', 'a' * 40, 'yours')


class BoardAndExampleNames(unittest.TestCase):
    def _refused(self, argv):
        with tempfile.TemporaryDirectory() as tmp, mock.patch.object(prof, 'OUT_DIR', tmp), \
             mock.patch.object(sd, 'build_board') as build, \
             contextlib.redirect_stderr(io.StringIO()) as err, self.assertRaises(SystemExit) as exit_:
            code_size.main(argv)
        build.assert_not_called()
        self.assertEqual(exit_.exception.code, 2)
        return err.getvalue()

    def test_an_unknown_board_is_refused_not_skipped(self):
        # diff names its base: CI's checkout has no master, the default --base-branch
        for argv in (['report'], ['diff', '--base-branch', 'HEAD']):
            self.assertIn('error: unknown board: nosuch', self._refused(argv + ['-b', 'nosuch', '--engine',
                                                                                'sections']))

    def test_an_example_in_neither_tree_is_refused_not_skipped(self):
        head = subprocess.run(['git', '-C', REPO, 'rev-parse', 'HEAD'], capture_output=True, text=True).stdout
        self.assertIn('error: no such example: device/typo',
                      self._refused(['report', '-b', 'stm32f407disco', '-e', 'device/typo', '--engine', 'sections']))
        self.assertIn(f'error: no such example here or in {head[:10]}: device/typo',
                      self._refused(['diff', '-b', 'stm32f407disco', '-e', 'device/typo', '--base-branch', 'HEAD',
                                     '--engine', 'sections']))

    def test_an_example_only_in_the_base_commit_is_accepted(self):
        def run(cmd):
            return subprocess.CompletedProcess(cmd, 0 if cmd[-1] == 'b' * 40 + ':examples/device/gone' else 128, '', '')
        with mock.patch.object(sd, 'run', side_effect=run):
            self.assertEqual(prof.missing_examples(['device/gone', 'device/typo'], 'b' * 40), ['device/typo'])
        self.assertEqual(prof.missing_examples(['device/cdc_msc']), [])
        # a role dir exists but names no example: build.py's -e check refuses it too
        self.assertEqual(prof.missing_examples(['device', 'device/cdc_msc/src']), ['device', 'device/cdc_msc/src'])


if __name__ == '__main__':
    unittest.main()
