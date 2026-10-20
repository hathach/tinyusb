#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# The farm check's tinyusb side: helper/farm_profile.py's verdicts and helper/hil_farm_check.py's
# wiring to agentrc's engine, with the bus, the flasher, gh and the tracked runner faked and the
# engine's lock dir in a temp dir: no board, no ssh, no download. Run directly:
#   python3 test/hil/test/test_hil_farm_check.py
import json
import os
import subprocess
import sys
import tempfile
import types
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import usbtest_harness                             # noqa: E402 - stubs pyserial
import hil_flash                                   # noqa: E402
from helper import farm_profile, hil_farm_check, hil_lock, hil_util   # noqa: E402
from usbtest_harness import patch                  # noqa: E402

BOARD = {'name': 'b', 'uid': 'U', 'flasher': {'name': 'openocd', 'uid': 'P'}}
DEVICE = dict(BOARD, tests={'device': True})
HOST = dict(BOARD, tests={'host': True})
eng = hil_farm_check.load_engine()


def args(**kw):
    return types.SimpleNamespace(**{'build_dir': None, 'verbose': False, **kw})


def op(rc=0, stdout=''):
    return types.SimpleNamespace(returncode=rc, stdout=stdout, stderr=None)


def context(test, *cleanups):
    """The engine's own Context for BOARD, run_tracked faked: each run ends with the next of
    `cleanups` ('done' once they run out), and a recheck finds it unchanged. `calls` lists the
    runs, `row` what the engine records; the step is 'test'."""
    queue, ends, calls = list(cleanups), {}, []

    def tracked(cmd, cwd=None, *, timeout, settle, state_dir, **kw):
        calls.append((cmd, cwd, timeout, kw))
        cleanup = queue.pop(0) if queue else 'done'
        r = subprocess.CompletedProcess(cmd, 0, '', None)
        r.cleanup, r.receipt = cleanup, None
        if cleanup != 'done':
            r.receipt = {'token': str(len(calls)), 'lock': '/nonexistent'}
            ends[r.receipt['token']] = cleanup
        return r
    patch(test, eng.hil_process, 'run_tracked', tracked)
    patch(test, eng.hil_process, 'recheck', lambda receipt: ends[receipt['token']])
    patch(test, eng, 'RECHECK_WAIT', 0)
    ctx = eng.Context(BOARD, args(), '/nonexistent', eng.new_row('b'))
    ctx.step, ctx.calls = 'test', calls
    return ctx


def capture(fn):
    """(fn's return, stdout, stderr) at the fd level: the engine moves fd 1 under --json."""
    with tempfile.TemporaryFile() as out, tempfile.TemporaryFile() as err:
        sys.stdout.flush()
        sys.stderr.flush()
        saved = os.dup(1), os.dup(2)
        streams = sys.stdout, sys.stderr
        os.dup2(out.fileno(), 1)
        os.dup2(err.fileno(), 2)
        # onto the fds, whatever a test runner put in sys.stdout/stderr
        sys.stdout, sys.stderr = (open(fd, 'w', encoding='utf-8', closefd=False) for fd in (1, 2))
        try:
            code = fn()
        finally:
            sys.stdout.flush()
            sys.stderr.flush()
            sys.stdout, sys.stderr = streams
            os.dup2(saved[0], 1)
            os.dup2(saved[1], 2)
            os.close(saved[0])
            os.close(saved[1])
        out.seek(0)
        err.seek(0)
        return code, out.read().decode(), err.read().decode()


class Roster(unittest.TestCase):
    def test_a_rig_host_gets_its_roster(self):
        got = farm_profile.roster('ci', None, args())
        # from test/hil the bare name resolves against the cwd first: the same file either way
        self.assertEqual(Path(got['path']).resolve(), hil_util.TINYUSB_ROOT / 'test/hil/tinyusb.json')
        self.assertTrue(got['boards'])
        self.assertIsInstance(got['named_only'], list)

    def test_an_unknown_host_without_a_roster_is_refused(self):
        # no local.json fallback: a dev PC names its own
        with self.assertRaises(LookupError):
            farm_profile.roster('htpc', None, args())

    def test_an_explicit_roster_is_used_on_any_host(self):
        with tempfile.TemporaryDirectory() as td:
            cfg = Path(td, 'r.json')
            cfg.write_text(json.dumps({'boards': [DEVICE], 'boards-skip': [dict(DEVICE, name='parked')]}))
            got = farm_profile.roster('htpc', str(cfg), args())
        self.assertEqual((got['path'], [b['name'] for b in got['boards']], [b['name'] for b in got['named_only']]),
                         (str(cfg), ['b'], ['parked']))


class Probe(unittest.TestCase):
    def bus(self, devices):
        patch(self, farm_profile, 'scan_usb', lambda: {bp: {'serial': s, 'vidpid': vp} for bp, s, vp in devices})

    def candidates(self, uid):
        return farm_profile.probe_candidates(dict(BOARD, flasher={'name': 'jlink', 'uid': uid}))

    def test_a_cafe_device_sharing_the_probe_serial_is_never_the_probe(self):
        # an Espressif USB-Serial-JTAG and its DUT share the MAC serial
        self.bus([('1-1', 'p', '303a:1001'), ('1-2', 'p', 'cafe:4001')])
        self.assertEqual(self.candidates('P'), ['1-1'])
        self.bus([('1-2', 'p', 'cafe:4001')])
        self.assertEqual(self.candidates('P'), [])

    def test_every_exact_match_is_returned_for_the_engine_to_refuse(self):
        self.bus([('1-2', '123', '1366:0101'), ('1-1', '123', '1366:0101'), ('1-3', '000123', '1366:0101')])
        self.assertEqual(self.candidates('123'), ['1-1', '1-2'])

    def test_a_zero_padded_jlink_serial_matches_only_without_an_exact_one(self):
        self.bus([('1-1', '000681295394', '1366:0101'), ('1-2', '0681295394', '1366:0101')])
        self.assertEqual(self.candidates('681295394'), ['1-1', '1-2'])
        self.assertEqual(self.candidates('0681295394'), ['1-2'])

    def test_an_all_zero_uid_never_matches_by_padding(self):
        self.bus([('1-1', '000000', '1366:0101'), ('1-2', '123456', '1366:0101')])
        self.assertEqual(self.candidates('0'), [])

    def test_scan_only_reads_the_boards_own_usb_state(self):
        patch(self, farm_profile, 'find_device', lambda uid: None)
        self.assertEqual(farm_profile.scan_device(DEVICE), {'device': '⚫ off bus (parked?)', 'notes': []})
        self.assertEqual(farm_profile.scan_device(HOST), {'device': '– n/a (host-only)', 'notes': ['host-only board']})
        patch(self, farm_profile, 'find_device', lambda uid: ('2-1', 'cafe:4001', 7))
        self.assertEqual(farm_profile.scan_device(DEVICE)['device'], '✅ cafe:4001 @2-1')


