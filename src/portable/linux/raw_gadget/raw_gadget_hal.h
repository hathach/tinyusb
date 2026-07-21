/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Roman Buchert
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------+
// Types
//--------------------------------------------------------------------+

/**
 * @brief Raw Gadget controller handle.
 *
 * The handle identifies one Linux Raw Gadget instance. Handle values are
 * mapped to dummy UDC instances by the implementation.
 */
typedef uint8_t raw_gadget_handle_t;

/**
 * @brief Result codes returned by the Raw Gadget HAL.
 */
typedef enum
{
  RAW_GADGET_RESULT_SUCCESS = 0,
  RAW_GADGET_RESULT_INVALID_ARGUMENT,
  RAW_GADGET_RESULT_INVALID_HANDLE,
  RAW_GADGET_RESULT_NOT_AVAILABLE,
  RAW_GADGET_RESULT_ALREADY_INITIALIZED,
  RAW_GADGET_RESULT_NOT_INITIALIZED,
  RAW_GADGET_RESULT_BUSY,
  RAW_GADGET_RESULT_UNSUPPORTED,
  RAW_GADGET_RESULT_NO_MEMORY,
  RAW_GADGET_RESULT_IO_ERROR,
  RAW_GADGET_RESULT_INTERNAL_ERROR
} raw_gadget_result_t;

/**
 * @brief USB device operating speed.
 */
typedef enum
{
  RAW_GADGET_SPEED_INVALID = 0,
  RAW_GADGET_SPEED_LOW,
  RAW_GADGET_SPEED_FULL,
  RAW_GADGET_SPEED_HIGH
} raw_gadget_speed_t;

/**
 * @brief Transfer completion status.
 */
typedef enum
{
  RAW_GADGET_TRANSFER_RESULT_SUCCESS = 0,
  RAW_GADGET_TRANSFER_RESULT_FAILED,
  RAW_GADGET_TRANSFER_RESULT_STALLED,
  RAW_GADGET_TRANSFER_RESULT_CANCELLED
} raw_gadget_transfer_result_t;

/**
 * @brief Raw Gadget event type.
 */
typedef enum
{
  RAW_GADGET_EVENT_CONNECT = 0,
  RAW_GADGET_EVENT_RESET,
  RAW_GADGET_EVENT_SUSPEND,
  RAW_GADGET_EVENT_RESUME,
  RAW_GADGET_EVENT_DISCONNECT,
  RAW_GADGET_EVENT_SETUP_RECEIVED,
  RAW_GADGET_EVENT_TRANSFER_COMPLETE
} raw_gadget_event_type_t;

/**
 * @brief USB control request received on endpoint zero.
 *
 * The request contains the unmodified eight-byte USB setup packet in wire
 * format. Interpretation and byte-order conversion are intentionally left to
 * the caller.
 */
typedef struct
{
  uint8_t data[8];
} raw_gadget_control_request_t;

/**
 * @brief Raw Gadget event.
 *
 * The callback receives a pointer to this object. The object is valid only for
 * the duration of the callback and must be copied by the caller if it is needed
 * afterwards.
 */
typedef struct
{
  raw_gadget_event_type_t type;

  union
  {
    struct
    {
      raw_gadget_speed_t speed;
    } connect;

    struct
    {
      raw_gadget_speed_t speed;
    } reset;

    raw_gadget_control_request_t setup;

    struct
    {
      uint8_t endpoint_address;
      size_t transferred_bytes;
      raw_gadget_transfer_result_t result;
    } transfer;
  } data;
} raw_gadget_event_t;

/**
 * @brief Raw Gadget event callback.
 *
 * The callback may be invoked from an implementation-owned worker thread.
 * Callback implementations must therefore be thread-safe and must not block
 * indefinitely.
 *
 * @param handle Raw Gadget controller handle that generated the event.
 * @param event Event information.
 */
typedef void (*raw_gadget_event_callback_t)(raw_gadget_handle_t handle,
                                            raw_gadget_event_t const *event);

//--------------------------------------------------------------------+
// Controller API
//--------------------------------------------------------------------+

/**
 * @brief Initialize a Raw Gadget controller.
 *
 * The implementation maps @p handle to a Linux dummy UDC instance, opens the
 * Raw Gadget device, initializes the kernel interface and starts event
 * processing.
 *
 * @param handle Raw Gadget controller handle.
 * @param speed Maximum USB device speed requested for the controller.
 * @param callback Callback used to report controller and transfer events.
 *
 * @return RAW_GADGET_RESULT_SUCCESS on success; otherwise an error code.
 */
raw_gadget_result_t raw_gadget_init(raw_gadget_handle_t handle,
                                    raw_gadget_speed_t speed,
                                    raw_gadget_event_callback_t callback);

