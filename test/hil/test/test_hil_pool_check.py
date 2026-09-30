#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# hil_pool_check's verdicts with the bus, the flasher and the lock dir faked: no board is
# touched. Run directly:
#   python3 test/hil/test/test_hil_pool_check.py
import json
import os
import subprocess
import sys
import tempfile
import types
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from helper import hil_lock, hil_pool_check   # noqa: E402
from usbtest_harness import patch               # noqa: E402

BOARD = {'name': 'b', 'uid': 'U', 'flasher': {'name': 'openocd', 'uid': 'P'}}


class Lock(unittest.TestCase):
    def setUp(self):
        td = tempfile.TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.dir = td.name
        patch(self, hil_lock, 'BOARD_LOCK_DIR', self.dir)

    def test_a_held_board_reports_its_holder(self):
        held = hil_lock.flock_nb('b')
        self.addCleanup(held.close)
        hil_lock.write_record(held, 'hil_test.py')
        got = hil_pool_check.lock_board('b')
        self.assertIsInstance(got, str)
        self.assertIn('hil_test.py', got)
        self.assertFalse(got.startswith('ERROR:'))

    def test_an_unopenable_lock_file_is_an_error_not_a_holder(self):
        os.mkdir(os.path.join(self.dir, 'b.lock'))   # open(O_RDWR) on a dir: EISDIR
        got = hil_pool_check.lock_board('b')
        self.assertTrue(got.startswith('ERROR:'), got)

    def test_a_free_board_is_taken_under_the_protected_reason(self):
        fh = hil_pool_check.lock_board('b')
        self.addCleanup(hil_pool_check.unlock_board, fh)
        self.assertEqual(hil_lock.read_record('b')['reason'], 'pool_check')
        self.assertIn('pool_check', hil_lock.PROTECTED_REASONS)


class HostStatus(unittest.TestCase):
    def status(self, data):
        patch(self, hil_pool_check, 'check_host_serial', lambda board: data)
        note = []
        return hil_pool_check.host_status(BOARD, note), note

    def test_board_test_output_after_an_example_flash_is_a_silent_no_op(self):
        status, note = self.status(b'Hello from TinyUSB\r\nU')
        self.assertEqual(status, 'flash-failed')
        self.assertIn('silent flash no-op', note[0])

    def test_example_output_is_alive(self):
        self.assertEqual(self.status(b'TinyUSB Host Example\r\n')[0], 'ok')

    def test_silence_is_not_alive(self):
        self.assertEqual(self.status(b'')[0], 'failed')


