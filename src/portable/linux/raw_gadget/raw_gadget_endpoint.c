/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Roman Buchert
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#include "raw_gadget_private.h"

#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>

#include <linux/usb/ch9.h>

//--------------------------------------------------------------------+
// Internal Helpers
//--------------------------------------------------------------------+

static bool raw_gadget_endpoint_address_valid(uint8_t endpoint_address)
{
  return (endpoint_address & 0x70u) == 0u;
}

static raw_gadget_result_t raw_gadget_endpoint_context_validate(
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

static raw_gadget_result_t raw_gadget_endpoint_disable_locked(
    raw_gadget_context_t *context,
    raw_gadget_endpoint_t *endpoint)
{
  uint32_t kernel_handle;

  if (!endpoint->enabled)
  {
    return RAW_GADGET_RESULT_SUCCESS;
  }

  kernel_handle = endpoint->kernel_handle;

  if (ioctl(context->file_descriptor,
            USB_RAW_IOCTL_EP_DISABLE,
            &kernel_handle) < 0)
  {
    return RAW_GADGET_RESULT_IO_ERROR;
  }

  endpoint->enabled = false;
  endpoint->address = 0;
  endpoint->kernel_handle = RAW_GADGET_INVALID_ENDPOINT_HANDLE;

  return RAW_GADGET_RESULT_SUCCESS;
}

//--------------------------------------------------------------------+
// Endpoint API
//--------------------------------------------------------------------+

raw_gadget_result_t raw_gadget_endpoint_open(raw_gadget_handle_t handle,
                                             uint8_t const *descriptor,
                                             size_t descriptor_length)
{
  raw_gadget_context_t *context;
  raw_gadget_endpoint_t *endpoint;
  struct usb_endpoint_descriptor kernel_descriptor;
  raw_gadget_result_t result;
  uint8_t endpoint_address;
  int kernel_handle;

  if ((descriptor == NULL) || (descriptor_length < USB_DT_ENDPOINT_SIZE))
  {
    return RAW_GADGET_RESULT_INVALID_ARGUMENT;
  }

  memset(&kernel_descriptor, 0, sizeof(kernel_descriptor));
  memcpy(&kernel_descriptor, descriptor, USB_DT_ENDPOINT_SIZE);

  if ((kernel_descriptor.bLength < USB_DT_ENDPOINT_SIZE) ||
      (kernel_descriptor.bDescriptorType != USB_DT_ENDPOINT))
  {
    return RAW_GADGET_RESULT_INVALID_ARGUMENT;
  }

  endpoint_address = kernel_descriptor.bEndpointAddress;

  if (!raw_gadget_endpoint_address_valid(endpoint_address) ||
      ((endpoint_address & USB_ENDPOINT_NUMBER_MASK) == 0u))
  {
    return RAW_GADGET_RESULT_INVALID_ARGUMENT;
  }

  context = raw_gadget_context_get(handle);
  result = raw_gadget_endpoint_context_validate(context);
  if (result != RAW_GADGET_RESULT_SUCCESS)
  {
    return result;
  }

  if (pthread_mutex_lock(&context->mutex) != 0)
  {
    return RAW_GADGET_RESULT_INTERNAL_ERROR;
  }

  endpoint = raw_gadget_endpoint_get(context, endpoint_address);
  if (endpoint->enabled)
  {
    (void) pthread_mutex_unlock(&context->mutex);
    return RAW_GADGET_RESULT_ALREADY_INITIALIZED;
  }

  kernel_handle = ioctl(context->file_descriptor,
                        USB_RAW_IOCTL_EP_ENABLE,
                        &kernel_descriptor);
  if (kernel_handle < 0)
  {
    TU_LOG1("Raw Gadget HAL: EP_ENABLE failed: ep=%02x errno=%d (%s)\r\n",
            endpoint_address,
            errno,
            strerror(errno));
    (void) pthread_mutex_unlock(&context->mutex);
    return RAW_GADGET_RESULT_IO_ERROR;
  }

  endpoint->enabled = true;
  endpoint->address = endpoint_address;
  endpoint->kernel_handle = (uint32_t) kernel_handle;

  (void) pthread_mutex_unlock(&context->mutex);

  return RAW_GADGET_RESULT_SUCCESS;
}

raw_gadget_result_t raw_gadget_endpoint_close(raw_gadget_handle_t handle,
                                              uint8_t endpoint_address)
{
  raw_gadget_context_t *context;
  raw_gadget_endpoint_t *endpoint;
  raw_gadget_result_t result;

  if (!raw_gadget_endpoint_address_valid(endpoint_address) ||
      ((endpoint_address & USB_ENDPOINT_NUMBER_MASK) == 0u))
  {
    return RAW_GADGET_RESULT_INVALID_ARGUMENT;
  }

  context = raw_gadget_context_get(handle);
  result = raw_gadget_endpoint_context_validate(context);
  if (result != RAW_GADGET_RESULT_SUCCESS)
  {
    return result;
  }

  result = raw_gadget_endpoint_transfer_cancel(handle, endpoint_address);
  if (result != RAW_GADGET_RESULT_SUCCESS)
  {
    return result;
  }

  if (pthread_mutex_lock(&context->mutex) != 0)
  {
    return RAW_GADGET_RESULT_INTERNAL_ERROR;
  }

  endpoint = raw_gadget_endpoint_get(context, endpoint_address);
  result = raw_gadget_endpoint_disable_locked(context, endpoint);

  (void) pthread_mutex_unlock(&context->mutex);

  return result;
}

raw_gadget_result_t raw_gadget_endpoint_close_all(raw_gadget_handle_t handle)
{
  raw_gadget_context_t *context;
  raw_gadget_result_t result;
  raw_gadget_result_t first_error = RAW_GADGET_RESULT_SUCCESS;

  context = raw_gadget_context_get(handle);
  result = raw_gadget_endpoint_context_validate(context);
  if (result != RAW_GADGET_RESULT_SUCCESS)
  {
    return result;
  }

  raw_gadget_transfer_cancel_all(context);

  if (pthread_mutex_lock(&context->mutex) != 0)
  {
    return RAW_GADGET_RESULT_INTERNAL_ERROR;
  }

  for (size_t index = 0; index < RAW_GADGET_ENDPOINT_COUNT; index++)
  {
    result =
        raw_gadget_endpoint_disable_locked(context, &context->endpoints[index]);

    if ((result != RAW_GADGET_RESULT_SUCCESS) &&
        (first_error == RAW_GADGET_RESULT_SUCCESS))
    {
      first_error = result;
    }
  }

  (void) pthread_mutex_unlock(&context->mutex);

  return first_error;
}

raw_gadget_result_t raw_gadget_endpoint_stall(raw_gadget_handle_t handle,
                                              uint8_t endpoint_address)
{
  raw_gadget_context_t *context;
  raw_gadget_endpoint_t *endpoint;
  raw_gadget_result_t result;
  uint32_t kernel_handle;

  context = raw_gadget_context_get(handle);
  result = raw_gadget_endpoint_context_validate(context);
  if (result != RAW_GADGET_RESULT_SUCCESS)
  {
    return result;
  }

  if (pthread_mutex_lock(&context->mutex) != 0)
  {
    return RAW_GADGET_RESULT_INTERNAL_ERROR;
  }

  if ((endpoint_address & USB_ENDPOINT_NUMBER_MASK) == 0u)
  {
    if (!context->ep0_request_active)
    {
      (void) pthread_mutex_unlock(&context->mutex);
      return RAW_GADGET_RESULT_SUCCESS;
    }

    if (ioctl(context->file_descriptor, USB_RAW_IOCTL_EP0_STALL, 0) < 0)
    {
      TU_LOG1("Raw Gadget: EP0_STALL failed: errno=%d (%s)\r\n",
              errno,
              strerror(errno));
      result = RAW_GADGET_RESULT_IO_ERROR;
    }
    else
    {
      result = RAW_GADGET_RESULT_SUCCESS;
    }

    (void) pthread_mutex_unlock(&context->mutex);
    raw_gadget_control_request_complete_internal(context);
    return result;
  }

  if (!raw_gadget_endpoint_address_valid(endpoint_address))
  {
    (void) pthread_mutex_unlock(&context->mutex);
    return RAW_GADGET_RESULT_INVALID_ARGUMENT;
  }

  endpoint = raw_gadget_endpoint_get(context, endpoint_address);
  if (!endpoint->enabled)
  {
    (void) pthread_mutex_unlock(&context->mutex);
    return RAW_GADGET_RESULT_NOT_AVAILABLE;
  }

  kernel_handle = endpoint->kernel_handle;

  if (ioctl(context->file_descriptor,
            USB_RAW_IOCTL_EP_SET_HALT,
            &kernel_handle) < 0)
  {
    result = RAW_GADGET_RESULT_IO_ERROR;
  }
  else
  {
    result = RAW_GADGET_RESULT_SUCCESS;
  }

  (void) pthread_mutex_unlock(&context->mutex);

  return result;
}

raw_gadget_result_t raw_gadget_endpoint_clear_stall(
    raw_gadget_handle_t handle,
    uint8_t endpoint_address)
{
  raw_gadget_context_t *context;
  raw_gadget_endpoint_t *endpoint;
  raw_gadget_result_t result;
  uint32_t kernel_handle;

  if (!raw_gadget_endpoint_address_valid(endpoint_address) ||
      ((endpoint_address & USB_ENDPOINT_NUMBER_MASK) == 0u))
  {
    return RAW_GADGET_RESULT_INVALID_ARGUMENT;
  }

  context = raw_gadget_context_get(handle);
  result = raw_gadget_endpoint_context_validate(context);
  if (result != RAW_GADGET_RESULT_SUCCESS)
  {
    return result;
  }

  if (pthread_mutex_lock(&context->mutex) != 0)
  {
    return RAW_GADGET_RESULT_INTERNAL_ERROR;
  }

  endpoint = raw_gadget_endpoint_get(context, endpoint_address);
  if (!endpoint->enabled)
  {
    (void) pthread_mutex_unlock(&context->mutex);
    return RAW_GADGET_RESULT_NOT_AVAILABLE;
  }

  kernel_handle = endpoint->kernel_handle;

  if (ioctl(context->file_descriptor,
            USB_RAW_IOCTL_EP_CLEAR_HALT,
            &kernel_handle) < 0)
  {
    result = RAW_GADGET_RESULT_IO_ERROR;
  }
  else
  {
    result = RAW_GADGET_RESULT_SUCCESS;
  }

  (void) pthread_mutex_unlock(&context->mutex);

  return result;
}
