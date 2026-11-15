/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 TinyUSB contributors
 * SPDX-License-Identifier: MIT
 */

// test_mtp_device's data OUT tests that end on a full buffer, built with CFG_TUD_MTP_EP_BUFSIZE = 1536: a multiple
// of the bulk packet size at either speed, but not a power of 2. test_mtp_device.c, built into this executable,
// supplies the fixture and the tests run below.

// what test_mtp_device.c includes and builds with, repeated for ceedling, which reads this file only
#include "unity.h"
#include "osal/osal.h"
#include "tusb_fifo.h"
#include "tusb.h"
#include "usbd.h"
#include "device/usbd_pvt.h"
TEST_SOURCE_FILE("usbd_control.c")
TEST_SOURCE_FILE("mtp_device.c")
TEST_SOURCE_FILE("test_mtp_device.c")
#include "mock_dcd.h"

TU_VERIFY_STATIC(CFG_TUD_MTP_EP_BUFSIZE == 1536, "test_mtp_device_1536 needs a 1536-byte buffer");

// Defined by test_mtp_device.c; the runner generated from this file runs the tests declared here. The last packet of
// a full buffer is full-sized, so the ZLP that ends the phase must be read before the response.
void setUp(void);
void tearDown(void);
void test_data_out_full_buffer_boundary_then_zlp(void);
void test_data_out_zlp_read_refused_stalls(void);
void test_data_out_app_read_takes_zlp(void);
