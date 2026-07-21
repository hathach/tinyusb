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

//--------------------------------------------------------------------+
// Internal Types
//--------------------------------------------------------------------+

typedef struct
{
  uint32_t type;
  uint32_t length;
  uint8_t data[sizeof(raw_gadget_control_request_t)];
} raw_gadget_kernel_event_t;

//--------------------------------------------------------------------+
// Internal Helpers
//--------------------------------------------------------------------+

static void raw_gadget_event_thread_cleanup(void *argument)
{
  raw_gadget_context_t *context = argument;

  if (context != NULL)
  {
    atomic_store(&context->event_thread_running, false);
  }
}

static bool raw_gadget_event_convert(raw_gadget_context_t const *context,
                                     raw_gadget_kernel_event_t const *kernel_event,
                                     raw_gadget_event_t *event)
{
  if ((context == NULL) || (kernel_event == NULL) || (event == NULL))
  {
    return false;
  }

  memset(event, 0, sizeof(*event));

  switch (kernel_event->type)
  {
    case USB_RAW_EVENT_CONNECT:
      event->type = RAW_GADGET_EVENT_CONNECT;
      event->data.connect.speed = context->speed;
      return true;

    case USB_RAW_EVENT_CONTROL:
      if (kernel_event->length != sizeof(event->data.setup.data))
      {
        return false;
      }

      event->type = RAW_GADGET_EVENT_SETUP_RECEIVED;
      memcpy(event->data.setup.data,
             kernel_event->data,
             sizeof(event->data.setup.data));
      return true;

    case USB_RAW_EVENT_SUSPEND:
      event->type = RAW_GADGET_EVENT_SUSPEND;
      return true;

    case USB_RAW_EVENT_RESUME:
      event->type = RAW_GADGET_EVENT_RESUME;
      return true;

    case USB_RAW_EVENT_RESET:
      event->type = RAW_GADGET_EVENT_RESET;
      event->data.reset.speed = context->speed;
      return true;

    case USB_RAW_EVENT_DISCONNECT:
      event->type = RAW_GADGET_EVENT_DISCONNECT;
      return true;

    case USB_RAW_EVENT_INVALID:
    default:
      return false;
  }
}

static void *raw_gadget_event_thread(void *argument)
{
  raw_gadget_context_t *context = argument;
  int previous_cancel_state;
  int previous_cancel_type;

  if (context == NULL)
  {
    return NULL;
  }

  (void) pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &previous_cancel_state);
  (void) pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &previous_cancel_type);

  pthread_cleanup_push(raw_gadget_event_thread_cleanup, context);

  atomic_store(&context->event_thread_running, true);

  while (atomic_load(&context->event_thread_running))
  {
    raw_gadget_kernel_event_t kernel_event = {
      .type = USB_RAW_EVENT_INVALID,
      .length = sizeof(kernel_event.data),
      .data = {0},
    };
    raw_gadget_event_t event;
    int ioctl_result;

    ioctl_result = ioctl(context->file_descriptor,
                         USB_RAW_IOCTL_EVENT_FETCH,
                         &kernel_event);

    if (ioctl_result < 0)
    {
      if (errno == EINTR)
      {
        continue;
      }

      TU_LOG1(
              "Raw Gadget: EVENT_FETCH failed: errno=%d (%s)\r\n",
              errno,
              strerror(errno));
      break;
    }

    if (raw_gadget_event_convert(context, &kernel_event, &event))
    {
      if (event.type == RAW_GADGET_EVENT_RESET)
      {
        raw_gadget_bus_reset_prepare(context);
      }
      else if (event.type == RAW_GADGET_EVENT_SETUP_RECEIVED)
      {
        if (pthread_mutex_lock(&context->mutex) != 0)
        {
          break;
        }

        while (context->ep0_request_active &&
               atomic_load(&context->event_thread_running))
        {
          (void) pthread_cond_wait(&context->ep0_condition, &context->mutex);
        }

        context->ep0_request_active = true;
        context->ep0_direction_in =
            (event.data.setup.data[0] & 0x80u) != 0u;
        context->ep0_requested_length =
            (uint16_t) event.data.setup.data[6] |
            ((uint16_t) event.data.setup.data[7] << 8u);
        context->ep0_accumulated_length = 0;
        (void) pthread_mutex_unlock(&context->mutex);
      }

      raw_gadget_event_dispatch(context, &event);
    }

    pthread_testcancel();
  }

  pthread_cleanup_pop(1);

  (void) pthread_setcanceltype(previous_cancel_type, NULL);
  (void) pthread_setcancelstate(previous_cancel_state, NULL);

  return NULL;
}

//--------------------------------------------------------------------+
// Event API
//--------------------------------------------------------------------+