class CheckBoard(unittest.TestCase):
    """check_board up to the flash, with the bus, the lock and the flasher faked."""

    def setUp(self):
        self.flashed = []
        patch(self, hil_pool_check, 'say', lambda msg: None)
        patch(self, hil_pool_check, 'find_usb', lambda uid: '1-1')
        patch(self, hil_pool_check, 'lock_board', lambda name: types.SimpleNamespace())
        patch(self, hil_pool_check, 'unlock_board', lambda fh: None)
        patch(self, hil_pool_check, 'find_device', lambda uid: None)
        patch(self, hil_pool_check, 'flash', lambda board, fw: self.flashed.append(fw) or '')
        patch(self, hil_pool_check, 'host_status', lambda *a: 'ok')
        patch(self, hil_pool_check, 'wait_device', lambda *a: ('1-1', 'cafe:4001', 2))
        self.args = types.SimpleNamespace(scan_only=False, no_park=False, uncached={})

    def images(self, *examples):
        patch(self, hil_pool_check.hil_flash, 'find_firmware', lambda variant, ex, **kw: (
            f'{variant}/{ex}.elf' if ex in examples else None))

    def test_a_host_board_under_no_park_parks_nothing(self):
        self.images('host/device_info')
        patch(self, hil_pool_check, 'park_board', lambda *a: self.fail('--no-park parks nothing'))
        self.args.no_park = True
        row = hil_pool_check.check_board(dict(BOARD, tests={'host': True}), self.args)
        self.assertEqual(row['status'], 'ok')

    def test_the_park_image_comes_from_the_light_images_variant(self):
        parked = []
        patch(self, hil_pool_check, 'park_board', lambda board, kind, fw, row, note: parked.append(fw))
        patch(self, hil_pool_check.hil_flash, 'find_firmware', lambda variant, ex, **kw: (
            f'{variant}/{ex}.elf' if variant == 'b-DMA' or ex == 'device/board_test' else None))
        board = dict(BOARD, tests={'device': True}, variant=[{'name': 'b'}, {'name': 'b-DMA'}])
        row = hil_pool_check.check_board(board, self.args)
        self.assertEqual((row['status'], self.flashed, parked),
                         ('ok', ['b-DMA/device/dfu_runtime.elf'], ['b-DMA/device/board_test.elf']))

    def test_a_device_that_does_not_re_enumerate_fails(self):
        self.images('device/dfu_runtime', 'device/board_test')
        patch(self, hil_pool_check, 'park_board', lambda *a: None)
        patch(self, hil_pool_check, 'wait_device', lambda *a: None)
        row = hil_pool_check.check_board(dict(BOARD, tests={'device': True}), self.args)
        self.assertEqual((row['status'], row['device']), ('failed', '❌ not enumerated'))

    def test_a_failed_flash_is_flash_failed_and_still_parks(self):
        self.images('device/dfu_runtime', 'device/board_test')
        parked = []
        patch(self, hil_pool_check, 'park_board', lambda *a: parked.append(a[2]))
        patch(self, hil_pool_check, 'flash', lambda board, fw: 'Error: no target')
        row = hil_pool_check.check_board(dict(BOARD, tests={'device': True}), self.args)
        self.assertEqual((row['status'], row['note'][-1]), ('flash-failed', 'flash: Error: no target'))
        self.assertEqual(parked, ['b/device/board_test.elf'])

    def test_no_park_image_fails_before_flashing(self):
        self.images('device/dfu_runtime')
        row = hil_pool_check.check_board(dict(BOARD, tests={'device': True}), self.args)
        self.assertEqual((row['status'], self.flashed), ('flash-failed', []))
        self.assertIn('no board_test firmware', row['note'][-1])

    def test_no_light_image_fails_before_flashing(self):
        self.images('device/board_test')
        row = hil_pool_check.check_board(dict(BOARD, tests={'device': True}), self.args)
        self.assertEqual((row['status'], self.flashed), ('flash-failed', []))

    def test_a_variant_the_fetch_could_not_cache_says_why(self):
        self.images()
        self.args.uncached = {'b': 'not cached: no usable artifact in the latest 10 completed master push runs'}
        row = hil_pool_check.check_board(dict(BOARD, tests={'device': True}), self.args)
        self.assertEqual(row['note'][-1], 'b: not cached: no usable artifact in the latest 10 completed master push runs')

    def test_a_malformed_variant_list_is_the_boards_own_error_row(self):
        row = hil_pool_check.check_board_safe(dict(BOARD, tests={'device': True}, variant=[{'name': ''}]), self.args)
        self.assertEqual((row['status'], row['device'], self.flashed), ('failed', '❌ error', []))
        self.assertIn('ValueError', row['note'][0])

    def test_an_rtt_host_board_is_unsupported_and_untouched(self):
        self.images('host/device_info', 'device/board_test')
        row = hil_pool_check.check_board(dict(BOARD, tests={'host': True}, logger='rtt'), self.args)
        self.assertEqual((row['status'], self.flashed), ('failed', []))
        self.assertIn('RTT host boards unsupported', row['note'])

    def test_an_only_list_board_gets_one_of_its_own_examples(self):
        self.images('device/dfu_runtime', 'device/cdc_dual_ports', 'device/board_test')
        board = dict(BOARD, tests={'only': ['device/cdc_dual_ports']})
        patch(self, hil_pool_check, 'park_board', lambda *a: None)
        hil_pool_check.check_board(board, self.args)
        self.assertEqual(self.flashed, ['b/device/cdc_dual_ports.elf'])


