#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Unit tests for ci_select.py — pure logic, no hardware, no git. Run directly:
#   python3 test/hil/test/test_ci_select.py
#
# Imports stay stdlib + ci_select/hil_util/hil_flash ONLY: the pre-commit hil-test
# hook runs this suite, on GitHub's bare runner in the pre-commit workflow as well as
# locally, and that runner has no pyserial/pymtp. hil_flash is admissible because it
# is stdlib + hil_util only (test_hil_util.BottomLayer enforces the stdlib closure of
# both) and the roster-dispatch tests need its flash_* table; never import hil_test,
# which pulls pyserial.
import contextlib
import glob
import io
import json
import os
import pathlib
import re
import subprocess
import sys
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.abspath(__file__)))))
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))  # test/hil, for hil_flash/helper
sys.path.insert(0, os.path.join(REPO, 'tools'))
import hil_flash
import ci_select
from helper.hil_util import device_tests, dual_tests


def real_rosters():
    """The actual rig rosters, for regression tests that need real-world data
    (a specific board/family/only-list) rather than the synthetic ROSTER above."""
    rosters = []
    for name in ('tinyusb.json', 'hfp.json'):
        path = os.path.join(REPO, 'test/hil', name)
        with open(path) as f:
            rosters.append((f'test/hil/{name}', json.load(f)['boards']))
    return rosters


def roster_flashers():
    """(roster path, board) for every board in the live rosters, `boards-skip`
    included: a parked board's flasher name must still dispatch, so that unparking it
    is not what discovers the name went stale."""
    for name in ('tinyusb.json', 'hfp.json'):
        path = os.path.join(REPO, 'test/hil', name)
        with open(path) as f:
            cfg = json.load(f)
        for key in ('boards', 'boards-skip'):
            for b in cfg.get(key, []):
                yield f'test/hil/{name}', b


def on_roster(tc, *names):
    """The subset of `names` currently in the live rig rosters, skipping the test
    when none are, because parking/unparking a board is routine rig maintenance.

    That skip now matters MORE than it used to, not less: this suite is a blocking
    pre-commit hook AND build.yml's selector steps gate on it (a failing suite falls
    open to the full matrix), so an assertion that depends on a specific board being
    present goes red on every PR -- including src/-only ones that never touched the
    rig -- until someone fixes the roster. Keep roster-dependent assertions behind
    on_roster."""
    have = {b['name'] for _, boards in real_rosters() for b in boards}
    got = [n for n in names if n in have]
    if not got:
        tc.skipTest(f'not in the rig roster: {", ".join(names)}')
    return got


ROSTER = [
    # device-only, rp2040 family
    {'name': 'raspberry_pi_pico', 'uid': 'u1', 'flasher': {'name': 'openocd'},
     'tests': {'device': True, 'host': True, 'dual': True}},
    # device-only, stm32f4 family
    {'name': 'stm32f407disco', 'uid': 'u2', 'flasher': {'name': 'jlink'},
     'tests': {'device': True, 'host': False, 'dual': False}},
    # host-only board
    {'name': 'raspberry_pi_pico2', 'uid': 'u3', 'flasher': {'name': 'openocd'},
     'tests': {'device': False, 'host': True, 'dual': False}},
    # only-list board (espressif-style), flashed by the CI leg that splits on esptool
    {'name': 'espressif_s3_devkitm', 'uid': 'u4', 'flasher': {'name': 'esptool'},
     'tests': {'only': ['device/cdc_msc_freertos', 'host/device_info']}},
]
ROSTERS = [('test/hil/tinyusb.json', ROSTER)]


def sel(files):
    return ci_select.classify(files, REPO, ROSTERS)


class TestPortRule(unittest.TestCase):
    def test_dcd_rp2040_selects_pico_family_only(self):
        s = sel(['src/portable/raspberrypi/rp2040/dcd_rp2040.c'])
        self.assertFalse(s['full'])
        self.assertIn('raspberry_pi_pico', s['boards'])
        self.assertNotIn('stm32f407disco', s['boards'])
        self.assertNotIn('espressif_s3_devkitm', s['boards'])
        # device role: no host tests in pico's list
        self.assertTrue(all(not t.startswith('host/') for t in s['boards']['raspberry_pi_pico']))
        # host-only boards drop out entirely on a device-role change
        self.assertNotIn('raspberry_pi_pico2', s['boards'])

    def test_shared_port_file_is_both_roles(self):
        s = sel(['src/portable/synopsys/dwc2/dwc2_common.c'])
        self.assertFalse(s['full'])
        self.assertNotIn('raspberry_pi_pico', s['boards'])  # rp2040 is not a dwc2 family
        self.assertIn('stm32f407disco', s['boards'])        # stm32f4 is


class TestCoreRoleRule(unittest.TestCase):
    def test_usbd_selects_all_device_tests_everywhere(self):
        s = sel(['src/device/usbd.c'])
        self.assertFalse(s['full'])
        self.assertNotIn('raspberry_pi_pico2', s['boards'])  # host-only board dropped
        pico = s['boards']['raspberry_pi_pico']
        self.assertTrue(set(device_tests).issubset(set(pico)))
        self.assertTrue(set(dual_tests).issubset(set(pico)))   # dual survives device role
        self.assertTrue(all(not t.startswith('host/') for t in pico))
        # only-list board: selection intersects its only-list
        esp = s['boards']['espressif_s3_devkitm']
        self.assertEqual(esp, ['device/cdc_msc_freertos'])

    def test_host_change_drops_device(self):
        s = sel(['src/host/usbh.c'])
        self.assertFalse(s['full'])
        self.assertIn('raspberry_pi_pico2', s['boards'])
        self.assertNotIn('stm32f407disco', s['boards'])  # device-only board dropped


class TestClassRule(unittest.TestCase):
    def test_cdc_device_selects_cdc_examples_only(self):
        s = sel(['src/class/cdc/cdc_device.c'])
        self.assertFalse(s['full'])
        pico = s['boards']['raspberry_pi_pico']
        self.assertIn('device/cdc_msc', pico)
        self.assertIn('device/cdc_dual_ports', pico)
        self.assertNotIn('device/msc_dual_lun', pico)   # CFG_TUD_CDC 0 there
        self.assertNotIn('device/usbtest', pico)        # CFG_TUD_CDC 0 there
        self.assertTrue(all(not t.startswith('host/') for t in pico))

    def test_msc_host_selects_host_side(self):
        s = sel(['src/class/msc/msc_host.c'])
        self.assertFalse(s['full'])
        self.assertNotIn('stm32f407disco', s['boards'])  # device-only board
        pico2 = s['boards']['raspberry_pi_pico2']
        self.assertIn('host/msc_file_explorer', pico2)
        self.assertTrue(all(not t.startswith('device/') for t in pico2))


class TestClassIncludeEdges(unittest.TestCase):
    """A class header another class includes reaches that class's examples too.
    src/class/midi/midi{,2}_{device,host}.h include class/audio/audio.h, so
    midi_test's firmware contains audio.h - but the class rule derives macros from
    the directory name alone, so an audio.h change used to select only
    device/audio_test_freertos. On boards that skip that example the per-board
    intersection emptied and an audio.h-only PR ran ZERO HIL on them."""
    def test_edges_derived_from_includes(self):
        edges = ci_select.class_include_edges(REPO)
        self.assertEqual(edges.get('audio/audio.h'), {'midi'})
        self.assertEqual(edges.get('cdc/cdc.h'), {'net'})

    def test_audio_header_selects_midi_example(self):
        s = ci_select.classify(['src/class/audio/audio.h'], REPO, real_rosters())
        self.assertFalse(s['full'])
        # every board that runs device/midi_test at all must run it here (boards with
        # a tests.only list, e.g. espressif, run the freertos examples instead)
        by_name = {b['name']: b for _, bs in real_rosters() for b in bs}
        checked = 0
        for name, tests in s['boards'].items():
            if 'device/midi_test' in ci_select.board_tests(by_name[name]):
                self.assertIn('device/midi_test', tests, name)
                checked += 1
        self.assertTrue(checked)

    def test_audio_header_reaches_boards_that_skip_audio(self):
        # both skip device/audio_test_freertos: without the midi edge their
        # intersection is empty and they drop out of the selection entirely
        boards = on_roster(self, 'metro_m4_express', 'nrf54lm20dk')
        s = ci_select.classify(['src/class/audio/audio.h'], REPO, real_rosters())
        for board in boards:
            self.assertEqual(s['boards'].get(board), ['device/midi_test'], board)

    def test_edge_is_per_header_not_per_class(self):
        # midi includes audio.h, not audio_device.h: an audio_device change must
        # not drag midi's examples in
        s = ci_select.classify(['src/class/audio/audio_device.c'], REPO, real_rosters())
        self.assertFalse(s['full'])
        for tests in s['boards'].values():
            if tests != 'all':
                self.assertNotIn('device/midi_test', tests)


class TestFallbackRules(unittest.TestCase):
    def test_unknown_tool_is_full(self):
        s = sel(['tools/random_new_script.py'])
        self.assertTrue(s['full'])

    def test_docs_only_is_empty_not_full(self):
        s = sel(['docs/info/contributing.rst', 'README.rst'])
        self.assertFalse(s['full'])
        self.assertEqual(s['boards'], {})

    def test_bsp_family_selects_family_boards(self):
        s = sel(['hw/bsp/rp2040/family.cmake'])
        self.assertFalse(s['full'])
        self.assertIn('raspberry_pi_pico', s['boards'])
        self.assertEqual(s['boards']['raspberry_pi_pico'], 'all')
        self.assertNotIn('stm32f407disco', s['boards'])

    def test_bsp_board_narrows_to_board(self):
        s = sel(['hw/bsp/rp2040/boards/raspberry_pi_pico/board.h'])
        self.assertFalse(s['full'])
        self.assertEqual(list(s['boards'].keys()), ['raspberry_pi_pico'])

    def test_example_change_selects_that_example(self):
        s = sel(['examples/device/cdc_msc/src/main.c'])
        self.assertFalse(s['full'])
        self.assertEqual(s['boards']['raspberry_pi_pico'], ['device/cdc_msc'])

    def test_core_common_is_full(self):
        for f in ['src/tusb.c', 'src/common/tusb_fifo.c', 'src/osal/osal_freertos.h']:
            self.assertTrue(sel([f])['full'], f)

    def test_board_test_example_is_full(self):
        # board_test is the park/teardown firmware hil_test.py flashes on every board,
        # not an unlisted example: a regression there must not skip the whole rig
        for f in ['examples/device/board_test/src/main.c',
                  'examples/device/board_test/CMakeLists.txt']:
            self.assertTrue(sel([f])['full'], f)

    def test_harness_is_full(self):
        # hw/mcu/ is no longer here: it resolves to families/boards via mcu_families()
        # instead of forcing full - see TestMcuHilRule
        for f in ['test/hil/hil_test.py', '.github/workflows/build.yml']:
            self.assertTrue(sel([f])['full'], f)

    def test_mixed_roles_no_pruning(self):
        s = sel(['src/device/usbd.c', 'src/host/usbh.c'])
        self.assertFalse(s['full'])
        self.assertIn('raspberry_pi_pico2', s['boards'])
        self.assertIn('stm32f407disco', s['boards'])

    def test_cmakelists_and_requirements_are_full(self):
        for f in ['src/CMakeLists.txt', 'examples/CMakeLists.txt',
                  'examples/device/CMakeLists.txt', 'test/hil/requirements.txt']:
            self.assertTrue(sel([f])['full'], f)

    def test_docs_txt_is_noncode(self):
        s = sel(['docs/info/changelog.txt'])
        self.assertFalse(s['full'])
        self.assertEqual(s['boards'], {})


