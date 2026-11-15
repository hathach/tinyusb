/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

// test_vendor_device's tests on the CFG_TUD_VENDOR_ALT_SETTINGS open path. That file, built into this executable,
// supplies the fixture; the runner generated from this file runs the tests declared here.

#include "unity.h"
#include "device/usbd_pvt.h"
#include "class/vendor/vendor_device.h"
TEST_SOURCE_FILE("vendor_device.c")
TEST_SOURCE_FILE("test_vendor_device.c")

TU_VERIFY_STATIC(CFG_TUD_VENDOR_ALT_SETTINGS, "test configuration");

void setUp(void);
void tearDown(void);
void test_vendor_open_rejects_out_mps_over_rx_buffer(void);
void test_vendor_open_accepts_out_mps_within_rx_buffer(void);
