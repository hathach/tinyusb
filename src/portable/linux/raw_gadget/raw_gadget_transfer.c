/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Roman Buchert
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#include "raw_gadget_private.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include <linux/usb/ch9.h>

//--------------------------------------------------------------------+
// Internal Types
//--------------------------------------------------------------------+

typedef struct
{
  raw_gadget_context_t *context;
  raw_gadget_endpoint_t *endpoint;

  uint8_t endpoint_address;
  uint8_t *buffer;
  size_t length;

  struct usb_raw_ep_io *io;
  raw_gadget_transfer_result_t result;
  size_t transferred_bytes;
  size_t completion_bytes;
  bool use_completion_bytes;
  bool perform_ioctl;
  bool force_ep0_read;
  bool complete_control_request;
  uint32_t generation;
} raw_gadget_transfer_job_t;

//--------------------------------------------------------------------+
// Internal Helpers
//--------------------------------------------------------------------+

static bool raw_gadget_transfer_endpoint_address_valid(uint8_t endpoint_address)
{
  return (endpoint_address & 0x70u) == 0u;
}

static raw_gadget_result_t raw_gadget_transfer_context_validate(
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

static raw_gadget_transfer_result_t raw_gadget_transfer_result_from_errno(
    int error_number)
{
  switch (error_number)
  {
    case EPIPE:
      return RAW_GADGET_TRANSFER_RESULT_STALLED;

    case ECANCELED:
      return RAW_GADGET_TRANSFER_RESULT_CANCELLED;

    default:
      return RAW_GADGET_TRANSFER_RESULT_FAILED;
  }
}

static void raw_gadget_transfer_job_cleanup(void *argument)
{
  raw_gadget_transfer_job_t *job = argument;
  raw_gadget_event_t event;
  bool detach_thread = false;
  bool dispatch_event = false;

  if (job == NULL)
  {
    return;
  }

  if (job->result == RAW_GADGET_TRANSFER_RESULT_CANCELLED)
  {
    job->transferred_bytes = 0;
  }

  memset(&event, 0, sizeof(event));
  event.type = RAW_GADGET_EVENT_TRANSFER_COMPLETE;
  event.data.transfer.endpoint_address = job->endpoint_address;
  event.data.transfer.transferred_bytes = job->transferred_bytes;
  event.data.transfer.result = job->result;

  if (pthread_mutex_lock(&job->context->mutex) == 0)
  {
    job->endpoint->transfer_active = false;

    if (!job->endpoint->transfer_cancel_pending)
    {
      job->endpoint->transfer_thread_created = false;
      memset(&job->endpoint->transfer_thread,
             0,
             sizeof(job->endpoint->transfer_thread));

      detach_thread = true;
      dispatch_event = job->context->initialized &&
                       !job->context->shutting_down;
    }

    (void) pthread_mutex_unlock(&job->context->mutex);
  }

  if (detach_thread)
  {
    (void) pthread_detach(pthread_self());
  }

  if (dispatch_event)
  {
    raw_gadget_event_dispatch_transfer(job->context, &event, job->generation);
  }

  if (job->complete_control_request && dispatch_event &&
      (job->result == RAW_GADGET_TRANSFER_RESULT_SUCCESS))
  {
    raw_gadget_control_request_complete_internal(job->context);
  }

  free(job->io);
  free(job);
}

static int raw_gadget_transfer_ioctl(raw_gadget_transfer_job_t *job)
{
  bool endpoint_zero;
  bool direction_in;
  unsigned long request;
  int result;
  int saved_errno;

  endpoint_zero = (job->endpoint_address & USB_ENDPOINT_NUMBER_MASK) == 0u;
  direction_in = (job->endpoint_address & USB_DIR_IN) != 0u;

  if (endpoint_zero)
  {
     request = job->force_ep0_read
                  ? USB_RAW_IOCTL_EP0_READ
                  : (direction_in ? USB_RAW_IOCTL_EP0_WRITE
                                  : USB_RAW_IOCTL_EP0_READ);
  }
  else
  {
     request = direction_in ? USB_RAW_IOCTL_EP_WRITE : USB_RAW_IOCTL_EP_READ;
  }

  result = ioctl(job->context->file_descriptor, request, job->io);
  saved_errno = errno;

  errno = saved_errno;
  return result;
}
static void *raw_gadget_transfer_thread(void *argument)
{
  raw_gadget_transfer_job_t *job = argument;
  int ioctl_result;
  int previous_cancel_state;
  int previous_cancel_type;

  if (job == NULL)
  {
    return NULL;
  }

  (void) pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &previous_cancel_state);
  (void) pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &previous_cancel_type);

  pthread_cleanup_push(raw_gadget_transfer_job_cleanup, job);

  if (!job->perform_ioctl)
  {
    job->result = RAW_GADGET_TRANSFER_RESULT_SUCCESS;
    job->transferred_bytes = job->completion_bytes;
  }
  else
  {
    ioctl_result = raw_gadget_transfer_ioctl(job);
    if (ioctl_result < 0)
    {
      job->result = raw_gadget_transfer_result_from_errno(errno);
      job->transferred_bytes = 0;
    }
    else
    {
      job->result = RAW_GADGET_TRANSFER_RESULT_SUCCESS;
      job->transferred_bytes = job->use_completion_bytes
                                 ? job->completion_bytes
                                 : (size_t) ioctl_result;

      if (((job->endpoint_address & USB_DIR_IN) == 0u) &&
          (job->transferred_bytes > 0))
      {
        memcpy(job->buffer, job->io->data, job->transferred_bytes);
      }
    }
  }

  pthread_cleanup_pop(1);

  (void) pthread_setcanceltype(previous_cancel_type, NULL);
  (void) pthread_setcancelstate(previous_cancel_state, NULL);

  return NULL;
}