class HostStatus(unittest.TestCase):
    def status(self, data):
        note = []
        return farm_profile.host_status(data, note), note

    def test_board_test_output_after_an_example_flash_is_a_silent_no_op(self):
        status, note = self.status(b'Hello from TinyUSB\r\nU')
        self.assertEqual(status, 'flash-failed')
        self.assertIn('silent flash no-op', note[0])

    def test_example_output_is_alive(self):
        self.assertEqual(self.status(b'TinyUSB Host Example\r\n')[0], 'ok')

    def test_silence_or_no_port_is_not_alive(self):
        self.assertEqual(self.status(b'')[0], 'failed')
        self.assertEqual(self.status(None)[0], 'failed')


class Plan(unittest.TestCase):
    """plan(): the light image, the park image and their provenance, or a refusal before the lock."""

    def setUp(self):
        td = tempfile.TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.cache = Path(td.name)
        patch(self, farm_profile, 'CACHE_DIR', self.cache)
        patch(self, farm_profile, 'input_problem', lambda board, fw: '')   # PlanInputs reads real trees
        self.args = args()

    def images(self, *examples, variants=None):
        patch(self, hil_flash, 'find_firmware', lambda variant, ex, **kw: (
            f'{variant}/{ex}.elf' if ex in examples and (variants is None or variant in variants) else None))

    def plan(self, board, prepared=None):
        return farm_profile.plan(board, self.args, prepared, False)

    def test_a_device_board_flashes_dfu_runtime_and_parks_with_board_test(self):
        self.images('device/dfu_runtime', 'device/board_test')
        p = self.plan(DEVICE)
        self.assertEqual((p['image']['path'], p['image']['label'], p['final']['image']['path'], p['kind']),
                         ('b/device/dfu_runtime.elf', 'dfu_runtime', 'b/device/board_test.elf', 'device'))
        self.assertIn('cafe device off the bus', p['final']['postcondition'])

    def test_the_park_image_comes_from_the_light_images_variant(self):
        patch(self, hil_flash, 'find_firmware', lambda variant, ex, **kw: (
            f'{variant}/{ex}.elf' if variant == 'b-DMA' or ex == 'device/board_test' else None))
        p = self.plan(dict(DEVICE, variant=[{'name': 'b'}, {'name': 'b-DMA'}]))
        self.assertEqual((p['image']['path'], p['final']['image']['path']),
                         ('b-DMA/device/dfu_runtime.elf', 'b-DMA/device/board_test.elf'))
        self.assertIn('variant: b-DMA', p['notes'])

    def test_a_host_board_takes_the_first_host_candidate(self):
        self.images('host/cdc_msc_hid', 'host/device_info', 'device/board_test')
        p = self.plan(HOST)
        self.assertEqual((p['image']['label'], p['kind'], p['notes']), ('device_info', 'host', ['host-only board']))

    def test_an_only_list_device_board_still_flashes_dfu_runtime(self):
        self.images('device/dfu_runtime', 'device/cdc_dual_ports', 'device/board_test')
        self.assertEqual(self.plan(dict(BOARD, tests={'only': ['device/cdc_dual_ports']}))['image']['label'],
                         'dfu_runtime')

    def test_an_only_list_host_board_is_limited_to_its_own_examples(self):
        self.assertEqual(farm_profile.light_candidates(dict(BOARD, tests={'only': ['host/cdc_msc_hid']})),
                         ('host', ['host/cdc_msc_hid']))
        self.assertEqual(farm_profile.light_candidates(dict(BOARD, tests={'host': True, 'skip': ['host/device_info']})),
                         ('host', ['host/cdc_msc_hid', 'host/msc_file_explorer_freertos']))

    def test_a_skipped_dfu_runtime_leaves_a_device_board_no_light_image(self):
        self.images('device/dfu_runtime', 'device/board_test')
        p = self.plan(dict(BOARD, tests={'device': True, 'skip': ['device/dfu_runtime']}))
        self.assertEqual(p['refused']['status'], 'flash-failed')

    def test_no_park_image_is_refused_before_the_lock(self):
        self.images('device/dfu_runtime')
        p = self.plan(DEVICE)
        self.assertEqual(p['refused']['status'], 'flash-failed')
        self.assertIn('no board_test firmware for b to park with', p['refused']['notes'])

    def test_a_variant_the_fetch_could_not_cache_says_why(self):
        self.images()
        why = 'not cached: no usable artifact in the latest 10 completed master push runs'
        p = self.plan(DEVICE, {'uncached': {'b': why}})
        self.assertEqual(p['refused'], {'status': 'flash-failed', 'notes': [f'b: {why}']})

    def test_an_rtt_host_board_is_unsupported(self):
        self.images('host/device_info', 'device/board_test')
        p = self.plan(dict(HOST, logger='rtt'))
        self.assertEqual(p['refused'], {'status': 'failed', 'notes': ['host-only board', 'RTT host boards unsupported']})

    def test_an_openocd_esp_board_is_refused(self):
        # only hil_test keeps a board locked when the provider leaves a session running
        self.images('device/dfu_runtime', 'device/board_test')
        esp = {'name': 'openocd_esp', 'uid': '30:ED:A0:E0:C9:02', 'args': '-f board/esp32p4-builtin.cfg'}
        p = self.plan(dict(DEVICE, flasher=esp))
        self.assertEqual(p['refused']['status'], 'flash-failed')
        self.assertTrue(any('only hil_test.py' in n for n in p['refused']['notes']), p)

    def test_a_cached_image_carries_its_ci_run(self):
        self.images('device/dfu_runtime', 'device/board_test')
        (self.cache / 'cmake-build-b').mkdir()
        (self.cache / 'cmake-build-b' / '.source').write_text(json.dumps(
            {'run': 7, 'sha': 'abc', 'artifact': 'binaries-arm-gcc--b b', 'conclusion': 'success'}))
        p = self.plan(DEVICE)
        want = {'kind': 'ci', 'repo': 'hathach/tinyusb', 'commit': 'abc', 'run': 7,
                'artifact': 'binaries-arm-gcc--b b', 'conclusion': 'success'}
        self.assertEqual((p['image']['provenance'], p['final']['image']['provenance']), (want, want))

    def test_a_tree_cached_before_conclusions_were_recorded_has_none(self):
        self.images('device/dfu_runtime', 'device/board_test')
        (self.cache / 'cmake-build-b').mkdir()
        (self.cache / 'cmake-build-b' / '.source').write_text(json.dumps({'run': 7, 'sha': 'abc', 'artifact': 'a'}))
        self.assertIsNone(self.plan(DEVICE)['image']['provenance']['conclusion'])

    def test_missing_provenance_is_explicit(self):
        self.images('device/dfu_runtime', 'device/board_test')
        self.assertEqual(self.plan(DEVICE)['image']['provenance']['kind'], 'missing')   # no .source
        self.args.build_dir = 'cmake-build'
        prov = self.plan(DEVICE)['image']['provenance']
        self.assertEqual(prov, {'kind': 'missing', 'reason': 'firmware root cmake-build: no build receipt'})