/**
 * @brief Deinitialize a Raw Gadget controller.
 *
 * Pending transfers are cancelled, enabled endpoints are disabled, event
 * processing is stopped and all resources owned by the controller are
 * released.
 *
 * @param handle Raw Gadget controller handle.
 *
 * @return RAW_GADGET_RESULT_SUCCESS on success; otherwise an error code.
 */
raw_gadget_result_t raw_gadget_deinit(raw_gadget_handle_t handle);

/**
 * @brief Connect the emulated USB device to its host.
 *
 * @param handle Raw Gadget controller handle.
 *
 * @return RAW_GADGET_RESULT_SUCCESS on success; otherwise an error code.
 */
raw_gadget_result_t raw_gadget_connect(raw_gadget_handle_t handle);

/**
 * @brief Disconnect the emulated USB device from its host.
 *
 * @param handle Raw Gadget controller handle.
 *
 * @return RAW_GADGET_RESULT_SUCCESS on success; otherwise an error code.
 */
raw_gadget_result_t raw_gadget_disconnect(raw_gadget_handle_t handle);

/**
 * @brief Notify the Raw Gadget controller that the device is configured.
 *
 * @param handle Raw Gadget controller handle.
 *
 * @return RAW_GADGET_RESULT_SUCCESS on success; otherwise an error code.
 */
raw_gadget_result_t raw_gadget_configure(raw_gadget_handle_t handle);

/**
 * @brief Set the USB bus current draw reported to the Raw Gadget controller.
 *
 * @param handle Raw Gadget controller handle.
 * @param current_ma Current draw in milliamperes.
 *
 * @return RAW_GADGET_RESULT_SUCCESS on success; otherwise an error code.
 */
raw_gadget_result_t raw_gadget_set_vbus_draw(raw_gadget_handle_t handle,
                                             uint16_t current_ma);

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+

/**
 * @brief Enable a non-control endpoint.
 *
 * @param handle Raw Gadget controller handle.
 * @param descriptor USB endpoint descriptor in wire format.
 * @param descriptor_length Size of @p descriptor in bytes.
 *
 * @return RAW_GADGET_RESULT_SUCCESS on success; otherwise an error code.
 */
raw_gadget_result_t raw_gadget_endpoint_open(raw_gadget_handle_t handle,
                                             uint8_t const *descriptor,
                                             size_t descriptor_length);

/**
 * @brief Disable an endpoint.
 *
 * @param handle Raw Gadget controller handle.
 * @param endpoint_address USB endpoint address.
 *
 * @return RAW_GADGET_RESULT_SUCCESS on success; otherwise an error code.
 */
raw_gadget_result_t raw_gadget_endpoint_close(raw_gadget_handle_t handle,
                                              uint8_t endpoint_address);

/**
 * @brief Disable all non-control endpoints.
 *
 * @param handle Raw Gadget controller handle.
 *
 * @return RAW_GADGET_RESULT_SUCCESS on success; otherwise an error code.
 */
raw_gadget_result_t raw_gadget_endpoint_close_all(raw_gadget_handle_t handle);

/**
 * @brief Submit an endpoint transfer.
 *
 * Transfer completion is reported asynchronously through
 * RAW_GADGET_EVENT_TRANSFER_COMPLETE.
 *
 * @param handle Raw Gadget controller handle.
 * @param endpoint_address USB endpoint address.
 * @param buffer Transfer buffer.
 * @param length Number of bytes to transfer.
 *
 * @return RAW_GADGET_RESULT_SUCCESS if the transfer was accepted; otherwise an
 * error code.
 */
raw_gadget_result_t raw_gadget_endpoint_transfer(raw_gadget_handle_t handle,
                                                 uint8_t endpoint_address,
                                                 uint8_t *buffer,
                                                 size_t length);

/**
 * @brief Cancel a pending endpoint transfer.
 *
 * @param handle Raw Gadget controller handle.
 * @param endpoint_address USB endpoint address.
 *
 * @return RAW_GADGET_RESULT_SUCCESS on success; otherwise an error code.
 */
raw_gadget_result_t raw_gadget_endpoint_transfer_cancel(raw_gadget_handle_t handle,
                                                        uint8_t endpoint_address);

/**
 * @brief Stall an endpoint.
 *
 * @param handle Raw Gadget controller handle.
 * @param endpoint_address USB endpoint address.
 *
 * @return RAW_GADGET_RESULT_SUCCESS on success; otherwise an error code.
 */
raw_gadget_result_t raw_gadget_endpoint_stall(raw_gadget_handle_t handle,
                                              uint8_t endpoint_address);

/**
 * @brief Clear the stall condition of an endpoint.
 *
 * @param handle Raw Gadget controller handle.
 * @param endpoint_address USB endpoint address.
 *
 * @return RAW_GADGET_RESULT_SUCCESS on success; otherwise an error code.
 */
raw_gadget_result_t raw_gadget_endpoint_clear_stall(raw_gadget_handle_t handle,
                                                    uint8_t endpoint_address);

#ifdef __cplusplus
}
#endif