class Fetch(unittest.TestCase):
    """fetch_missing against a fake gh: runs newest first, artifacts that unpack like CI's."""

    def setUp(self):
        td = tempfile.TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.cache = Path(td.name) / 'cache'
        patch(self, hil_pool_check, 'CACHE_DIR', self.cache)
        patch(self, hil_pool_check, 'say', lambda msg: None)
        self.runs = []          # [(run dict, {artifact name: examples in it})]
        self.downloads = []
        patch(self, hil_pool_check, 'gh', self.fake_gh)

    def add_run(self, run_id, artifacts, status='completed', age_days=1, event='push', branch='master'):
        created = datetime.now(timezone.utc) - timedelta(days=age_days)
        self.runs.append(({'id': run_id, 'head_sha': f'{run_id:040x}', 'status': status, 'event': event,
                           'head_branch': branch, 'created_at': created.isoformat()}, artifacts))

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
        variant = hil_pool_check.artifact_variant(name)
        for ex in next(arts for r, arts in self.runs if r['id'] == run_id)[name]:
            ex, *sidecars = ex.split('+')    # 'device/x.bin+config.env+flash_args': an ESP image
            fw = dest / f'cmake-build-{variant}' / Path(ex).with_suffix('') / Path(ex).name
            fw = fw if fw.suffix else fw.with_suffix('.elf')
            fw.parent.mkdir(parents=True)
            fw.write_text(f'run {run_id}')
            for f in sidecars:
                (fw.parent / f).write_text('')
        return ''

    def source(self, variant):
        return json.loads((self.cache / f'cmake-build-{variant}' / '.source').read_text())

    GOOD = ['device/dfu_runtime', 'device/board_test']

    def test_artifact_names_map_to_variants(self):
        v = hil_pool_check.artifact_variant
        self.assertEqual(v('binaries-arm-gcc--b stm32g0b1nucleo'), 'stm32g0b1nucleo')
        self.assertEqual(v('binaries-esp-idf--b espressif_s3_devkitm --build-name espressif_s3_devkitm-DMA '
                           '--cflag=-DCFG_TUD_DWC2_DMA_ENABLE=1'), 'espressif_s3_devkitm-DMA')
        self.assertEqual(v('binaries-arm-gcc--b ea4088_quickstart -DLOGGER=rtt'), 'ea4088_quickstart')

    MULTI = dict(BOARD, tests={'device': True}, variant=[{'name': 'b'}, {'name': 'b-DMA'}])

    def test_a_board_gets_its_first_variant_from_the_newest_run(self):
        self.add_run(3, {'binaries-arm-gcc--b b': self.GOOD, 'binaries-arm-gcc--b b --build-name b-DMA': self.GOOD})
        self.add_run(2, {'binaries-arm-gcc--b b': self.GOOD})
        self.assertEqual(hil_pool_check.fetch_missing([self.MULTI]), {})
        self.assertEqual(self.source('b')['run'], 3)
        self.assertEqual([p.name for p in self.cache.iterdir()], ['cmake-build-b'])

    def test_a_board_with_any_variant_cached_is_satisfied(self):
        (self.cache / 'cmake-build-b-DMA').mkdir(parents=True)
        patch(self, hil_pool_check, 'gh', lambda *a, **kw: self.fail('asked GitHub'))
        self.assertEqual(hil_pool_check.fetch_missing([self.MULTI]), {})

    def test_an_unusable_first_variant_falls_back_to_the_next(self):
        self.add_run(3, {'binaries-arm-gcc--b b': ['device/dfu_runtime'],
                         'binaries-arm-gcc--b b --build-name b-DMA': self.GOOD})
        self.assertEqual(hil_pool_check.fetch_missing([self.MULTI]), {})
        self.assertEqual([p.name for p in self.cache.iterdir()], ['cmake-build-b-DMA'])

    def test_the_first_usable_variant_wins_even_with_a_less_preferred_example(self):
        # fetch stops at variant b's cdc_msc, where a full cache would flash b-DMA's dfu_runtime
        self.add_run(3, {'binaries-arm-gcc--b b': ['device/cdc_msc', 'device/board_test'],
                         'binaries-arm-gcc--b b --build-name b-DMA': self.GOOD})
        self.assertEqual(hil_pool_check.fetch_missing([self.MULTI]), {})
        self.assertEqual([p.name for p in self.cache.iterdir()], ['cmake-build-b'])

    OTHER = {'binaries-arm-gcc--b other': GOOD}

    def test_the_search_stops_after_the_last_budgeted_master_run(self):
        for i in range(hil_pool_check.MASTER_RUNS):
            self.add_run(100 - i, self.OTHER)
        self.add_run(50, {'binaries-arm-gcc--b b': self.GOOD})
        got = hil_pool_check.fetch_missing([dict(BOARD, tests={'device': True})])
        self.assertIn(f'latest {hil_pool_check.MASTER_RUNS} completed master push runs with firmware', got['b'])

    def test_the_last_budgeted_master_run_is_searched(self):
        for i in range(hil_pool_check.MASTER_RUNS - 1):
            self.add_run(100 - i, self.OTHER)
        self.add_run(50, {'binaries-arm-gcc--b b': self.GOOD})
        self.assertEqual(hil_pool_check.fetch_missing([dict(BOARD, tests={'device': True})]), {})

    def test_untrusted_or_unfinished_runs_do_not_use_the_budget(self):
        for i in range(hil_pool_check.MASTER_RUNS):
            self.add_run(200 - i, {'binaries-arm-gcc--b b': self.GOOD}, event='pull_request')
            self.add_run(150 - i, self.OTHER, status='in_progress')
        for i in range(hil_pool_check.MASTER_RUNS - 1):
            self.add_run(100 - i, self.OTHER)
        self.add_run(50, {'binaries-arm-gcc--b b': self.GOOD})
        self.assertEqual(hil_pool_check.fetch_missing([dict(BOARD, tests={'device': True})]), {})
        self.assertEqual(self.source('b')['run'], 50)

    def test_runs_without_firmware_do_not_use_the_budget(self):
        for i in range(hil_pool_check.MASTER_RUNS):
            self.add_run(100 - i, {})    # a push that changed no code: build.yml skips hil-build
        self.add_run(50, {'binaries-arm-gcc--b b': self.GOOD})
        self.assertEqual(hil_pool_check.fetch_missing([dict(BOARD, tests={'device': True})]), {})
        self.assertEqual(self.source('b')['run'], 50)

    def test_a_run_shifted_onto_the_next_page_is_yielded_once(self):
        self.add_run(3, {})
        self.add_run(2, {})
        self.add_run(1, {})
        r3, r2, r1 = (r for r, _ in self.runs)
        pages = {1: [r3, r2], 2: [r2, r1], 3: []}   # a run created after page 1 pushed r2 down
        patch(self, hil_pool_check, 'gh', lambda *a, **kw: json.dumps(
            {'workflow_runs': pages[int(a[1].rsplit('page=', 1)[1])]}))
        self.assertEqual([r['id'] for r in hil_pool_check.master_runs()], [3, 2, 1])

    def test_a_malformed_variant_list_does_not_stop_other_boards(self):
        self.add_run(3, {'binaries-arm-gcc--b b': self.GOOD})
        bad = dict(BOARD, name='bad', tests={'device': True}, variant=[{'name': ''}])
        self.assertEqual(hil_pool_check.fetch_missing([bad, dict(BOARD, tests={'device': True})]), {})
        self.assertEqual(self.source('b')['run'], 3)

    def test_a_board_without_a_name_does_not_stop_other_boards(self):
        self.add_run(3, {'binaries-arm-gcc--b b': self.GOOD})
        nameless = {'uid': 'U', 'flasher': BOARD['flasher'], 'tests': {'device': True}}
        self.assertEqual(hil_pool_check.fetch_missing([nameless, dict(BOARD, tests={'device': True})]), {})
        self.assertEqual(self.source('b')['run'], 3)

    def test_a_board_left_without_firmware_reports_every_variant(self):
        got = hil_pool_check.fetch_missing([self.MULTI])
        self.assertEqual(set(got), {'b', 'b-DMA'})

    def test_an_artifact_without_board_test_falls_back_to_an_older_run(self):
        self.add_run(3, {'binaries-arm-gcc--b b': ['device/dfu_runtime']})
        self.add_run(2, {'binaries-arm-gcc--b b': self.GOOD})
        self.assertEqual(hil_pool_check.fetch_missing([dict(BOARD, tests={'device': True})]), {})
        self.assertEqual(self.source('b')['run'], 2)

    def test_runs_past_the_first_page_are_searched(self):
        self.add_run(9, {})
        self.add_run(8, {})
        self.add_run(7, {'binaries-arm-gcc--b b': self.GOOD})
        self.assertEqual(hil_pool_check.fetch_missing([dict(BOARD, tests={'device': True})]), {})
        self.assertEqual(self.source('b')['run'], 7)

    def test_an_esp_image_needs_its_flash_sidecars(self):
        esp = dict(BOARD, tests={'device': True}, flasher={'name': 'esptool', 'uid': 'P'})
        self.add_run(3, {'binaries-esp-idf--b b': ['device/dfu_runtime.bin', 'device/board_test.bin']})
        self.add_run(2, {'binaries-esp-idf--b b': ['device/dfu_runtime.bin+config.env+flash_args',
                                                   'device/board_test.bin+config.env+flash_args']})
        self.assertEqual(hil_pool_check.fetch_missing([esp]), {})
        self.assertEqual(self.source('b')['run'], 2)

    def test_a_failed_download_is_reported_not_skipped(self):
        self.add_run(3, {'binaries-arm-gcc--b b': self.GOOD})
        self.add_run(2, {'binaries-arm-gcc--b b': self.GOOD})
        real = self.fake_gh

        def full_disk(*args, env=None):
            if args[0] == 'run':
                raise OSError(28, 'No space left on device')
            return real(*args, env=env)
        patch(self, hil_pool_check, 'gh', full_disk)
        got = hil_pool_check.fetch_missing([dict(BOARD, tests={'device': True})])
        self.assertEqual(got, {'b': 'not cached, fetch failed: [Errno 28] No space left on device'})

    def test_an_unusable_cache_dir_is_a_reason_not_a_crash(self):
        self.cache.write_text('')    # a file where the cache dir belongs
        got = hil_pool_check.fetch_missing([dict(BOARD, tests={'device': True})])
        self.assertIn('not cached, fetch failed', got['b'])

    def test_untrusted_unfinished_or_expired_runs_are_skipped(self):
        self.add_run(6, {'binaries-arm-gcc--b b': self.GOOD}, event='pull_request')
        self.add_run(5, {'binaries-arm-gcc--b b': self.GOOD}, branch='fork-branch')
        self.add_run(4, {'binaries-arm-gcc--b b': self.GOOD}, status='in_progress')
        self.add_run(3, {'binaries-arm-gcc--b b': self.GOOD}, age_days=91)
        self.add_run(2, {'binaries-arm-gcc--b b': self.GOOD}, age_days=92)
        got = hil_pool_check.fetch_missing([dict(BOARD, tests={'device': True})])
        self.assertIn('no usable artifact', got['b'])
        self.assertEqual(self.downloads, [])

    def test_a_cached_variant_is_never_refetched(self):
        (self.cache / 'cmake-build-b').mkdir(parents=True)
        patch(self, hil_pool_check, 'gh', lambda *a, **kw: self.fail('asked GitHub'))
        self.assertEqual(hil_pool_check.fetch_missing([dict(BOARD, tests={'device': True})]), {})

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
        patch(self, hil_pool_check, 'gh', racing_gh)
        self.assertEqual(hil_pool_check.fetch_missing([dict(BOARD, tests={'device': True})]), {})
        self.assertEqual([p.name for p in (self.cache / 'cmake-build-b').iterdir()], ['winner'])
        self.assertEqual([p.name for p in self.cache.iterdir()], ['cmake-build-b'])

    def test_a_gh_failure_is_every_missing_variants_reason(self):
        def broken(*args, env=None):
            raise subprocess.CalledProcessError(4, 'gh', stderr='gh: To get started, run: gh auth login\n')
        patch(self, hil_pool_check, 'gh', broken)
        got = hil_pool_check.fetch_missing([dict(BOARD, tests={'device': True})])
        self.assertEqual(got, {'b': 'not cached, fetch failed: gh: To get started, run: gh auth login'})