class PlanInputs(unittest.TestCase):
    """Every input of both images is checked before the lock, from a cached tree or a -B root
    alike: an ESP build's config.env and IDF_TARGET, its flash_args and every file it names."""

    ESP = dict(DEVICE, flasher={'name': 'esptool', 'uid': 'P'})

    def setUp(self):
        td = tempfile.TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.root = Path(td.name)
        patch(self, farm_profile, 'CACHE_DIR', self.root / 'cache')
        patch(self, hil_flash, 'build_dir', hil_flash.build_dir)

    def tree(self, root, flasher='esptool', skip=()):
        """A variant tree of dfu_runtime and board_test; `skip` names (example, file) left out."""
        for ex in ('dfu_runtime', 'board_test'):
            d = root / 'cmake-build-b' / 'device' / ex
            d.mkdir(parents=True)
            files = {f'{ex}.bin': b'app', 'bootloader.bin': b'boot',
                     'config.env': json.dumps({'IDF_TARGET': 'esp32s3'}),
                     'flash_args': f'--flash_mode dio --flash_freq=80m 0x0 bootloader.bin 0x10000 {ex}.bin\n'}
            if flasher != 'esptool':
                files = {f'{ex}.elf': b'app'}
            for name, data in files.items():
                if (ex, name) not in skip:
                    (d / name).write_bytes(data.encode() if isinstance(data, str) else data)
        return root

    def plan(self, board, root, build_dir=None):
        hil_flash.build_dir = str(root)
        return farm_profile.plan(board, args(build_dir=build_dir), None, False)

    def test_complete_trees_pass_from_the_cache_and_from_a_build_root(self):
        cache = self.tree(self.root / 'cache')
        self.assertNotIn('refused', self.plan(self.ESP, cache))
        local = self.tree(self.root / 'local', flasher='openocd')
        self.assertNotIn('refused', self.plan(DEVICE, local, build_dir=str(local)))

    def test_a_missing_or_broken_input_of_either_image_refuses(self):
        cases = {('board_test', 'bootloader.bin'): 'park image: ', ('dfu_runtime', 'bootloader.bin'): 'light image: ',
                 ('board_test', 'config.env'): 'park image: ', ('dfu_runtime', 'flash_args'): 'light image: '}
        for i, (skip, want) in enumerate(cases.items()):
            for where in ('cache', 'local'):
                with self.subTest(skip=skip, where=where):
                    root = self.tree(self.root / f'{where}{i}', skip={skip})
                    got = self.plan(self.ESP, root, build_dir=None if where == 'cache' else str(root))
                    self.assertEqual(got['refused']['status'], 'flash-failed')
                    self.assertTrue(got['refused']['notes'][-1].startswith(want), got)

    def test_a_config_env_without_a_target_and_a_bad_flash_args_refuse(self):
        for name, text in (('config.env', '{}'), ('config.env', '{"IDF_TARGET": ""}'),
                           ('flash_args', '--flash_mode dio 0x0'), ('flash_args', 'zero bootloader.bin')):
            with self.subTest(name=name, text=text):
                root = self.tree(Path(tempfile.mkdtemp(dir=self.root)))
                (root / 'cmake-build-b/device/board_test' / name).write_text(text)
                self.assertIn('park image: ', self.plan(self.ESP, root)['refused']['notes'][-1])