raw_gadget_result_t raw_gadget_event_start(raw_gadget_context_t *context)
{
  int create_result;

  if (context == NULL)
  {
    return RAW_GADGET_RESULT_INVALID_ARGUMENT;
  }

  if (!context->initialized)
  {
    return RAW_GADGET_RESULT_NOT_INITIALIZED;
  }

  if (context->file_descriptor == RAW_GADGET_INVALID_FILE_DESCRIPTOR)
  {
    return RAW_GADGET_RESULT_NOT_INITIALIZED;
  }

  if (context->event_callback == NULL)
  {
    return RAW_GADGET_RESULT_INVALID_ARGUMENT;
  }

  if (context->event_thread_created)
  {
    return RAW_GADGET_RESULT_ALREADY_INITIALIZED;
  }

  atomic_store(&context->event_thread_running, true);

  create_result =
      pthread_create(&context->event_thread, NULL, raw_gadget_event_thread, context);
  if (create_result != 0)
  {
    atomic_store(&context->event_thread_running, false);
    return RAW_GADGET_RESULT_INTERNAL_ERROR;
  }

  context->event_thread_created = true;

  return RAW_GADGET_RESULT_SUCCESS;
}

void raw_gadget_event_stop(raw_gadget_context_t *context)
{
  if ((context == NULL) || !context->event_thread_created)
  {
    return;
  }

  atomic_store(&context->event_thread_running, false);

  if (pthread_mutex_lock(&context->mutex) == 0)
  {
    context->ep0_request_active = false;
    (void) pthread_cond_broadcast(&context->ep0_condition);
    (void) pthread_mutex_unlock(&context->mutex);
  }

  /*
   * USB_RAW_IOCTL_EVENT_FETCH blocks until a kernel event is available.
   * Cancellation is used to wake the worker without generating a synthetic
   * USB event or closing a file descriptor still owned by the context.
   */
  (void) pthread_cancel(context->event_thread);
  (void) pthread_join(context->event_thread, NULL);

  memset(&context->event_thread, 0, sizeof(context->event_thread));
  context->event_thread_created = false;
}

void raw_gadget_event_dispatch(raw_gadget_context_t *context,
                               raw_gadget_event_t const *event)
{
  raw_gadget_event_callback_t callback;

  if ((context == NULL) || (event == NULL))
  {
    return;
  }

  if (pthread_mutex_lock(&context->callback_mutex) != 0)
  {
    return;
  }

  if (pthread_mutex_lock(&context->mutex) != 0)
  {
    (void) pthread_mutex_unlock(&context->callback_mutex);
    return;
  }

  callback = context->shutting_down ? NULL : context->event_callback;

  (void) pthread_mutex_unlock(&context->mutex);

  if (callback != NULL)
  {
    callback(context->handle, event);
  }

  (void) pthread_mutex_unlock(&context->callback_mutex);
}

void raw_gadget_event_dispatch_transfer(raw_gadget_context_t *context,
                                        raw_gadget_event_t const *event,
                                        uint32_t generation)
{
  raw_gadget_event_callback_t callback;
  uint32_t current_generation;

  if ((context == NULL) || (event == NULL))
  {
    return;
  }

  if (pthread_mutex_lock(&context->callback_mutex) != 0)
  {
    return;
  }

  if (pthread_mutex_lock(&context->mutex) != 0)
  {
    (void) pthread_mutex_unlock(&context->callback_mutex);
    return;
  }

  current_generation = context->transfer_generation;
  callback = (!context->shutting_down &&
              (generation == current_generation))
                ? context->event_callback
                : NULL;

  (void) pthread_mutex_unlock(&context->mutex);

  if (callback != NULL)
  {
    callback(context->handle, event);
  }
  else
  {
  }

  (void) pthread_mutex_unlock(&context->callback_mutex);
}

void raw_gadget_control_request_complete_internal(raw_gadget_context_t *context)
{
  if (context == NULL)
  {
    return;
  }

  if (pthread_mutex_lock(&context->mutex) != 0)
  {
    return;
  }

  context->ep0_request_active = false;
  context->ep0_accumulated_length = 0;
  (void) pthread_cond_broadcast(&context->ep0_condition);

  (void) pthread_mutex_unlock(&context->mutex);
}

void raw_gadget_bus_reset_prepare(raw_gadget_context_t *context)
{
  if (context == NULL)
  {
    return;
  }

  if (pthread_mutex_lock(&context->mutex) != 0)
  {
    return;
  }

  context->configured = false;
  context->transfer_generation++;
  context->ep0_request_active = false;
  context->ep0_direction_in = false;
  context->ep0_requested_length = 0;
  context->ep0_accumulated_length = 0;
  (void) pthread_cond_broadcast(&context->ep0_condition);

  (void) pthread_mutex_unlock(&context->mutex);

  raw_gadget_transfer_cancel_all(context);
}