class Park(unittest.TestCase):
    """A park is verified: board_test never enumerates, so the device must leave the bus."""

    def park(self, on_bus_after, flash_rc=0, status='ok'):
        state = {'on_bus': True}
        patch(self, hil_pool_check, 'find_device',
              lambda uid: ('1-1', 'cafe:4001', 1) if state['on_bus'] else None)

        def flash(board, fw):
            state['on_bus'] = on_bus_after
            return types.SimpleNamespace(returncode=flash_rc, stdout='Error: no target')
        patch(self, hil_pool_check.hil_flash, 'flash_primitive', lambda name: flash)
        patch(self, hil_pool_check.time, 'sleep', lambda s: None)
        clock = iter(range(0, 1000, 5))
        patch(self, hil_pool_check.time, 'monotonic', lambda: next(clock))
        note, row = [], {'status': status}
        hil_pool_check.park_board(BOARD, 'device', 'board_test.elf', row, note)
        return note, row

    def test_a_device_that_leaves_the_bus_is_parked(self):
        note, row = self.park(on_bus_after=False)
        self.assertEqual(row['status'], 'ok')
        self.assertEqual(note, [])

    def test_a_device_still_enumerated_is_a_silent_park(self):
        note, row = self.park(on_bus_after=True)
        self.assertEqual(row['status'], 'flash-failed')
        self.assertIn('park unverified', note[-1])

    def park_host(self, hello, status='ok'):
        patch(self, hil_pool_check.hil_flash, 'flash_primitive', lambda name: (
            lambda board, fw: types.SimpleNamespace(returncode=0, stdout='')))
        patch(self, hil_pool_check, 'check_host_serial', lambda board, **kw: hello)
        note, row = [], {'status': status}
        hil_pool_check.park_board(BOARD, 'host', 'board_test.elf', row, note)
        return note, row

    def test_a_host_park_must_say_hello(self):
        self.assertEqual(self.park_host(b'Hello from TinyUSB\r\n'), ([], {'status': 'ok'}))
        note, row = self.park_host(b'')
        self.assertEqual(row['status'], 'flash-failed')
        self.assertIn('park unverified: no board_test output', note)

    def test_a_failed_park_flash_fails_an_ok_row(self):
        note, row = self.park(on_bus_after=True, flash_rc=1)
        self.assertEqual(row['status'], 'flash-failed')
        self.assertIn('park flash failed', note[-1])

    def test_a_failed_park_keeps_a_failed_row(self):
        for (note, row), want in (
                (self.park(on_bus_after=True, flash_rc=1, status='failed'), 'park flash failed'),
                (self.park(on_bus_after=True, status='failed'), 'park unverified: device still enumerated'),
                (self.park_host(b'', status='failed'), 'park unverified: no board_test output')):
            self.assertEqual(row['status'], 'failed')
            self.assertIn(want, note[-1])


