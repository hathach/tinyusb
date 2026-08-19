#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Unit tests for hil_select.py — pure logic, no hardware, no git. Run directly:
#   python3 test/hil/test/test_hil_select.py
#
# Imports stay stdlib + hil_select/hil_util/hil_flash ONLY: the pre-commit hil-test
# hook runs this suite, on GitHub's bare runner in the pre-commit workflow as well as
# locally, and that runner has no pyserial/pymtp. hil_flash is admissible because it
# is stdlib + hil_util only (test_hil_util.BottomLayer enforces the stdlib closure of
# both) and the roster-dispatch tests need its flash_* table; never import hil_test,
# which pulls pyserial.
import glob
import json
import os
import sys
import unittest

# the modules under test live in the parent dir (test/hil), not here
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import hil_flash
from helper import hil_select
from helper.hil_util import device_tests, dual_tests

REPO = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.dirname(os.path.abspath(__file__)))))


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
    return hil_select.classify(files, REPO, ROSTERS)


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
        edges = hil_select.class_include_edges(REPO)
        self.assertEqual(edges.get('audio/audio.h'), {'midi'})
        self.assertEqual(edges.get('cdc/cdc.h'), {'net'})

    def test_audio_header_selects_midi_example(self):
        s = hil_select.classify(['src/class/audio/audio.h'], REPO, real_rosters())
        self.assertFalse(s['full'])
        # every board that runs device/midi_test at all must run it here (boards with
        # a tests.only list, e.g. espressif, run the freertos examples instead)
        by_name = {b['name']: b for _, bs in real_rosters() for b in bs}
        checked = 0
        for name, tests in s['boards'].items():
            if 'device/midi_test' in hil_select.board_tests(by_name[name]):
                self.assertIn('device/midi_test', tests, name)
                checked += 1
        self.assertTrue(checked)

    def test_audio_header_reaches_boards_that_skip_audio(self):
        # both skip device/audio_test_freertos: without the midi edge their
        # intersection is empty and they drop out of the selection entirely
        boards = on_roster(self, 'metro_m4_express', 'nrf54lm20dk')
        s = hil_select.classify(['src/class/audio/audio.h'], REPO, real_rosters())
        for board in boards:
            self.assertEqual(s['boards'].get(board), ['device/midi_test'], board)

    def test_edge_is_per_header_not_per_class(self):
        # midi includes audio.h, not audio_device.h: an audio_device change must
        # not drag midi's examples in
        s = hil_select.classify(['src/class/audio/audio_device.c'], REPO, real_rosters())
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
        for f in ['test/hil/hil_test.py', '.github/workflows/build.yml', 'hw/mcu/nxp/x.c', 'lib/foo/x.c']:
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
        args = hil_select.selection_args(s, ROSTERS)
        a = args['tinyusb.json']
        self.assertIn('-b raspberry_pi_pico', a)
        self.assertNotIn('stm32f407disco', a)
        self.assertIn('-bt raspberry_pi_pico:', a)   # device-only subset of a device+host board

    def test_args_full_is_empty(self):
        s = sel(['tools/random_new_script.py'])
        self.assertEqual(hil_select.selection_args(s, ROSTERS), {'tinyusb.json': ''})

    def test_args_all_board_gets_bare_b(self):
        s = sel(['hw/bsp/rp2040/boards/raspberry_pi_pico/board.h'])
        a = hil_select.selection_args(s, ROSTERS)['tinyusb.json']
        self.assertIn('-b raspberry_pi_pico', a)
        self.assertNotIn('-bt', a)

    def test_args_by_flasher_splits_esp_from_the_rest(self):
        s = sel(['src/device/usbd.c'])
        per = hil_select.selection_args_by_flasher(s, ROSTERS)['tinyusb.json']
        self.assertIn('espressif_s3_devkitm', per['esptool'])
        self.assertIn('raspberry_pi_pico', per['openocd'])
        self.assertNotIn('espressif_s3_devkitm', per.get('openocd', '') + per.get('jlink', ''))

    def test_args_by_flasher_omits_a_flasher_with_no_selected_board(self):
        # the esp CI leg must see no args at all here, not a filter matching zero boards
        s = sel(['hw/bsp/rp2040/boards/raspberry_pi_pico/board.h'])
        per = hil_select.selection_args_by_flasher(s, ROSTERS)['tinyusb.json']
        self.assertEqual(per, {'openocd': '-b raspberry_pi_pico'})

    def test_args_by_flasher_full_is_empty(self):
        s = sel(['tools/random_new_script.py'])
        self.assertEqual(hil_select.selection_args_by_flasher(s, ROSTERS), {'tinyusb.json': {}})

    def test_cli_diff_file(self):
        import subprocess, tempfile, json as j
        with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
            f.write('src/class/cdc/cdc_device.c\n')
            path = f.name
        r = subprocess.run([sys.executable, os.path.join(REPO, 'test/hil/helper/hil_select.py'),
                            '--diff-file', path, os.path.join(REPO, 'test/hil/tinyusb.json')],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        out = j.loads(r.stdout)
        self.assertFalse(out['full'])
        self.assertIn('tinyusb.json', out['args'])
        self.assertTrue(any('cdc_device' in line for line in out['reasons']))
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
        s = hil_select.classify(['src/portable/synopsys/dwc2/dcd_dwc2.c'], REPO, real_rosters())
        self.assertFalse(s['full'])
        for board in boards:
            self.assertIn(board, s['boards'])


class TestOptionGatedPort(unittest.TestCase):
    """Regression: family_support.cmake compiles some ports from a build option
    (MAX3421_HOST=1 -> hcd_max3421.c), so a board's family file never names them."""
    # host-side option board (max3421 as host controller), off any max3421 family
    OPT_ROSTER = [('test/hil/opt.json', [
        {'name': 'fake_dual_board', 'uid': 'o1', 'flasher': {'name': 'jlink'},
         'build': {'args': ['MAX3421_HOST=1']},
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
        s = hil_select.classify(['src/portable/analog/max3421/hcd_max3421.c'], REPO, real_rosters())
        self.assertFalse(s['full'])
        for board in boards:
            self.assertIn(board, s['boards'])

    def test_option_selects_via_args_defines_and_flags(self):
        s = hil_select.classify(['src/portable/analog/max3421/hcd_max3421.c'], REPO, self.OPT_ROSTER)
        self.assertFalse(s['full'])
        self.assertIn('fake_dual_board', s['boards'])    # build.args
        self.assertIn('fake_host_board', s['boards'])    # variant flags
        self.assertNotIn('fake_off_board', s['boards'])  # variant defines, but =0

    def test_device_role_port_does_not_pull_host_only_option_board(self):
        s = hil_select.classify(['src/portable/analog/max3421/dcd_max3421.c'], REPO, self.OPT_ROSTER)
        self.assertFalse(s['full'])
        self.assertNotIn('fake_host_board', s['boards'])  # host-only board, device change
        self.assertIn('fake_dual_board', s['boards'])     # device-capable option board

    def test_gates_parsed_from_family_support(self):
        self.assertEqual(hil_select.port_option_gates(REPO).get('analog/max3421'),
                         {'MAX3421_HOST'})

    def test_board_cmake_option_counts(self):
        """A board can enable a gated port in its own BSP rather than via the roster
        (hw/bsp/espressif/boards/*/board.cmake -> set(MAX3421_HOST 1)); board_options()
        must see those too, or such a board joining the roster is silently dropped."""
        self.assertIn('MAX3421_HOST',
                      hil_select.bsp_board_options('adafruit_feather_esp32s3', REPO))
        self.assertIn('CFG_TUH_RPI_PIO_USB',
                      hil_select.bsp_board_options('adafruit_fruit_jam', REPO))
        # commented-out `# set(MAX3421_HOST 1)` must not count
        self.assertNotIn('MAX3421_HOST',
                         hil_select.bsp_board_options('feather_nrf52840_express', REPO))

    def test_board_cmake_option_selects_off_family_board(self):
        # adafruit_feather_esp32s3 is not on any rig roster; stand it in as one to
        # prove the BSP-sourced option alone pulls a max3421 change onto the board
        roster = [('test/hil/opt.json', [
            {'name': 'adafruit_feather_esp32s3', 'uid': 'o1', 'flasher': {'name': 'esptool'},
             'tests': {'device': False, 'host': True, 'dual': False}}])]
        s = hil_select.classify(['src/portable/analog/max3421/hcd_max3421.c'], REPO, roster)
        self.assertFalse(s['full'])
        self.assertIn('adafruit_feather_esp32s3', s['boards'])

    def test_board_mk_option_is_ignored(self):
        """Make-only options must not select: HIL CI builds with CMake exclusively, so
        hw/bsp/nrf/boards/nrf5340dk/board.mk's MAX3421_HOST compiles nothing here."""
        roster = [('test/hil/opt.json', [
            {'name': 'nrf5340dk', 'uid': 'o1', 'flasher': {'name': 'jlink'},
             'tests': {'device': False, 'host': True, 'dual': False}}])]
        s = hil_select.classify(['src/portable/analog/max3421/hcd_max3421.c'], REPO, roster)
        self.assertFalse(s['full'])
        self.assertEqual(s['boards'], {})


class TestPortFamiliesCmakeOnly(unittest.TestCase):
    """port_families() is CMake-only (HIL CI never builds with Make) and matches on
    'port_dir/' so a port dir is not a prefix of a sibling."""
    def test_make_only_family_is_not_a_family(self):
        # hw/bsp/pic32mz has family.mk but no family.cmake
        self.assertEqual(hil_select.port_families('microchip/pic32mz', REPO), set())

    def test_prefix_port_does_not_inherit_sibling_families(self):
        # bare-substring matching let 'microchip/pic' match '.../microchip/pic32mz/...'
        self.assertEqual(hil_select.port_families('microchip/pic', REPO), set())

    def test_make_only_port_forces_full(self):
        s = sel(['src/portable/microchip/pic32mz/dcd_pic32mz.c'])
        self.assertTrue(s['full'])
        self.assertTrue(any('no board family' in r for r in s['reasons']), s['reasons'])

    def test_cmake_families_still_found(self):
        self.assertEqual(hil_select.port_families('raspberrypi/rp2040', REPO), {'rp2040'})
        self.assertIn('stm32f4', hil_select.port_families('synopsys/dwc2', REPO))


class TestPortFamiliesCoverage(unittest.TestCase):
    """Systematic guard: every real dcd_*/hcd_* port directory should map to at
    least one board family, so a future family.cmake/CMakeLists.txt layout that
    port_families() doesn't scan fails loudly instead of silently dropping boards
    (as espressif's dwc2 reference did - see TestRealRosterPortFamilies)."""
    # Ports with no board family: not a bug, just not wired into any rig board.
    # Add here (with a reason) only if port_families() legitimately can't find one.
    # A port listed here force-fulls (fail-open), so it is never under-selected.
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
            fams = hil_select.port_families(port, REPO)
            self.assertTrue(fams, f'{port}: no family references this port '
                                   f'(port_families() scan gap, or add to NO_FAMILY)')


class TestRealRosterOnlyListTests(unittest.TestCase):
    """Regression for roster-only-list tests (e.g. espressif's hid_composite_freertos)
    being invisible to the selector because it only knew the shared hil_util lists."""
    def test_only_list_example_change_selects_it(self):
        boards = on_roster(self, 'espressif_s3_devkitm', 'espressif_p4_function_ev')
        s = hil_select.classify(['examples/device/hid_composite_freertos/src/main.c'], REPO, real_rosters())
        self.assertFalse(s['full'])
        for board in boards:
            self.assertEqual(s['boards'][board], ['device/hid_composite_freertos'])

    def test_class_change_includes_only_list_boards(self):
        boards = on_roster(self, 'espressif_s3_devkitm', 'espressif_p4_function_ev')
        s = hil_select.classify(['src/class/hid/hid_device.c'], REPO, real_rosters())
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
        s = hil_select.classify(['src/portable/synopsys/dwc2/dcd_dwc2.c'], REPO, real_rosters())
        self.assertFalse(s['full'])
        for board in boards:
            tests = s['boards'][board]
            self.assertIn('device/hid_composite_freertos', tests)
            self.assertIn('device/cdc_msc_freertos', tests)
            self.assertIn('device/audio_test_freertos', tests)
            self.assertIn('device/usbtest', tests)

    def test_core_device_change_includes_only_list_test(self):
        boards = on_roster(self, 'espressif_s3_devkitm', 'espressif_p4_function_ev')
        s = hil_select.classify(['src/device/usbd.c'], REPO, real_rosters())
        self.assertFalse(s['full'])
        for board in boards:
            tests = s['boards'][board]
            self.assertIn('device/hid_composite_freertos', tests)
            self.assertIn('device/cdc_msc_freertos', tests)
            self.assertIn('device/audio_test_freertos', tests)
            self.assertIn('device/usbtest', tests)

    def test_host_change_does_not_leak_device_only_list_test(self):
        s = hil_select.classify(['src/host/usbh.c'], REPO, real_rosters())
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
        self.assertEqual(hil_select.selection_args(s, ROSTERS), {'tinyusb.json': ''})
        self.assertEqual(hil_select.selection_args_by_flasher(s, ROSTERS), {'tinyusb.json': {}})

    def test_family_order_does_not_matter(self):
        # same as above with the full-matrix file last (was the only order that worked)
        s = sel(['src/portable/microchip/samx7x/dcd_samx7x.c', 'src/common/tusb_fifo.c'])
        self.assertTrue(s['full'])
        self.assertIn('same7x', s['families'])


class TestGitDiffArgv(unittest.TestCase):
    def test_diff_disables_rename_detection(self):
        """Without --no-renames git reports only a rename's destination, so moving an
        HIL-relevant file to a non-code path would be classified as non-code only."""
        self.assertIn('--no-renames', hil_select.GIT_DIFF_ARGV)


class TestPortWithoutFamilyIsFull(unittest.TestCase):
    """A port dir no family file references must widen (full matrix), not silently
    contribute zero boards — the fail-open contract."""
    def test_unreferenced_port_forces_full(self):
        orig = hil_select.port_families
        hil_select.port_families = lambda port_dir, repo_root: set()
        try:
            s = sel(['src/portable/vendor/newip/dcd_newip.c'])
        finally:
            hil_select.port_families = orig
        self.assertTrue(s['full'])
        self.assertTrue(any('no board family' in r for r in s['reasons']), s['reasons'])


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

    def test_openocd_seq_is_convoy_safe_over_jlink(self):
        self.assertTrue(hil_flash.convoy_safe(
            {'name': 'openocd_seq', 'args': '-f interface/jlink.cfg -f target/stm32f4x.cfg'}))

    def test_openocd_seq_uses_explicit_flash_commands_not_program(self):
        """`program` fails over the jlink transport: Examination failed -> auto_probe
        failed, measured on stm32f4x and stm32f0x."""
        from helper import hil_util
        seen = {}
        real = hil_util.run_cmd
        hil_util.run_cmd = lambda cmd, **k: seen.setdefault('cmd', cmd) or real('true')
        try:
            hil_flash.flash_openocd_seq(
                {'flasher': {'name': 'openocd_seq', 'uid': 'X', 'vid_pid': '0x1366 0x0101',
                             'args': '-f interface/jlink.cfg'}},
                '/tmp/fw.elf', timeout=5)
        finally:
            hil_util.run_cmd = real
        self.assertIn('flash write_image erase /tmp/fw.elf', seen['cmd'])
        self.assertIn('verify_image /tmp/fw.elf', seen['cmd'])
        self.assertNotIn('program ', seen['cmd'])

    def test_roster_recover_entries_are_convoy_safe_and_named_openocd_seq(self):
        recover = [b for path, b in roster_flashers()
                   if path == 'test/hil/tinyusb.json' and 'flasher_recover' in b]
        self.assertGreaterEqual(len(recover), 7)
        for b in recover:
            f = b['flasher_recover']
            self.assertEqual(f['name'], 'openocd_seq', b['name'])
            self.assertIn('interface/jlink.cfg', f['args'], b['name'])
            self.assertIn('adapter speed', f['args'], b['name'])   # required; see below
            self.assertTrue(hil_flash.convoy_safe(f), b['name'])


if __name__ == '__main__':
    unittest.main(verbosity=1)
