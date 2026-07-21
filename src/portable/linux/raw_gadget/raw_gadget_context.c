/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Roman Buchert
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#include "raw_gadget_private.h"

#include <stdlib.h>
#include <string.h>

//--------------------------------------------------------------------+
// Internal Data
//--------------------------------------------------------------------+

static raw_gadget_manager_t raw_gadget_manager = {
  .contexts = NULL,
  .context_count = 0,
  .mutex = PTHREAD_MUTEX_INITIALIZER,
  .discovered = false,
};

//--------------------------------------------------------------------+
// Internal Helpers
//--------------------------------------------------------------------+

static void raw_gadget_endpoints_reset(raw_gadget_context_t *context)
{
  for (size_t index = 0; index < RAW_GADGET_ENDPOINT_COUNT; index++)
  {
    raw_gadget_endpoint_t *endpoint = &context->endpoints[index];

    endpoint->enabled = false;
    endpoint->address = 0;
    endpoint->kernel_handle = RAW_GADGET_INVALID_ENDPOINT_HANDLE;
    endpoint->transfer_active = false;
    endpoint->transfer_thread_created = false;
    endpoint->transfer_cancel_pending = false;
    memset(&endpoint->transfer_thread, 0, sizeof(endpoint->transfer_thread));
  }
}

//--------------------------------------------------------------------+
// Manager and Context API
//--------------------------------------------------------------------+

raw_gadget_manager_t *raw_gadget_manager_get(void)
{
  return &raw_gadget_manager;
}

raw_gadget_result_t raw_gadget_context_table_create(size_t context_count)
{
  raw_gadget_context_t *contexts;
  size_t initialized_count = 0;

  if ((context_count == 0) || (context_count > (size_t) UINT8_MAX + 1u))
  {
    return RAW_GADGET_RESULT_INVALID_ARGUMENT;
  }

  if (pthread_mutex_lock(&raw_gadget_manager.mutex) != 0)
  {
    return RAW_GADGET_RESULT_INTERNAL_ERROR;
  }

  if (raw_gadget_manager.contexts != NULL)
  {
    (void) pthread_mutex_unlock(&raw_gadget_manager.mutex);
    return RAW_GADGET_RESULT_ALREADY_INITIALIZED;
  }

  contexts = calloc(context_count, sizeof(*contexts));
  if (contexts == NULL)
  {
    (void) pthread_mutex_unlock(&raw_gadget_manager.mutex);
    return RAW_GADGET_RESULT_NO_MEMORY;
  }

  for (size_t index = 0; index < context_count; index++)
  {
    raw_gadget_context_t *context = &contexts[index];

    if (pthread_mutex_init(&context->mutex, NULL) != 0)
    {
      initialized_count = index;
      goto context_initialization_failed;
    }

    if (pthread_mutex_init(&context->callback_mutex, NULL) != 0)
    {
      (void) pthread_mutex_destroy(&context->mutex);
      initialized_count = index;
      goto context_initialization_failed;
    }

    if (pthread_cond_init(&context->ep0_condition, NULL) != 0)
    {
      (void) pthread_mutex_destroy(&context->callback_mutex);
      (void) pthread_mutex_destroy(&context->mutex);
      initialized_count = index;
      goto context_initialization_failed;
    }

    context->available = false;
    context->initialized = false;
    context->shutting_down = false;
    context->event_thread_created = false;
    context->handle = (raw_gadget_handle_t) index;
    context->speed = RAW_GADGET_SPEED_INVALID;
    context->udc_name[0] = '\0';
    context->file_descriptor = RAW_GADGET_INVALID_FILE_DESCRIPTOR;
    memset(&context->event_thread, 0, sizeof(context->event_thread));
    atomic_init(&context->event_thread_running, false);
    context->ep0_request_active = false;
    context->ep0_direction_in = false;
    context->ep0_requested_length = 0;
    context->ep0_max_packet_size = 0;
    context->ep0_accumulated_length = 0;
    context->ep0_buffer = NULL;
    context->ep0_buffer_capacity = 0;
    context->transfer_generation = 0;
    context->event_callback = NULL;
    raw_gadget_endpoints_reset(context);
  }

  raw_gadget_manager.contexts = contexts;
  raw_gadget_manager.context_count = context_count;
  raw_gadget_manager.discovered = false;

  (void) pthread_mutex_unlock(&raw_gadget_manager.mutex);
  return RAW_GADGET_RESULT_SUCCESS;

context_initialization_failed:
  for (size_t index = 0; index < initialized_count; index++)
  {
    (void) pthread_cond_destroy(&contexts[index].ep0_condition);
    (void) pthread_mutex_destroy(&contexts[index].callback_mutex);
    (void) pthread_mutex_destroy(&contexts[index].mutex);
  }

  free(contexts);
  (void) pthread_mutex_unlock(&raw_gadget_manager.mutex);

  return RAW_GADGET_RESULT_INTERNAL_ERROR;
}

