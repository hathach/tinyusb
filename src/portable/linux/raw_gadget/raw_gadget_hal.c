/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Roman Buchert
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#include "raw_gadget_private.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <linux/usb/ch9.h>

//--------------------------------------------------------------------+
// Internal Helpers
//--------------------------------------------------------------------+

static raw_gadget_result_t raw_gadget_speed_convert(raw_gadget_speed_t speed,
                                                    uint8_t *kernel_speed)
{
  if (kernel_speed == NULL)
  {
    return RAW_GADGET_RESULT_INVALID_ARGUMENT;
  }

  switch (speed)
  {
    case RAW_GADGET_SPEED_LOW:
      *kernel_speed = USB_SPEED_LOW;
      return RAW_GADGET_RESULT_SUCCESS;

    case RAW_GADGET_SPEED_FULL:
      *kernel_speed = USB_SPEED_FULL;
      return RAW_GADGET_RESULT_SUCCESS;

    case RAW_GADGET_SPEED_HIGH:
      *kernel_speed = USB_SPEED_HIGH;
      return RAW_GADGET_RESULT_SUCCESS;

    case RAW_GADGET_SPEED_INVALID:
    default:
      return RAW_GADGET_RESULT_INVALID_ARGUMENT;
  }
}

static raw_gadget_result_t raw_gadget_context_validate(
    raw_gadget_context_t const *context)
{
  if (context == NULL)
  {
    return RAW_GADGET_RESULT_INVALID_HANDLE;
  }

  if (!context->available)
  {
    return RAW_GADGET_RESULT_NOT_AVAILABLE;
  }

  if (!context->initialized ||
      (context->file_descriptor == RAW_GADGET_INVALID_FILE_DESCRIPTOR))
  {
    return RAW_GADGET_RESULT_NOT_INITIALIZED;
  }

  return RAW_GADGET_RESULT_SUCCESS;
}

//--------------------------------------------------------------------+
// Controller API
//--------------------------------------------------------------------+

raw_gadget_result_t raw_gadget_init(raw_gadget_handle_t handle,
                                    raw_gadget_speed_t speed,
                                    raw_gadget_event_callback_t callback)
{
  raw_gadget_context_t *context;
  raw_gadget_result_t result;
  struct usb_raw_init init = {0};
  uint8_t kernel_speed;
  int file_descriptor;

  if (callback == NULL)
  {
    return RAW_GADGET_RESULT_INVALID_ARGUMENT;
  }

  result = raw_gadget_speed_convert(speed, &kernel_speed);
  if (result != RAW_GADGET_RESULT_SUCCESS)
  {
    return result;
  }

  result = raw_gadget_udc_discover_once();
  if (result != RAW_GADGET_RESULT_SUCCESS)
  {
    return result;
  }

  context = raw_gadget_context_get(handle);
  if (context == NULL)
  {
    return RAW_GADGET_RESULT_INVALID_HANDLE;
  }

  if (pthread_mutex_lock(&context->mutex) != 0)
  {
    return RAW_GADGET_RESULT_INTERNAL_ERROR;
  }

  if (!context->available)
  {
    (void) pthread_mutex_unlock(&context->mutex);
    return RAW_GADGET_RESULT_NOT_AVAILABLE;
  }

  if (context->initialized)
  {
    (void) pthread_mutex_unlock(&context->mutex);
    return RAW_GADGET_RESULT_ALREADY_INITIALIZED;
  }

  file_descriptor = open(RAW_GADGET_DEVICE_PATH, O_RDWR | O_CLOEXEC);
  if (file_descriptor < 0)
  {
    (void) pthread_mutex_unlock(&context->mutex);
    return RAW_GADGET_RESULT_IO_ERROR;
  }

  if ((strlen(RAW_GADGET_UDC_DRIVER_NAME) >= sizeof(init.driver_name)) ||
      (strlen(context->udc_name) >= sizeof(init.device_name)))
  {
    (void) close(file_descriptor);
    (void) pthread_mutex_unlock(&context->mutex);
    return RAW_GADGET_RESULT_INTERNAL_ERROR;
  }

  (void) strcpy((char *) init.driver_name, RAW_GADGET_UDC_DRIVER_NAME);
  (void) strcpy((char *) init.device_name, context->udc_name);
  init.speed = kernel_speed;

  if (ioctl(file_descriptor, USB_RAW_IOCTL_INIT, &init) < 0)
  {
    (void) close(file_descriptor);
    (void) pthread_mutex_unlock(&context->mutex);
    return RAW_GADGET_RESULT_IO_ERROR;
  }

  context->speed = speed;
  context->ep0_max_packet_size =
      speed == RAW_GADGET_SPEED_LOW ? 8u : 64u;
  context->file_descriptor = file_descriptor;
  context->event_callback = callback;
  context->initialized = true;

  (void) pthread_mutex_unlock(&context->mutex);

  return RAW_GADGET_RESULT_SUCCESS;
}

