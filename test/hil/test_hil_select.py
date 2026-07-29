#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Unit tests for hil_select.py — pure logic, no hardware, no git. Run directly:
#   python3 test/hil/test_hil_select.py
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hil_select
from hil_examples import device_tests, dual_tests, host_test

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

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


if __name__ == '__main__':
    unittest.main(verbosity=1)