def row(name, status):
    return hil_pool_check.new_row(name, probe='✅ 1-1', probe_busport='1-1', status=status)


class Document(unittest.TestCase):
    def test_coverage_says_what_each_status_covers(self):
        doc = hil_pool_check.pool_document('ci', 'tinyusb.json', False,
                                           [row('a', 'ok'), row('b', 'locked'), row('c', 'failed')])
        self.assertEqual([r['coverage'] for r in doc['rows']],
                         ['full-attempted', 'skipped-locked', 'full-attempted'])
        self.assertEqual(doc['counts'], {'ok': 1, 'flash-failed': 0, 'failed': 1, 'locked': 1})
        self.assertEqual((doc['host'], doc['config'], doc['mode']), ('ci', 'tinyusb.json', 'full'))

    def test_every_scan_only_row_is_probe_only(self):
        doc = hil_pool_check.pool_document('ci', 'x.json', True, [row('a', 'ok'), row('b', 'flash-failed')])
        self.assertEqual({r['coverage'] for r in doc['rows']}, {'probe-only'})
        self.assertEqual(doc['mode'], 'scan-only')

    def test_the_table_is_a_rendering_of_the_document(self):
        doc = hil_pool_check.pool_document('ci', 'x.json', False, [dict(row('a', 'locked'), note=['held'])])
        lines = hil_pool_check.render_table(doc).splitlines()
        self.assertTrue(lines[1].startswith('| Board'))
        self.assertIn('🔒 locked', lines[3])
        self.assertIn('held', lines[3])
        self.assertTrue(lines[-1].startswith('0 ok · 0 flash-failed · 0 failed · 1 locked'))


