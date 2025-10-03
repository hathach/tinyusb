/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 Ennebi Elettronica (https://ennebielettronica.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "bsp/board_api.h"
#include "tusb.h"

#include "tinyusb_logo_png.h"

//--------------------------------------------------------------------+
// Dataset
//--------------------------------------------------------------------+

//------------- device info -------------//
#define DEV_INFO_MANUFACTURER   "TinyUSB"
#define DEV_INFO_MODEL          "MTP Example"
#define DEV_INFO_VERSION        "1.0"
#define DEV_PROP_FRIENDLY_NAME  "TinyUSB MTP"

//------------- storage info -------------//
#define STORAGE_DESCRIPTRION { 'd', 'i', 's', 'k', 0 }
#define VOLUME_IDENTIFIER { 'v', 'o', 'l', 0 }

typedef MTP_STORAGE_INFO_STRUCT(TU_ARRAY_SIZE((uint16_t[]) STORAGE_DESCRIPTRION),
                                 TU_ARRAY_SIZE(((uint16_t[])VOLUME_IDENTIFIER))
) storage_info_t;

storage_info_t storage_info = {
  #ifdef CFG_EXAMPLE_MTP_READONLY
  .storage_type = MTP_STORAGE_TYPE_FIXED_ROM,
  #else
  .storage_type = MTP_STORAGE_TYPE_FIXED_RAM,
  #endif

  .filesystem_type = MTP_FILESYSTEM_TYPE_GENERIC_HIERARCHICAL,
  .access_capability = MTP_ACCESS_CAPABILITY_READ_WRITE,
  .max_capacity_in_bytes = 0, // calculated at runtime
  .free_space_in_bytes = 0, // calculated at runtime
  .free_space_in_objects = 0, // calculated at runtime
  .storage_description = {
    .count = (TU_FIELD_SZIE(storage_info_t, storage_description)-1) / sizeof(uint16_t),
    .utf16 = STORAGE_DESCRIPTRION
  },
  .volume_identifier = {
    .count = (TU_FIELD_SZIE(storage_info_t, volume_identifier)-1) / sizeof(uint16_t),
    .utf16 = VOLUME_IDENTIFIER
  }
};

//--------------------------------------------------------------------+
// MTP FILESYSTEM
//--------------------------------------------------------------------+
// only allow to add 1 more object to make it simpler to manage memory
#define FS_MAX_FILE_COUNT 3UL
#define FS_MAX_FILENAME_LEN 16

#ifdef CFG_EXAMPLE_MTP_READONLY
  #define FS_MAX_CAPACITY_BYTES  0
#else
  #define FS_MAX_CAPACITY_BYTES (4 * 1024UL)

  // object data buffer (excluding 2 predefined files) with simple allocation pointer
  uint8_t fs_buf[FS_MAX_CAPACITY_BYTES];
#endif

#define FS_FIXED_DATETIME "20250808T173500.0" // "YYYYMMDDTHHMMSS.s"
#define README_TXT_CONTENT "TinyUSB MTP Filesystem example"

typedef struct {
  uint16_t name[FS_MAX_FILENAME_LEN];
  uint16_t object_format;
  uint16_t protection_status;
  uint32_t image_pix_width;
  uint32_t image_pix_height;
  uint32_t image_bit_depth;
  uint32_t parent;
  uint16_t association_type;
  uint32_t size;
  uint8_t* data;
} fs_file_t;

// Files system, handle is index + 1
static fs_file_t fs_objects[FS_MAX_FILE_COUNT] = {
  {
    .name = { 'r', 'e', 'a', 'd', 'm', 'e', '.', 't', 'x', 't', 0 }, // readme.txt
    .object_format = MTP_OBJ_FORMAT_TEXT,
    .protection_status = MTP_PROTECTION_STATUS_READ_ONLY,
    .image_pix_width = 0,
    .image_pix_height = 0,
    .image_bit_depth = 0,
    .parent = 0,
    .association_type = MTP_ASSOCIATION_UNDEFINED,
    .size = sizeof(README_TXT_CONTENT)-1,
    .data = (uint8_t*) (uintptr_t) README_TXT_CONTENT,
  },
  {
    .name = { 't', 'i', 'n', 'y', 'u', 's', 'b', '.', 'p', 'n', 'g', 0 }, // "tinyusb.png"
    .object_format = MTP_OBJ_FORMAT_PNG,
    .protection_status = MTP_PROTECTION_STATUS_READ_ONLY,
    .image_pix_width = 128,
    .image_pix_height = 64,
    .image_bit_depth = 32,
    .parent = 0,
    .association_type = MTP_ASSOCIATION_UNDEFINED,
    .size = LOGO_LEN,
    .data = (uint8_t*) (uintptr_t) logo_bin
  }
};

enum {
  SUPPORTED_STORAGE_ID = 0x00010001u // physical = 1, logical = 1
};

static int32_t fs_get_device_info(tud_mtp_cb_data_t* cb_data);
static int32_t fs_open_close_session(tud_mtp_cb_data_t* cb_data);
static int32_t fs_get_storage_ids(tud_mtp_cb_data_t* cb_data);
static int32_t fs_get_storage_info(tud_mtp_cb_data_t* cb_data);
static int32_t fs_get_device_properties(tud_mtp_cb_data_t* cb_data);
static int32_t fs_get_object_handles(tud_mtp_cb_data_t* cb_data);
static int32_t fs_get_object_info(tud_mtp_cb_data_t* cb_data);
static int32_t fs_get_object(tud_mtp_cb_data_t* cb_data);
static int32_t fs_delete_object(tud_mtp_cb_data_t* cb_data);
static int32_t fs_send_object_info(tud_mtp_cb_data_t* cb_data);
static int32_t fs_send_object(tud_mtp_cb_data_t* cb_data);

typedef int32_t (*fs_op_handler_t)(tud_mtp_cb_data_t* cb_data);
typedef struct {
  uint32_t op_code;
  fs_op_handler_t handler;
}fs_op_handler_dict_t;

fs_op_handler_dict_t fs_op_handler_dict[] = {
  { MTP_OP_GET_DEVICE_INFO,       fs_get_device_info    },
  { MTP_OP_OPEN_SESSION,          fs_open_close_session },
  { MTP_OP_CLOSE_SESSION,         fs_open_close_session },
  { MTP_OP_GET_STORAGE_IDS,       fs_get_storage_ids       },
  { MTP_OP_GET_STORAGE_INFO,      fs_get_storage_info      },
  { MTP_OP_GET_DEVICE_PROP_DESC,  fs_get_device_properties  },
  { MTP_OP_GET_DEVICE_PROP_VALUE, fs_get_device_properties },
  { MTP_OP_GET_OBJECT_HANDLES,    fs_get_object_handles    },
  { MTP_OP_GET_OBJECT_INFO,       fs_get_object_info       },
  { MTP_OP_GET_OBJECT,            fs_get_object            },
  { MTP_OP_DELETE_OBJECT,         fs_delete_object         },
  { MTP_OP_SEND_OBJECT_INFO,      fs_send_object_info      },
  { MTP_OP_SEND_OBJECT,           fs_send_object           },
};

static bool is_session_opened = false;
static uint32_t send_obj_handle = 0;

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+
// Get pointer to object info from handle
static inline fs_file_t* fs_get_file(uint32_t handle) {
  if (handle == 0 || handle > FS_MAX_FILE_COUNT) {
    return NULL;
  }
  return &fs_objects[handle-1];
}

static inline bool fs_file_exist(fs_file_t* f) {
  return f->name[0] != 0;
}

// Get the number of allocated nodes in filesystem
static uint32_t fs_get_file_count(void) {
  uint32_t count = 0;
  for (size_t i = 0; i < FS_MAX_FILE_COUNT; i++) {
    if (fs_file_exist(&fs_objects[i])) {
      count++;
    }
  }
  return count;
}

static inline fs_file_t* fs_create_file(void) {
  for (size_t i = 0; i < FS_MAX_FILE_COUNT; i++) {
    fs_file_t* f = &fs_objects[i];
    if (!fs_file_exist(f)) {
      send_obj_handle = i + 1;
      return f;
    }
  }
  return NULL;
}

// simple malloc
static inline uint8_t* fs_malloc(size_t size) {
#ifdef CFG_EXAMPLE_MTP_READONLY
  (void) size;
  return NULL;
#else
  if (size > FS_MAX_CAPACITY_BYTES)  {
    return NULL;
  }
  return fs_buf;
#endif
}

//--------------------------------------------------------------------+
// Control Request callback
//--------------------------------------------------------------------+
bool tud_mtp_request_cancel_cb(tud_mtp_request_cb_data_t* cb_data) {
  mtp_request_reset_cancel_data_t cancel_data;
  memcpy(&cancel_data, cb_data->buf, sizeof(cancel_data));
  (void) cancel_data.code;
  (void ) cancel_data.transaction_id;
  return true;
}

// Invoked when received Device Reset request
// return false to stall the request
bool tud_mtp_request_device_reset_cb(tud_mtp_request_cb_data_t* cb_data) {
  (void) cb_data;
  return true;
}

// Invoked when received Get Extended Event request. Application fill callback data's buffer for response
// return negative to stall the request
int32_t tud_mtp_request_get_extended_event_cb(tud_mtp_request_cb_data_t* cb_data) {
  (void) cb_data;
  return false; // not implemented yet
}

// Invoked when received Get DeviceStatus request. Application fill callback data's buffer for response
// return negative to stall the request
int32_t tud_mtp_request_get_device_status_cb(tud_mtp_request_cb_data_t* cb_data) {
  uint16_t* buf16 = (uint16_t*)(uintptr_t) cb_data->buf;
  buf16[0] = 4; // length
  buf16[1] = MTP_RESP_OK; // status
  return 4;
}

//--------------------------------------------------------------------+
// Bulk Only Protocol
//--------------------------------------------------------------------+
int32_t tud_mtp_command_received_cb(tud_mtp_cb_data_t* cb_data) {
  const mtp_container_command_t* command = cb_data->command_container;
  mtp_container_info_t* io_container = &cb_data->io_container;
  fs_op_handler_t handler = NULL;
  for (size_t i = 0; i < TU_ARRAY_SIZE(fs_op_handler_dict); i++) {
    if (fs_op_handler_dict[i].op_code == command->header.code) {
      handler = fs_op_handler_dict[i].handler;
      break;
    }
  }

  int32_t resp_code;
  if (handler == NULL) {
    resp_code = MTP_RESP_OPERATION_NOT_SUPPORTED;
  } else {
    resp_code = handler(cb_data);
    if (resp_code > MTP_RESP_UNDEFINED) {
      // send response if needed
      io_container->header->code = (uint16_t)resp_code;
      tud_mtp_response_send(io_container);
    }
  }

  return resp_code;
}

int32_t tud_mtp_data_xfer_cb(tud_mtp_cb_data_t* cb_data) {
  const mtp_container_command_t* command = cb_data->command_container;
  mtp_container_info_t* io_container = &cb_data->io_container;

  fs_op_handler_t handler = NULL;
  for (size_t i = 0; i < TU_ARRAY_SIZE(fs_op_handler_dict); i++) {
    if (fs_op_handler_dict[i].op_code == command->header.code) {
      handler = fs_op_handler_dict[i].handler;
      break;
    }
  }

  int32_t resp_code;
  if (handler == NULL) {
    resp_code = MTP_RESP_OPERATION_NOT_SUPPORTED;
  } else {
    resp_code = handler(cb_data);
    if (resp_code > MTP_RESP_UNDEFINED) {
      // send response if needed
      io_container->header->code = (uint16_t)resp_code;
      tud_mtp_response_send(io_container);
    }
  }

  return 0;
}

int32_t tud_mtp_data_complete_cb(tud_mtp_cb_data_t* cb_data) {
  const mtp_container_command_t* command = cb_data->command_container;
  mtp_container_info_t* resp = &cb_data->io_container;
  switch (command->header.code) {
    case MTP_OP_SEND_OBJECT_INFO: {
      fs_file_t* f = fs_get_file(send_obj_handle);
      if (f == NULL) {
        resp->header->code = MTP_RESP_GENERAL_ERROR;
        break;
      }
      // parameter is: storage id, parent handle, new handle
      mtp_container_add_uint32(resp, SUPPORTED_STORAGE_ID);
      mtp_container_add_uint32(resp, f->parent);
      mtp_container_add_uint32(resp, send_obj_handle);
      resp->header->code = MTP_RESP_OK;
      break;
    }

    default:
      resp->header->code = (cb_data->xfer_result == XFER_RESULT_SUCCESS) ? MTP_RESP_OK : MTP_RESP_GENERAL_ERROR;
      break;
  }

  tud_mtp_response_send(resp);
  return 0;
}

int32_t tud_mtp_response_complete_cb(tud_mtp_cb_data_t* cb_data) {
  (void) cb_data;
  return 0; // nothing to do
}

//--------------------------------------------------------------------+
// File System Handlers
//--------------------------------------------------------------------+
static int32_t fs_get_device_info(tud_mtp_cb_data_t* cb_data) {
  // Device info is already prepared up to playback formats. Application only need to add string fields
  mtp_container_info_t* io_container = &cb_data->io_container;
  mtp_container_add_cstring(io_container, DEV_INFO_MANUFACTURER);
  mtp_container_add_cstring(io_container, DEV_INFO_MODEL);
  mtp_container_add_cstring(io_container, DEV_INFO_VERSION);

  enum { MAX_SERIAL_NCHARS = 32 };
  uint16_t serial_utf16[MAX_SERIAL_NCHARS+1];
  size_t nchars = board_usb_get_serial(serial_utf16, MAX_SERIAL_NCHARS);
  serial_utf16[tu_min32(nchars, MAX_SERIAL_NCHARS)] = 0; // ensure null termination
  mtp_container_add_string(io_container, serial_utf16);

  tud_mtp_data_send(io_container);
  return 0;
}

static int32_t fs_open_close_session(tud_mtp_cb_data_t* cb_data) {
  const mtp_container_command_t* command = cb_data->command_container;
  if (command->header.code == MTP_OP_OPEN_SESSION) {
    if (is_session_opened) {
      return MTP_RESP_SESSION_ALREADY_OPEN;
    }
    is_session_opened = true;
  } else { // close session
    if (!is_session_opened) {
      return MTP_RESP_SESSION_NOT_OPEN;
    }
    is_session_opened = false;
  }
  return MTP_RESP_OK;
}

static int32_t fs_get_storage_ids(tud_mtp_cb_data_t* cb_data) {
  mtp_container_info_t* io_container = &cb_data->io_container;
  uint32_t storage_ids [] = { SUPPORTED_STORAGE_ID };
  mtp_container_add_auint32(io_container, 1, storage_ids);
  tud_mtp_data_send(io_container);
  return 0;
}

static int32_t fs_get_storage_info(tud_mtp_cb_data_t* cb_data) {
  const mtp_container_command_t* command = cb_data->command_container;
  mtp_container_info_t* io_container = &cb_data->io_container;
  const uint32_t storage_id = command->params[0];
  TU_VERIFY(SUPPORTED_STORAGE_ID == storage_id, -1);
  // update storage info with current free space
  storage_info.max_capacity_in_bytes = sizeof(README_TXT_CONTENT) + LOGO_LEN + FS_MAX_CAPACITY_BYTES;
  storage_info.free_space_in_objects = FS_MAX_FILE_COUNT - fs_get_file_count();
  storage_info.free_space_in_bytes = storage_info.free_space_in_objects ? FS_MAX_CAPACITY_BYTES : 0;
  mtp_container_add_raw(io_container, &storage_info, sizeof(storage_info));
  tud_mtp_data_send(io_container);
  return 0;
}

static int32_t fs_get_device_properties(tud_mtp_cb_data_t* cb_data) {
  const mtp_container_command_t* command = cb_data->command_container;
  mtp_container_info_t* io_container = &cb_data->io_container;
  const uint16_t dev_prop_code = (uint16_t) command->params[0];

  if (command->header.code == MTP_OP_GET_DEVICE_PROP_DESC) {
    // get describing dataset
    mtp_device_prop_desc_header_t device_prop_header;
    device_prop_header.device_property_code = dev_prop_code;
    switch (dev_prop_code) {
      case MTP_DEV_PROP_DEVICE_FRIENDLY_NAME:
        device_prop_header.datatype = MTP_DATA_TYPE_STR;
        device_prop_header.get_set = MTP_MODE_GET;
        mtp_container_add_raw(io_container, &device_prop_header, sizeof(device_prop_header));
        mtp_container_add_cstring(io_container, DEV_PROP_FRIENDLY_NAME); // factory
        mtp_container_add_cstring(io_container, DEV_PROP_FRIENDLY_NAME); // current
        mtp_container_add_uint8(io_container, 0); // no form
        tud_mtp_data_send(io_container);
        break;

      default:
        return MTP_RESP_PARAMETER_NOT_SUPPORTED;
    }
  } else {
    // get value
    switch (dev_prop_code) {
      case MTP_DEV_PROP_DEVICE_FRIENDLY_NAME:
        mtp_container_add_cstring(io_container, DEV_PROP_FRIENDLY_NAME);
        tud_mtp_data_send(io_container);
        break;

      default:
        return MTP_RESP_PARAMETER_NOT_SUPPORTED;
    }
  }
  return 0;
}

static int32_t fs_get_object_handles(tud_mtp_cb_data_t* cb_data) {
  const mtp_container_command_t* command = cb_data->command_container;
  mtp_container_info_t* io_container = &cb_data->io_container;

  const uint32_t storage_id = command->params[0];
  const uint32_t obj_format = command->params[1]; // optional
  const uint32_t parent_handle = command->params[2]; // folder handle, 0xFFFFFFFF is root
  (void)obj_format;

  if (storage_id != 0xFFFFFFFF && storage_id != SUPPORTED_STORAGE_ID) {
    return MTP_RESP_INVALID_STORAGE_ID;
  }

  uint32_t handles[FS_MAX_FILE_COUNT] = { 0 };
  uint32_t count = 0;
  for (uint8_t i = 0; i < FS_MAX_FILE_COUNT; i++) {
    fs_file_t* f = &fs_objects[i];
    if (fs_file_exist(f) &&
        (parent_handle == f->parent || (parent_handle == 0xFFFFFFFF && f->parent == 0))) {
      handles[count++] = i + 1; // handle is index + 1
    }
  }
  mtp_container_add_auint32(io_container, count, handles);
  tud_mtp_data_send(io_container);

  return 0;
}

static int32_t fs_get_object_info(tud_mtp_cb_data_t* cb_data) {
  const mtp_container_command_t* command = cb_data->command_container;
  mtp_container_info_t* io_container = &cb_data->io_container;
  const uint32_t obj_handle = command->params[0];
  fs_file_t* f = fs_get_file(obj_handle);
  if (f == NULL) {
    return MTP_RESP_INVALID_OBJECT_HANDLE;
  }
  mtp_object_info_header_t obj_info_header = {
    .storage_id = SUPPORTED_STORAGE_ID,
    .object_format = f->object_format,
    .protection_status =  f->protection_status,
    .object_compressed_size = f->size,
    .thumb_format = MTP_OBJ_FORMAT_UNDEFINED,
    .thumb_compressed_size = 0,
    .thumb_pix_width = 0,
    .thumb_pix_height = 0,
    .image_pix_width = f->image_pix_width,
    .image_pix_height = f->image_pix_height,
    .image_bit_depth = f->image_bit_depth,
    .parent_object = f->parent,
    .association_type = f->association_type,
    .association_desc = 0,
    .sequence_number = 0
  };
  mtp_container_add_raw(io_container, &obj_info_header, sizeof(obj_info_header));
  mtp_container_add_string(io_container, f->name);
  mtp_container_add_cstring(io_container, FS_FIXED_DATETIME);
  mtp_container_add_cstring(io_container, FS_FIXED_DATETIME);
  mtp_container_add_cstring(io_container, ""); // keywords, not used
  tud_mtp_data_send(io_container);

  return 0;
}

static int32_t fs_get_object(tud_mtp_cb_data_t* cb_data) {
  const mtp_container_command_t* command = cb_data->command_container;
  mtp_container_info_t* io_container = &cb_data->io_container;
  const uint32_t obj_handle = command->params[0];
  const fs_file_t* f = fs_get_file(obj_handle);
  if (f == NULL) {
    return MTP_RESP_INVALID_OBJECT_HANDLE;
  }

  if (cb_data->phase == MTP_PHASE_COMMAND) {
    // If file contents is larger than CFG_TUD_MTP_EP_BUFSIZE, data may only partially is added here
    // the rest will be sent in tud_mtp_data_more_cb
    mtp_container_add_raw(io_container, f->data, f->size);
    tud_mtp_data_send(io_container);
  } else if (cb_data->phase == MTP_PHASE_DATA) {
    // continue sending remaining data: file contents offset is xferred byte minus header size
    const uint32_t offset = cb_data->total_xferred_bytes - sizeof(mtp_container_header_t);
    const uint32_t xact_len = tu_min32(f->size - offset, io_container->payload_bytes);
    if (xact_len > 0) {
      memcpy(io_container->payload, f->data + offset, xact_len);
      tud_mtp_data_send(io_container);
    }
  }

  return 0;
}

static int32_t fs_send_object_info(tud_mtp_cb_data_t* cb_data) {
  const mtp_container_command_t* command = cb_data->command_container;
  mtp_container_info_t* io_container = &cb_data->io_container;
  const uint32_t storage_id = command->params[0];
  const uint32_t parent_handle = command->params[1]; // folder handle, 0xFFFFFFFF is root
  (void) parent_handle;

  if (!is_session_opened) {
    return MTP_RESP_SESSION_NOT_OPEN;
  }
  if (storage_id != 0xFFFFFFFF && storage_id != SUPPORTED_STORAGE_ID) {
    return MTP_RESP_INVALID_STORAGE_ID;
  }

  if (cb_data->phase == MTP_PHASE_COMMAND) {
    tud_mtp_data_receive(io_container);
  } else if (cb_data->phase == MTP_PHASE_DATA) {
    mtp_object_info_header_t* obj_info = (mtp_object_info_header_t*) io_container->payload;
    if (obj_info->storage_id != 0 && obj_info->storage_id != SUPPORTED_STORAGE_ID) {
      return MTP_RESP_INVALID_STORAGE_ID;
    }

    if (obj_info->parent_object) {
      fs_file_t* parent = fs_get_file(obj_info->parent_object);
      if (parent == NULL || !parent->association_type) {
        return MTP_RESP_INVALID_PARENT_OBJECT;
      }
    }

    uint8_t* f_buf = fs_malloc(obj_info->object_compressed_size);
    if (f_buf == NULL) {
      return MTP_RESP_STORE_FULL;
    }
    fs_file_t* f = fs_create_file();
    if (f == NULL) {
      return MTP_RESP_STORE_FULL;
    }

    f->object_format = obj_info->object_format;
    f->protection_status = obj_info->protection_status;
    f->image_pix_width = obj_info->image_pix_width;
    f->image_pix_height = obj_info->image_pix_height;
    f->image_bit_depth = obj_info->image_bit_depth;
    f->parent = obj_info->parent_object;
    f->association_type = obj_info->association_type;
    f->size = obj_info->object_compressed_size;
    f->data = f_buf;
    uint8_t* buf = io_container->payload + sizeof(mtp_object_info_header_t);
    mtp_container_get_string(buf, f->name);
    // ignore date created/modified/keywords
  }

  return 0;
}

static int32_t fs_send_object(tud_mtp_cb_data_t* cb_data) {
  mtp_container_info_t* io_container = &cb_data->io_container;
  fs_file_t* f = fs_get_file(send_obj_handle);
  if (f == NULL) {
    return MTP_RESP_INVALID_OBJECT_HANDLE;
  }

  if (cb_data->phase == MTP_PHASE_COMMAND) {
    io_container->header->len += f->size;
    tud_mtp_data_receive(io_container);
  } else {
    // file contents offset is total xferred minus header size minus last received chunk
    const uint32_t offset = cb_data->total_xferred_bytes - sizeof(mtp_container_header_t) - io_container->payload_bytes;
    memcpy(f->data + offset, io_container->payload, io_container->payload_bytes);
    if (cb_data->total_xferred_bytes - sizeof(mtp_container_header_t) < f->size) {
      tud_mtp_data_receive(io_container);
    }
  }

  return 0;
}

static int32_t fs_delete_object(tud_mtp_cb_data_t* cb_data) {
  const mtp_container_command_t* command = cb_data->command_container;
  const uint32_t obj_handle = command->params[0];
  const uint32_t obj_format = command->params[1]; // optional
  (void) obj_format;

  if (!is_session_opened) {
    return MTP_RESP_SESSION_NOT_OPEN;
  }
  fs_file_t* f = fs_get_file(obj_handle);
  if (f == NULL) {
    return MTP_RESP_INVALID_OBJECT_HANDLE;
  }

  // delete object by clear the name
  f->name[0] = 0;
  return MTP_RESP_OK;
}