class Fetch(unittest.TestCase):
    """fetch_missing against a fake gh: runs newest first, artifacts that unpack like CI's."""

    def setUp(self):
        td = tempfile.TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.cache = Path(td.name) / 'cache'
        patch(self, farm_profile, 'CACHE_DIR', self.cache)
        self.runs = []          # [(run dict, {artifact name: examples in it})]
        self.downloads = []
        patch(self, farm_profile, 'gh', self.fake_gh)

    def fetch(self, boards):
        return farm_profile.fetch_missing(boards, lambda msg: None)

    def add_run(self, run_id, artifacts, status='completed', age_days=1, event='push', branch='master'):
        created = datetime.now(timezone.utc) - timedelta(days=age_days)
        self.runs.append(({'id': run_id, 'head_sha': f'{run_id:040x}', 'status': status, 'event': event,
                           'conclusion': 'success', 'head_branch': branch, 'created_at': created.isoformat()},
                          artifacts))

    def fake_gh(self, *args, env=None):
        if args[0] == 'api' and '/workflows/' in args[1]:
            page = int(args[1].rsplit('page=', 1)[1])
            return json.dumps({'workflow_runs': [r for r, _ in self.runs][2 * page - 2:2 * page]})
        if args[0] == 'api':
            run_id = int(args[2].split('/runs/')[1].split('/')[0])
            return ''.join(f'{n}\n' for r, arts in self.runs if r['id'] == run_id for n in arts)
        run_id, name, dest = int(args[2]), args[6], Path(args[8])
        self.assertEqual(env['TMPDIR'], str(self.cache))
        self.downloads.append((run_id, name))
        variant = farm_profile.artifact_variant(name)
        for ex in next(arts for r, arts in self.runs if r['id'] == run_id)[name]:
            ex, *sidecars = ex.split('+')    # 'device/x.bin+config.env+flash_args': an ESP image
            fw = dest / f'cmake-build-{variant}' / Path(ex).with_suffix('') / Path(ex).name
            fw = fw if fw.suffix else fw.with_suffix('.elf')
            fw.parent.mkdir(parents=True)
            fw.write_text(f'run {run_id}')
            for f in sidecars:
                (fw.parent / f).write_text({'config.env': '{"IDF_TARGET": "esp32s3"}',
                                            'flash_args': f'0x0 {fw.name}'}[f])
        return ''

    def source(self, variant):
        return json.loads((self.cache / f'cmake-build-{variant}' / '.source').read_text())

    GOOD = ['device/dfu_runtime', 'device/board_test']
    MULTI = dict(DEVICE, variant=[{'name': 'b'}, {'name': 'b-DMA'}])
    OTHER = {'binaries-arm-gcc--b other': GOOD}

    def test_artifact_names_map_to_variants(self):
        v = farm_profile.artifact_variant
        self.assertEqual(v('binaries-arm-gcc--b stm32g0b1nucleo'), 'stm32g0b1nucleo')
        self.assertEqual(v('binaries-esp-idf--b espressif_s3_devkitm --build-name espressif_s3_devkitm-DMA '
                           '--cflag=-DCFG_TUD_DWC2_DMA_ENABLE=1'), 'espressif_s3_devkitm-DMA')
        self.assertEqual(v('binaries-arm-gcc--b ea4088_quickstart -DLOGGER=rtt'), 'ea4088_quickstart')

    def test_a_board_gets_its_first_variant_from_the_newest_run(self):
        self.add_run(3, {'binaries-arm-gcc--b b': self.GOOD, 'binaries-arm-gcc--b b --build-name b-DMA': self.GOOD})
        self.add_run(2, {'binaries-arm-gcc--b b': self.GOOD})
        self.assertEqual(self.fetch([self.MULTI]), {})
        self.assertEqual(self.source('b'), {'run': 3, 'sha': f'{3:040x}', 'artifact': 'binaries-arm-gcc--b b',
                                            'conclusion': 'success'})
        self.assertEqual([p.name for p in self.cache.iterdir()], ['cmake-build-b'])

    def test_a_board_with_any_variant_cached_is_satisfied(self):
        (self.cache / 'cmake-build-b-DMA').mkdir(parents=True)
        patch(self, farm_profile, 'gh', lambda *a, **kw: self.fail('asked GitHub'))
        self.assertEqual(self.fetch([self.MULTI]), {})

    def test_an_unusable_first_variant_falls_back_to_the_next(self):
        self.add_run(3, {'binaries-arm-gcc--b b': ['device/dfu_runtime'],
                         'binaries-arm-gcc--b b --build-name b-DMA': self.GOOD})
        self.assertEqual(self.fetch([self.MULTI]), {})
        self.assertEqual([p.name for p in self.cache.iterdir()], ['cmake-build-b-DMA'])

    def test_the_first_usable_variant_wins_even_with_a_less_preferred_example(self):
        self.add_run(3, {'binaries-arm-gcc--b b': ['host/cdc_msc_hid', 'device/board_test'],
                         'binaries-arm-gcc--b b --build-name b-DMA': ['host/device_info', 'device/board_test']})
        self.assertEqual(self.fetch([dict(self.MULTI, tests={'host': True})]), {})
        self.assertEqual([p.name for p in self.cache.iterdir()], ['cmake-build-b'])

    def test_the_search_stops_after_the_last_budgeted_master_run(self):
        for i in range(farm_profile.MASTER_RUNS):
            self.add_run(100 - i, self.OTHER)
        self.add_run(50, {'binaries-arm-gcc--b b': self.GOOD})
        got = self.fetch([DEVICE])
        self.assertIn(f'latest {farm_profile.MASTER_RUNS} completed master push runs with firmware', got['b'])

    def test_the_last_budgeted_master_run_is_searched(self):
        for i in range(farm_profile.MASTER_RUNS - 1):
            self.add_run(100 - i, self.OTHER)
        self.add_run(50, {'binaries-arm-gcc--b b': self.GOOD})
        self.assertEqual(self.fetch([DEVICE]), {})

    def test_untrusted_or_unfinished_runs_do_not_use_the_budget(self):
        for i in range(farm_profile.MASTER_RUNS):
            self.add_run(200 - i, {'binaries-arm-gcc--b b': self.GOOD}, event='pull_request')
            self.add_run(150 - i, self.OTHER, status='in_progress')
        for i in range(farm_profile.MASTER_RUNS - 1):
            self.add_run(100 - i, self.OTHER)
        self.add_run(50, {'binaries-arm-gcc--b b': self.GOOD})
        self.assertEqual(self.fetch([DEVICE]), {})
        self.assertEqual(self.source('b')['run'], 50)

    def test_runs_without_firmware_do_not_use_the_budget(self):
        for i in range(farm_profile.MASTER_RUNS):
            self.add_run(100 - i, {})    # a push that changed no code: build.yml skips hil-build
        self.add_run(50, {'binaries-arm-gcc--b b': self.GOOD})
        self.assertEqual(self.fetch([DEVICE]), {})
        self.assertEqual(self.source('b')['run'], 50)

    def test_a_run_shifted_onto_the_next_page_is_yielded_once(self):
        self.add_run(3, {})
        self.add_run(2, {})
        self.add_run(1, {})
        r3, r2, r1 = (r for r, _ in self.runs)
        pages = {1: [r3, r2], 2: [r2, r1], 3: []}   # a run created after page 1 pushed r2 down
        patch(self, farm_profile, 'gh', lambda *a, **kw: json.dumps(
            {'workflow_runs': pages[int(a[1].rsplit('page=', 1)[1])]}))
        self.assertEqual([r['id'] for r in farm_profile.master_runs()], [3, 2, 1])

    def test_a_malformed_or_nameless_board_does_not_stop_other_boards(self):
        self.add_run(3, {'binaries-arm-gcc--b b': self.GOOD})
        bad = dict(DEVICE, name='bad', variant=[{'name': ''}])
        nameless = {'uid': 'U', 'flasher': BOARD['flasher'], 'tests': {'device': True}}
        self.assertEqual(self.fetch([bad, nameless, DEVICE]), {})
        self.assertEqual(self.source('b')['run'], 3)

    def test_a_board_left_without_firmware_reports_every_variant(self):
        self.assertEqual(set(self.fetch([self.MULTI])), {'b', 'b-DMA'})

    def test_an_artifact_without_board_test_falls_back_to_an_older_run(self):
        self.add_run(3, {'binaries-arm-gcc--b b': ['device/dfu_runtime']})
        self.add_run(2, {'binaries-arm-gcc--b b': self.GOOD})
        self.assertEqual(self.fetch([DEVICE]), {})
        self.assertEqual(self.source('b')['run'], 2)

    def test_runs_past_the_first_page_are_searched(self):
        self.add_run(9, {})
        self.add_run(8, {})
        self.add_run(7, {'binaries-arm-gcc--b b': self.GOOD})
        self.assertEqual(self.fetch([DEVICE]), {})
        self.assertEqual(self.source('b')['run'], 7)

    def test_an_esp_image_needs_its_flash_sidecars(self):
        esp = dict(DEVICE, flasher={'name': 'esptool', 'uid': 'P'})
        self.add_run(3, {'binaries-esp-idf--b b': ['device/dfu_runtime.bin', 'device/board_test.bin']})
        self.add_run(2, {'binaries-esp-idf--b b': ['device/dfu_runtime.bin+config.env+flash_args',
                                                   'device/board_test.bin+config.env+flash_args']})
        self.assertEqual(self.fetch([esp]), {})
        self.assertEqual(self.source('b')['run'], 2)

    def test_a_failed_download_is_reported_not_skipped(self):
        self.add_run(3, {'binaries-arm-gcc--b b': self.GOOD})
        real = self.fake_gh

        def full_disk(*args, env=None):
            if args[0] == 'run':
                raise OSError(28, 'No space left on device')
            return real(*args, env=env)
        patch(self, farm_profile, 'gh', full_disk)
        self.assertEqual(self.fetch([DEVICE]), {'b': 'not cached, fetch failed: [Errno 28] No space left on device'})

    def test_an_unusable_cache_dir_is_a_reason_not_a_crash(self):
        self.cache.write_text('')    # a file where the cache dir belongs
        self.assertIn('not cached, fetch failed', self.fetch([DEVICE])['b'])

    def test_untrusted_unfinished_or_expired_runs_are_skipped(self):
        self.add_run(6, {'binaries-arm-gcc--b b': self.GOOD}, event='pull_request')
        self.add_run(5, {'binaries-arm-gcc--b b': self.GOOD}, branch='fork-branch')
        self.add_run(4, {'binaries-arm-gcc--b b': self.GOOD}, status='in_progress')
        self.add_run(3, {'binaries-arm-gcc--b b': self.GOOD}, age_days=91)
        self.add_run(2, {'binaries-arm-gcc--b b': self.GOOD}, age_days=92)
        self.assertIn('no usable artifact', self.fetch([DEVICE])['b'])
        self.assertEqual(self.downloads, [])

    def test_losing_the_publish_race_keeps_the_winners_tree(self):
        self.add_run(3, {'binaries-arm-gcc--b b': self.GOOD})
        real = self.fake_gh

        def racing_gh(*args, env=None):
            out = real(*args, env=env)
            if args[0] == 'run':
                winner = self.cache / 'cmake-build-b'
                winner.mkdir()
                (winner / 'winner').write_text('')
            return out
        patch(self, farm_profile, 'gh', racing_gh)
        self.assertEqual(self.fetch([DEVICE]), {})
        self.assertEqual([p.name for p in (self.cache / 'cmake-build-b').iterdir()], ['winner'])
        self.assertEqual([p.name for p in self.cache.iterdir()], ['cmake-build-b'])

    def test_a_gh_failure_is_every_missing_variants_reason(self):
        def broken(*args, env=None):
            raise subprocess.CalledProcessError(4, 'gh', stderr='gh: To get started, run: gh auth login\n')
        patch(self, farm_profile, 'gh', broken)
        self.assertEqual(self.fetch([DEVICE]), {'b': 'not cached, fetch failed: gh: To get started, run: gh auth login'})

    def test_prepare_fetches_only_into_the_default_cache(self):
        for obj, name in ((hil_flash, 'build_dir'), (hil_util, 'verbose')):
            patch(self, obj, name, getattr(obj, name))
        fetched = []
        patch(self, farm_profile, 'fetch_missing', lambda boards, log: fetched.append(boards) or {'b': 'why'})
        self.assertEqual(farm_profile.prepare([DEVICE], args(build_dir='cmake-build'), print), {'uncached': {}})
        self.assertEqual((hil_flash.build_dir, fetched), ('cmake-build', []))
        self.assertEqual(farm_profile.prepare([DEVICE], args(), print), {'uncached': {'b': 'why'}})
        self.assertEqual((hil_flash.build_dir, fetched), (str(self.cache), [[DEVICE]]))