class Main(unittest.TestCase):
    """main() on a one-board roster with check_board faked; fds 1 and 2 are captured."""

    def setUp(self):
        td = tempfile.TemporaryDirectory()
        self.addCleanup(td.cleanup)
        self.cfg = os.path.join(td.name, 'roster.json')
        with open(self.cfg, 'w') as f:
            json.dump({'boards': [dict(BOARD, tests={'device': True})]}, f)
        for obj, name in ((hil_pool_check.hil_flash, 'build_dir'),
                          (hil_pool_check.hil_util, 'verbose')):
            patch(self, obj, name, getattr(obj, name))
        self.fetched = []
        patch(self, hil_pool_check, 'fetch_missing', lambda boards: self.fetched.append(boards) or {})

    def noisy_board(self, board, args):
        print('python noise')
        os.write(1, b'child noise\n')
        return row(board['name'], 'failed')

    def run_main(self, *argv):
        patch(self, sys, 'argv', ['hil_pool_check.py', self.cfg, *argv])
        with tempfile.TemporaryFile() as out, tempfile.TemporaryFile() as err:
            sys.stdout.flush()
            sys.stderr.flush()
            saved = os.dup(1), os.dup(2)
            os.dup2(out.fileno(), 1)
            os.dup2(err.fileno(), 2)
            code = None
            try:
                hil_pool_check.main()
            except SystemExit as e:
                code = e.code
            except RuntimeError as e:
                code = e
            finally:
                sys.stdout.flush()
                sys.stderr.flush()
                os.write(1, b'after\n')   # must land where fd 1 pointed before main()
                os.dup2(saved[0], 1)
                os.dup2(saved[1], 2)
                os.close(saved[0])
                os.close(saved[1])
            out.seek(0)
            err.seek(0)
            return code, out.read().decode(), err.read().decode()

    def assert_json_only(self, *argv):
        patch(self, hil_pool_check, 'check_board_safe', self.noisy_board)
        code, out, err = self.run_main('--json', *argv)
        doc_line, after = out.splitlines()
        self.assertEqual(after, 'after')
        doc = json.loads(doc_line)
        self.assertEqual(doc['rows'][0]['name'], 'b')
        self.assertEqual(doc['config'], os.path.realpath(self.cfg))
        self.assertEqual(code, 1)
        self.assertIn('pool check: host', err)
        return err

    def test_json_stdout_carries_only_the_document(self):
        err = self.assert_json_only()
        self.assertIn('child noise', err)

    def test_json_stdout_stays_pure_under_verbose(self):
        err = self.assert_json_only('-v')
        self.assertIn('python noise', err)
        self.assertIn('child noise', err)

    def test_an_exception_restores_stdout(self):
        def boom(*a):
            raise RuntimeError('boom')
        patch(self, hil_pool_check, 'check_pool', boom)
        code, out, _ = self.run_main('--json')
        self.assertIsInstance(code, RuntimeError)
        self.assertEqual(out, 'after\n')

    def test_only_the_default_cache_is_fetched_into(self):
        patch(self, hil_pool_check, 'check_board_safe', lambda board, args: row(board['name'], 'ok'))
        self.run_main('-B', 'cmake-build')
        self.run_main('--scan-only')
        self.assertEqual(self.fetched, [])
        self.run_main()
        self.assertEqual([[b['name'] for b in boards] for boards in self.fetched], [['b']])

    def test_the_default_output_is_the_table(self):
        patch(self, hil_pool_check, 'check_board_safe', lambda board, args: row(board['name'], 'ok'))
        code, out, _ = self.run_main()
        self.assertEqual(code, 0)
        self.assertIn('| Board', out)
        self.assertIn('1 ok · 0 flash-failed', out)

    def test_a_board_without_a_name_is_its_own_error_row(self):
        with open(self.cfg, 'w') as f:
            json.dump({'boards': [{'uid': 'U', 'flasher': BOARD['flasher']}, dict(BOARD, tests={'device': True})]}, f)
        patch(self, hil_pool_check, 'check_board', lambda board, args: row(board['name'], 'ok'))
        code, out, _ = self.run_main('--json')
        self.assertEqual(code, 1)
        rows = json.loads(out.splitlines()[0])['rows']
        self.assertEqual([(r['name'], r['status']) for r in rows], [('?', 'failed'), ('b', 'ok')])
        self.assertIn('KeyError', rows[0]['note'][0])
        code, out, _ = self.run_main('--json', '-b', 'b')
        self.assertEqual((code, [r['name'] for r in json.loads(out.splitlines()[0])['rows']]), (0, ['b']))


if __name__ == '__main__':
    unittest.main()
