#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Unit tests for hil_select.py — pure logic, no hardware, no git. Run directly:
#   python3 test/hil/test_hil_select.py
import glob
import json
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hil_select
from hil_examples import device_tests, dual_tests, host_test

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def real_rosters():
    """The actual rig rosters, for regression tests that need real-world data
    (a specific board/family/only-list) rather than the synthetic ROSTER above."""
    rosters = []
    for name in ('tinyusb.json', 'hfp.json'):
        path = os.path.join(REPO, 'test/hil', name)
        with open(path) as f:
            rosters.append((f'test/hil/{name}', json.load(f)['boards']))
    return rosters


ROSTER = [
    # device-only, rp2040 family
    {'name': 'raspberry_pi_pico', 'uid': 'u1',
     'tests': {'device': True, 'host': True, 'dual': True}},
    # device-only, stm32f4 family
    {'name': 'stm32f407disco', 'uid': 'u2',
     'tests': {'device': True, 'host': False, 'dual': False}},
    # host-only board
    {'name': 'raspberry_pi_pico2', 'uid': 'u3',
     'tests': {'device': False, 'host': True, 'dual': False}},
    # only-list board (espressif-style)
    {'name': 'espressif_s3_devkitm', 'uid': 'u4',
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

    def test_cli_diff_file(self):
        import subprocess, tempfile, json as j
        with tempfile.NamedTemporaryFile('w', suffix='.txt', delete=False) as f:
            f.write('src/class/cdc/cdc_device.c\n')
            path = f.name
        r = subprocess.run([sys.executable, os.path.join(REPO, 'test/hil/hil_select.py'),
                            '--diff-file', path, os.path.join(REPO, 'test/hil/tinyusb.json')],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        out = j.loads(r.stdout)
        self.assertFalse(out['full'])
        self.assertIn('tinyusb.json', out['args'])
        self.assertTrue(any('cdc_device' in line for line in out['reasons']))
        os.unlink(path)


class TestRealRosterPortFamilies(unittest.TestCase):
    """Regression for port_families() missing espressif's dwc2 reference, which
    lives in a component CMakeLists.txt rather than family.cmake/family.mk."""
    def test_dwc2_change_selects_espressif_boards(self):
        s = hil_select.classify(['src/portable/synopsys/dwc2/dcd_dwc2.c'], REPO, real_rosters())
        self.assertFalse(s['full'])
        self.assertIn('espressif_s3_devkitm', s['boards'])
        self.assertIn('espressif_p4_function_ev', s['boards'])


class TestPortFamiliesCoverage(unittest.TestCase):
    """Systematic guard: every real dcd_*/hcd_* port directory should map to at
    least one board family, so a future family.cmake/CMakeLists.txt layout that
    port_families() doesn't scan fails loudly instead of silently dropping boards
    (as espressif's dwc2 reference did - see TestRealRosterPortFamilies)."""
    # Ports with no board family: not a bug, just not wired into any rig board.
    # Add here (with a reason) only if port_families() legitimately can't find one.
    NO_FAMILY = {
        'template',  # reference/example port, not built by any board
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
    being invisible to the selector because it only knew the shared hil_examples lists."""
    def test_only_list_example_change_selects_it(self):
        s = hil_select.classify(['examples/device/hid_composite_freertos/src/main.c'], REPO, real_rosters())
        self.assertFalse(s['full'])
        self.assertEqual(s['boards']['espressif_s3_devkitm'], ['device/hid_composite_freertos'])
        self.assertEqual(s['boards']['espressif_p4_function_ev'], ['device/hid_composite_freertos'])

    def test_class_change_includes_only_list_boards(self):
        s = hil_select.classify(['src/class/hid/hid_device.c'], REPO, real_rosters())
        self.assertFalse(s['full'])
        self.assertIn('espressif_s3_devkitm', s['boards'])
        self.assertIn('espressif_p4_function_ev', s['boards'])


class TestPortAndCoreRoleUseExtras(unittest.TestCase):
    """Regression: the port rule and core-role rule must thread the roster-only
    test universe (extras) the same way the class rule already does, so a DCD
    or device-stack change doesn't silently drop espressif's only-list tests
    (e.g. hid_composite_freertos) that aren't in the shared device_tests list."""
    def test_dcd_change_includes_only_list_test(self):
        s = hil_select.classify(['src/portable/synopsys/dwc2/dcd_dwc2.c'], REPO, real_rosters())
        self.assertFalse(s['full'])
        for board in ('espressif_s3_devkitm', 'espressif_p4_function_ev'):
            tests = s['boards'][board]
            self.assertIn('device/hid_composite_freertos', tests)
            self.assertIn('device/cdc_msc_freertos', tests)
            self.assertIn('device/audio_test_freertos', tests)
            self.assertIn('device/usbtest', tests)

    def test_core_device_change_includes_only_list_test(self):
        s = hil_select.classify(['src/device/usbd.c'], REPO, real_rosters())
        self.assertFalse(s['full'])
        for board in ('espressif_s3_devkitm', 'espressif_p4_function_ev'):
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


if __name__ == '__main__':
    unittest.main(verbosity=1)