class RunPlumbing(unittest.TestCase):
    """tinyusb's hil_flash adapters take a keyword-only run=, which the profile fills with ctx.run;
    without one they keep hil_util.run_cmd, read per call."""

    def test_a_flash_runs_through_ctx_run_into_a_provider_operation(self):
        ctx = context(self)
        board = dict(DEVICE, flasher={'name': 'lm4flash', 'uid': 'P', 'args': ''})
        got = farm_profile.flash(ctx, board, {'image': {'path': '/fw/dfu_runtime.bin'}})
        (cmd, cwd, timeout, kw), = ctx.calls
        self.assertEqual((cmd, timeout, kw['verbose']), ('lm4flash -s P  /fw/dfu_runtime.bin', hil_util.CMD_TIMEOUT,
                                                         hil_util.verbose))
        self.assertEqual((got.returncode, got.cleanup, len(got.attempts)), (0, 'done', 1))   # a provider Operation
        self.assertEqual([o['step'] for o in ctx.row['operations']], ['test'])

    def test_every_adapter_but_openocd_esp_hands_its_run_to_the_provider(self):
        calls = []

        class Provider:
            def __getattr__(self, name):
                return lambda *a, **kw: calls.append((name, kw['run']))
        patch(self, hil_util, 'provider', Provider)
        patch(self, hil_util, 'get_serial_dev', lambda *a: '/dev/ttyX')
        patch(self, hil_flash, '_idf_target', lambda fw_dir: 'esp32s3')
        mine = object()
        board = {'name': 'b', 'flasher': {'name': 'x', 'uid': 'P', 'args': ''}}
        with tempfile.TemporaryDirectory() as td:
            (Path(td) / 'flash_args').write_text('0x0 fw.bin')
            for name in ('jlink', 'stlink', 'openocd', 'lm4flash', 'pyocd', 'esptool'):
                for fn in (hil_flash.flash_primitive(name), hil_flash.reset_primitive(name)):
                    if fn is None:
                        continue
                    a = (str(Path(td) / 'fw.bin'),) if fn.__name__.startswith('flash_') else ()
                    calls.clear()
                    fn(board, *a, run=mine)
                    fn(board, *a)
                    self.assertEqual(calls, [(fn.__name__, mine), (fn.__name__, hil_util.run_cmd)], fn.__name__)
        with self.assertRaises(TypeError):     # its sessions are the provider's own: no runner to take
            hil_flash.flash_openocd_esp(board, 'fw.bin', run=mine)