class TestArgsEmission(unittest.TestCase):
    def test_args_for_scoped_selection(self):
        s = sel(['src/portable/raspberrypi/rp2040/dcd_rp2040.c'])
        args = ci_select.selection_args(s, ROSTERS)
        a = args['tinyusb.json']
        self.assertIn('-b raspberry_pi_pico', a)
        self.assertNotIn('stm32f407disco', a)
        self.assertIn('-bt raspberry_pi_pico:', a)   # device-only subset of a device+host board

    def test_args_full_is_empty(self):
        s = sel(['tools/random_new_script.py'])
        self.assertEqual(ci_select.selection_args(s, ROSTERS), {'tinyusb.json': ''})

    def test_args_all_board_gets_bare_b(self):
        s = sel(['hw/bsp/rp2040/boards/raspberry_pi_pico/board.h'])
        a = ci_select.selection_args(s, ROSTERS)['tinyusb.json']
        self.assertIn('-b raspberry_pi_pico', a)
        self.assertNotIn('-bt', a)

    def test_args_by_flasher_splits_esp_from_the_rest(self):
        s = sel(['src/device/usbd.c'])
        per = ci_select.selection_args_by_flasher(s, ROSTERS)['tinyusb.json']
        self.assertIn('espressif_s3_devkitm', per['esptool'])
        self.assertIn('raspberry_pi_pico', per['openocd'])
        self.assertNotIn('espressif_s3_devkitm', per.get('openocd', '') + per.get('jlink', ''))

    def test_args_by_flasher_omits_a_flasher_with_no_selected_board(self):
        # the esp CI leg must see no args at all here, not a filter matching zero boards
        s = sel(['hw/bsp/rp2040/boards/raspberry_pi_pico/board.h'])
        per = ci_select.selection_args_by_flasher(s, ROSTERS)['tinyusb.json']
        self.assertEqual(per, {'openocd': '-b raspberry_pi_pico'})

    def test_args_by_flasher_full_is_empty(self):
        s = sel(['tools/random_new_script.py'])
        self.assertEqual(ci_select.selection_args_by_flasher(s, ROSTERS), {'tinyusb.json': {}})

    def test_cli_diff_file(self):
        import subprocess, tempfile, json as j
        with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
            f.write('src/class/cdc/cdc_device.c\n')
            path = f.name
        r = subprocess.run([sys.executable, os.path.join(REPO, 'tools/ci_select.py'),
                            '--diff-file', path, os.path.join(REPO, 'test/hil/tinyusb.json')],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        out = j.loads(r.stdout)
        self.assertFalse(out['full'])
        self.assertIn('tinyusb.json', out['args'])
        # reasons are a stderr diagnostic, deliberately NOT in the payload: they were
        # 97% of a 9.8 MB JSON on a dep bump, and every consumer re-parses that file
        self.assertNotIn('reasons', out, 'reasons must not ride in the machine-read JSON')
        self.assertNotIn('reasons', out['build'])
        self.assertIn('cdc_device', r.stderr)
        # A core-class diff must select boards THROUGH THE CLI: the in-process tests
        # inject their own repo root, so only this subprocess path catches a broken
        # repo_root derivation -- which once made every repo-relative glob match
        # nothing and turned this exact diff into a silent full-HIL skip.
        self.assertTrue(out['boards'],
                        'CLI selected zero boards for a src/class change: repo_root broken?')
        os.unlink(path)


class TestRealRosterPortFamilies(unittest.TestCase):
    """Regression for port_families() missing espressif's dwc2 reference, which
    lives in a component CMakeLists.txt rather than family.cmake/family.mk."""
    def test_dwc2_change_selects_espressif_boards(self):
        boards = on_roster(self, 'espressif_s3_devkitm', 'espressif_p4_function_ev')
        s = ci_select.classify(['src/portable/synopsys/dwc2/dcd_dwc2.c'], REPO, real_rosters())
        self.assertFalse(s['full'])
        for board in boards:
            self.assertIn(board, s['boards'])


class TestOptionGatedPort(unittest.TestCase):
    """Regression: family_support.cmake compiles some ports from a build option
    (MAX3421_HOST=1 -> hcd_max3421.c), so a board's family file never names them."""
    # host-side option board (max3421 as host controller), off any max3421 family
    OPT_ROSTER = [('test/hil/opt.json', [
        {'name': 'fake_dual_board', 'uid': 'o1', 'flasher': {'name': 'jlink'},
         'variant': [{'name': 'fake_dual_board', 'defines': ['MAX3421_HOST=1']}],
         'tests': {'device': True, 'host': False, 'dual': True}},
        {'name': 'fake_host_board', 'uid': 'o2', 'flasher': {'name': 'jlink'},
         'variant': [{'name': 'fake_host_board', 'flags': '-DMAX3421_HOST=1'}],
         'tests': {'device': False, 'host': True, 'dual': False}},
        {'name': 'fake_off_board', 'uid': 'o3', 'flasher': {'name': 'jlink'},
         'variant': [{'name': 'fake_off_board', 'defines': ['MAX3421_HOST=0']}],
         'tests': {'device': True, 'host': True, 'dual': True}},
    ])]

    def test_real_roster_max3421_selects_option_board(self):
        boards = on_roster(self, 'metro_m4_express')
        s = ci_select.classify(['src/portable/analog/max3421/hcd_max3421.c'], REPO, real_rosters())
        self.assertFalse(s['full'])
        for board in boards:
            self.assertIn(board, s['boards'])

    def test_option_selects_via_defines_and_flags(self):
        s = ci_select.classify(['src/portable/analog/max3421/hcd_max3421.c'], REPO, self.OPT_ROSTER)
        self.assertFalse(s['full'])
        self.assertIn('fake_dual_board', s['boards'])    # variant defines
        self.assertIn('fake_host_board', s['boards'])    # variant flags
        self.assertNotIn('fake_off_board', s['boards'])  # variant defines, but =0

    def test_device_role_port_does_not_pull_host_only_option_board(self):
        s = ci_select.classify(['src/portable/analog/max3421/dcd_max3421.c'], REPO, self.OPT_ROSTER)
        self.assertFalse(s['full'])
        self.assertNotIn('fake_host_board', s['boards'])  # host-only board, device change
        self.assertIn('fake_dual_board', s['boards'])     # device-capable option board

    def test_gates_parsed_from_family_support(self):
        self.assertEqual(ci_select.port_option_gates(REPO).get('analog/max3421'),
                         {'MAX3421_HOST'})

    def test_board_cmake_option_counts(self):
        """A board can enable a gated port in its own BSP rather than via the roster
        (hw/bsp/espressif/boards/*/board.cmake -> set(MAX3421_HOST 1)); board_options()
        must see those too, or such a board joining the roster is silently dropped."""
        self.assertIn('MAX3421_HOST',
                      ci_select.bsp_board_options('adafruit_feather_esp32s3', REPO))
        self.assertIn('CFG_TUH_RPI_PIO_USB',
                      ci_select.bsp_board_options('adafruit_fruit_jam', REPO))
        # commented-out `# set(MAX3421_HOST 1)` must not count
        self.assertNotIn('MAX3421_HOST',
                         ci_select.bsp_board_options('feather_nrf52840_express', REPO))

    def test_board_cmake_option_selects_off_family_board(self):
        # adafruit_feather_esp32s3 is not on any rig roster; stand it in as one to
        # prove the BSP-sourced option alone pulls a max3421 change onto the board
        roster = [('test/hil/opt.json', [
            {'name': 'adafruit_feather_esp32s3', 'uid': 'o1', 'flasher': {'name': 'esptool'},
             'tests': {'device': False, 'host': True, 'dual': False}}])]
        s = ci_select.classify(['src/portable/analog/max3421/hcd_max3421.c'], REPO, roster)
        self.assertFalse(s['full'])
        self.assertIn('adafruit_feather_esp32s3', s['boards'])

    def test_board_mk_option_is_ignored(self):
        """Make-only options must not select: HIL CI builds with CMake exclusively, so
        hw/bsp/nrf/boards/nrf5340dk/board.mk's MAX3421_HOST compiles nothing here."""
        roster = [('test/hil/opt.json', [
            {'name': 'nrf5340dk', 'uid': 'o1', 'flasher': {'name': 'jlink'},
             'tests': {'device': False, 'host': True, 'dual': False}}])]
        s = ci_select.classify(['src/portable/analog/max3421/hcd_max3421.c'], REPO, roster)
        self.assertFalse(s['full'])
        self.assertEqual(s['boards'], {})


class TestPortFamiliesCmakeOnly(unittest.TestCase):
    """port_families() is CMake-only (HIL CI never builds with Make) and matches on
    'port_dir/' so a port dir is not a prefix of a sibling."""
    def test_make_only_family_is_not_a_family(self):
        # hw/bsp/pic32mz has family.mk but no family.cmake
        self.assertEqual(ci_select.port_families('microchip/pic32mz', REPO), set())

    def test_prefix_port_does_not_inherit_sibling_families(self):
        # bare-substring matching let 'microchip/pic' match '.../microchip/pic32mz/...'
        self.assertEqual(ci_select.port_families('microchip/pic', REPO), set())

    def test_make_only_port_contributes_nothing(self):
        s = sel(['src/portable/microchip/pic32mz/dcd_pic32mz.c'])
        self.assertFalse(s['full'])
        self.assertEqual(s['boards'], {})
        self.assertTrue(any('no board family' in r for r in s['reasons']), s['reasons'])

    def test_cmake_families_still_found(self):
        self.assertEqual(ci_select.port_families('raspberrypi/rp2040', REPO), {'rp2040'})
        self.assertIn('stm32f4', ci_select.port_families('synopsys/dwc2', REPO))


class TestPortFamiliesCoverage(unittest.TestCase):
    """Systematic guard: every real dcd_*/hcd_* port directory should map to at
    least one board family, so a future family.cmake/CMakeLists.txt layout that
    port_families() doesn't scan fails loudly instead of silently dropping boards
    (as espressif's dwc2 reference did - see TestRealRosterPortFamilies)."""
    # Ports with no board family: not a bug, just not wired into any rig board.
    # Add here (with a reason) only if port_families() legitimately can't find one.
    # A port listed here contributes NOTHING on either axis (empty means empty), so
    # this list is the tripwire: a port that stops resolving must show up as a test
    # failure, not as a PR that quietly builds and tests nothing.
    NO_FAMILY = {
        'template',           # reference/example port, not built by any board
        # hw/bsp/pic32mz has family.mk only (no family.cmake), and port_families()
        # is CMake-only because HIL CI builds every board with CMake - so this port
        # is compiled for no HIL board.
        'microchip/pic32mz',
        'microchip/pic',      # same: only ever referenced from pic32mz's family.mk
    }

    @staticmethod
    def _dcd_hcd_ports():
        portable_root = os.path.join(REPO, 'src/portable')
        ports = []
        for entry in sorted(os.listdir(portable_root)):
            d = os.path.join(portable_root, entry)
            if not os.path.isdir(d):
                continue
            if glob.glob(os.path.join(d, 'dcd_*.c')) or glob.glob(os.path.join(d, 'hcd_*.c')):
                ports.append(entry)
                continue
            for sub in sorted(os.listdir(d)):
                sd = os.path.join(d, sub)
                if os.path.isdir(sd) and (glob.glob(os.path.join(sd, 'dcd_*.c')) or
                                           glob.glob(os.path.join(sd, 'hcd_*.c'))):
                    ports.append(f'{entry}/{sub}')
        return ports

    def test_every_port_maps_to_a_family(self):
        ports = self._dcd_hcd_ports()
        self.assertTrue(ports)  # sanity: the scan itself found something
        for port in ports:
            if port in self.NO_FAMILY:
                continue
            fams = ci_select.port_families(port, REPO)
            self.assertTrue(fams, f'{port}: no family references this port '
                                   f'(port_families() scan gap, or add to NO_FAMILY)')


class TestRealRosterOnlyListTests(unittest.TestCase):
    """Regression for roster-only-list tests (e.g. espressif's hid_composite_freertos)
    being invisible to the selector because it only knew the shared hil_util lists."""
    def test_only_list_example_change_selects_it(self):
        boards = on_roster(self, 'espressif_s3_devkitm', 'espressif_p4_function_ev')
        s = ci_select.classify(['examples/device/hid_composite_freertos/src/main.c'], REPO, real_rosters())
        self.assertFalse(s['full'])
        for board in boards:
            self.assertEqual(s['boards'][board], ['device/hid_composite_freertos'])

    def test_class_change_includes_only_list_boards(self):
        boards = on_roster(self, 'espressif_s3_devkitm', 'espressif_p4_function_ev')
        s = ci_select.classify(['src/class/hid/hid_device.c'], REPO, real_rosters())
        self.assertFalse(s['full'])
        for board in boards:
            self.assertIn(board, s['boards'])


class TestPortAndCoreRoleUseExtras(unittest.TestCase):
    """Regression: the port rule and core-role rule must thread the roster-only
    test universe (extras) the same way the class rule already does, so a DCD
    or device-stack change doesn't silently drop espressif's only-list tests
    (e.g. hid_composite_freertos) that aren't in the shared device_tests list."""
    def test_dcd_change_includes_only_list_test(self):
        boards = on_roster(self, 'espressif_s3_devkitm', 'espressif_p4_function_ev')
        s = ci_select.classify(['src/portable/synopsys/dwc2/dcd_dwc2.c'], REPO, real_rosters())
        self.assertFalse(s['full'])
        for board in boards:
            tests = s['boards'][board]
            if tests == 'all':
                continue  # a board whose whole allowed set is selected collapses to 'all'
            self.assertIn('device/hid_composite_freertos', tests)
            self.assertIn('device/cdc_msc_freertos', tests)
            self.assertIn('device/audio_test_freertos', tests)
            self.assertIn('device/usbtest', tests)

    def test_core_device_change_includes_only_list_test(self):
        boards = on_roster(self, 'espressif_s3_devkitm', 'espressif_p4_function_ev')
        s = ci_select.classify(['src/device/usbd.c'], REPO, real_rosters())
        self.assertFalse(s['full'])
        for board in boards:
            tests = s['boards'][board]
            if tests == 'all':
                continue  # a board whose whole allowed set is selected collapses to 'all'
            self.assertIn('device/hid_composite_freertos', tests)
            self.assertIn('device/cdc_msc_freertos', tests)
            self.assertIn('device/audio_test_freertos', tests)
            self.assertIn('device/usbtest', tests)

    def test_host_change_does_not_leak_device_only_list_test(self):
        s = ci_select.classify(['src/host/usbh.c'], REPO, real_rosters())
        self.assertFalse(s['full'])
        for board, tests in s['boards'].items():
            if tests == 'all':
                continue
            self.assertNotIn('device/hid_composite_freertos', tests, board)


class TestFamilies(unittest.TestCase):
    """`families` exists for consumers that build (not just test) the diff: most
    families have no rig board, so `boards` alone would compile nothing for them."""
    def test_off_rig_port_still_reports_family(self):
        s = sel(['src/portable/microchip/samx7x/dcd_samx7x.c'])
        self.assertFalse(s['full'])
        self.assertEqual(s['boards'], {})        # no same7x board on the rig
        self.assertEqual(s['families'], ['same7x'])

    def test_port_families_are_reported(self):
        s = sel(['src/portable/raspberrypi/rp2040/dcd_rp2040.c'])
        self.assertIn('rp2040', s['families'])

    def test_bsp_family_and_board_report_family(self):
        self.assertEqual(sel(['hw/bsp/rp2040/family.cmake'])['families'], ['rp2040'])
        self.assertEqual(sel(['hw/bsp/rp2040/boards/raspberry_pi_pico/board.h'])['families'],
                         ['rp2040'])

    def test_docs_only_has_no_families(self):
        self.assertEqual(sel(['docs/info/contributing.rst'])['families'], [])

    def test_full_selection_still_reports_families(self):
        """A full-matrix file must not hide the families of the other changed files:
        consumers that build from `families` (e.g. /pre-pr) ignore `boards` when full."""
        s = sel(['src/common/tusb_fifo.c', 'src/portable/microchip/samx7x/dcd_samx7x.c'])
        self.assertTrue(s['full'])
        self.assertIn('same7x', s['families'])
        # full stays full: every roster board, and no args to narrow the run
        self.assertEqual(set(s['boards']), {b['name'] for b in ROSTER})
        self.assertTrue(all(v == 'all' for v in s['boards'].values()))
        self.assertEqual(ci_select.selection_args(s, ROSTERS), {'tinyusb.json': ''})
        self.assertEqual(ci_select.selection_args_by_flasher(s, ROSTERS), {'tinyusb.json': {}})

    def test_family_order_does_not_matter(self):
        # same as above with the full-matrix file last (was the only order that worked)
        s = sel(['src/portable/microchip/samx7x/dcd_samx7x.c', 'src/common/tusb_fifo.c'])
        self.assertTrue(s['full'])
        self.assertIn('same7x', s['families'])


class TestGitDiffArgv(unittest.TestCase):
    def test_diff_disables_rename_detection(self):
        """Without --no-renames git reports only a rename's destination, so moving an
        HIL-relevant file to a non-code path would be classified as non-code only."""
        self.assertIn('--no-renames', ci_select.GIT_DIFF_ARGV)


class TestPortWithoutFamilyContributesNothing(unittest.TestCase):
    """A port dir no family file references contributes nothing on BOTH axes (the
    maintainer's empty-means-empty ruling): nothing compiles the file, so there is
    nothing to run. Forcing the full 30-board rig here bought no coverage - the build
    walk answered the identical condition with zero families for the same path."""
    def test_unreferenced_port_contributes_nothing(self):
        orig = ci_select.port_families
        ci_select.port_families = lambda port_dir, repo_root: set()
        try:
            s = sel(['src/portable/vendor/newip/dcd_newip.c'])
            b = ci_select.classify_build(['src/portable/vendor/newip/dcd_newip.c'], REPO)
        finally:
            ci_select.port_families = orig
        self.assertFalse(s['full'])
        self.assertEqual(s['boards'], {})
        self.assertTrue(any('no board family' in r for r in s['reasons']), s['reasons'])
        self.assertFalse(b['full'])
        self.assertEqual(b['families'], [])


class TestOpenocdVidPid(unittest.TestCase):
    """The roster's optional flasher `vid_pid` field (openocd-verbatim, e.g.
    "0x1a86 0x8010", more pairs appended) pins openocd's probe discovery so it
    never opens foreign usbfs nodes. It must be emitted BEFORE the args: the
    rescue cfgs run `init` internally (rp2350-rescue.cfg errors on any
    config-stage command after its init; rp2040.cfg under RESCUE scans before a
    trailing flag is even parsed), and no rig cfg sets a competing list
    (the 2026-08-10 convoy mechanism)."""

    def test_vid_pid_flag_precedes_args(self):
        cmd = hil_flash._openocd_cmd_base(
            {'uid': 'S1', 'args': '-f target/wch-riscv.cfg', 'vid_pid': '0x1a86 0x8010'})
        self.assertIn('-c "adapter usb vid_pid 0x1a86 0x8010" -f target/wch-riscv.cfg', cmd)
        self.assertTrue(cmd.endswith('-f target/wch-riscv.cfg'), cmd)

    def test_rescue_cfg_command_keeps_vid_pid_before_init(self):
        """rescue_openocd swaps the target cfg for one that runs `init` internally;
        a vid_pid flag after the args would error there (rp2350) or be skipped
        (rp2040) -- in exactly the wedged-rig scenario the pin exists for."""
        flasher = {'name': 'openocd', 'uid': 'S1', 'vid_pid': '0x2e8a 0x000c',
                   'args': '-c "set RESCUE 1" -f target/rp2040.cfg'}
        cmd = hil_flash._openocd_cmd_base(flasher)
        self.assertLess(cmd.index('adapter usb vid_pid'), cmd.index('-f target/'), cmd)

    def test_vid_pid_multiple_pairs(self):
        cmd = hil_flash._openocd_cmd_base(
            {'uid': 'S1', 'args': '-f i.cfg', 'vid_pid': '0x2e8a 0x000c 0x2e8a 0x000d'})
        self.assertIn('-c "adapter usb vid_pid 0x2e8a 0x000c 0x2e8a 0x000d"', cmd)

    def test_no_field_no_flag_but_warns(self):
        # the roster lint only covers the committed rosters; a dev PC's local.json entry
        # without the field must at least say what it is giving up -- on STDERR, since
        # hil_test captures stdout per test and would swallow it on a passing run
        import io
        from contextlib import redirect_stderr
        hil_flash._VID_PID_WARNED.discard('S-warn')
        cap = io.StringIO()
        with redirect_stderr(cap):
            cmd = hil_flash._openocd_cmd_base({'uid': 'S-warn', 'args': '-f i.cfg'})
        self.assertNotIn('vid_pid', cmd)
        self.assertIn('vid_pid', cap.getvalue())

    def test_roster_openocd_entries_all_pin_vid_pid(self):
        # every openocd probe on the rig has a known VID/PID; a new entry without the
        # pin silently reintroduces open-everything discovery
        for path, board in roster_flashers():
            f = board['flasher']
            # tinyusb.json only: hfp.json is the hifiphile rig owner's file, and a
            # blocking repo-wide lint over someone else's roster would red every PR the
            # moment they add an openocd board (hil_flash treats the field as optional)
            if f['name'] == 'openocd' and path.endswith('tinyusb.json'):
                self.assertIn('vid_pid', f,
                              f"{path}: {board['name']} openocd flasher lacks vid_pid")
                self.assertNotIn('vid_pid', f.get('args', ''),
                                 f"{path}: {board['name']} packs vid_pid into args; use the field")


class TestRosterFlashersDispatch(unittest.TestCase):
    """hil_test and hil_pool_check resolve a board's flasher with a bare
    getattr(hil_flash, f'flash_{name}'), and hil_test does it inside a redirect_stdout —
    so a renamed or typo'd roster name raises an AttributeError whose output is swallowed,
    with nothing pointing at the roster as the thing to edit. Renaming a flash_*/reset_*
    pair without updating every roster must fail here instead."""

    def test_flash_and_reset_exist_for_every_roster_flasher(self):
        for path, board in roster_flashers():
            name = board['flasher']['name'].lower()
            for fn in (f'flash_{name}', f'reset_{name}'):
                self.assertTrue(callable(getattr(hil_flash, fn, None)),
                                f'{path}: {board["name"]} uses flasher "{name}" '
                                f'but hil_flash.{fn} does not exist')

    def test_firmware_suffix_known_for_every_roster_flasher(self):
        """find_firmware falls back to accepting .elf-or-.bin when a flasher is missing
        from FLASHER_SUFFIX, silently restoring the mismatch that map exists to catch."""
        for path, board in roster_flashers():
            name = board['flasher']['name'].lower()
            self.assertIn(name, hil_flash.FLASHER_SUFFIX,
                          f'{path}: {board["name"]} uses flasher "{name}" '
                          f'with no hil_flash.FLASHER_SUFFIX entry')


class FlasherRecoverEntry(unittest.TestCase):
    """Optional roster key: a SECOND flasher used only to deliver recovery while a usbfs
    node is poisoned. Boards whose primary flasher cannot get past a convoy (jlink,
    stlink, lm4flash) name an openocd entry here instead of changing how they are
    normally flashed."""

    def test_recover_flasher_prefers_the_optional_entry(self):
        prim = {'name': 'jlink', 'uid': 'X', 'args': '-device MIMXRT1064xxx6A'}
        rec = {'name': 'openocd', 'uid': 'X', 'args': '-f interface/jlink.cfg -f target/foo.cfg'}
        self.assertEqual(hil_flash.recover_flasher({'flasher': prim, 'flasher_recover': rec}), rec)
        self.assertEqual(hil_flash.recover_flasher({'flasher': prim}), prim)

    def test_openocd_over_jlink_is_convoy_safe_without_a_pin(self):
        """libjaylink discovery returns early unless idVendor == 0x1366 (SEGGER) and the PID
        is in its table, and only THEN calls libusb_open (discovery_usb.c) -- it never opens
        a foreign node. `adapter usb vid_pid` is a no-op for this driver: jlink.c reads
        adapter_serial / usb address / usb location, never the vid/pid."""
        self.assertTrue(hil_flash.convoy_safe(
            {'name': 'openocd', 'args': '-f interface/jlink.cfg -f target/stm32f4x.cfg'}))

    def test_openocd_with_neither_a_pin_nor_jlink_is_not_safe(self):
        self.assertFalse(hil_flash.convoy_safe(
            {'name': 'openocd', 'args': '-f interface/stlink.cfg -f target/stm32h7x.cfg'}))

    def test_the_existing_rules_are_unchanged(self):
        self.assertTrue(hil_flash.convoy_safe(
            {'name': 'openocd', 'vid_pid': '0x2e8a 0x000c', 'args': '-f interface/cmsis-dap.cfg'}))
        self.assertFalse(hil_flash.convoy_safe({'name': 'jlink', 'uid': 'X'}))
        self.assertTrue(hil_flash.convoy_safe({'name': 'esptool'}))


class TestModuleMove(unittest.TestCase):
    def test_repo_root_guard(self):
        # __file__-derived root: moving the module without re-deriving the parent
        # count re-points every scan at the wrong tree (it happened once already)
        self.assertTrue(os.path.isdir(os.path.join(ci_select._REPO_ROOT, 'src')))
        self.assertTrue(os.path.isdir(os.path.join(ci_select._REPO_ROOT, 'hw', 'bsp')))
        self.assertEqual(os.path.realpath(ci_select._REPO_ROOT), os.path.realpath(REPO))


class TestPathFamilies(unittest.TestCase):
    def test_port_wrapper_unchanged(self):
        self.assertEqual(ci_select.port_families('raspberrypi/rp2040', REPO), {'rp2040'})
        self.assertIn('stm32f4', ci_select.port_families('synopsys/dwc2', REPO))

    def test_boundary_without_trailing_slash(self):
        # hw/bsp/nrf/family.cmake writes `${TOP}/hw/mcu/nordic/nrfx` — no trailing
        # slash; the match must accept a directory-boundary end-of-token
        self.assertEqual(ci_select.path_families('hw/mcu/nordic/nrfx', REPO), {'nrf'})

    def test_boundary_rejects_prefix_sibling(self):
        # 'microchip/pic' must not inherit pic32mz's references (and pic32mz itself
        # is family.mk-only, which the CMake-only scan never reads)
        self.assertEqual(ci_select.port_families('microchip/pic', REPO), set())
        self.assertEqual(ci_select.port_families('microchip/pic32mz', REPO), set())

    def test_mcu_families_prefix_walk(self):
        self.assertEqual(ci_select.mcu_families('hw/mcu/nordic/nrf5x/nrf_clock.h', REPO), {'nrf'})
        self.assertEqual(ci_select.mcu_families('hw/mcu/dialog/da1469x/x.h', REPO), {'da1469x'})
        self.assertEqual(ci_select.mcu_families('hw/mcu/no_such_vendor/x.c', REPO), set())


class TestMcuHilRule(unittest.TestCase):
    def test_mcu_no_longer_forces_full(self):
        s = ci_select.classify(['hw/mcu/nordic/nrf5x/nrf_clock.h'], REPO, ROSTERS)
        self.assertFalse(s['full'])
        self.assertIn('nrf', s['families'])   # recorded even with no nrf rig board

    def test_mcu_selects_family_boards(self):
        got = on_roster(self, 'feather_nrf52840_express', 'pca10056', 'pca10095')
        s = ci_select.classify(['hw/mcu/nordic/nrf5x/nrf_clock.h'], REPO, real_rosters())
        self.assertFalse(s['full'])
        for b in got:
            self.assertIn(b, s['boards'])

    def test_unresolved_mcu_path_selects_nothing(self):
        # empty means empty (maintainer ruling): if no family's build references the
        # path, no build consumes the change - there is nothing to compile or run.
        # test_tracked_mcu_vendors_resolve is the drift guard for a real vendor dir
        s = ci_select.classify(['hw/mcu/no_such_vendor/x.c'], REPO, ROSTERS)
        self.assertFalse(s['full'])
        self.assertEqual(s['boards'], {})
        self.assertEqual(s['families'], [])


class TestOrphanInvariant(unittest.TestCase):
    ALLOW = {'microchip/pic', 'microchip/pic32mz'}   # spec: known orphans, CMake builds neither

    def test_every_port_resolves_to_a_family(self):
        for d in sorted(glob.glob(os.path.join(REPO, 'src/portable/*/*'))):
            if not os.path.isdir(d):
                continue
            port = os.path.relpath(d, os.path.join(REPO, 'src/portable')).replace(os.sep, '/')
            fams = ci_select.port_families(port, REPO)
            if port in self.ALLOW:
                self.assertEqual(fams, set(), f'{port}: no longer an orphan - drop it from ALLOW')
            else:
                self.assertTrue(fams, f'{port}: no family.cmake references it - wire it up or allowlist it')

    def test_tracked_mcu_vendors_resolve(self):
        import subprocess as sp
        r = sp.run(['git', 'ls-files', 'hw/mcu'], cwd=REPO, capture_output=True, text=True)
        if r.returncode != 0:
            self.skipTest('not a git checkout')
        vendors = sorted({'/'.join(p.split('/')[:3]) for p in r.stdout.split()})
        for v in vendors:
            self.assertTrue(ci_select.mcu_families(v + '/x.c', REPO), f'{v}: resolves to no family')

    # hw/bsp families ci_set_matrix's family_list does not map to any toolchain. Master
    # gave a PR touching one of these no compile coverage either - none of the other 64
    # families compiles same7x's board.h - so this is not new. What IS new is that the
    # gap used to be masked by a full matrix and is now the whole answer, which is why
    # ci_set_matrix treats a selection that intersects family_list to NOTHING as
    # unusable (UNSCOPED -> full matrix) rather than emitting an all-empty one.
    # espressif is here because hil-build-esp builds its boards by name rather than by
    # family - though only on hathach/tinyusb: that job is gated on repository_owner,
    # so on a fork an espressif-only PR builds nowhere.
    UNBUILT_FAMILIES = {'cxd56', 'efm32', 'espressif', 'f1c100s', 'pic32mz', 'py32f0',
                        'same7x'}

    def test_every_bsp_family_is_in_the_ci_matrix(self):
        sys.path.insert(0, os.path.join(REPO, '.github/scripts'))
        import ci_set_matrix
        fams = set(ci_select.all_bsp_families(REPO))
        self.assertEqual(fams - set(ci_set_matrix.family_list), self.UNBUILT_FAMILIES,
                         'a hw/bsp family that no toolchain in ci_set_matrix.family_list '
                         'builds: a PR touching only it now selects zero build legs. Wire '
                         'it into family_list, or add it here with a reason.')

    def test_every_get_deps_family_token_resolves_or_is_a_known_alias(self):
        """Same drift guard, dep side. A token naming no hw/bsp dir makes the entry
        unreachable for its family in get_deps.py itself (`f in entry[2].split()`), and
        makes a bump of it select nothing here. The four known ones are pinned; a fifth
        appearing is a real bug in get_deps.py, not something to swallow."""
        sys.path.insert(0, os.path.join(REPO, 'tools'))
        import get_deps
        fams = set(ci_select.all_bsp_families(REPO))
        stale = {}
        for name, d in (('deps_mandatory', get_deps.deps_mandatory),
                        ('deps_optional', get_deps.deps_optional)):
            for path, entry in d.items():
                for tok in str(entry[2]).split():
                    if tok != 'all' and tok not in fams:
                        stale.setdefault(tok, []).append(f'{name}[{path}]')
        # subset, not equality: correcting a token in get_deps.py (fc100s -> f1c100s)
        # should be a one-file change, while a NEW unmappable token - which force-fulls
        # every get_deps edit that touches its entry - has to be a deliberate act
        self.assertFalse(set(stale) - set(ci_select._DEPS_ALIAS_TOKENS),
                         f'get_deps family tokens naming no hw/bsp dir: '
                         f'{ {k: v for k, v in stale.items() if k not in ci_select._DEPS_ALIAS_TOKENS} }')


class TestRostersDoNotOverlap(unittest.TestCase):
    """sel['boards'] is one map across every roster, so a board listed in TWO rosters
    with different test lists would get the union - and hil_test.py on the rig that
    only runs half of them would be handed a -t it has no fixture for. No overlap
    exists today; this is the tripwire for the day one is added."""

    def test_no_board_name_is_in_two_rosters(self):
        seen = {}
        for name in ('tinyusb.json', 'hfp.json'):
            cfg = json.load(open(os.path.join(REPO, 'test/hil', name)))
            for b in cfg['boards']:
                if b['name'] in seen:
                    self.assertEqual(
                        seen[b['name']], b.get('tests'),
                        f"{b['name']}: on two rosters with different test lists - "
                        f"selection_args must then filter per roster, not from the union")
                seen[b['name']] = b.get('tests')


class TestTypecRule(unittest.TestCase):
    """Rule 12b. src/typec/usbc.c is listed unconditionally by src/CMakeLists.txt and
    src/tinyusb.mk, but its whole body is `#if CFG_TUC_ENABLED`, which only
    examples/typec/power_delivery sets - so it is parsed by every build and compiled by
    one. Same shape as the class rule, same answer. Before this rule it matched nothing
    and force-fulled 82 families and all 30 rig boards."""

    def test_build_axis_selects_only_the_typec_examples(self):
        s = ci_select.classify_build(['src/typec/usbc.c'], REPO)
        self.assertFalse(s['full'])
        self.assertTrue(s['families'], 'typec must be compiled somewhere')
        self.assertTrue(s['family_examples'], 'and the examples must be named')
        for fam, exs in s['family_examples'].items():
            self.assertTrue(exs, fam)
            for e in exs:
                self.assertTrue(e.startswith('typec/'), f'{fam}: {e} is not a typec example')

    def test_every_typec_file_answers_the_same(self):
        for f in ('src/typec/usbc.c', 'src/typec/usbc.h', 'src/typec/tcd.h',
                  'src/typec/pd_types.h'):
            s = ci_select.classify_build([f], REPO)
            self.assertFalse(s['full'], f)
            self.assertTrue(s['families'], f)

    def test_no_rig_board_runs_typec(self):
        # typec is not a HIL role, so the rig cannot exercise it whatever it selects
        s = sel(['src/typec/usbc.c'])
        self.assertFalse(s['full'])
        self.assertEqual(s['boards'], {})

    def test_it_tracks_the_enabling_config_rather_than_a_hardcoded_list(self):
        # the answer must come from CFG_TUC_ENABLED in the example configs, so it
        # follows a new typec example (or an old one switched off) on its own
        want = ci_select.examples_enabling(
            ci_select.role_examples(REPO, ('typec',)), ('CFG_TUC_ENABLED',), REPO)
        self.assertTrue(want, 'no example enables CFG_TUC_ENABLED - rule 12b is dead')
        got = set()
        for exs in ci_select.classify_build(['src/typec/usbc.c'], REPO)['family_examples'].values():
            got |= set(exs)
        self.assertEqual(got, want)


class TestCachesAreKeyedOnTheTree(unittest.TestCase):
    """build_utils caches on repo-RELATIVE paths while ci_select._in_repo() chdirs
    between trees, so the cwd has to be part of every cache key. Without it a second
    tree gets the first tree's skip.txt/only.txt and FAMILY_MCUS - which is exactly the
    base-vs-branch comparison the code-size skill does in one process."""

    def test_a_second_tree_is_not_answered_from_the_first(self):
        import build_utils, tempfile
        old = os.getcwd()
        try:
            os.chdir(REPO)
            self.assertFalse(build_utils.skip_example('host/bare_api', 'metro_m0_express'))
            with tempfile.TemporaryDirectory() as d:
                os.makedirs(os.path.join(d, 'hw/bsp'), exist_ok=True)
                os.chdir(d)
                # the board does not exist in this tree at all -> unknown board -> skip
                self.assertTrue(build_utils.skip_example('host/bare_api', 'metro_m0_express'),
                                'the empty tree was answered from the repo tree cache')
            os.chdir(REPO)
            self.assertFalse(build_utils.skip_example('host/bare_api', 'metro_m0_express'),
                             'and the repo answer must survive the excursion')
        finally:
            os.chdir(old)


class TestClassesWithNoEnablingExample(unittest.TestCase):
    """The class rule is the one rule with no drift guard: ports, hw/mcu, get_deps
    tokens and bsp families all have one. A class dir that no example config enables
    selects NOTHING on both axes (the maintainer's empty-means-empty ruling), which is
    right - but it must be a listed state, not a surprise, or a class added before its
    first example silently stops being built."""

    # class dirs no example's tusb_config.h turns on, for either role. Must only shrink:
    # a new entry means a class nothing compiles, so a break in it reaches master.
    NO_EXAMPLE = {'bth'}

    def test_only_the_known_classes_select_nothing(self):
        import glob as _glob
        dead = set()
        for d in sorted(_glob.glob(os.path.join(REPO, 'src/class/*'))):
            if not os.path.isdir(d):
                continue
            cls = os.path.basename(d)
            hit = False
            for base in sorted(os.path.basename(f) for f in _glob.glob(os.path.join(d, '*.[ch]'))):
                roles = ci_select._class_roles(base)
                if ci_select._build_class_examples(cls, base, roles, REPO):
                    hit = True
                    break
            if not hit:
                dead.add(cls)
        self.assertEqual(dead, self.NO_EXAMPLE,
                         'a class dir enabled by no example config: it selects nothing on '
                         'both axes, so nothing compiles it until the next master push')


class TestTheHarnessTestsAreNotTheHarness(unittest.TestCase):
    """test/hil/test/ selects nothing; test/hil/ itself still selects everything.

    Rule 2 is a bare `test/hil/` prefix, so the harness's own unit tests were booking
    the full 27-board rig - ~11 minutes of exclusive hardware for a diff that cannot
    reach it. Nothing on the rig runs them: pre-commit does, and build.yml runs
    test_ci_select.py as the gate before trusting a selection at all.

    The carve-out is only safe while that directory holds nothing rig-affecting, which
    is what the second test pins."""

    def test_the_harness_own_tests_select_nothing_on_either_axis(self):
        for p in ('test/hil/test/test_ci_select.py', 'test/hil/test/test_ci_metrics.py',
                  'test/hil/test/test_hil_bounded.py', 'test/hil/test/stubs/pymtp.py',
                  'test/hil/test/stubs/hid.py'):
            s = ci_select.classify([p], REPO, ROSTERS)
            self.assertFalse(s['full'], p)
            self.assertFalse(s['boards'], p)
            b = ci_select.classify_build([p], REPO)
            self.assertFalse(b['full'], p)
            self.assertFalse(b['families'], p)

    def test_the_harness_itself_still_takes_the_whole_rig(self):
        # the thing rule 2 exists for: these decide what the rig does, so they cannot be
        # trusted to narrow their own blast radius
        for p in ('test/hil/hil_test.py', 'test/hil/tinyusb.json',
                  'test/hil/helper/hil_ci_set_matrix.py'):
            s = ci_select.classify([p], REPO, ROSTERS)
            self.assertTrue(s['full'], f'{p} must still force the full rig')

    def test_nothing_rig_affecting_has_moved_into_the_carve_out(self):
        """The carve-out is a claim about that directory's contents; pin them.

        A new file there that the rig DOES read would silently stop selecting the rig.
        Listing them costs one line per file and makes that a failing test instead."""
        out = subprocess.run(['git', 'ls-files', 'test/hil/test'], cwd=REPO,
                             capture_output=True, text=True, check=True)
        self.assertEqual(sorted(out.stdout.split()), [
            'test/hil/test/stubs/hid.py',
            'test/hil/test/stubs/pymtp.py',
            'test/hil/test/test_ci_metrics.py',
            'test/hil/test/test_ci_select.py',
            'test/hil/test/test_hil_bounded.py',
            'test/hil/test/test_hil_health.py',
            'test/hil/test/test_hil_report.py',
            'test/hil/test/test_hil_rtt.py',
            'test/hil/test/test_hil_util.py',
        ], 'test/hil/test/ gained or lost a file; it is carved out of rule 2, so confirm '
           'the rig still does not read anything in there before updating this list')


class TestExampleMapOmitsFullFamilies(unittest.TestCase):
    """A family whose selection is ALREADY everything it can build carries no -e list.

    Sixth of the same shape as the class below, found the same way: a perf rewrite of
    _prune_buildable dropped the `set(kept) != set(buildable)` test and all 216 tests
    stayed green. The build outcome is identical either way -- build.py applies the same
    skip_example the pruner just did -- so nothing compiled differently and only the
    payload grew (22 families x 33 examples on one dcd_dwc2.c diff). That is exactly the
    kind of drift no build failure ever reports."""

    def test_a_device_only_port_diff_still_omits_families_it_cannot_narrow(self):
        # dcd_dwc2.c selects device+dual examples only, but a family whose host examples
        # are all unbuildable anyway ends up wanting its entire buildable set
        b = ci_select.classify_build(['src/portable/synopsys/dwc2/dcd_dwc2.c'], REPO)
        self.assertFalse(b['full'])
        self.assertTrue(b['families'])
        omitted = [f for f in b['families'] if f not in b['family_examples']]
        self.assertTrue(omitted, 'no family omitted its -e list; the "already everything '
                                 'this family builds" case stopped being detected')
        for fam in omitted:
            self.assertNotIn(fam, b['family_examples'])

    def test_a_family_that_can_build_more_than_the_diff_wants_keeps_its_list(self):
        # the other direction: one example selects itself and nothing else, so every
        # family it lands on must carry an explicit -e or CI builds all 46
        b = ci_select.classify_build(['examples/device/cdc_msc/src/main.c'], REPO)
        self.assertFalse(b['full'])
        for fam in b['families']:
            self.assertEqual(b['family_examples'].get(fam), ['device/cdc_msc'], fam)


class TestSelectionBehavioursThatHadNoTest(unittest.TestCase):
    """Five behaviours a reviewer's mutation pass proved were unpinned: break each one
    and the whole suite stayed green. Each test here fails against its mutant.

    They are grouped because they share a shape - every one is a small expression whose
    removal silently NARROWS the selection, which is the failure direction that merges a
    regression rather than wasting a runner."""

    def test_build_defines_reach_the_prefilter(self):
        # mutant: `defines = ()` in build.py's build_boards_list. metro_m4_express gets
        # MAX3421_HOST=1 from its roster variant, never from its BSP, so without the
        # defines the -e prefilter drops the rig's only MAX3421 firmware and hil-tinyusb
        # has nothing to flash.
        import build as build_py, build_utils, inspect
        src = inspect.getsource(build_py.build_boards_list)
        self.assertIn('defines = tuple(sorted(build_defines))', src,
                      'the -D tokens must reach cmake_board/skip_example')
        old = os.getcwd()
        os.chdir(REPO)
        try:
            ex, board = 'dual/host_info_to_device_cdc', 'metro_m4_express'
            self.assertTrue(build_utils.skip_example(ex, board),
                            'without the define this example is correctly skipped')
            self.assertFalse(build_utils.skip_example(ex, board, ('MAX3421_HOST=1',)),
                             'with it, it must build - that is what the roster passes')
        finally:
            os.chdir(old)

    def test_one_first_prefers_a_board_that_can_build_the_filter(self):
        # mutant: buildable() -> True, i.e. back to all_boards[0]. lpc54's first board
        # skips every msc_file_explorer example, so the leg would compile nothing.
        import build as build_py
        old_env, old = os.environ.get('GITHUB_ACTIONS'), os.getcwd()
        os.environ['GITHUB_ACTIONS'] = 'true'
        os.chdir(REPO)
        try:
            unfiltered = build_py.get_family_boards('lpc54', False, True)
            filtered = build_py.get_family_boards('lpc54', False, True,
                                                  ['host/msc_file_explorer'])
            self.assertEqual(unfiltered, ['lpcxpresso54114'], 'unfiltered pick must not move')
            self.assertNotEqual(filtered, unfiltered,
                                'the -e pick must avoid a board that skips the whole filter')
            import build_utils
            self.assertFalse(build_utils.skip_example('host/msc_file_explorer', filtered[0]),
                             f'{filtered[0]} must actually build the filtered example')
        finally:
            os.chdir(old)
            if old_env is None:
                os.environ.pop('GITHUB_ACTIONS', None)
            else:
                os.environ['GITHUB_ACTIONS'] = old_env

    def test_a_class_file_selects_its_own_macro_not_just_the_directory(self):
        # mutant: delete the _CLS_STEM_RE block. src/class/midi holds MIDI 1.0 AND 2.0;
        # examples/device/midi2_device is the only example enabling CFG_TUD_MIDI2 and the
        # only one that compiles midi2_device.c, but the directory macro alone misses it.
        got = ci_select._build_class_examples('midi', 'midi2_device.c', {'device'}, REPO)
        self.assertIn('device/midi2_device', got,
                      'a midi2 change must select the example that compiles it')
        host = ci_select._build_class_examples('midi', 'midi2_host.c', {'host'}, REPO)
        self.assertIn('host/midi2_host', host)
        # and the plain midi files must NOT drag midi2 in
        plain = ci_select._build_class_examples('midi', 'midi_device.c', {'device'}, REPO)
        self.assertNotIn('device/midi2_device', plain)

    def test_a_port_change_selects_the_dual_examples(self):
        # mutant: drop `+ ('dual',)`. A dcd/hcd change must build the dual examples -
        # they exercise both stacks on one board, so a dwc2 break lands there first.
        s = ci_select.classify_build(['src/portable/synopsys/dwc2/dcd_dwc2.c'], REPO)
        duals = {e for exs in s['family_examples'].values() for e in exs
                 if e.startswith('dual/')}
        self.assertTrue(duals, 'a dcd change selected no dual example')

    def test_the_selector_answers_the_same_with_and_without_ci_env(self):
        # mutant: drop ci=True from _prune_buildable. ci_skip_boards/ci_preferred_boards
        # only apply when GITHUB_ACTIONS/CIRCLECI is set, so without the pin a laptop and
        # a runner disagree - and /pre-pr would report a family list CI will not build.
        files = ['examples/host/cdc_msc_hid_freertos/src/main.c']
        old = os.environ.get('GITHUB_ACTIONS')
        os.environ.pop('GITHUB_ACTIONS', None)
        try:
            local = ci_select.classify_build(files, REPO)['families']
            os.environ['GITHUB_ACTIONS'] = 'true'
            import importlib
            importlib.reload(ci_select)
            runner = ci_select.classify_build(files, REPO)['families']
        finally:
            if old is None:
                os.environ.pop('GITHUB_ACTIONS', None)
            else:
                os.environ['GITHUB_ACTIONS'] = old
            import importlib
            importlib.reload(ci_select)
        self.assertEqual(local, runner, 'the selector must not depend on the CI env vars')


class TestRuleTableIsCarbonOfTheSpec(unittest.TestCase):
    """ci_select's module docstring carries the rule table so a reader landing in the
    code does not have to open the spec to learn what rule 6 is. Both are maintained by
    hand, so this pins them cell-for-cell: edit one without the other and this fails.

    It also pins the table against the CODE - every rule id the docstring claims must
    appear as a `# rule N` marker on a branch of _classify_build_one, so a row cannot be
    documented without a branch, or a branch renumbered without the table."""

    @staticmethod
    def _rows(text):
        import re as _re
        out = []
        for l in text.splitlines():
            if not l.startswith('| '):
                continue
            c = [x.strip() for x in l.strip().strip('|').split('|')]
            if len(c) == 5 and _re.fullmatch(r'\d+[a-z]?', c[0]):
                out.append(c)
        return out

    def test_docstring_table_matches_the_spec(self):
        spec = open(os.path.join(
            REPO, 'docs/superpowers/specs/2026-08-19-ci-build-family-filter-design.md')).read()
        doc, spec_rows = self._rows(ci_select.__doc__), self._rows(spec)
        self.assertTrue(spec_rows, 'no rule table found in the spec')
        self.assertEqual([r[0] for r in doc], [r[0] for r in spec_rows],
                         'rule ids differ between ci_select.__doc__ and the spec')
        for d, s in zip(doc, spec_rows):
            self.assertEqual(d, s, f'rule {d[0]} differs between the docstring and the spec')

    def test_every_documented_rule_has_a_branch(self):
        import re as _re
        src = open(os.path.join(REPO, 'tools/ci_select.py')).read()
        marked = set()
        # handles `# rule 6`, `# rules 1, 1b` and `# rules 8-10`
        for m in _re.finditer(r'#\s*rules?\s+([0-9a-z, -]+)', src):
            for tok in _re.split(r',\s*', m.group(1).strip()):
                rng = _re.fullmatch(r'(\d+)\s*-\s*(\d+)', tok.strip())
                if rng:
                    marked.update(str(n) for n in range(int(rng.group(1)), int(rng.group(2)) + 1))
                elif _re.fullmatch(r'\d+[a-z]?', tok.strip()):
                    marked.add(tok.strip())
        documented = {r[0] for r in self._rows(ci_select.__doc__)}
        missing = sorted(documented - marked, key=lambda s: (int(_re.match(r'\d+', s).group()), s))
        self.assertEqual(missing, [], f'documented rules with no `# rule N` branch marker: {missing}')


class TestNoTrackedFileIsUnclassified(unittest.TestCase):
    """Rule 17 (unclassified -> full on both axes) is the fail-open net for paths nobody
    anticipated. It must stay that way - a wrong `full` costs runner minutes and is
    visible in the run, a wrong `empty` costs a merged regression and is invisible - but
    nothing in the tree should REACH it. Every tracked file is classified by a rule, so
    17 fires only for genuinely new shapes, and this test is what tells the author to
    write the row instead of letting the fall-through pick an answer for them.

    Before this guard, 254 tracked files reached 17: .gitignore took a docs-only PR to
    74 cmake legs and the whole rig, while examples/<role>/CMakeLists.txt got the RIGHT
    answer from the wrong rule - row 15 names it, the regex never matched it."""

    def _unclassified(self, axis):
        import subprocess as sp
        r = sp.run(['git', 'ls-files'], cwd=REPO, capture_output=True, text=True)
        if r.returncode != 0:
            self.skipTest('not a git checkout')
        files = r.stdout.split()
        self.assertGreater(len(files), 1000, 'suspiciously few tracked files')
        out = []
        for f in files:
            s = (ci_select.classify_build([f], REPO) if axis == 'build'
                 else ci_select.classify([f], REPO, real_rosters()))
            if any('unclassified' in why for why in s['reasons']):
                out.append(f)
        return out

    def test_build_axis(self):
        left = self._unclassified('build')
        self.assertEqual(left, [], f'{len(left)} tracked files fall through to rule 17 on '
                                   f'the build axis, e.g. {left[:5]} - classify them, or '
                                   f'add the pattern to _META_RE if no build reads them')

    def test_hil_axis(self):
        left = self._unclassified('hil')
        self.assertEqual(left, [], f'{len(left)} tracked files fall through to rule 17 on '
                                   f'the HIL axis, e.g. {left[:5]}')


class TestLibRule(unittest.TestCase):
    """lib/** is not a full-matrix path: only the examples that build the lib need it."""

    def b(self, files):
        return ci_select.classify_build(files, REPO)

    def test_lib_examples_ground_truth(self):
        self.assertEqual(ci_select.lib_examples('embedded-cli', REPO),
                         {'host/msc_file_explorer', 'host/msc_file_explorer_freertos'})
        self.assertEqual(ci_select.lib_examples('networking', REPO),
                         {'device/net_lwip_webserver'})
        # only family_support.cmake's LOGGER=rtt plumbing names it, and no CI example
        # build turns that on - the scan is per-example on purpose
        self.assertEqual(ci_select.lib_examples('SEGGER_RTT', REPO), set())
        self.assertEqual(ci_select.lib_examples('rt-thread', REPO), set())

    def test_lib_examples_matches_at_a_directory_boundary(self):
        # 'lib/net' must not inherit lib/networking's example
        self.assertEqual(ci_select.lib_examples('net', REPO), set())

    def test_build_lib_selects_only_the_using_examples(self):
        s = self.b(['lib/embedded-cli/embedded_cli.h'])
        self.assertFalse(s['full'])
        self.assertTrue(s['families'])
        want = {'host/msc_file_explorer', 'host/msc_file_explorer_freertos'}
        mapped = set()
        for fam, exs in s['family_examples'].items():
            self.assertTrue(set(exs) <= want, f'{fam}: {exs}')
            mapped |= set(exs)
        self.assertEqual(mapped, want)

    def test_build_lib_nobody_builds_selects_nothing(self):
        s = self.b(['lib/SEGGER_RTT/RTT/SEGGER_RTT.c'])
        self.assertFalse(s['full'])
        self.assertEqual(s['families'], [])
        self.assertEqual(s['family_examples'], {})

    def test_hil_lib_selects_the_using_tests(self):
        s = sel(['lib/embedded-cli/embedded_cli.h'])
        self.assertFalse(s['full'])
        want = {'host/msc_file_explorer', 'host/msc_file_explorer_freertos'}
        self.assertEqual(set(s['boards']['raspberry_pi_pico']), want)
        self.assertEqual(set(s['boards']['raspberry_pi_pico2']), want)
        # device-only board and the only-list board run neither test
        self.assertNotIn('stm32f407disco', s['boards'])
        self.assertNotIn('espressif_s3_devkitm', s['boards'])

    def test_hil_lib_used_only_by_a_disabled_test_selects_nothing(self):
        # device/net_lwip_webserver is commented out of hil_util.device_tests, so the
        # intersection with the HIL universe is empty
        s = sel(['lib/networking/dhserver.c'])
        self.assertFalse(s['full'])
        self.assertEqual(s['boards'], {})

    def test_hil_lib_nobody_builds_selects_nothing(self):
        s = sel(['lib/SEGGER_RTT/RTT/SEGGER_RTT.c'])
        self.assertFalse(s['full'])
        self.assertEqual(s['boards'], {})


# A miniature get_deps.py: the module shape the parser must cope with (imports,
# both dep dicts, the derived deps_all, a function) without the real 300-entry file.
_GD_BASE = """#!/usr/bin/env python3
import argparse

deps_mandatory = {
    'lib/fatfs': ['https://github.com/abbrev/fatfs.git', 'aaa', 'all'],
}

deps_optional = {
    'hw/mcu/st/cmsis_device_f4': ['https://github.com/x/f4.git', 'bbb', 'stm32f4 stm32f7'],
    'hw/mcu/nordic/nrfx': ['https://github.com/x/nrfx.git', 'ccc', 'nrf'],
}

deps_all = {**deps_mandatory, **deps_optional}


def main():
    return 1
"""


class TestGetDepsChangedFamilies(unittest.TestCase):
    """Pure text-in, families-out: no git, no exec of the parsed module."""

    def f(self, head, base=_GD_BASE):
        return ci_select.get_deps_changed_families(base, head, REPO)

    def test_no_change_selects_nothing(self):
        self.assertEqual(self.f(_GD_BASE), set())

    def test_comment_only_change_selects_nothing(self):
        self.assertEqual(self.f(_GD_BASE.replace('import argparse',
                                                 'import argparse  # noqa')), set())

    def test_optional_commit_bump_selects_its_families(self):
        self.assertEqual(self.f(_GD_BASE.replace("'bbb'", "'bbb2'")),
                         {'stm32f4', 'stm32f7'})

    def test_mandatory_all_entry_is_full(self):
        self.assertIsNone(self.f(_GD_BASE.replace("'aaa'", "'aaa2'")))

    def test_logic_change_is_full(self):
        self.assertIsNone(self.f(_GD_BASE.replace('return 1', 'return 2')))

    def test_unparseable_text_is_full(self):
        self.assertIsNone(self.f('def broken(:\n'))

    def test_unresolvable_token_is_full(self):
        # a changed entry we cannot map to a family is NOT "nothing changed": reading it
        # that way empties the whole build matrix for a dep bump. Fall open instead -
        # even when a sibling token does resolve, because the unmapped one may be the
        # family that actually needed the new revision
        base = _GD_BASE.replace("'ccc', 'nrf'", "'ccc', 'zz_gone samd5x_e5x'")
        self.assertIsNone(self.f(base.replace("'ccc'", "'ccc2'"), base))
        base = _GD_BASE.replace("'ccc', 'nrf'", "'ccc', 'zz_gone'")
        self.assertIsNone(self.f(base.replace("'ccc'", "'ccc2'"), base))

    def test_family_token_change_unions_both_sides(self):
        # the family list itself edited: both sides contribute
        head = _GD_BASE.replace("'ccc', 'nrf'", "'ccc', 'rp2040 samd5x_e5x'")
        self.assertEqual(self.f(head), {'nrf', 'rp2040', 'samd5x_e5x'})

    def test_known_alias_tokens_select_nothing(self):
        # the tokens in _DEPS_ALIAS_TOKENS name no hw/bsp dir: either a pre-rename
        # spelling sitting beside the current name in the same entry, or a family with
        # no boards in the tree. Changing one selects nothing rather than force-fulling
        # every get_deps edit that touches its entry.
        base = _GD_BASE.replace("'ccc', 'nrf'", "'ccc', 'stm32l5'")
        self.assertEqual(self.f(base.replace("'ccc'", "'ccc2'"), base), set())

    def test_moving_an_entry_between_the_two_dicts_is_seen(self):
        # value untouched, dict changed: mandatory deps are fetched for every family, so
        # demoting one stops families fetching it. Merging the dicts before diffing (or
        # comparing the ast dump of deps_all) hides this completely.
        head = _GD_BASE.replace(
            "    'hw/mcu/nordic/nrfx': ['https://github.com/x/nrfx.git', 'ccc', 'nrf'],\n", '')
        head = head.replace(
            "deps_mandatory = {\n",
            "deps_mandatory = {\n    'hw/mcu/nordic/nrfx': ['https://github.com/x/nrfx.git', 'ccc', 'nrf'],\n")
        self.assertEqual(self.f(head), {'nrf'})

    def test_added_entry_selects_its_families(self):
        head = _GD_BASE.replace(
            "deps_optional = {\n",
            "deps_optional = {\n    'hw/mcu/x': ['https://github.com/x/x.git', 'ddd', 'rp2040'],\n")
        self.assertEqual(self.f(head), {'rp2040'})

    def test_removed_entry_selects_its_base_side_families(self):
        head = _GD_BASE.replace(
            "    'hw/mcu/nordic/nrfx': ['https://github.com/x/nrfx.git', 'ccc', 'nrf'],\n", '')
        self.assertEqual(self.f(head), {'nrf'})

    def test_family_list_change_unions_both_sides(self):
        head = _GD_BASE.replace("'stm32f4 stm32f7'", "'stm32f4 stm32h7'")
        self.assertEqual(self.f(head), {'stm32f4', 'stm32f7', 'stm32h7'})

    def test_real_get_deps_parses(self):
        with open(os.path.join(REPO, 'tools/get_deps.py')) as f:
            real = f.read()
        self.assertEqual(ci_select.get_deps_changed_families(real, real, REPO), set())
        # a real optional entry bumped resolves to that entry's real family. The commit
        # is read out of get_deps.py rather than pinned here - a routine dep bump must
        # not fail this suite, and pinning a hash tests the tree, not the code
        sys.path.insert(0, os.path.join(REPO, 'tools'))
        import get_deps
        commit, tokens = get_deps.deps_optional['hw/mcu/nordic/nrfx'][1:3]
        bumped = real.replace(commit, '0' * len(commit))
        self.assertNotEqual(bumped, real)
        self.assertEqual(ci_select.get_deps_changed_families(real, bumped, REPO),
                         set(tokens.split()))


class TestGetDepsRule(unittest.TestCase):
    """tools/get_deps.py: the changed dep entries' families, or full when unknowable."""

    def test_build_selects_the_changed_families(self):
        s = ci_select.classify_build(['tools/get_deps.py'], REPO,
                                     get_deps_families={'stm32f4'})
        self.assertFalse(s['full'])
        self.assertEqual(s['families'], ['stm32f4'])
        self.assertNotIn('stm32f4', s['family_examples'])   # every example it builds

    def test_build_without_a_base_is_full(self):
        # --diff-file mode has no git and so no base content: fail open
        self.assertTrue(ci_select.classify_build(['tools/get_deps.py'], REPO)['full'])

    def test_build_no_dep_entry_changed_selects_nothing(self):
        s = ci_select.classify_build(['tools/get_deps.py'], REPO, get_deps_families=set())
        self.assertFalse(s['full'])
        self.assertEqual(s['families'], [])

    def test_hil_selects_the_changed_families_boards(self):
        s = ci_select.classify(['tools/get_deps.py'], REPO, ROSTERS,
                               get_deps_families={'stm32f4'})
        self.assertFalse(s['full'])
        self.assertEqual(list(s['boards']), ['stm32f407disco'])
        self.assertEqual(s['families'], ['stm32f4'])

    def test_hil_without_a_base_is_full(self):
        self.assertTrue(ci_select.classify(['tools/get_deps.py'], REPO, ROSTERS)['full'])

    def test_hil_no_dep_entry_changed_selects_nothing(self):
        s = ci_select.classify(['tools/get_deps.py'], REPO, ROSTERS, get_deps_families=set())
        self.assertFalse(s['full'])
        self.assertEqual(s['boards'], {})

    def test_cli_diff_file_mode_is_full(self):
        import tempfile, json as j
        with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
            f.write('tools/get_deps.py\n')
            path = f.name
        r = subprocess.run([sys.executable, os.path.join(REPO, 'tools/ci_select.py'),
                            '--diff-file', path, os.path.join(REPO, 'test/hil/tinyusb.json')],
                           capture_output=True, text=True)
        os.unlink(path)
        self.assertEqual(r.returncode, 0, r.stderr)
        out = j.loads(r.stdout)
        self.assertTrue(out['full'])
        self.assertTrue(out['build']['full'])


class TestGetDepsGitPlumbing(unittest.TestCase):
    """--base mode: merge-base, the diff, and both blobs come from git, and only
    tools/get_deps.py in the diff triggers the blob reads."""

    HEAD = _GD_BASE.replace("'bbb'", "'bbb2'")

    def run_main(self, diff):
        from unittest import mock
        calls = []

        def fake_run(argv, **kw):
            calls.append(argv)
            if argv[:2] == ['git', 'merge-base']:
                out = 'MB123\n'
            elif argv[:3] == ci_select.GIT_DIFF_ARGV[:3]:
                out = diff
            elif argv[:2] == ['git', 'show']:
                out = _GD_BASE if argv[2].startswith('MB123:') else self.HEAD
            else:
                raise AssertionError(f'unexpected git call: {argv}')
            return subprocess.CompletedProcess(argv, 0, stdout=out, stderr='')

        buf = io.StringIO()
        argv = [sys.executable, '--base', 'origin/master']
        with mock.patch.object(ci_select.subprocess, 'run', fake_run), \
             mock.patch.object(sys, 'argv', argv), \
             contextlib.redirect_stdout(buf), contextlib.redirect_stderr(io.StringIO()):
            ci_select.main()
        return json.loads(buf.getvalue()), calls

    def test_base_mode_reads_the_merge_base_blob(self):
        out, calls = self.run_main('tools/get_deps.py\n')
        self.assertIn(['git', 'show', 'MB123:tools/get_deps.py'], calls)
        self.assertIn(['git', 'show', 'HEAD:tools/get_deps.py'], calls)
        self.assertFalse(out['build']['full'])
        self.assertEqual(out['build']['families'], ['stm32f4', 'stm32f7'])

    def test_no_get_deps_in_the_diff_reads_no_blob(self):
        out, calls = self.run_main('src/class/cdc/cdc_device.c\n')
        self.assertFalse(any(c[:2] == ['git', 'show'] for c in calls))
        self.assertFalse(out['build']['full'])

    def test_git_failure_falls_open(self):
        from unittest import mock

        def fake_run(argv, **kw):
            if argv[:2] == ['git', 'show']:
                raise subprocess.CalledProcessError(128, argv)
            out = 'MB123\n' if argv[:2] == ['git', 'merge-base'] else 'tools/get_deps.py\n'
            return subprocess.CompletedProcess(argv, 0, stdout=out, stderr='')

        buf = io.StringIO()
        with mock.patch.object(ci_select.subprocess, 'run', fake_run), \
             mock.patch.object(sys, 'argv', [sys.executable, '--base', 'origin/master']), \
             contextlib.redirect_stdout(buf), contextlib.redirect_stderr(io.StringIO()):
            ci_select.main()
        self.assertTrue(json.loads(buf.getvalue())['build']['full'])


class TestBuildClassifier(unittest.TestCase):
    def b(self, files):
        return ci_select.classify_build(files, REPO)

    def test_noncode_and_test_hil_contribute_nothing(self):        # rules 1, 2
        s = self.b(['docs/info/index.rst', 'README.rst', 'test/hil/hil_test.py', '.claude/skills/hil/SKILL.md'])
        self.assertFalse(s['full'])
        self.assertEqual(s['families'], [])
        self.assertEqual(s['family_examples'], {})

    def test_port_device_rule(self):                               # rule 3
        s = self.b(['src/portable/raspberrypi/rp2040/dcd_rp2040.c'])
        self.assertFalse(s['full'])
        self.assertEqual(s['families'], ['rp2040'])
        exs = s['family_examples']['rp2040']
        self.assertIn('device/cdc_msc', exs)
        self.assertFalse(any(e.startswith(('host/', 'typec/')) for e in exs))
        # dual inclusion asserted on the pure role helper: whether a dual example
        # survives Task 4's buildability pruning depends on the environment-gated
        # CI board pick, so the classifier-output assertion must not rely on it
        self.assertIn('dual/host_info_to_device_cdc',
                      ci_select.role_examples(REPO, ('device', 'dual')))
        self.assertNotIn('host/bare_api', ci_select.role_examples(REPO, ('device', 'dual')))

    def test_port_host_rule(self):                                 # rule 4
        s = self.b(['src/portable/analog/max3421/hcd_max3421.c'])
        self.assertFalse(s['full'])
        # rp2040's family.cmake unconditionally lists hcd_max3421.c as a source of its
        # tinyusb_host_max3421 INTERFACE lib (linked only when MAX3421_HOST=1, e.g. the
        # real feather_rp2040_max3421 board) and espressif's component CMakeLists also
        # references it — so the raw (unpruned) scan legitimately finds both; Task 4's
        # buildability post-filter is what may later prune either away
        # non-empty FIRST: a subset assertion is satisfied by set(), and since ports are
        # now empty-means-empty (fail-closed) an unnoticed regression to zero families
        # would select no build leg at all and merge an uncompiled HCD
        self.assertTrue(s['families'], 'a host-port change must select some family')
        self.assertLessEqual(set(s['families']), {'espressif', 'rp2040'})
        self.assertTrue(s['family_examples'], 'and must name the examples for them')
        for exs in s['family_examples'].values():
            self.assertTrue(exs)
            self.assertFalse(any(e.startswith(('device/', 'typec/')) for e in exs))

    def test_port_shared_file_selects_all_examples(self):          # rule 5
        s = self.b(['src/portable/synopsys/dwc2/dwc2_common.c'])
        self.assertFalse(s['full'])
        self.assertIn('stm32f4', s['families'])
        self.assertNotIn('rp2040', s['families'])
        self.assertNotIn('stm32f4', s['family_examples'])   # 'all' => no map key

    def test_bsp_family_rule(self):                                # rule 6
        s = self.b(['hw/bsp/stm32f4/boards/stm32f407disco/board.h'])
        self.assertEqual(s['families'], ['stm32f4'])
        self.assertNotIn('stm32f4', s['family_examples'])

    def test_bsp_top_level_file_is_full(self):                     # rule 16
        self.assertTrue(self.b(['hw/bsp/board.c'])['full'])
        self.assertTrue(self.b(['hw/bsp/family_support.cmake'])['full'])

    def test_mcu_rule(self):                                       # rule 7
        s = self.b(['hw/mcu/nordic/nrf5x/nrf_clock.h'])
        self.assertEqual(s['families'], ['nrf'])
        # empty means empty: no family's build references the path, so no build
        # compiles it - nothing to select
        s = self.b(['hw/mcu/no_such_vendor/x.c'])
        self.assertFalse(s['full'])
        self.assertEqual(s['families'], [])
        self.assertEqual(s['family_examples'], {})

    def test_class_device_rule(self):                              # rule 8
        s = self.b(['src/class/cdc/cdc_device.c'])
        self.assertFalse(s['full'])
        # near-all families (Task 4's pruning may drop a few); never equality
        # against all_bsp_families — that's a tuple, and pruning shrinks the list
        self.assertIn('stm32f4', s['families'])
        self.assertGreater(len(s['families']), 50)
        exs = s['family_examples']['stm32f4']
        self.assertIn('device/cdc_msc', exs)
        self.assertNotIn('device/hid_composite', exs)
        self.assertNotIn('host/cdc_msc_hid', exs)          # TUH_CDC examples are rule 9's

    def test_class_host_rule(self):                                # rule 9
        s = self.b(['src/class/msc/msc_host.c'])
        exs = s['family_examples']['stm32f4']
        self.assertIn('host/msc_file_explorer', exs)
        self.assertNotIn('device/cdc_msc', exs)

    def test_class_shared_header_and_include_edge(self):           # rule 10
        s = self.b(['src/class/audio/audio.h'])
        exs = s['family_examples']['stm32f4']
        self.assertIn('device/audio_test', exs)
        self.assertIn('device/midi_test', exs)              # midi headers include audio.h

    def test_core_device_rule(self):                               # rule 11
        s = self.b(['src/device/usbd.c'])
        exs = s['family_examples']['stm32f4']
        self.assertIn('device/cdc_msc', exs)
        # no dual In-assertion: dual examples are only.txt-gated to max3421/pio-usb
        # boards, so pruning legitimately drops them on a plain stm32f4 board
        self.assertFalse(any(e.startswith(('host/', 'typec/')) for e in exs))

    def test_core_host_rule(self):                                 # rule 12
        s = self.b(['src/host/usbh.c'])
        exs = s['family_examples']['stm32f4']
        self.assertFalse(any(e.startswith(('device/', 'typec/')) for e in exs))

    def test_example_rule(self):                                   # rules 13, 14
        s = self.b(['examples/device/cdc_msc/src/main.c'])
        self.assertEqual(s['family_examples']['stm32f4'], ['device/cdc_msc'])
        s = self.b(['examples/device/board_test/src/main.c'])
        self.assertEqual(s['family_examples']['stm32f4'], ['device/board_test'])
        s = self.b(['examples/device/no_such_example/src/main.c'])  # deleted example: nothing
        self.assertFalse(s['full'])
        self.assertEqual(s['families'], [])

    def test_full_paths(self):                                     # rules 15-17
        for p in ('src/common/tusb_fifo.c', 'src/osal/osal.h', 'src/tusb.c',
                  'src/tusb_option.h',
                  'tools/build.py', 'tools/cmake/cpu/cortex-m4.cmake',
                  'examples/CMakeLists.txt', 'examples/device/CMakeLists.txt',
                  'examples/build_system/cmake/cpu.cmake', '.github/workflows/build.yml',
                  '.circleci/config.yml', 'src/CMakeLists.txt', 'src/tinyusb.mk',
                  'hw/bsp/family_support.mk', 'tools/build_utils.py',
                  'some/unknown/path.c'):
            self.assertTrue(self.b([p])['full'], p)

    def test_repo_metadata_is_not_a_build_input(self):
        # these used to reach `full` through rule 17: a PR touching only .gitignore and a
        # README created 74 cmake legs and booked the whole rig. No Build step reads them.
        for p in ('sonar-project.properties', '.gitignore', '.gitattributes',
                  '.clang-format', '.idea/misc.xml', 'version.yml', 'library.json',
                  'examples/CMakePresets.json', 'test/fuzz/fuzz.cc',
                  'test/unit-test/project.yml', '.github/workflows/pr_comment.yml',
                  'tools/gen_doc.py'):
            s = self.b([p])
            self.assertFalse(s['full'], p)
            self.assertEqual(s['families'], [], p)

    def test_the_build_machinery_is_still_full(self):
        # the other side of the same line: these DECIDE what gets built
        for p in ('.circleci/config.yml', '.github/workflows/build.yml',
                  '.github/scripts/ci_set_matrix.py', 'tools/ci_select.py',
                  'tools/build_utils.py', 'tools/metrics.py'):
            self.assertTrue(self.b([p])['full'], p)

    def test_mixed_diff_unions_per_family(self):
        s = self.b(['src/portable/raspberrypi/rp2040/dcd_rp2040.c', 'src/class/cdc/cdc_device.c'])
        self.assertFalse(s['full'])
        self.assertIn('stm32f4', s['families'])
        self.assertGreater(len(s['families']), 50)
        self.assertIn('device/hid_composite', s['family_examples']['rp2040'])   # from the dcd rule
        self.assertNotIn('device/hid_composite', s['family_examples']['stm32f4'])  # cdc-only there

    def test_example_names_are_real_dirs(self):
        for ex in ci_select.all_examples(REPO):
            role, name = ex.split('/')
            self.assertTrue(os.path.isdir(os.path.join(REPO, 'examples', role, name)), ex)
            self.assertRegex(ex, r'^(device|dual|host|typec)/[A-Za-z0-9_]+$')


class TestBuildPostFilter(unittest.TestCase):
    def test_kept_examples_are_buildable(self):
        import build_utils, build as build_py
        s = ci_select.classify_build(['src/class/msc/msc_host.c'], REPO)
        self.assertFalse(s['full'])
        # families that cannot build a single TUH_MSC example drop out entirely
        self.assertNotIn('msp430', s['families'])
        old = os.getcwd()
        os.chdir(REPO)
        try:
            # buildable on SOME board of the family - CircleCI builds them all
            for fam, exs in s['family_examples'].items():
                boards = build_py.get_family_boards(fam, False, False)
                for e in exs:
                    self.assertTrue(any(not build_utils.skip_example(e, b) for b in boards),
                                    f'{fam}: {e}')
        finally:
            os.chdir(old)

    def test_unfiltered_family_has_no_map_key(self):
        s = ci_select.classify_build(['hw/bsp/stm32f4/family.c'], REPO)
        self.assertEqual(s['families'], ['stm32f4'])
        self.assertEqual(s['family_examples'], {})

    def test_espressif_prunes_to_what_its_build_path_can_build(self):
        # build.py's espressif branch builds get_examples('espressif') only (the
        # *_freertos examples plus a short extra list), so keeping espressif for a
        # device/mtp diff spins CircleCI's most expensive leg up to skip everything
        s = ci_select.classify_build(['examples/device/mtp/src/main.c'], REPO)
        self.assertFalse(s['full'])
        self.assertNotIn('espressif', s['families'])

    def test_espressif_survives_an_example_it_does_build(self):
        s = ci_select.classify_build(['examples/device/cdc_msc_freertos/src/main.c'], REPO)
        self.assertFalse(s['full'])
        self.assertIn('espressif', s['families'])

    def test_ra_survives_the_dual_example_prune(self):
        # ra's only buildable dual example is gated on only.txt's mcu:ra6m5, which
        # exists only if the ${MCU_VARIANT} token in FAMILY_MCUS resolves
        s = ci_select.classify_build(
            ['examples/dual/host_info_to_device_cdc/src/main.c'], REPO)
        self.assertFalse(s['full'])
        self.assertIn('ra', s['families'], s['families'])

    def test_deleted_family_dir_does_not_crash(self):
        # rule 6 extracts a family from the path; a PR that deletes or renames
        # hw/bsp/<fam> used to traceback in get_family_boards' scandir
        s = ci_select.classify_build(['hw/bsp/no_such_family_xyz/family.cmake'], REPO)
        self.assertFalse(s['full'])
        self.assertEqual(s['families'], [])
        self.assertTrue(any('gone from tree' in r for r in s['reasons']), s['reasons'])

    def test_class_source_selecting_nothing_selects_nothing(self):
        # a class-with-no-enabling-config case: no config enables CFG_TUH_VENDOR, so
        # nothing exercises it and nothing builds - empty means empty (maintainer
        # decision; the file is still parsed by every full master-push build, which is
        # the accepted net for a break outside its #if guard). src/class/bth is the
        # live instance of this state today; TestClassesWithNoEnablingExample pins the
        # whole set, so a new one cannot appear unnoticed.
        # src/class/bth/bth_device.c, a file that EXISTS: the old assertion named
        # src/class/vendor/vendor_host.c, deleted by the same branch, so any made-up
        # path reached the same branch and the test passed vacuously.
        real = os.path.join(REPO, 'src/class/bth/bth_device.c')
        self.assertTrue(os.path.isfile(real), 'the case needs a file that exists')
        s = ci_select.classify_build(['src/class/bth/bth_device.c'], REPO)
        self.assertFalse(s['full'])
        self.assertEqual(s['families'], [])
        self.assertTrue(any('no contribution' in r for r in s['reasons']), s['reasons'])
        # and the reason must name the class, not just any empty answer
        self.assertTrue(any('bth' in r for r in s['reasons']), s['reasons'])

    def test_class_source_with_examples_still_scopes(self):
        s = ci_select.classify_build(['src/class/cdc/cdc_device.c'], REPO)
        self.assertFalse(s['full'])

    def test_no_stdout_pollution(self):
        # get_family_boards prints on odd families; the selector's stdout is JSON
        import io, contextlib
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            ci_select.classify_build(['src/class/msc/msc_host.c'], REPO)
        self.assertEqual(buf.getvalue(), '')


class TestNoContributionPaths(unittest.TestCase):
    """Paths that are inside build.yml's code filter but cannot change a compiled byte.
    Unclassified means FULL on both axes, so a metrics-only PR would otherwise cost the
    whole build matrix plus an exclusive full-rig sweep - where master ran nothing."""

    def test_metrics_scripts_run_on_no_board_but_still_build(self):
        # HIL axis only. tools/metrics.py IS executed by a build - examples/CMakeLists.txt
        # makes it the `tinyusb_metrics` target and build_util.yml adds
        # `--target tinyusb_metrics` - so the build axis must keep exercising it, or a
        # break merges green and reds the next master push. Nothing on the rig runs it.
        for p in ('tools/metrics.py', '.github/scripts/metrics_pair_compare.py'):
            h = sel([p])
            self.assertFalse(h['full'], p)
            self.assertEqual(h['boards'], {}, p)
            self.assertTrue(ci_select.classify_build([p], REPO)['full'], p)

    def test_typec_example_builds_but_runs_nothing(self):
        # examples/typec is compiled by the build matrix and run by no rig board; the
        # HIL walk used to not recognise the role at all -> unclassified -> full rig
        p = 'examples/typec/power_delivery/src/main.c'
        h = sel([p])
        self.assertFalse(h['full'])
        self.assertEqual(h['boards'], {})
        b = ci_select.classify_build([p], REPO)
        self.assertFalse(b['full'])
        self.assertTrue(b['families'], 'typec still has to be compiled somewhere')


class TestHilExamples(unittest.TestCase):
    def test_board_test_always_present_and_full_emits(self):
        s = ci_select.classify(['src/common/tusb_fifo.c'], REPO, ROSTERS)  # full
        he = ci_select.hil_examples(s, ROSTERS)
        self.assertEqual(set(he), {b['name'] for b in ROSTER})
        for name, exs in he.items():
            self.assertIn('device/board_test', exs)

    def test_narrowed_board_gets_chosen_tests_only(self):
        s = ci_select.classify(['examples/device/cdc_msc/src/main.c'], REPO, ROSTERS)
        he = ci_select.hil_examples(s, ROSTERS)
        self.assertEqual(he['stm32f407disco'], ['device/board_test', 'device/cdc_msc'])

    def test_full_board_gets_its_whole_test_list(self):
        s = ci_select.classify(['hw/bsp/stm32f4/boards/stm32f407disco/board.h'], REPO, ROSTERS)
        he = ci_select.hil_examples(s, ROSTERS)
        want = set(ci_select.board_tests(ROSTER[1])) | {'device/board_test'}
        self.assertEqual(set(he['stm32f407disco']), want)
        self.assertNotIn('raspberry_pi_pico', he)   # deselected board: no firmware needed


class TestHilExamplesDuplicateRosters(unittest.TestCase):
    """Rosters are disjoint today, but a board moved between rigs (or listed on both
    during a migration) must get the UNION of its test lists: superset firmware is
    harmless, a missing image fails the run on whichever rig lost the coin toss."""

    ROSTERS = [
        ('test/hil/a.json', [{'name': 'dup_board', 'uid': 'd1', 'flasher': {'name': 'jlink'},
                              'tests': {'only': ['device/cdc_msc']}}]),
        ('test/hil/b.json', [{'name': 'dup_board', 'uid': 'd1', 'flasher': {'name': 'jlink'},
                              'tests': {'only': ['device/hid_boot_interface']}}]),
    ]

    def test_duplicate_board_unions_the_test_lists(self):
        he = ci_select.hil_examples({'full': True, 'boards': {}}, self.ROSTERS)
        self.assertEqual(he['dup_board'],
                         ['device/board_test', 'device/cdc_msc',
                          'device/hid_boot_interface'])


class TestCliJson(unittest.TestCase):
    def test_build_key_without_rosters(self):
        r = subprocess.run([sys.executable, os.path.join(REPO, 'tools/ci_select.py'),
                            '--diff-file', '/dev/null'], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        j = json.loads(r.stdout)
        self.assertIn('build', j)
        self.assertNotIn('hil_examples', j)     # rosters not given

    def test_build_and_hil_keys_with_rosters(self):
        import tempfile
        with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
            f.write('src/portable/raspberrypi/rp2040/dcd_rp2040.c\n')
            df = f.name
        r = subprocess.run([sys.executable, os.path.join(REPO, 'tools/ci_select.py'),
                            '--diff-file', df, os.path.join(REPO, 'test/hil/tinyusb.json')],
                           capture_output=True, text=True)
        os.unlink(df)
        j = json.loads(r.stdout)
        self.assertEqual(j['build']['families'], ['rp2040'])
        self.assertIn('hil_examples', j)
        for exs in j['hil_examples'].values():
            self.assertIn('device/board_test', exs)


SET_MATRIX = os.path.join(REPO, '.github/scripts/ci_set_matrix.py')


class TestCiSetMatrix(unittest.TestCase):
    def run_matrix(self, *args):
        return subprocess.run([sys.executable, SET_MATRIX, *args],
                              capture_output=True, text=True)

    def test_no_flags_is_todays_output(self):
        r = self.run_matrix()
        self.assertEqual(r.returncode, 0, r.stderr)
        self.baseline = json.loads(r.stdout)
        self.assertIn('stm32f4', self.baseline['arm-gcc'])

    def test_select_full_is_identical(self):
        base = json.loads(self.run_matrix().stdout)
        sel = json.dumps({'build': {'full': True, 'families': [], 'family_examples': {}}})
        self.assertEqual(json.loads(self.run_matrix('--select', sel).stdout), base)

    def test_select_narrow_is_a_subset(self):
        sel = json.dumps({'build': {'full': False, 'families': ['rp2040', 'stm32f4'],
                                    'family_examples': {}}})
        m = json.loads(self.run_matrix('--select', sel).stdout)
        self.assertEqual(m['arm-gcc'], ['rp2040', 'stm32f4'])
        self.assertEqual(m['riscv-gcc'], [])
        self.assertEqual(set(m), set(json.loads(self.run_matrix().stdout)))  # all keys kept

    def test_malformed_select_falls_open(self):
        base = json.loads(self.run_matrix().stdout)
        r = self.run_matrix('--select', 'not json {')
        self.assertEqual(r.returncode, 0)
        self.assertEqual(json.loads(r.stdout), base)
        self.assertIn('ci_set_matrix: UNSCOPED', r.stderr)   # build.yml greps this

    def test_wrong_shaped_select_falls_open_too(self):
        # valid JSON, wrong types: the matrix is built AFTER main()'s try/except, so an
        # AttributeError here reds the step - the very outcome that handler exists to
        # prevent (GHA and CircleCI only survive it through their own shell `||`)
        base = json.loads(self.run_matrix().stdout)
        for bad in ('{"build": ["stm32f4"]}', '{"build": {"full": false}}',
                    '{"build": {"full": false, "families": "stm32f4"}}', '["stm32f4"]'):
            r = self.run_matrix('--select', bad)
            self.assertEqual(r.returncode, 0, f'{bad}: {r.stderr}')
            self.assertEqual(json.loads(r.stdout), base, bad)

    def test_base_flag_with_empty_diff_selects_nothing(self):
        # --base HEAD => empty diff => build.families [] => every toolchain scopes to []
        base = json.loads(self.run_matrix().stdout)
        r = self.run_matrix('--base', 'HEAD')
        self.assertEqual(r.returncode, 0, r.stderr)
        m = json.loads(r.stdout)
        self.assertEqual(set(m), set(base))
        self.assertTrue(all(v == [] for v in m.values()), m)

    def test_select_file_matches_select(self):
        # build.yml hands the selection over as a FILE: a ~128KiB step env var makes
        # the step's own exec fail with E2BIG before any fallback can run
        import tempfile
        sel = json.dumps({'build': {'full': False, 'families': ['rp2040'],
                                    'family_examples': {}}})
        with tempfile.NamedTemporaryFile('w', suffix='.json', delete=False) as f:
            f.write(sel)
            path = f.name
        try:
            self.assertEqual(self.run_matrix('--select-file', path).stdout,
                             self.run_matrix('--select', sel).stdout)
        finally:
            os.unlink(path)

    def test_absent_families_key_falls_open(self):
        # `{"build": {"full": false}}` with no families key is an unusable selection,
        # not "nothing selected": scoping every toolchain to [] would report a
        # vacuous green with zero families built
        base = json.loads(self.run_matrix().stdout)
        r = self.run_matrix('--select', json.dumps({'build': {'full': False}}))
        self.assertEqual(r.returncode, 0)
        self.assertEqual(json.loads(r.stdout), base)
        self.assertIn('ci_set_matrix: UNSCOPED', r.stderr)   # build.yml greps this

    def test_families_no_toolchain_builds_falls_open(self):
        # hw/bsp/same7x is real but in no toolchain's list, so scoping to it emits an
        # all-empty matrix: every leg skips and the PR goes green from a build job that
        # ran no compiler. Unusable, not "nothing selected" - and the marker matters,
        # because that is what build.yml and CircleCI grep to drop the build extras too.
        base = json.loads(self.run_matrix().stdout)
        r = self.run_matrix('--select',
                            json.dumps({'build': {'full': False, 'families': ['same7x']}}))
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(json.loads(r.stdout), base)
        self.assertIn('ci_set_matrix: UNSCOPED', r.stderr)

    def test_a_partial_toolchain_miss_still_scopes(self):
        # one buildable family is real coverage: scope to it and just note the other
        r = self.run_matrix('--select', json.dumps(
            {'build': {'full': False, 'families': ['stm32f4', 'same7x']}}))
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(json.loads(r.stdout)['arm-gcc'], ['stm32f4'])
        self.assertNotIn('UNSCOPED', r.stderr)
        self.assertIn('same7x', r.stderr)

    def test_explicit_empty_families_selects_nothing(self):
        # an explicit [] IS a legitimate answer (a diff that builds nothing)
        r = self.run_matrix('--select',
                            json.dumps({'build': {'full': False, 'families': []}}))
        self.assertEqual(r.returncode, 0)
        self.assertEqual(set().union(*json.loads(r.stdout).values()), set())

    def test_missing_select_file_falls_open(self):
        base = json.loads(self.run_matrix().stdout)
        r = self.run_matrix('--select-file', '/no/such/selection.json')
        self.assertEqual(r.returncode, 0)
        self.assertEqual(json.loads(r.stdout), base)
        self.assertIn('ci_set_matrix: UNSCOPED', r.stderr)   # build.yml greps this

    def test_base_flag_bad_ref_falls_open(self):
        base = json.loads(self.run_matrix().stdout)
        r = self.run_matrix('--base', 'no-such-ref-xyz')
        self.assertEqual(r.returncode, 0)
        self.assertEqual(json.loads(r.stdout), base)
        self.assertIn('ci_set_matrix: UNSCOPED', r.stderr)   # build.yml greps this


HIL_SET_MATRIX = os.path.join(REPO, '.github/scripts/hil_ci_set_matrix.py')


class TestHilCiSetMatrixExamples(unittest.TestCase):
    def run_matrix(self, *args):
        r = subprocess.run([sys.executable, HIL_SET_MATRIX, *args,
                            os.path.join(REPO, 'test/hil/tinyusb.json')],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        return r.stdout

    def test_no_hil_examples_is_byte_identical(self):
        plain = self.run_matrix()
        sel = json.dumps({'full': True, 'boards': {}})
        self.assertEqual(self.run_matrix('--select', sel), plain)

    def test_absent_boards_key_falls_open_to_the_full_roster(self):
        # the mirror of ci_set_matrix's families guard: reading an ABSENT boards key as
        # "nothing selected" filters every board out, so every hil-build leg skips and
        # both rig jobs skip through needs: - an all-green PR with zero hardware
        # coverage. An explicit boards: {} stays a legitimate nothing-selected.
        plain = self.run_matrix()
        for bad in ('{"full": false, "hil_examples": {}}', '{"full": false, "boards": []}',
                    'not json {', '["a board"]',
                    # the whole selection is unusable, hil_examples included: keeping the
                    # -e lists builds a few examples per board while the rig, unfiltered,
                    # runs that board's whole test list
                    '{"full": false, "hil_examples": {"frdm_k64f": ["device/cdc_msc"]}}'):
            self.assertEqual(self.run_matrix('--select', bad), plain, bad)
        self.assertNotEqual(self.run_matrix('--select', '{"full": false, "boards": {}}'),
                            plain, 'an explicit empty boards map still means nothing')

    def test_select_file_matches_select(self):
        # hil-hfp-iar passes the whole selection; as one argv it can exceed
        # MAX_ARG_STRLEN on a big diff, so the file form must be equivalent
        import tempfile
        board = on_roster(self, 'stm32f407disco')[0]
        sel = json.dumps({'full': False, 'boards': {board: 'all'},
                          'hil_examples': {board: ['device/board_test']}})
        with tempfile.NamedTemporaryFile('w', suffix='.json', delete=False) as f:
            f.write(sel)
            path = f.name
        try:
            self.assertEqual(self.run_matrix('--select-file', path),
                             self.run_matrix('--select', sel))
        finally:
            os.unlink(path)

    def test_examples_appended_per_board(self):
        board = on_roster(self, 'stm32f407disco')[0]
        sel = json.dumps({'full': False, 'boards': {board: 'all'},
                          'hil_examples': {board: ['device/board_test', 'device/cdc_msc']}})
        m = json.loads(self.run_matrix('--select', sel))
        entries = [e for entries in m.values() for e in entries]
        self.assertTrue(entries)
        for e in entries:
            self.assertIn(f'-b {board}', e)
            self.assertIn('-e device/board_test', e)
            self.assertIn('-e device/cdc_msc', e)


class TestBuildPyExampleFilter(unittest.TestCase):
    def setUp(self):
        import build as build_py
        self.build = build_py
        self.old = os.getcwd()
        os.chdir(REPO)                      # skip_example uses repo-relative paths

    def tearDown(self):
        os.chdir(self.old)

    def test_all_maps_to_example_names(self):
        # ONE group: the examples of a '--target all' build go into a single
        # `cmake --build --target a b c`, so they build in parallel
        t = self.build.resolve_example_target_groups(['all'], ['device/cdc_msc', 'device/dfu'],
                                                     'stm32f407disco')
        self.assertEqual(t, [['cdc_msc', 'dfu']])

    def test_other_targets_pass_through_in_their_own_group(self):
        # a target that is not 'all' keeps its own invocation, so ordering against the
        # examples is preserved (tinyusb_metrics runs after them, as it did unfiltered)
        t = self.build.resolve_example_target_groups(['all', 'tinyusb_metrics'],
                                                     ['device/cdc_msc'], 'stm32f407disco')
        self.assertEqual(t, [['cdc_msc'], ['tinyusb_metrics']])

    def test_unbuildable_examples_drop_and_empty_is_none(self):
        # typec/power_delivery only builds on stm32g4-class parts, never on f4
        t = self.build.resolve_example_target_groups(['all'],
                                                     ['typec/power_delivery', 'device/cdc_msc'],
                                                     'stm32f407disco')
        self.assertEqual(t, [['cdc_msc']])
        self.assertIsNone(self.build.resolve_example_target_groups(['all'],
                                                                   ['typec/power_delivery'],
                                                                   'stm32f407disco'))

    def test_espressif_empty_intersection_skips_without_building(self):
        # cmake_board's espressif branch must short-circuit on an empty -e
        # intersection the same way the generic cmake/make branches do, and
        # must do so before touching idf.py (no real esp-idf build here).
        calls = []
        real_run_cmd = self.build.run_cmd            # `del` here would drop the real one
        self.build.run_cmd = lambda cmd: calls.append(cmd)  # would only run for a real build
        try:
            r = self.build.cmake_board('espressif_s3_devkitc', [], None, [], ['all'],
                                       examples=['nonexistent/example'])
        finally:
            self.build.run_cmd = real_run_cmd
        self.assertEqual(r, [0, 0, 1])
        self.assertEqual(calls, [])

    def test_make_one_example_uses_make_semantics(self):
        # F1 end to end: the make path must ask skip_example with build_system='make',
        # or lpc54's cmake-only FAMILY_MCUS un-skips a host example whose make build
        # compiles no HCD source and fails to link
        calls = []
        real_run_cmd = self.build.run_cmd
        self.build.run_cmd = lambda cmd: calls.append(cmd)
        try:
            r = self.build.make_one_example('host/msc_file_explorer_freertos',
                                            'lpcxpresso54628', '', ['all'])
        finally:
            self.build.run_cmd = real_run_cmd
        self.assertEqual(r, [0, 0, 1])      # skipped, nothing handed to make
        self.assertEqual(calls, [])

    def test_example_flag_rejects_a_bare_name(self):
        # `-e cdc_msc` (no role) used to IndexError inside the target resolver;
        # argparse rejects the shape now, with a message that names it
        r = subprocess.run([sys.executable, os.path.join(REPO, 'tools', 'build.py'),
                            '-b', 'stm32f407disco', '-e', 'cdc_msc'],
                           capture_output=True, text=True, cwd=REPO)
        self.assertEqual(r.returncode, 2, r.stdout + r.stderr)
        self.assertIn('role/name', r.stderr)

    def test_no_example_basename_is_reused_across_roles(self):
        # -e maps role/name onto the BARE cmake target name, so device/foo and host/foo
        # would collapse into one `--target foo`: one of them would never build while
        # the post-configure check still reports both as covered. No collision today,
        # and the -e lists are machine-generated, so nothing else would notice one.
        seen = {}
        for ex in ci_select.all_examples(REPO):
            role, name = ex.split('/', 1)
            self.assertNotIn(name, seen,
                             f'{ex} and {seen.get(name)}/{name} share a cmake target name; '
                             f'build.py -e cannot tell them apart')
            seen[name] = role

    def test_example_flag_rejects_a_name_no_example_dir_answers_to(self):
        # right shape, no such dir: every board would report Skipped and the run would
        # still exit 0 (main returns the FAILED count), so an entirely stale -e list -
        # from the example map or from a roster test name - reads as a green build
        r = subprocess.run([sys.executable, os.path.join(REPO, 'tools', 'build.py'),
                            '-b', 'stm32f407disco', '-e', 'device/no_such_example'],
                           capture_output=True, text=True, cwd=REPO)
        self.assertEqual(r.returncode, 2, r.stdout + r.stderr)
        self.assertIn('no such example directory', r.stderr)

    def test_pr_filter_answers_before_configuring(self):
        # nothing the -e list names is buildable here: the skip.txt mirror needs no
        # configure output, so the whole cmake run must be skipped, not just its build
        calls = []
        real_run_cmd = self.build.run_cmd
        self.build.run_cmd = lambda cmd: calls.append(cmd)
        try:
            r = self.build.cmake_board('stm32f407disco', [], None, [], ['all'],
                                       examples=['typec/power_delivery'])
        finally:
            self.build.run_cmd = real_run_cmd
        self.assertEqual(r, [0, 0, 1])
        self.assertEqual(calls, [])

    def _cmake_board_with_targets(self, registered, examples):
        """cmake_board with the configure/build stubbed and CMake's registered-target
        list forced. Returns (result, target names handed to `cmake --build`)."""
        class Ok:
            returncode = 0
        calls = []

        def fake_run(cmd):
            calls.append(cmd)
            return Ok()
        real_run_cmd = self.build.run_cmd
        real_targets = self.build.cmake_registered_targets
        self.build.run_cmd = fake_run
        self.build.cmake_registered_targets = lambda d: registered
        try:
            r = self.build.cmake_board('stm32f407disco', [], None, [], ['all'],
                                       examples=examples)
        finally:
            self.build.run_cmd = real_run_cmd
            self.build.cmake_registered_targets = real_targets
        # everything after --target: one invocation carries the whole group
        built = [c[c.index('--target') + 1:] for c in calls if '--target' in c]
        return r, built

    def test_example_without_a_cmake_target_is_dropped(self):
        # an example dir CMake never registered (absent from the role CMakeLists, or
        # a stale roster name) must not reach `cmake --build --target <it>`: that is a
        # hard red, and skip.txt cannot see it
        r, built = self._cmake_board_with_targets({'cdc_msc'},
                                                  ['device/cdc_msc', 'device/dfu'])
        self.assertEqual(built, [['cdc_msc']])
        self.assertEqual(r, [1, 0, 0])

    def test_the_selected_examples_build_in_one_invocation(self):
        # one `cmake --build --target a b c`, not one invocation per example: the
        # per-example loop serialised every scoped leg, and hil-build gets an -e list
        # on EVERY PR (~14 examples per board), so it is on the critical path to the rig
        r, built = self._cmake_board_with_targets({'cdc_msc', 'dfu', 'hid_generic_inout'},
                                                  ['device/cdc_msc', 'device/dfu',
                                                   'device/hid_generic_inout'])
        self.assertEqual(built, [['cdc_msc', 'dfu', 'hid_generic_inout']])

    def test_no_registered_target_at_all_skips_the_build(self):
        r, built = self._cmake_board_with_targets({'cdc_msc'}, ['device/dfu'])
        self.assertEqual(built, [])
        self.assertEqual(r, [0, 0, 1])

    def test_unparseable_target_help_keeps_the_skip_txt_answer(self):
        # ground truth unavailable (a non-Ninja generator, an old cmake): fall back
        # to the mirror rather than dropping every example
        r, built = self._cmake_board_with_targets(None, ['device/cdc_msc'])
        self.assertEqual(built, [['cdc_msc']])

    def test_target_help_parse(self):
        text = ('[1/1] All primary targets available:\n'
                'tinyusb_metrics: phony\n'
                'cdc_msc: phony\n'
                'cdc_msc-membrowse-upload: phony\n'
                'device/edit_cache: phony\n'
                '/abs/build/device/cdc_msc/CMakeFiles/cdc_msc-jlink: CUSTOM_COMMAND\n')
        self.assertEqual(self.build.parse_target_help(text),
                         {'tinyusb_metrics', 'cdc_msc', 'cdc_msc-membrowse-upload'})

    def test_build_defines_reach_the_example_filter(self):
        # metro_m4_express gets MAX3421_HOST=1 from its roster variant, never
        # from its BSP: without threading them through, -e drops the rig's only
        # MAX3421 dual firmware that --target all used to build
        self.assertIsNone(self.build.resolve_example_target_groups(
            ['all'], ['dual/host_info_to_device_cdc'], 'metro_m4_express'))
        self.assertEqual(self.build.resolve_example_target_groups(
            ['all'], ['dual/host_info_to_device_cdc'], 'metro_m4_express',
            extra_defines=('MAX3421_HOST=1',)), [['host_info_to_device_cdc']])



class TestFamilyMcusFallback(unittest.TestCase):
    """A family whose family.cmake sets FAMILY_MCUS only inside if() blocks gets its
    whole MCU answer from _board_mcu's CFG_TUSB_MCU scrape (build_utils._family_mcus
    does not evaluate cmake conditionals). For mcx that answer is load-bearing - six
    examples' skip.txt name mcu:MCXA15 - and it comes out right only because every
    mcx board still carries the token in a make-only board.mk the scrape falls
    through to. A board.cmake-only board (MCU_VARIANT, no CFG_TUSB_MCU) would scrape
    'NONE' and silently skip EVERY example on it, in CI as well as in -e."""

    @staticmethod
    def conditional_only_families():
        """hw/bsp/<family> dirs whose family.cmake has no unconditional
        set(FAMILY_MCUS ...) - computed, not listed, so a family that grows or loses
        one moves in and out of this guard on its own."""
        import build_utils
        out = []
        for fc in sorted(glob.glob(os.path.join(REPO, 'hw/bsp/*/family.cmake'))):
            depth, uncond = 0, False
            for line in open(fc).read().splitlines():
                line = line.strip()
                if build_utils._FAMILY_MCUS_RE.match(line) and depth == 0:
                    uncond = True
                if re.match(r'if\s*\(', line):
                    depth += 1
                elif re.match(r'endif\s*\(', line):
                    depth = max(0, depth - 1)
            if not uncond:
                out.append(os.path.dirname(fc))
        return out

    def test_every_board_of_such_a_family_scrapes_an_mcu(self):
        import build_utils
        fams = self.conditional_only_families()
        self.assertTrue(fams, 'no family sets FAMILY_MCUS conditionally any more')
        for fam_dir in fams:
            fam = os.path.basename(fam_dir)
            for bd in sorted(glob.glob(os.path.join(fam_dir, 'boards', '*'))):
                if not os.path.isdir(bd):
                    continue
                mcu, _ = build_utils._board_mcu(bd, fam_dir, fam)
                self.assertNotEqual(
                    mcu, 'NONE',
                    f'{fam}/{os.path.basename(bd)}: nothing to scrape a CFG_TUSB_MCU '
                    f'token from, and {fam}/family.cmake sets FAMILY_MCUS only inside '
                    f'if() - skip_example would skip every example on this board. Fix '
                    f'by evaluating the if(MCU_VARIANT STREQUAL ...) branches.')


class TestMcuTokensResolve(unittest.TestCase):
    """The cmake-side MCU mirror must never answer with an unexpanded ${VAR} or with
    nothing at all: both make every `mcu:` token miss, which reads as 'skip' for any
    example carrying an only.txt and silently drops compile coverage."""

    @staticmethod
    def _every_board():
        import build as build_py
        old = os.getcwd()
        os.chdir(REPO)
        try:
            for fam in sorted(os.path.basename(os.path.dirname(f))
                              for f in glob.glob(os.path.join(REPO, 'hw/bsp/*/boards'))):
                for b in build_py.get_family_boards(fam, False, False):
                    yield fam, b
        finally:
            os.chdir(old)

    def test_no_board_answers_with_an_unexpanded_variable(self):
        import build_utils
        for fam, board in self._every_board():
            fam_dir, board_dir = f'{REPO}/hw/bsp/{fam}', f'{REPO}/hw/bsp/{fam}/boards/{board}'
            mcus = set(build_utils._family_mcus(fam_dir, board_dir))
            mcus.add(build_utils._board_mcu(board_dir, fam_dir, fam)[0])
            self.assertFalse([m for m in mcus if '${' in m],
                             f'{fam}/{board}: unexpanded cmake variable in {sorted(mcus)} - '
                             f'teach build_utils._cmake_expand the construct that produces it')
            self.assertTrue(mcus - {'NONE'},
                            f'{fam}/{board}: no MCU name resolved at all')

    # skip.txt/only.txt tokens no board in the tree answers to: stale spellings left
    # behind by a family rename. Each one silently changes what CI builds, so this list
    # must only ever SHRINK - a new entry means either a live token the mirror cannot
    # produce, or a rename nobody followed through. `family:samd21` was one of these
    # until the nine examples/host/*/only.txt files were corrected to samd2x_l2x.
    #
    # The remaining `mcu:` entries sit beside a live token in the same file, so they gate
    # nothing either way. MKL25ZXX (7 files) and SAME5X (1) were dead too, but unlike
    # these they were the ONLY token for their board - the examples were already being
    # built on the very boards those lines meant to exclude. Dropping them is a no-op for
    # the build (verified per example) and was chosen over re-pointing, which would have
    # removed working coverage.
    UNREACHABLE_TOKENS = {
        'mcu': {'LPC177X_8X', 'MIMXRT10XX', 'MIMXRT11XX', 'STM32U3'},
        'family': set(),
        'board': set(),
    }

    def test_every_skip_only_token_is_reachable(self):
        import build_utils
        wanted = {ns: set() for ns in self.UNREACHABLE_TOKENS}
        for f in glob.glob(os.path.join(REPO, 'examples/*/*/*.txt')):
            if os.path.basename(f) in ('skip.txt', 'only.txt'):
                for tok in open(f).read().split():
                    ns, _, name = tok.partition(':')
                    if ns in wanted and name:
                        wanted[ns].add(name)
        have = {ns: set() for ns in wanted}
        have['mcu'].add('MAX3421')               # synthetic, from family_support.cmake:940
        for fam, board in self._every_board():
            fam_dir, board_dir = f'{REPO}/hw/bsp/{fam}', f'{REPO}/hw/bsp/{fam}/boards/{board}'
            have['family'].add(fam)
            have['board'].add(board)
            have['mcu'] |= set(build_utils._family_mcus(fam_dir, board_dir))
            have['mcu'].add(build_utils._board_mcu(board_dir, fam_dir, fam)[0])
            have['mcu'].add(build_utils._scrape_mcu(pathlib.Path(fam_dir),
                                                    pathlib.Path(board_dir), fam)[0])  # make
        for ns in wanted:
            self.assertEqual(
                wanted[ns] - have[ns], self.UNREACHABLE_TOKENS[ns] & wanted[ns],
                f'a skip.txt/only.txt {ns}: token nothing in hw/bsp answers to. Either '
                f'the token is stale (a rename just changed what CI builds), or the '
                f'mirror cannot produce it - both silently skip that example everywhere.')

    def test_the_mcx_skip_tokens_are_still_live(self):
        # the reason the mcx scrape is load-bearing rather than academic
        named = [os.path.dirname(f) for f in glob.glob(os.path.join(REPO, 'examples/*/*/skip.txt'))
                 if 'mcu:MCXA15' in open(f).read().split()]
        self.assertTrue(named, 'no skip.txt names mcu:MCXA15 any more')


class TestSkipExampleMirrorsFamilyFilter(unittest.TestCase):
    """build_utils.skip_example is the python mirror of CMake's family_filter
    (hw/bsp/family_support.cmake:171-207). family_filter loops over the whole
    FAMILY_MCUS list; a per-board CFG_TUSB_MCU scrape alone lets -e ask for a
    target CMake never created, and `cmake --build --target <it>` hard-fails."""

    def setUp(self):
        import build_utils
        self.build_utils = build_utils
        self.old = os.getcwd()
        os.chdir(REPO)                  # skip_example uses repo-relative paths

    def tearDown(self):
        os.chdir(self.old)

    def test_any_family_mcu_can_skip(self):
        # broadcom_64bit: set(FAMILY_MCUS BCM2711 BCM2835); raspberrypi_cm4 is
        # BCM2711, and examples/device/dfu/skip.txt lists mcu:BCM2835
        self.assertTrue(self.build_utils.skip_example('device/dfu', 'raspberrypi_cm4'))

    def test_any_family_mcu_can_satisfy_only(self):
        # lpc55: family.mk says LPC55XX, family.cmake sets FAMILY_MCUS LPC55, and
        # host/cdc_msc_hid/only.txt lists mcu:LPC55 - CMake builds it
        self.assertFalse(self.build_utils.skip_example('host/cdc_msc_hid', 'lpcxpresso55s69'))

    def test_existing_decisions_are_unchanged(self):
        self.assertFalse(self.build_utils.skip_example('device/cdc_msc', 'stm32f407disco'))
        self.assertTrue(self.build_utils.skip_example('typec/power_delivery', 'stm32f407disco'))

    def test_build_define_enables_max3421_only_list(self):
        # family_support.cmake:940 appends MAX3421 to FAMILY_MCUS when
        # MAX3421_HOST=1; on metro_m4_express that define comes from the roster
        # variant defines, so skip_example has to be told about it
        ex = 'dual/host_info_to_device_cdc'
        self.assertTrue(self.build_utils.skip_example(ex, 'metro_m4_express'))
        self.assertFalse(self.build_utils.skip_example(ex, 'metro_m4_express',
                                              extra_defines=('MAX3421_HOST=1',)))

    def test_family_mcus_variable_token_resolves(self):
        """hw/bsp/ra/family.cmake: `set(FAMILY_MCUS RAXXX ${MCU_VARIANT})`, and
        ra6m5_ek/board.cmake sets MCU_VARIANT ra6m5 — which is exactly the token
        dual/host_info_to_device_cdc/only.txt spells (mcu:ra6m5). Dropping the
        ${...} token silently removed ra from every scoped dual-example build."""
        self.assertFalse(self.build_utils.skip_example(
            'dual/host_info_to_device_cdc', 'ra6m5_ek'))

    def test_board_cmake_max3421_counts(self):
        """feather_rp2040_max3421/board.cmake sets MAX3421_HOST 1 while the MCU
        token comes from rp2040's family.cmake; scanning only the file the token
        came from misses it, and only.txt's mcu:MAX3421 never matches."""
        self.assertFalse(self.build_utils.skip_example(
            'host/cdc_msc_hid_freertos', 'feather_rp2040_max3421'))


class TestSkipExampleMakeSemantics(unittest.TestCase):
    """FAMILY_MCUS is a CMAKE fact. hw/bsp/lpc54/family.cmake sets it to LPC54 and
    wires the ohci host sources; family.mk builds OPT_MCU_LPC54XXX and compiles no
    HCD source at all — so applying the cmake MCU union to a Make build un-skips
    the 9 host examples only.txt gates on mcu:LPC54 and they fail to link
    (undefined reference to hcd_init). Make keeps master's exact algorithm."""

    def setUp(self):
        import build_utils
        self.build_utils = build_utils
        self.old = os.getcwd()
        os.chdir(REPO)                  # skip_example uses repo-relative paths

    def tearDown(self):
        os.chdir(self.old)

    def test_make_keeps_cmake_only_family_mcus_out(self):
        self.assertTrue(self.build_utils.skip_example(
            'host/msc_file_explorer_freertos', 'lpcxpresso54628', build_system='make'))

    def test_make_does_not_skip_on_a_sibling_family_mcu(self):
        # broadcom_64bit sets FAMILY_MCUS "BCM2711 BCM2835"; raspberrypi_cm4 is the
        # BCM2711 one and device/dfu/skip.txt names mcu:BCM2835. The aarch64 make leg
        # built device/dfu before the union and must keep building it.
        for ex in ('device/dfu', 'device/usbtmc'):
            self.assertFalse(self.build_utils.skip_example(
                ex, 'raspberrypi_cm4', build_system='make'), ex)

    def test_cmake_is_the_default_and_still_unions(self):
        self.assertTrue(self.build_utils.skip_example('device/dfu', 'raspberrypi_cm4'))
        self.assertEqual(
            self.build_utils.skip_example('device/dfu', 'raspberrypi_cm4'),
            self.build_utils.skip_example('device/dfu', 'raspberrypi_cm4',
                                          build_system='cmake'))

    def test_build_system_is_part_of_the_cache_key(self):
        # one lru_cache shared by both semantics would answer the second caller
        # with the first caller's verdict
        ex, board = 'host/msc_file_explorer_freertos', 'lpcxpresso54628'
        self.assertFalse(self.build_utils.skip_example(ex, board, build_system='cmake'))
        self.assertTrue(self.build_utils.skip_example(ex, board, build_system='make'))
        self.assertFalse(self.build_utils.skip_example(ex, board, build_system='cmake'))


class TestConfigEnables(unittest.TestCase):
    """_config_enables decides which examples a class change selects, on BOTH the
    build and the HIL axis. A define it cannot evaluate must read as ON: reading
    it as OFF is fail-closed, and lets a compile break merge green."""

    def test_identifier_value_is_enabled(self):
        # examples/host/midi_rx: `#define CFG_TUH_MIDI CFG_TUH_DEVICE_MAX`
        cfg = os.path.join(REPO, 'examples/host/midi_rx/src/tusb_config.h')
        self.assertTrue(ci_select._config_enables(cfg, ['CFG_TUH_MIDI']))

    def test_literal_zero_is_disabled(self):
        import tempfile
        with tempfile.TemporaryDirectory() as td:
            cfg = os.path.join(td, 'tusb_config.h')
            with open(cfg, 'w') as f:
                f.write('#define CFG_TUD_CDC   0\n'
                        '#define CFG_TUD_MSC   (0)\n'
                        '#define CFG_TUD_HID   00\n'
                        '#define CFG_TUH_HID   0   // typical keyboard + mouse\n'
                        '#define CFG_TUD_MIDI  01\n'
                        '#define CFG_TUD_DFU   (1)\n')
            for m in ('CFG_TUD_CDC', 'CFG_TUD_MSC', 'CFG_TUD_HID', 'CFG_TUH_HID'):
                self.assertFalse(ci_select._config_enables(cfg, [m]), m)
            for m in ('CFG_TUD_MIDI', 'CFG_TUD_DFU'):
                self.assertTrue(ci_select._config_enables(cfg, [m]), m)
            self.assertFalse(ci_select._config_enables(cfg, ['CFG_TUD_VIDEO']))

    def test_two_branch_define_reads_on(self):
        # examples/device/uac2_speaker_fb defines CFG_TUD_HID 1 under
        # `#if CFG_AUDIO_DEBUG` and 0 in the #else. The default build (CFG_AUDIO_DEBUG
        # defaults to 1) compiles the HID class in, so a CFG_TUD_HID change must keep
        # this example on both axes - the #else's zero must not decide it.
        cfg = os.path.join(REPO, 'examples/device/uac2_speaker_fb/src/tusb_config.h')
        self.assertTrue(ci_select._config_enables(cfg, ['CFG_TUD_HID']))

    def test_any_nonzero_define_wins_over_a_zero_one(self):
        import tempfile
        with tempfile.TemporaryDirectory() as td:
            cfg = os.path.join(td, 'tusb_config.h')
            with open(cfg, 'w') as f:
                f.write('#if FOO\n#define CFG_TUD_MSC 1\n#else\n'
                        '#define CFG_TUD_MSC 0\n#endif\n'
                        '#if BAR\n#define CFG_TUD_CDC 0\n#else\n'
                        '#define CFG_TUD_CDC (0)\n#endif\n')
            self.assertTrue(ci_select._config_enables(cfg, ['CFG_TUD_MSC']))
            self.assertFalse(ci_select._config_enables(cfg, ['CFG_TUD_CDC']))

    def test_midi_host_change_selects_midi_rx(self):
        s = ci_select.classify_build(['src/class/midi/midi_host.c'], REPO)
        self.assertFalse(s['full'])
        self.assertTrue(s['families'], 'a TUH_MIDI change must select some family')
        self.assertTrue(any('host/midi_rx' in exs
                            for exs in s['family_examples'].values()),
                        s['family_examples'])


class TestPruneUsesEveryFamilyBoard(unittest.TestCase):
    """CircleCI's cmake legs build EVERY board of a family, so an example gated to
    one board (only.txt board:mimxrt1060_evk) must keep its family even though the
    family's one-first board cannot build it."""

    def test_board_gated_example_keeps_its_family(self):
        s = ci_select.classify_build(
            ['examples/dual/host_hid_to_device_cdc/src/main.c'], REPO)
        self.assertFalse(s['full'])
        self.assertIn('imxrt', s['families'], s['families'])
        self.assertEqual(s['family_examples'].get('imxrt'),
                         ['dual/host_hid_to_device_cdc'])

    def test_either_build_system_keeps_the_family(self):
        """This one family list gates CircleCI's MAKE legs too, and the two build
        systems answer skip.txt differently. device/dfu carries mcu:BCM2835, which the
        cmake FAMILY_MCUS union (BCM2711 BCM2835) applies to every broadcom_64bit board
        and the make scrape applies to none - asking cmake alone drops the only
        aarch64-gcc family in the matrix, so build-make-aarch64-gcc silently stops
        compiling dfu at all."""
        import build_utils
        old = os.getcwd()
        os.chdir(REPO)
        try:
            self.assertTrue(build_utils.skip_example('device/dfu', 'raspberrypi_cm4'))
            self.assertFalse(build_utils.skip_example('device/dfu', 'raspberrypi_cm4',
                                                      (), 'make'))
        finally:
            os.chdir(old)
        s = ci_select.classify_build(['examples/device/dfu/src/main.c'], REPO)
        self.assertIn('broadcom_64bit', s['families'], s['families'])


class TestPrunePoolIsBuildPys(unittest.TestCase):
    """_prune_buildable asks build.py what each family's build path can see, the same
    way for every family - the espressif carve-out lives in build.py.get_examples and
    needs no second copy here. Measured identical on all 82 families."""

    def setUp(self):
        import build as build_py
        self.build_py = build_py
        self.old = os.getcwd()
        os.chdir(REPO)                  # get_examples scans relative paths

    def tearDown(self):
        os.chdir(self.old)

    def test_only_espressif_narrows_the_pool(self):
        allex = list(ci_select.all_examples(REPO))
        for fam in ci_select.all_bsp_families(REPO):
            pool = [e for e in allex if e in set(self.build_py.get_examples(fam))]
            if fam == 'espressif':
                self.assertNotEqual(pool, allex)          # the carve-out is real
            else:
                self.assertEqual(pool, allex, f'{fam}: build.py narrows this family')

    def test_selections_are_what_the_espressif_only_rule_gave(self):
        # espressif's own list is the one value that ever differed from the unfiltered
        # example set. Recomputed from build.py rather than pinned as literals: a new
        # board, family or example moves the counts, and a suite that fails for that
        # teaches people to edit the numbers instead of reading the diff. What is pinned
        # is the RELATION - espressif gets exactly the rule's answer narrowed to its own
        # pool, every other family gets the answer unnarrowed.
        pool = set(self.build_py.get_examples('espressif'))
        # the third diff names an example espressif DOES build, so there is nothing for
        # the carve-out to remove - it pins that the narrowing does not over-reach
        for files, carve in ((['src/portable/synopsys/dwc2/dcd_dwc2.c'], True),
                             (['src/class/msc/msc_host.c'], True),
                             (['examples/device/cdc_msc_freertos/src/main.c'], False)):
            s = ci_select.classify_build(files, REPO)
            self.assertFalse(s['full'], files)
            self.assertIn('espressif', s['families'], files)
            esp = set(s['family_examples'].get('espressif') or [])
            self.assertTrue(esp, f'{files}: espressif selected nothing')
            # the pool narrowing is what _prune_buildable adds here, so it must hold...
            self.assertTrue(esp <= pool, f'{files}: {sorted(esp - pool)} is outside the pool')
            # ...and it must actually bite: some other family was given an example that
            # espressif's build path cannot see, and espressif did not get it
            other = set().union(*(set(v) for f, v in s['family_examples'].items()
                                  if f != 'espressif'), set())
            self.assertEqual(bool(other - pool), carve,
                             f'{files}: carve-out expected={carve}, other-side extras '
                             f'{sorted(other - pool)}')
            self.assertFalse(esp & (other - pool), files)


class TestGetDepsExampleShim(unittest.TestCase):
    """hil_ci_set_matrix emits `-b <board> -e role/name` entries that .github/actions/
    get_deps and build.yml's hfp job hand verbatim to get_deps.py. argparse must not
    reject -e there (exit 2 = every PR's Get Dependencies step red)."""

    # get_deps.main() with its process pool stubbed out: argparse runs for real,
    # nothing is cloned (this suite also runs on GitHub's bare pre-commit runner)
    CODE = ('import sys\n'
            'import get_deps\n'
            'class P:\n'
            '    def __enter__(self): return self\n'
            '    def __exit__(self, *a): return False\n'
            '    def map(self, fn, items): return [0] * len(items)\n'
            'get_deps.Pool = P\n'
            "sys.argv = ['get_deps.py'] + sys.argv[1:]\n"
            'sys.exit(get_deps.main())\n')

    def run_get_deps(self, *args):
        env = dict(os.environ, PYTHONPATH=os.path.join(REPO, 'tools'))
        return subprocess.run([sys.executable, '-c', self.CODE, *args],
                              capture_output=True, text=True, cwd=REPO, env=env)

    def test_example_flag_is_accepted(self):
        r = self.run_get_deps('-b', 'stm32f407disco', '-e', 'device/cdc_msc')
        self.assertNotIn('unrecognized arguments', r.stderr)
        self.assertEqual(r.returncode, 0, r.stderr)

    def test_plain_board_still_works(self):
        r = self.run_get_deps('-b', 'stm32f407disco')
        self.assertEqual(r.returncode, 0, r.stderr)


if __name__ == '__main__':
    unittest.main(verbosity=1)