static raw_gadget_result_t raw_gadget_transfer_cancel_endpoint(
    raw_gadget_context_t *context,
    raw_gadget_endpoint_t *endpoint)
{
  pthread_t transfer_thread;
  bool cancel_required = false;
  bool join_required = false;
  int cancel_result = 0;
  int join_result = 0;

  if (pthread_mutex_lock(&context->mutex) != 0)
  {
    return RAW_GADGET_RESULT_INTERNAL_ERROR;
  }

  if (endpoint->transfer_thread_created)
  {
    transfer_thread = endpoint->transfer_thread;
    endpoint->transfer_cancel_pending = true;
    cancel_required = endpoint->transfer_active;
    join_required = true;
  }

  (void) pthread_mutex_unlock(&context->mutex);

  if (cancel_required)
  {
    cancel_result = pthread_cancel(transfer_thread);
  }

  if (join_required)
  {
    join_result = pthread_join(transfer_thread, NULL);

    if (pthread_mutex_lock(&context->mutex) != 0)
    {
      return RAW_GADGET_RESULT_INTERNAL_ERROR;
    }

    endpoint->transfer_active = false;
    endpoint->transfer_thread_created = false;
    endpoint->transfer_cancel_pending = false;
    memset(&endpoint->transfer_thread, 0, sizeof(endpoint->transfer_thread));

    (void) pthread_mutex_unlock(&context->mutex);
  }

  if ((cancel_result != 0) || (join_result != 0))
  {
    return RAW_GADGET_RESULT_INTERNAL_ERROR;
  }

  return RAW_GADGET_RESULT_SUCCESS;
}

//--------------------------------------------------------------------+
// Transfer API
//--------------------------------------------------------------------+