class UartWaitsForCompletion(unittest.TestCase):
    """No UART open, poll or write while a run is not proven ended: ctx.settle raises Outstanding,
    the operation stays on the row, and the park is never verified."""

    def setUp(self):
        self.uart = []
        uart = self.uart

        class Console:
            def __init__(self, *a, **kw):
                uart.append('open')

            def reset_input_buffer(self):
                uart.append('flush')

            def write(self, data):
                uart.append('write')

            def read(self, n):
                return b'Hello from TinyUSB\r\n'

            def close(self):
                uart.append('close')
        patch(self, sys.modules['serial'], 'Serial', Console)
        patch(self, hil_util, 'get_serial_dev', lambda *a: '/dev/null')
        patch(self, farm_profile, 'say', lambda msg: None)
        patch(self, farm_profile, 'SERIAL_WAIT', 0.05)   # long enough to poll, not the real 6 s
        reset = lambda board, run=None: run('reset', timeout=1)
        patch(self, hil_flash, 'reset_primitive', lambda name: reset)
        patch(self, hil_flash, 'flash_primitive', lambda name: lambda board, fw, run=None: run(f'flash {fw}', timeout=1))

    def test_a_reset_not_proven_ended_stops_the_observation_before_the_uart_poll(self):
        for cleanup in ('pending', 'unknown'):
            with self.subTest(cleanup=cleanup):
                self.uart.clear()
                ctx = context(self, cleanup)
                with self.assertRaises(eng.Outstanding):
                    farm_profile.observe(ctx, HOST, {'kind': 'host'}, None)
                self.assertEqual(self.uart, ['open', 'flush', 'close'])     # opened before the reset, never polled
                self.assertEqual([(o['step'], o['cleanup']) for o in ctx.row['operations']], [('test', cleanup)])

    def test_a_park_not_proven_ended_is_never_verified_and_never_listened_to(self):
        for cleanup in ('pending', 'unknown'):
            with self.subTest(cleanup=cleanup):
                self.uart.clear()
                ctx = context(self, cleanup)
                with self.assertRaises(eng.Outstanding):
                    farm_profile.final_state(ctx, HOST, {'kind': 'host', 'final': {'image': {'path': 'bt.elf'}}}, None)
                self.assertEqual(self.uart, [])
                self.assertEqual([(o['step'], o['cleanup']) for o in ctx.row['operations']], [('test', cleanup)])

    def test_a_proven_reset_lets_the_observation_listen(self):
        ctx = context(self)
        got = farm_profile.observe(ctx, HOST, {'kind': 'host'}, None)
        self.assertEqual((got['status'], self.uart[:3]), ('flash-failed', ['open', 'flush', 'write']))
        self.assertEqual(len(ctx.row['operations']), 1)


class Observe(unittest.TestCase):
    def setUp(self):
        patch(self, farm_profile, 'ENUM_WAIT', 0.01)
        patch(self, farm_profile.time, 'sleep', lambda s: None)

    def device(self, before, after):
        patch(self, farm_profile, 'find_device', lambda uid: after)
        return farm_profile.observe(context(self), DEVICE, {'kind': 'device'}, before)

    def test_a_device_that_re_enumerates_is_ok(self):
        got = self.device(None, ('1-1', 'cafe:4001', 2))
        self.assertEqual((got['status'], got['device'], got['unverified']), ('ok', '✅ cafe:4001', []))

    def test_the_same_vidpid_before_the_flash_leaves_the_new_image_unverified(self):
        got = self.device(('1-1', 'cafe:4001', 1), ('1-1', 'cafe:4001', 2))
        self.assertEqual(got['status'], 'ok')
        self.assertIn('new image unverified', got['unverified'][0])

    def test_a_device_that_does_not_re_enumerate_fails(self):
        got = self.device(('1-1', 'cafe:4001', 1), ('1-1', 'cafe:4001', 1))
        self.assertEqual((got['status'], got['device']), ('failed', '❌ not enumerated'))

    def test_board_test_output_after_a_host_flash_is_flash_failed(self):
        patch(self, farm_profile, 'check_host_serial', lambda ctx, board, **kw: b'Hello from TinyUSB\r\n')
        got = farm_profile.observe(context(self), HOST, {'kind': 'host'}, None)
        self.assertEqual((got['status'], got['device']), ('flash-failed', '❌ no serial out'))


