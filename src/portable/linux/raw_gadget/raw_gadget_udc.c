/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Roman Buchert
 * SPDX-License-Identifier: MIT
 *
 * This file is part of the TinyUSB stack.
 */

#include "raw_gadget_private.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

//--------------------------------------------------------------------+
// Internal Types
//--------------------------------------------------------------------+

typedef struct
{
  raw_gadget_handle_t handle;
  char name[UDC_NAME_LENGTH_MAX];
} raw_gadget_udc_entry_t;

//--------------------------------------------------------------------+
// Internal Helpers
//--------------------------------------------------------------------+

static bool raw_gadget_udc_name_parse(char const *name,
                                      raw_gadget_handle_t *handle)
{
  static char const prefix[] = RAW_GADGET_UDC_DRIVER_NAME ".";
  char *end;
  unsigned long index;

  if ((name == NULL) || (handle == NULL))
  {
    return false;
  }

  if (strncmp(name, prefix, sizeof(prefix) - 1u) != 0)
  {
    return false;
  }

  if (name[sizeof(prefix) - 1u] == '\0')
  {
    return false;
  }

  errno = 0;
  index = strtoul(&name[sizeof(prefix) - 1u], &end, 10);

  if ((errno != 0) || (*end != '\0') || (index > UINT8_MAX))
  {
    return false;
  }

  *handle = (raw_gadget_handle_t) index;

  return true;
}

static raw_gadget_result_t raw_gadget_udc_entries_append(
    raw_gadget_udc_entry_t **entries,
    size_t *entry_count,
    size_t *entry_capacity,
    char const *name,
    raw_gadget_handle_t handle)
{
  raw_gadget_udc_entry_t *new_entries;
  size_t new_capacity;
  raw_gadget_udc_entry_t *entry;

  if ((entries == NULL) || (entry_count == NULL) ||
      (entry_capacity == NULL) || (name == NULL))
  {
    return RAW_GADGET_RESULT_INVALID_ARGUMENT;
  }

  if (*entry_count == *entry_capacity)
  {
    new_capacity = (*entry_capacity == 0u) ? 4u : (*entry_capacity * 2u);

    if (new_capacity > (SIZE_MAX / sizeof(**entries)))
    {
      return RAW_GADGET_RESULT_NO_MEMORY;
    }

    new_entries = realloc(*entries, new_capacity * sizeof(**entries));
    if (new_entries == NULL)
    {
      return RAW_GADGET_RESULT_NO_MEMORY;
    }

    *entries = new_entries;
    *entry_capacity = new_capacity;
  }

  entry = &(*entries)[*entry_count];
  entry->handle = handle;

  if (strlen(name) >= sizeof(entry->name))
  {
    return RAW_GADGET_RESULT_INTERNAL_ERROR;
  }

  (void) strcpy(entry->name, name);
  (*entry_count)++;

  return RAW_GADGET_RESULT_SUCCESS;
}

//--------------------------------------------------------------------+
// UDC Discovery API
//--------------------------------------------------------------------+

raw_gadget_result_t raw_gadget_udc_discover(void)
{
  raw_gadget_manager_t *manager = raw_gadget_manager_get();
  raw_gadget_udc_entry_t *entries = NULL;
  size_t entry_count = 0;
  size_t entry_capacity = 0;
  raw_gadget_handle_t highest_handle = 0;
  raw_gadget_result_t result = RAW_GADGET_RESULT_SUCCESS;
  DIR *directory;
  struct dirent *directory_entry;

  if (manager == NULL)
  {
    return RAW_GADGET_RESULT_INTERNAL_ERROR;
  }

  directory = opendir(RAW_GADGET_UDC_SYSFS_PATH);
  if (directory == NULL)
  {
    return RAW_GADGET_RESULT_IO_ERROR;
  }

  while ((directory_entry = readdir(directory)) != NULL)
  {
    raw_gadget_handle_t handle;

    if (!raw_gadget_udc_name_parse(directory_entry->d_name, &handle))
    {
      continue;
    }

    result = raw_gadget_udc_entries_append(&entries,
                                           &entry_count,
                                           &entry_capacity,
                                           directory_entry->d_name,
                                           handle);
    if (result != RAW_GADGET_RESULT_SUCCESS)
    {
      break;
    }

    if ((entry_count == 1u) || (handle > highest_handle))
    {
      highest_handle = handle;
    }
  }

  if (closedir(directory) != 0)
  {
    if (result == RAW_GADGET_RESULT_SUCCESS)
    {
      result = RAW_GADGET_RESULT_IO_ERROR;
    }
  }

  if (result != RAW_GADGET_RESULT_SUCCESS)
  {
    free(entries);
    return result;
  }

  if (entry_count == 0u)
  {
    free(entries);
    return RAW_GADGET_RESULT_NOT_AVAILABLE;
  }

  result = raw_gadget_context_table_create((size_t) highest_handle + 1u);
  if (result != RAW_GADGET_RESULT_SUCCESS)
  {
    free(entries);
    return result;
  }

  for (size_t index = 0; index < entry_count; index++)
  {
    raw_gadget_context_t *context =
        raw_gadget_context_get(entries[index].handle);

    if (context == NULL)
    {
      raw_gadget_context_table_destroy();
      free(entries);
      return RAW_GADGET_RESULT_INTERNAL_ERROR;
    }

    context->available = true;
    (void) strcpy(context->udc_name, entries[index].name);
  }

  free(entries);

  if (pthread_mutex_lock(&manager->mutex) != 0)
  {
    raw_gadget_context_table_destroy();
    return RAW_GADGET_RESULT_INTERNAL_ERROR;
  }

  manager->discovered = true;

  (void) pthread_mutex_unlock(&manager->mutex);

  return RAW_GADGET_RESULT_SUCCESS;
}

raw_gadget_result_t raw_gadget_udc_discover_once(void)
{
  raw_gadget_manager_t *manager = raw_gadget_manager_get();
  bool discovered;

  if (manager == NULL)
  {
    return RAW_GADGET_RESULT_INTERNAL_ERROR;
  }

  if (pthread_mutex_lock(&manager->mutex) != 0)
  {
    return RAW_GADGET_RESULT_INTERNAL_ERROR;
  }

  discovered = manager->discovered;

  (void) pthread_mutex_unlock(&manager->mutex);

  if (discovered)
  {
    return RAW_GADGET_RESULT_SUCCESS;
  }

  return raw_gadget_udc_discover();
}