raw_gadget_result_t raw_gadget_deinit(raw_gadget_handle_t handle)
{
  raw_gadget_context_t *context;
  raw_gadget_result_t result;
  raw_gadget_result_t close_result;
  int file_descriptor;

  context = raw_gadget_context_get(handle);
  result = raw_gadget_context_validate(context);
  if (result != RAW_GADGET_RESULT_SUCCESS)
  {
    return result;
  }

  if (pthread_mutex_lock(&context->mutex) != 0)
  {
    return RAW_GADGET_RESULT_INTERNAL_ERROR;
  }

  context->shutting_down = true;

  (void) pthread_mutex_unlock(&context->mutex);

  /*
   * Endpoint close cancels all transfer workers before disabling the kernel
   * endpoints. The event worker is stopped afterwards so no new HAL events can
   * be dispatched while the context is reset.
   */
  close_result = raw_gadget_endpoint_close_all(handle);
  raw_gadget_event_stop(context);

  if (pthread_mutex_lock(&context->mutex) != 0)
  {
    return RAW_GADGET_RESULT_INTERNAL_ERROR;
  }

  file_descriptor = context->file_descriptor;
  raw_gadget_context_reset(context);

  (void) pthread_mutex_unlock(&context->mutex);

  if ((file_descriptor != RAW_GADGET_INVALID_FILE_DESCRIPTOR) &&
      (close(file_descriptor) != 0))
  {
    return RAW_GADGET_RESULT_IO_ERROR;
  }

  return close_result;
}

raw_gadget_result_t raw_gadget_connect(raw_gadget_handle_t handle)
{
  raw_gadget_context_t *context;
  raw_gadget_result_t result;

  context = raw_gadget_context_get(handle);
  result = raw_gadget_context_validate(context);
  if (result != RAW_GADGET_RESULT_SUCCESS)
  {
    return result;
  }

  if (context->event_thread_created)
  {
    return RAW_GADGET_RESULT_SUCCESS;
  }

  if (ioctl(context->file_descriptor, USB_RAW_IOCTL_RUN, 0) < 0)
  {
    TU_LOG1(
            "Raw Gadget: USB_RAW_IOCTL_RUN failed: errno=%d (%s)\r\n",
            errno,
            strerror(errno));
    return RAW_GADGET_RESULT_IO_ERROR;
  }

  result = raw_gadget_event_start(context);
  if (result != RAW_GADGET_RESULT_SUCCESS)
  {
    TU_LOG1(
            "Raw Gadget: event thread start failed: result=%d\r\n",
            (int) result);
    /*
     * USB_RAW_IOCTL_RUN cannot be reversed. Destroy the Raw Gadget instance
     * if the event worker cannot be started, otherwise the connected device
     * would remain active without processing control requests.
     */
    (void) raw_gadget_deinit(handle);
    return result;
  }

  return RAW_GADGET_RESULT_SUCCESS;
}

raw_gadget_result_t raw_gadget_disconnect(raw_gadget_handle_t handle)
{
  raw_gadget_context_t *context;
  raw_gadget_result_t result;

  context = raw_gadget_context_get(handle);
  result = raw_gadget_context_validate(context);
  if (result != RAW_GADGET_RESULT_SUCCESS)
  {
    return result;
  }

  /*
   * Raw Gadget currently has no ioctl for a reversible software disconnect.
   * Closing the file descriptor disconnects and destroys the instance, which
   * is handled by raw_gadget_deinit().
   */
  return RAW_GADGET_RESULT_UNSUPPORTED;
}

raw_gadget_result_t raw_gadget_configure(raw_gadget_handle_t handle)
{
  raw_gadget_context_t *context;
  raw_gadget_result_t result;

  context = raw_gadget_context_get(handle);
  result = raw_gadget_context_validate(context);
  if (result != RAW_GADGET_RESULT_SUCCESS)
  {
    return result;
  }

  if (pthread_mutex_lock(&context->mutex) != 0)
  {
    return RAW_GADGET_RESULT_INTERNAL_ERROR;
  }

  if (context->configured)
  {
    (void) pthread_mutex_unlock(&context->mutex);
    return RAW_GADGET_RESULT_SUCCESS;
  }

  if (ioctl(context->file_descriptor, USB_RAW_IOCTL_CONFIGURE, 0) < 0)
  {
    TU_LOG1("Raw Gadget HAL: CONFIGURE failed: errno=%d (%s)\r\n",
            errno,
            strerror(errno));
    (void) pthread_mutex_unlock(&context->mutex);
    return RAW_GADGET_RESULT_IO_ERROR;
  }

  context->configured = true;
  (void) pthread_mutex_unlock(&context->mutex);

  return RAW_GADGET_RESULT_SUCCESS;
}

raw_gadget_result_t raw_gadget_set_vbus_draw(raw_gadget_handle_t handle,
                                             uint16_t current_ma)
{
  raw_gadget_context_t *context;
  raw_gadget_result_t result;
  uint32_t current_units;

  context = raw_gadget_context_get(handle);
  result = raw_gadget_context_validate(context);
  if (result != RAW_GADGET_RESULT_SUCCESS)
  {
    return result;
  }

  /*
   * USB_RAW_IOCTL_VBUS_DRAW expects the current limit in units of 2 mA.
   * Round odd milliampere values up so the requested limit is not reduced.
   */
  current_units = ((uint32_t) current_ma + 1u) / 2u;

  if (ioctl(context->file_descriptor,
            USB_RAW_IOCTL_VBUS_DRAW,
            &current_units) < 0)
  {
    return RAW_GADGET_RESULT_IO_ERROR;
  }

  return RAW_GADGET_RESULT_SUCCESS;
}