class Park(unittest.TestCase):
    """final_state: verified only where the park is seen to take."""

    PLAN = {'final': {'image': {'path': 'board_test.elf'}}}

    def setUp(self):
        patch(self, farm_profile.time, 'sleep', lambda s: None)
        clock = iter(range(0, 1000, 5))
        patch(self, farm_profile.time, 'monotonic', lambda: next(clock))

    def flasher(self, then=None, rc=0):
        def flash(board, fw, run=None):
            if then:
                then()
            return op(rc, 'Error: no target')
        patch(self, hil_flash, 'flash_primitive', lambda name: flash)

    def park(self, on_bus_before=True, on_bus_after=False, flash_rc=0):
        state = {'on_bus': on_bus_before}
        patch(self, farm_profile, 'find_device', lambda uid: ('1-1', 'cafe:4001', 1) if state['on_bus'] else None)
        self.flasher(lambda: state.update(on_bus=on_bus_after), flash_rc)
        self.ctx = context(self)
        return farm_profile.final_state(self.ctx, DEVICE, dict(self.PLAN, kind='device'), None)

    def test_a_device_that_leaves_the_bus_is_parked(self):
        got = self.park()
        self.assertEqual((got['verified'], got['notes']), (True, []))
        self.assertEqual(len(self.ctx.row['operations']), 1)

    def test_a_device_still_enumerated_is_unverified(self):
        got = self.park(on_bus_after=True)
        self.assertEqual((got['verified'], got['notes']), (False, ['park unverified: device still enumerated']))

    def test_a_device_already_off_the_bus_cannot_prove_the_park(self):
        got = self.park(on_bus_before=False)
        self.assertEqual((got['verified'], got['notes']), (False, ['park unverified (device already off bus)']))

    def test_a_failed_park_flash_is_unverified_with_its_error(self):
        got = self.park(flash_rc=1)
        self.assertEqual((got['verified'], got['notes']), (False, ['park flash failed: Error: no target']))
        self.assertEqual(len(self.ctx.row['operations']), 1)

    def park_host(self, hello):
        self.flasher()
        patch(self, farm_profile, 'check_host_serial', lambda ctx, board, **kw: hello)
        return farm_profile.final_state(context(self), HOST, dict(self.PLAN, kind='host'), None)

    def test_a_host_park_must_say_hello(self):
        self.assertTrue(self.park_host(b'stale\r\nHello from TinyUSB\r\n')['verified'])
        # also a CI_BUILD board_test, which never says hello: unverified, the board stays blocked
        got = self.park_host(b'')
        self.assertEqual((got['verified'], got['notes']), (False, ['park unverified: no board_test output']))

    def test_a_park_operation_survives_a_later_exception(self):
        self.flasher()

        def broken(ctx, board, **kw):
            raise OSError('bridge gone')
        patch(self, farm_profile, 'check_host_serial', broken)
        ctx = context(self)
        with self.assertRaises(OSError):
            farm_profile.final_state(ctx, HOST, dict(self.PLAN, kind='host'), None)
        self.assertEqual(len(ctx.row['operations']), 1)


class Launcher(unittest.TestCase):
    def test_the_engine_comes_from_lib_agentrc_at_the_pin(self):
        self.assertEqual(Path(eng.__file__), hil_util.TINYUSB_ROOT / 'lib/agentrc/skills/hil/scripts/hil_farm_check.py')
        self.assertEqual(eng.REASON, 'farm_check')
        self.assertIsNone(eng._lock_api_problem(hil_lock))

    def test_the_profile_and_lock_module_are_passed_and_the_exit_status_passed_through(self):
        for code in (0, 1, 2, 4):
            calls = []
            fake = types.SimpleNamespace(main=lambda profile, argv, lock_module: calls.append(
                (profile, argv, lock_module)) or code)
            patch(self, hil_farm_check, 'load_engine', lambda: fake)
            self.assertEqual(hil_farm_check.main(['--scan-only']), code)
            self.assertEqual(calls, [(farm_profile, ['--scan-only'], hil_lock)])

    def test_an_engine_that_cannot_load_refuses(self):
        def missing():
            raise ImportError('lib/agentrc must be at X')
        patch(self, hil_farm_check, 'load_engine', missing)
        code, out, err = capture(lambda: hil_farm_check.main([]))
        self.assertEqual((code, out), (2, ''))
        self.assertIn('lib/agentrc must be at X', err)

    def test_the_load_requires_the_engines_closure(self):
        seen = []
        patch(self, hil_farm_check.agentrc, 'load', lambda rel, name, requires=(): seen.append((rel, requires)))
        hil_farm_check.load_engine()
        (rel, requires), = seen
        self.assertEqual(rel, 'skills/hil/scripts/hil_farm_check.py')
        # every module the engine _load()s beside it, transitively; the processes it launches
        # (itself as the cleanup holder, hil_process as run_tracked's supervisor) are among them
        import ast
        dep = hil_util.TINYUSB_ROOT / 'lib/agentrc'
        closure, todo = set(), [rel]
        while todo:
            for node in ast.walk(ast.parse((dep / todo.pop()).read_text())):
                if isinstance(node, ast.Call) and ast.unparse(node.func) == '_load':
                    f = f'{Path(rel).parent}/{node.args[0].value}.py'
                    if f not in closure:
                        closure.add(f)
                        todo.append(f)
        self.assertIn('skills/hil/scripts/hil_process.py', closure, 'the walk went vacuous')
        self.assertEqual(set(requires), closure)


class EngineFixture(unittest.TestCase):
    """The real engine with this profile and tinyusb's own hil_lock, its lock dir and the state
    dir in a temp dir, a -B firmware root, the bus faked by what each flash leaves on it."""

    def setUp(self):
        td = tempfile.TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.d = Path(td.name)
        self.state = self.d / 'state'
        self.state.mkdir(mode=0o700)
        fw = self.d / 'fw' / 'cmake-build-b'
        for ex in ('device/dfu_runtime', 'device/board_test', 'host/device_info'):
            (fw / ex).mkdir(parents=True)
            (fw / ex / f'{Path(ex).name}.bin').write_bytes(ex.encode())
        self.cfg = self.d / 'roster.json'
        self.board = dict(DEVICE, flasher={'name': 'lm4flash', 'uid': 'P', 'args': ''})
        self.roster(self.board)
        patch(self, hil_lock, 'BOARD_LOCK_DIR', str(self.d / 'locks'))
        patch(self, eng, 'RECHECK_WAIT', 0)
        for obj, name in ((hil_flash, 'build_dir'), (hil_util, 'verbose')):
            patch(self, obj, name, getattr(obj, name))
        patch(self, farm_profile, 'ENUM_WAIT', 0.01)
        patch(self, farm_profile, 'PARK_WAIT', 0.01)
        patch(self, farm_profile.time, 'sleep', lambda s: None)
        patch(self, farm_profile, 'scan_usb', lambda: {'1-1': {'serial': 'p', 'vidpid': '1cbe:00fd'}})
        patch(self, farm_profile, 'fetch_missing', lambda *a: self.fail('fetched'))

    def roster(self, board):
        self.cfg.write_text(json.dumps({'boards': [board]}))

    def run_engine(self, *argv, host='htpc'):
        code, out, _ = capture(lambda: eng.main(farm_profile, ['--json', '-B', str(self.d / 'fw'), *argv, str(self.cfg)],
                                                state_dir=str(self.state), host=host, lock_module=hil_lock))
        return code, json.loads(out)