void raw_gadget_context_table_destroy(void)
{
  if (pthread_mutex_lock(&raw_gadget_manager.mutex) != 0)
  {
    return;
  }

  if (raw_gadget_manager.contexts == NULL)
  {
    raw_gadget_manager.context_count = 0;
    raw_gadget_manager.discovered = false;
    (void) pthread_mutex_unlock(&raw_gadget_manager.mutex);
    return;
  }

  for (size_t index = 0; index < raw_gadget_manager.context_count; index++)
  {
    free(raw_gadget_manager.contexts[index].ep0_buffer);
    raw_gadget_manager.contexts[index].ep0_buffer = NULL;
    (void) pthread_cond_destroy(&raw_gadget_manager.contexts[index].ep0_condition);
    (void) pthread_mutex_destroy(&raw_gadget_manager.contexts[index].callback_mutex);
    (void) pthread_mutex_destroy(&raw_gadget_manager.contexts[index].mutex);
  }

  free(raw_gadget_manager.contexts);

  raw_gadget_manager.contexts = NULL;
  raw_gadget_manager.context_count = 0;
  raw_gadget_manager.discovered = false;

  (void) pthread_mutex_unlock(&raw_gadget_manager.mutex);
}

raw_gadget_context_t *raw_gadget_context_get(raw_gadget_handle_t handle)
{
  raw_gadget_context_t *context = NULL;

  if (pthread_mutex_lock(&raw_gadget_manager.mutex) != 0)
  {
    return NULL;
  }

  if ((raw_gadget_manager.contexts != NULL) &&
      ((size_t) handle < raw_gadget_manager.context_count))
  {
    context = &raw_gadget_manager.contexts[handle];
  }

  (void) pthread_mutex_unlock(&raw_gadget_manager.mutex);

  return context;
}

void raw_gadget_context_reset(raw_gadget_context_t *context)
{
  if (context == NULL)
  {
    return;
  }

  context->initialized = false;
  context->configured = false;
  context->shutting_down = false;
  context->event_thread_created = false;
  context->speed = RAW_GADGET_SPEED_INVALID;
  context->file_descriptor = RAW_GADGET_INVALID_FILE_DESCRIPTOR;
  memset(&context->event_thread, 0, sizeof(context->event_thread));
  atomic_store(&context->event_thread_running, false);
  context->ep0_request_active = false;
  context->ep0_direction_in = false;
  context->ep0_requested_length = 0;
  context->ep0_max_packet_size = 0;
  context->ep0_accumulated_length = 0;
  free(context->ep0_buffer);
  context->ep0_buffer = NULL;
  context->ep0_buffer_capacity = 0;
  context->transfer_generation = 0;
  context->event_callback = NULL;
  raw_gadget_endpoints_reset(context);
}

//--------------------------------------------------------------------+
// Endpoint Helpers
//--------------------------------------------------------------------+

size_t raw_gadget_endpoint_index(uint8_t endpoint_address)
{
  size_t index = endpoint_address & 0x0fu;

  if ((endpoint_address & 0x80u) != 0u)
  {
    index += 16u;
  }

  return index;
}

raw_gadget_endpoint_t *raw_gadget_endpoint_get(raw_gadget_context_t *context,
                                               uint8_t endpoint_address)
{
  if (context == NULL)
  {
    return NULL;
  }

  return &context->endpoints[raw_gadget_endpoint_index(endpoint_address)];
}
