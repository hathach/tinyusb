#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# HIL example test lists, shared by hil_test.py (runner) and hil_select.py
# (PR-diff selector). Stdlib-only: hil_select runs on bare CI runners.

# The per-board run order is shuffled (see test_board).
# Every example carries a unique hardcoded idProduct (see its usb_descriptors.c)

# device tests
device_tests = [
    'device/cdc_dual_ports',
    'device/cdc_msc',
    'device/dfu',
    'device/cdc_msc_throughput',
    'device/audio_test_freertos',
    'device/dfu_runtime',
    'device/cdc_msc_freertos',
    'device/hid_boot_interface',
    'device/msc_dual_lun',
    'device/hid_generic_inout',
    'device/printer_to_cdc',
    'device/midi_test',
    'device/mtp',
    'device/usbtest',  # cafe:4010, unique PID; runs the Linux testusb tier-4 battery via usbtest.py
    # 'device/net_lwip_webserver',  # disabled for PR #3605: USB net iface enum is flaky on the CI HIL host
]

dual_tests = [
    'dual/host_info_to_device_cdc',
]

host_test = [
    'host/cdc_msc_hid',
    'host/msc_file_explorer',
    'host/msc_file_explorer_freertos',
    'host/device_info',
]