raw_gadget_result_t raw_gadget_endpoint_transfer(raw_gadget_handle_t handle,
                                                 uint8_t endpoint_address,
                                                 uint8_t *buffer,
                                                 size_t length)
{
  raw_gadget_context_t *context;
  raw_gadget_endpoint_t *endpoint;
  raw_gadget_transfer_job_t *job;
  raw_gadget_result_t result;
  size_t allocation_size;
  size_t ioctl_length;
  bool endpoint_zero;
  bool direction_in;
  bool ep0_status_stage = false;
  bool ep0_real_status_stage = false;
  bool ep0_in_data = false;
  bool ep0_in_complete = false;
  int create_result;

  if (!raw_gadget_transfer_endpoint_address_valid(endpoint_address) ||
      ((buffer == NULL) && (length != 0u)) || (length > UINT32_MAX))
  {
    return RAW_GADGET_RESULT_INVALID_ARGUMENT;
  }

  context = raw_gadget_context_get(handle);
  result = raw_gadget_transfer_context_validate(context);
  if (result != RAW_GADGET_RESULT_SUCCESS)
  {
    return result;
  }

  endpoint = raw_gadget_endpoint_get(context, endpoint_address);
  endpoint_zero = (endpoint_address & USB_ENDPOINT_NUMBER_MASK) == 0u;
  direction_in = (endpoint_address & USB_DIR_IN) != 0u;

  if (pthread_mutex_lock(&context->mutex) != 0)
  {
    return RAW_GADGET_RESULT_INTERNAL_ERROR;
  }

  if (endpoint_zero && context->ep0_request_active)
  {
    ep0_status_stage = (length == 0u) &&
                       (direction_in != context->ep0_direction_in);
    ep0_real_status_stage = ep0_status_stage &&
                            (context->ep0_requested_length == 0u);
    ep0_in_data = context->ep0_direction_in && direction_in &&
                  !ep0_status_stage;
  }

  if (ep0_status_stage && !ep0_real_status_stage)
  {
    raw_gadget_event_t event = {0};

    (void) pthread_mutex_unlock(&context->mutex);

    event.type = RAW_GADGET_EVENT_TRANSFER_COMPLETE;
    event.data.transfer.endpoint_address = endpoint_address;
    event.data.transfer.transferred_bytes = 0;
    event.data.transfer.result = RAW_GADGET_TRANSFER_RESULT_SUCCESS;

    raw_gadget_event_dispatch(context, &event);
    raw_gadget_control_request_complete_internal(context);
    return RAW_GADGET_RESULT_SUCCESS;
  }

  if (endpoint->transfer_active || endpoint->transfer_thread_created ||
      endpoint->transfer_cancel_pending)
  {
    (void) pthread_mutex_unlock(&context->mutex);
    return RAW_GADGET_RESULT_BUSY;
  }

  if (!endpoint_zero && !endpoint->enabled)
  {
    (void) pthread_mutex_unlock(&context->mutex);
    return RAW_GADGET_RESULT_NOT_AVAILABLE;
  }

  if (!endpoint_zero && (endpoint->kernel_handle > UINT16_MAX))
  {
    (void) pthread_mutex_unlock(&context->mutex);
    return RAW_GADGET_RESULT_INTERNAL_ERROR;
  }

  ioctl_length = length;

  if (ep0_in_data)
  {
    size_t required_length;
    uint8_t *new_buffer;

    if (length > SIZE_MAX - context->ep0_accumulated_length)
    {
      (void) pthread_mutex_unlock(&context->mutex);
      return RAW_GADGET_RESULT_INVALID_ARGUMENT;
    }

    required_length = context->ep0_accumulated_length + length;
    if (required_length > context->ep0_buffer_capacity)
    {
      new_buffer = realloc(context->ep0_buffer, required_length);
      if (new_buffer == NULL)
      {
        (void) pthread_mutex_unlock(&context->mutex);
        return RAW_GADGET_RESULT_NO_MEMORY;
      }

      context->ep0_buffer = new_buffer;
      context->ep0_buffer_capacity = required_length;
    }

    if (length > 0u)
    {
      memcpy(&context->ep0_buffer[context->ep0_accumulated_length],
             buffer,
             length);
    }

    context->ep0_accumulated_length = required_length;
    ep0_in_complete =
        (required_length >= context->ep0_requested_length) ||
        (length < context->ep0_max_packet_size);
    ioctl_length = ep0_in_complete ? required_length : 0u;
  }

  allocation_size = sizeof(struct usb_raw_ep_io) + ioctl_length;
  job = calloc(1, sizeof(*job));
  if (job == NULL)
  {
    (void) pthread_mutex_unlock(&context->mutex);
    return RAW_GADGET_RESULT_NO_MEMORY;
  }

  job->io = malloc(allocation_size);
  if (job->io == NULL)
  {
    (void) pthread_mutex_unlock(&context->mutex);
    free(job);
    return RAW_GADGET_RESULT_NO_MEMORY;
  }

  job->context = context;
  job->endpoint = endpoint;
  job->endpoint_address = endpoint_address;
  job->buffer = buffer;
  job->length = ioctl_length;
  job->result = RAW_GADGET_TRANSFER_RESULT_CANCELLED;
  job->transferred_bytes = 0;
  job->completion_bytes = length;
  job->use_completion_bytes = ep0_in_data;
  job->perform_ioctl = !ep0_in_data || ep0_in_complete;
  job->force_ep0_read = ep0_real_status_stage && !context->ep0_direction_in;
  job->complete_control_request = ep0_real_status_stage;
  job->generation = context->transfer_generation;

  job->io->ep = endpoint_zero ? 0u : (uint16_t) endpoint->kernel_handle;
  job->io->flags = 0;
  job->io->length = (uint32_t) ioctl_length;

  if (ep0_in_data && ep0_in_complete && (ioctl_length > 0u))
  {
    memcpy(job->io->data, context->ep0_buffer, ioctl_length);
  }
  else if (!ep0_in_data && direction_in && (length > 0u))
  {
    memcpy(job->io->data, buffer, length);
  }

  endpoint->transfer_active = true;
  endpoint->transfer_cancel_pending = false;

  create_result =
      pthread_create(&endpoint->transfer_thread, NULL, raw_gadget_transfer_thread, job);
  if (create_result != 0)
  {
    endpoint->transfer_active = false;
    memset(&endpoint->transfer_thread, 0, sizeof(endpoint->transfer_thread));
    (void) pthread_mutex_unlock(&context->mutex);
    free(job->io);
    free(job);
    return RAW_GADGET_RESULT_INTERNAL_ERROR;
  }

  endpoint->transfer_thread_created = true;

  (void) pthread_mutex_unlock(&context->mutex);

  return RAW_GADGET_RESULT_SUCCESS;
}

raw_gadget_result_t raw_gadget_endpoint_transfer_cancel(
    raw_gadget_handle_t handle,
    uint8_t endpoint_address)
{
  raw_gadget_context_t *context;
  raw_gadget_endpoint_t *endpoint;
  raw_gadget_result_t result;

  if (!raw_gadget_transfer_endpoint_address_valid(endpoint_address))
  {
    return RAW_GADGET_RESULT_INVALID_ARGUMENT;
  }

  context = raw_gadget_context_get(handle);
  result = raw_gadget_transfer_context_validate(context);
  if (result != RAW_GADGET_RESULT_SUCCESS)
  {
    return result;
  }

  endpoint = raw_gadget_endpoint_get(context, endpoint_address);

  return raw_gadget_transfer_cancel_endpoint(context, endpoint);
}

void raw_gadget_transfer_cancel_all(raw_gadget_context_t *context)
{
  if (context == NULL)
  {
    return;
  }

  for (size_t index = 0; index < RAW_GADGET_ENDPOINT_COUNT; index++)
  {
    (void) raw_gadget_transfer_cancel_endpoint(context,
                                               &context->endpoints[index]);
  }
}