class EngineWithProfile(EngineFixture):
    """run_tracked faked."""

    def setUp(self):
        super().setUp()
        patch(self, eng.hil_process, 'run_tracked', self.tracked)
        patch(self, farm_profile, 'find_device', lambda uid: self.device)
        self.device, self.ran, self.park_leaves = None, [], None

    def tracked(self, cmd, cwd=None, *, timeout, settle, state_dir, **kw):
        self.assertEqual(state_dir, str(self.state))
        self.ran.append(cmd.rsplit('/', 1)[-1])
        self.device = ('1-1', 'cafe:4001', len(self.ran)) if self.ran[-1] == 'dfu_runtime.bin' else self.park_leaves
        r = subprocess.CompletedProcess(cmd, 0, '', None)
        r.cleanup, r.receipt = 'done', None
        return r

    def test_a_board_that_comes_back_and_parks_is_ok_and_released(self):
        code, doc = self.run_engine()
        r, = doc['rows']
        self.assertEqual((code, r['status'], r['coverage'], self.ran), (0, 'ok', 'full-attempted',
                                                                        ['dfu_runtime.bin', 'board_test.bin']))
        self.assertEqual((r['verification']['result'], r['cleanup']['result'], r['cleanup']['released']),
                         ('verified', 'verified', True))
        self.assertEqual([o['step'] for o in r['operations']], ['flash', 'final'])
        self.assertEqual(r['image']['provenance']['kind'], 'missing')
        self.assertIsNone(hil_lock.read_record('b'))   # no record: free

    def test_an_unverified_park_leaves_the_board_blocked(self):
        self.park_leaves = ('1-1', 'cafe:4001', 9)    # still enumerated after board_test
        code, doc = self.run_engine()
        r, = doc['rows']
        self.assertEqual((code, r['status'], r['check_status'], r['coverage']), (4, 'blocked', 'ok', 'blocked-after'))
        self.assertEqual(r['cleanup']['blocker'], 'stale-record')
        rec = hil_lock.read_record('b')
        self.assertEqual((rec['reason'], rec['protected']), ('farm_check', True))

    def test_operations_survive_a_later_exception_in_each_step(self):
        """A host board whose UART raises after the reset, then after the park flash: the reset
        and the park flash stay on the row."""
        self.roster(dict(self.board, tests={'host': True}))
        reset = lambda board, run=None: run('reset', timeout=1)
        patch(self, hil_flash, 'reset_primitive', lambda name: reset)
        patch(self, hil_util, 'get_serial_dev', lambda *a: '/dev/null')

        class Console:
            def __init__(self, *a, **kw):
                pass

            def reset_input_buffer(self):
                pass

            def write(self, data):
                raise OSError('bridge gone')

            def close(self):
                pass
        patch(self, sys.modules['serial'], 'Serial', Console)
        code, doc = self.run_engine()
        r, = doc['rows']
        self.assertEqual(self.ran, ['device_info.bin', 'reset', 'board_test.bin'])
        self.assertEqual([o['step'] for o in r['operations']], ['flash', 'observe', 'final'])
        self.assertEqual((code, r['status'], r['cleanup']['result']), (4, 'blocked', 'error'))

    def test_an_unknown_host_is_refused_before_any_board(self):
        code, out, err = capture(lambda: eng.main(farm_profile, ['--json'], state_dir=str(self.state), host='htpc',
                                                  lock_module=hil_lock))
        self.assertEqual((code, out, self.ran), (2, '', []))
        self.assertIn('host htpc has no roster here', err)

    def test_scan_only_excludes_the_cafe_device_and_takes_no_lock(self):
        patch(self, farm_profile, 'scan_usb', lambda: {'1-1': {'serial': 'p', 'vidpid': 'cafe:4001'}})
        patch(self, hil_lock, 'take_board_lock', lambda *a, **kw: self.fail('locked'))
        code, doc = self.run_engine('--scan-only')
        r, = doc['rows']
        self.assertEqual((code, r['status'], r['probe'], r['coverage'], self.ran),
                         (1, 'flash-failed', '❌ missing', 'probe-only', []))


class RealProcessChain(EngineFixture):
    """profile -> tinyusb's hil_flash adapter -> agentrc's provider -> the real run_tracked: a
    fake lm4flash on PATH logs what it flashed, and the bus follows that log."""

    def setUp(self):
        super().setUp()
        bin_dir = self.d / 'bin'
        bin_dir.mkdir()
        self.log = self.d / 'flashed.log'
        flasher = bin_dir / 'lm4flash'
        flasher.write_text(f'#!/bin/sh\nfor a; do last=$a; done\necho "$last" >> {self.log}\n')
        flasher.chmod(0o755)
        env = mock.patch.dict(os.environ, PATH=f'{bin_dir}{os.pathsep}{os.environ["PATH"]}')
        env.start()
        self.addCleanup(env.stop)
        patch(self, farm_profile, 'find_device', self.device)

    def flashed(self):
        return self.log.read_text().split() if self.log.exists() else []

    def device(self, uid):
        """The light image enumerates (a new inode per flash); board_test never does."""
        f = self.flashed()
        return ('1-1', 'cafe:4001', len(f)) if f and f[-1].endswith('dfu_runtime.bin') else None

    def test_a_real_flasher_process_through_the_tracked_runner_ends_verified_and_released(self):
        code, doc = self.run_engine()
        r, = doc['rows']
        self.assertEqual([Path(f).name for f in self.flashed()], ['dfu_runtime.bin', 'board_test.bin'])
        self.assertEqual((code, r['status'], r['cleanup']['result'], r['cleanup']['released']), (0, 'ok', 'verified', True))
        self.assertEqual([(o['step'], o['cleanup']) for o in r['operations']], [('flash', 'done'), ('final', 'done')])
        self.assertIsNone(hil_lock.read_record('b'))
        self.assertEqual(sorted(p.name for p in self.state.iterdir()), [])   # proven receipts are tidied


if __name__ == '__main__':
    unittest.main()
