/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Roman Buchert
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#pragma once

#include "raw_gadget_hal.h"
#include "common/tusb_debug.h"

#include <pthread.h>
#include <stdatomic.h>

#include <linux/usb/raw_gadget.h>

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// Constants
//--------------------------------------------------------------------+

/** Linux Raw Gadget device node. */
#define RAW_GADGET_DEVICE_PATH "/dev/raw-gadget"

/** Linux dummy UDC driver name. */
#define RAW_GADGET_UDC_DRIVER_NAME "dummy_udc"

/** Linux sysfs directory containing the available UDC instances. */
#define RAW_GADGET_UDC_SYSFS_PATH "/sys/class/udc"

/** Invalid file descriptor value. */
#define RAW_GADGET_INVALID_FILE_DESCRIPTOR (-1)

/** Maximum number of USB endpoint addresses, including both directions. */
#define RAW_GADGET_ENDPOINT_COUNT 32u

/** Invalid Raw Gadget kernel endpoint handle. */
#define RAW_GADGET_INVALID_ENDPOINT_HANDLE UINT32_MAX

//--------------------------------------------------------------------+
// Internal Types
//--------------------------------------------------------------------+

/**
 * @brief Internal state of one USB endpoint address.
 *
 * Endpoint zero uses only the transfer-related members. Non-control endpoints
 * additionally use @ref enabled and @ref kernel_handle.
 */
typedef struct
{
  bool enabled;
  uint8_t address;
  uint32_t kernel_handle;

  bool transfer_active;
  bool transfer_thread_created;
  bool transfer_cancel_pending;
  pthread_t transfer_thread;
} raw_gadget_endpoint_t;

/**
 * @brief Internal state of one Raw Gadget controller.
 */
typedef struct
{
  bool available;
  bool initialized;
  bool shutting_down;
  bool event_thread_created;

  raw_gadget_handle_t handle;
  raw_gadget_speed_t speed;

  char udc_name[UDC_NAME_LENGTH_MAX];

  int file_descriptor;

  pthread_t event_thread;
  pthread_mutex_t mutex;
  pthread_mutex_t callback_mutex;
  pthread_cond_t ep0_condition;
  atomic_bool event_thread_running;

  bool ep0_request_active;
  bool ep0_direction_in;
  uint16_t ep0_requested_length;
  uint16_t ep0_max_packet_size;
  size_t ep0_accumulated_length;
  uint8_t *ep0_buffer;
  size_t ep0_buffer_capacity;
  bool configured;
  uint32_t transfer_generation;

  raw_gadget_event_callback_t event_callback;

  raw_gadget_endpoint_t endpoints[RAW_GADGET_ENDPOINT_COUNT];
} raw_gadget_context_t;

/**
 * @brief Global Raw Gadget context manager.
 */
typedef struct
{
  raw_gadget_context_t *contexts;
  size_t context_count;

  pthread_mutex_t mutex;
  bool discovered;
} raw_gadget_manager_t;

//--------------------------------------------------------------------+
// Manager and Context API
//--------------------------------------------------------------------+

raw_gadget_manager_t *raw_gadget_manager_get(void);
raw_gadget_result_t raw_gadget_context_table_create(size_t context_count);
void raw_gadget_context_table_destroy(void);
raw_gadget_context_t *raw_gadget_context_get(raw_gadget_handle_t handle);
void raw_gadget_context_reset(raw_gadget_context_t *context);

//--------------------------------------------------------------------+
// Endpoint Helpers
//--------------------------------------------------------------------+

size_t raw_gadget_endpoint_index(uint8_t endpoint_address);

raw_gadget_endpoint_t *raw_gadget_endpoint_get(raw_gadget_context_t *context,
                                               uint8_t endpoint_address);

//--------------------------------------------------------------------+
// Transfer Helpers
//--------------------------------------------------------------------+

/**
 * @brief Cancel and reap all endpoint transfer threads of a context.
 *
 * The context must still be initialized and its file descriptor must remain
 * valid while this function runs.
 *
 * @param context Raw Gadget context.
 */
void raw_gadget_transfer_cancel_all(raw_gadget_context_t *context);

//--------------------------------------------------------------------+
// UDC Discovery API
//--------------------------------------------------------------------+

raw_gadget_result_t raw_gadget_udc_discover(void);
raw_gadget_result_t raw_gadget_udc_discover_once(void);

//--------------------------------------------------------------------+
// Event API
//--------------------------------------------------------------------+

raw_gadget_result_t raw_gadget_event_start(raw_gadget_context_t *context);
void raw_gadget_event_stop(raw_gadget_context_t *context);
void raw_gadget_event_dispatch(raw_gadget_context_t *context,
                               raw_gadget_event_t const *event);
void raw_gadget_event_dispatch_transfer(raw_gadget_context_t *context,
                                        raw_gadget_event_t const *event,
                                        uint32_t generation);
void raw_gadget_control_request_complete_internal(raw_gadget_context_t *context);
void raw_gadget_bus_reset_prepare(raw_gadget_context_t *context);

#ifdef __cplusplus
}
#endif
