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

#include "class/mtp/mtp_device_storage.h"
#include "tusb.h"

#define MTPD_STORAGE_DESCRIPTION "storage"
#define MTPD_VOLUME_IDENTIFIER "volume"

//--------------------------------------------------------------------+
// Device Info
//--------------------------------------------------------------------+

// device info string (including terminating null)
#define DEV_INFO_MANUFACTURER   "TinyUSB"
#define DEV_INFO_MODEL          "MTP Example"
#define DEV_INFO_VERSION        "1.0"
#define DEV_INFO_SERIAL         "123456"

#define DEV_PROP_FRIENDLY_NAME  "TinyUSB MTP"

//--------------------------------------------------------------------+
// RAM FILESYSTEM
//--------------------------------------------------------------------+
#define FS_MAX_NODES 5UL
#define FS_MAX_NODE_BYTES 128UL
#define FS_MAX_NODE_NAME_LEN 64UL
#define FS_ISODATETIME_LEN 26UL

typedef struct {
  uint32_t handle;
  uint32_t parent;
  uint32_t size;
  bool allocated;
  bool association;
  char name[FS_MAX_NODE_NAME_LEN];
  char created[FS_ISODATETIME_LEN];
  char modified[FS_ISODATETIME_LEN];
  uint8_t data[FS_MAX_NODE_BYTES];
} fs_object_info_t;

// Sample object file
static fs_object_info_t fs_objects[FS_MAX_NODES] = {
  {
    .handle = 1,
    .parent = 0xffffffff,
    .allocated = true,
    .association = false,
    .name = "readme.txt",
    .created = "20240104T111134.0",
    .modified = "20241214T121110.0",
    .data = "USB MTP on RAM Filesystem example\n",
    .size = 34
  }
};

//------------- Storage Info -------------//
#define STORAGE_DESCRIPTRION { 'd', 'i', 's', 'k', 0 }
#define VOLUME_IDENTIFIER { 'v', 'o', 'l', 0 }

typedef MTP_STORAGE_INFO_TYPEDEF(TU_ARRAY_SIZE((uint16_t[]) STORAGE_DESCRIPTRION),
                                 TU_ARRAY_SIZE(((uint16_t[])VOLUME_IDENTIFIER))
) storage_info_t;

storage_info_t storage_info = {
  .storage_type = MTP_STORAGE_TYPE_FIXED_RAM,
  .filesystem_type = MTP_FILESYSTEM_TYPE_GENERIC_HIERARCHICAL,
  .access_capability = MTP_ACCESS_CAPABILITY_READ_WRITE,
  .max_capacity_in_bytes = FS_MAX_NODES * FS_MAX_NODE_BYTES,
  .free_space_in_bytes = FS_MAX_NODES * FS_MAX_NODE_BYTES,
  .free_space_in_objects = FS_MAX_NODES,
  .storage_description = {
    .count = (TU_FIELD_SZIE(storage_info_t, storage_description)-1) / sizeof(uint16_t),
    .utf16 = STORAGE_DESCRIPTRION
  },
  .volume_identifier = {
    .count = (TU_FIELD_SZIE(storage_info_t, volume_identifier)-1) / sizeof(uint16_t),
    .utf16 = VOLUME_IDENTIFIER
  }
};

enum {
  SUPPORTED_STORAGE_ID = 0x00010001u // physical = 1, logical = 1
};


//--------------------------------------------------------------------+
// OPERATING STATUS
//--------------------------------------------------------------------+
typedef struct {
  // Session
  uint32_t session_id;
  // Association traversal
  uint32_t traversal_parent;
  uint32_t traversal_index;
  // Object open for reading
  uint32_t read_handle;
  uint32_t read_pos;
  // Object open for writing
  uint32_t write_handle;
  uint32_t write_pos;
  // Unique identifier
  uint32_t last_handle;
} fs_operation_t;

static fs_operation_t _fs_operation = {
  .last_handle = 1
};

//--------------------------------------------------------------------+
// INTERNAL FUNCTIONS
//--------------------------------------------------------------------+

// Get pointer to object info from handle
fs_object_info_t* fs_object_get_from_handle(uint32_t handle);

// Get the number of allocated nodes in filesystem
unsigned int fs_get_object_count(void);

fs_object_info_t* fs_object_get_from_handle(uint32_t handle) {
  fs_object_info_t* obj;
  for (unsigned int i = 0; i < FS_MAX_NODES; i++) {
    obj = &fs_objects[i];
    if (obj->allocated && obj->handle == handle)
      return obj;
  }
  return NULL;
}

unsigned int fs_get_object_count(void) {
  unsigned int s = 0;
  for (unsigned int i = 0; i < FS_MAX_NODES; i++) {
    if (fs_objects[i].allocated)
      s++;
  }
  return s;
}

int32_t tud_mtp_data_complete_cb(uint8_t idx, mtp_container_header_t* cmd_header, mtp_generic_container_t* resp_block, tusb_xfer_result_t xfer_result, uint32_t xferred_bytes) {
  (void) idx;
  // switch (cmd_header->code) {
  //   default: break;
  // }
  resp_block->len = MTP_CONTAINER_HEADER_LENGTH;
  resp_block->code = (xfer_result == XFER_RESULT_SUCCESS) ? MTP_RESP_OK : MTP_RESP_GENERAL_ERROR;

  tud_mtp_response_send(resp_block);

  return 0;
}

int32_t tud_mtp_response_complete_cb(uint8_t idx, mtp_container_header_t* cmd_header, mtp_generic_container_t* resp_block, tusb_xfer_result_t xfer_result, uint32_t xferred_bytes) {
  (void) idx;
  (void) cmd_header;
  (void) resp_block;
  (void) xfer_result;
  (void) xferred_bytes;
  return 0; // nothing to do
}

int32_t tud_mtp_command_received_cb(uint8_t idx, mtp_generic_container_t* cmd_block, mtp_generic_container_t* out_block) {
  (void)idx;
  switch (cmd_block->code) {
    case MTP_OP_GET_DEVICE_INFO: {
      // Device info is already prepared up to playback formats. Application only need to add string fields
      mtp_container_add_cstring(out_block, DEV_INFO_MANUFACTURER);
      mtp_container_add_cstring(out_block, DEV_INFO_MODEL);
      mtp_container_add_cstring(out_block, DEV_INFO_VERSION);
      mtp_container_add_cstring(out_block, DEV_INFO_SERIAL);

      tud_mtp_data_send(out_block);
      break;
    }

    case MTP_OP_OPEN_SESSION:
      out_block->code = MTP_RESP_OK;
      tud_mtp_response_send(out_block);
      break;

    case MTP_OP_GET_STORAGE_IDS: {
      uint32_t storage_ids [] = { SUPPORTED_STORAGE_ID }; // physical = 1, logical = 1
      mtp_container_add_auint32(out_block, 1, storage_ids);
      tud_mtp_data_send(out_block);
      break;
    }

    case MTP_OP_GET_STORAGE_INFO: {
      TU_VERIFY(SUPPORTED_STORAGE_ID == cmd_block->data[0], -1);
      // update storage info with current free space
      storage_info.free_space_in_objects = FS_MAX_NODES - fs_get_object_count();
      storage_info.free_space_in_bytes = storage_info.free_space_in_objects * FS_MAX_NODE_BYTES;
      mtp_container_add_raw(out_block, &storage_info, sizeof(storage_info));
      tud_mtp_data_send(out_block);
      break;
    }

    case MTP_OP_GET_DEVICE_PROP_DESC: {
      const uint16_t dev_prop_code = (uint16_t) cmd_block->data[0];
      mtp_device_prop_desc_header_t device_prop_header;
      device_prop_header.device_property_code = dev_prop_code;
      switch (dev_prop_code) {
        case MTP_DEV_PROP_DEVICE_FRIENDLY_NAME:
          device_prop_header.datatype = MTP_DATA_TYPE_STR;
          device_prop_header.get_set = MTP_MODE_GET;
          mtp_container_add_raw(out_block, &device_prop_header, sizeof(device_prop_header));
          mtp_container_add_cstring(out_block, DEV_PROP_FRIENDLY_NAME); // factory
          mtp_container_add_cstring(out_block, DEV_PROP_FRIENDLY_NAME); // current
          mtp_container_add_uint8(out_block, 0); // no form
          tud_mtp_data_send(out_block);
          break;

        default: return MTP_RESP_PARAMETER_NOT_SUPPORTED;
      }
      break;
    }

    case MTP_OP_GET_DEVICE_PROP_VALUE: {
      const uint16_t dev_prop_code = (uint16_t) cmd_block->data[0];
      switch (dev_prop_code) {
        case MTP_DEV_PROP_DEVICE_FRIENDLY_NAME:
          mtp_container_add_cstring(out_block, DEV_PROP_FRIENDLY_NAME);
          tud_mtp_data_send(out_block);
          break;

        default: return MTP_RESP_PARAMETER_NOT_SUPPORTED;
      }
      break;
    }

    case MTP_OP_GET_OBJECT_HANDLES: {
      const uint32_t storage_id = cmd_block->data[0];
      const uint32_t obj_format = cmd_block->data[1]; // optional
      (void) obj_format;
      const uint32_t parent_handle = cmd_block->data[2]; // folder handle, 0xFFFFFFFF is root
      if (storage_id != 0xFFFFFFFF && storage_id != SUPPORTED_STORAGE_ID) {
        return MTP_RESP_INVALID_STORAGE_ID;
      }

      uint32_t handles[FS_MAX_NODES] = { 0 };
      uint32_t count = 0;
      for (uint8_t i = 0, h = 0; i < FS_MAX_NODES; i++) {
        if (fs_objects[i].allocated && parent_handle == fs_objects[i].parent) {
          handles[count++] = fs_objects[i].handle;
        }
      }
      mtp_container_add_auint32(out_block, count, handles);
      tud_mtp_data_send(out_block);
      break;
    }

    default: return MTP_RESP_OPERATION_NOT_SUPPORTED;
  }

  return MTP_RESP_OK;
}

//--------------------------------------------------------------------+
// API
//--------------------------------------------------------------------+
mtp_response_t tud_mtp_storage_close_session(uint32_t session_id) {
  if (session_id != _fs_operation.session_id) {
    TU_LOG1("ERR: Session %ld not open\r\n", session_id);
    return MTP_RESP_SESSION_NOT_OPEN;
  }
  _fs_operation.session_id = 0;
  TU_LOG1("Session closed\r\n");
  return MTP_RESP_OK;
}

mtp_response_t tud_mtp_storage_format(uint32_t storage_id) {
  if (_fs_operation.session_id == 0) {
    TU_LOG1("ERR: Session not open\r\n");
    return MTP_RESP_SESSION_NOT_OPEN;
  }
  if (storage_id != STORAGE_ID(0x0001, 0x0001)) {
    TU_LOG1("ERR: Unexpected storage id %ld\r\n", storage_id);
    return MTP_RESP_INVALID_STORAGE_ID;
  }

  // Simply deallocate all entries
  for (unsigned int i = 0; i < FS_MAX_NODES; i++)
    fs_objects[i].allocated = false;
  TU_LOG1("Format completed\r\n");
  return MTP_RESP_OK;
}

mtp_response_t tud_mtp_storage_association_get_object_handle(uint32_t storage_id, uint32_t parent_object_handle,
                                                             uint32_t* next_child_handle) {
  fs_object_info_t* obj;

  if (_fs_operation.session_id == 0) {
    TU_LOG1("ERR: Session not open\r\n");
    return MTP_RESP_SESSION_NOT_OPEN;
  }
  // We just have one storage, same reply if querying all storages
  if (storage_id != 0xFFFFFFFF && storage_id != STORAGE_ID(0x0001, 0x0001)) {
    TU_LOG1("ERR: Unexpected storage id %ld\r\n", storage_id);
    return MTP_RESP_INVALID_STORAGE_ID;
  }

  // Request for objects with no parent (0xFFFFFFFF) are considered root objects
  // Note: implementation may pass 0 as parent_object_handle
  if (parent_object_handle == 0xFFFFFFFF)
    parent_object_handle = 0;

  if (parent_object_handle != _fs_operation.traversal_parent) {
    _fs_operation.traversal_parent = parent_object_handle;
    _fs_operation.traversal_index = 0;
  }

  for (unsigned int i = _fs_operation.traversal_index; i < FS_MAX_NODES; i++) {
    obj = &fs_objects[i];
    if (obj->allocated && obj->parent == parent_object_handle) {
      _fs_operation.traversal_index = i + 1;
      *next_child_handle = obj->handle;
      TU_LOG1("Association %ld -> child %ld\r\n", parent_object_handle, obj->handle);
      return MTP_RESP_OK;
    }
  }
  TU_LOG1("Association traversal completed\r\n");
  _fs_operation.traversal_index = 0;
  *next_child_handle = 0;
  return MTP_RESP_OK;
}

mtp_response_t tud_mtp_storage_object_write_info(uint32_t storage_id, uint32_t parent_object,
                                                 uint32_t* new_object_handle, const mtp_object_info_t* info) {
  fs_object_info_t* obj = NULL;

  if (_fs_operation.session_id == 0) {
    TU_LOG1("ERR: Session not open\r\n");
    return MTP_RESP_SESSION_NOT_OPEN;
  }
  // Accept command on default storage
  if (storage_id != 0xFFFFFFFF && storage_id != STORAGE_ID(0x0001, 0x0001)) {
    TU_LOG1("ERR: Unexpected storage id %ld\r\n", storage_id);
    return MTP_RESP_INVALID_STORAGE_ID;
  }

  if (info->object_compressed_size > FS_MAX_NODE_BYTES) {
    TU_LOG1("Object size %ld is more than maximum %ld\r\n", info->object_compressed_size, FS_MAX_NODE_BYTES);
    return MTP_RESP_STORE_FULL;
  }

  // Request for objects with no parent (0xFFFFFFFF) are considered root objects
  if (parent_object == 0xFFFFFFFF)
    parent_object = 0;

  // Ensure we are not creating an orphaned object outside root
  if (parent_object != 0) {
    obj = fs_object_get_from_handle(parent_object);
    if (obj == NULL) {
      TU_LOG1("Parent %ld does not exist\r\n", parent_object);
      return MTP_RESP_INVALID_PARENT_OBJECT;
    }
    if (!obj->association) {
      TU_LOG1("Parent %ld is not an association\r\n", parent_object);
      return MTP_RESP_INVALID_PARENT_OBJECT;
    }
  }

  // Search for first free object
  for (unsigned int i = 0; i < FS_MAX_NODES; i++) {
    if (!fs_objects[i].allocated) {
      obj = &fs_objects[i];
      break;
    }
  }

  if (obj == NULL) {
    TU_LOG1("No space left on device\r\n");
    return MTP_RESP_STORE_FULL;
  }

  // Fill-in structure
  obj->allocated = true;
  obj->handle = ++_fs_operation.last_handle;
  obj->parent = parent_object;
  obj->size = info->object_compressed_size;
  obj->association = info->object_format == MTP_OBJ_FORMAT_ASSOCIATION;

  // Extract variable data
  uint16_t offset_data = sizeof(mtp_object_info_t);
  mtpd_gct_get_string(&offset_data, obj->name, FS_MAX_NODE_NAME_LEN);
  mtpd_gct_get_string(&offset_data, obj->created, FS_ISODATETIME_LEN);
  mtpd_gct_get_string(&offset_data, obj->modified, FS_ISODATETIME_LEN);

  TU_LOG1("Create %s %s with handle %ld, parent %ld and size %ld\r\n",
          obj->association ? "association" : "object",
          obj->name, obj->handle, obj->parent, obj->size);
  *new_object_handle = obj->handle;
  // Initialize operation
  _fs_operation.write_handle = obj->handle;
  _fs_operation.write_pos = 0;
  return MTP_RESP_OK;
}

mtp_response_t tud_mtp_storage_object_read_info(uint32_t object_handle, mtp_object_info_t* info) {
  const fs_object_info_t* obj;

  if (_fs_operation.session_id == 0) {
    TU_LOG1("ERR: Session not open\r\n");
    return MTP_RESP_SESSION_NOT_OPEN;
  }

  obj = fs_object_get_from_handle(object_handle);
  if (obj == NULL) {
    TU_LOG1("ERR: Object with handle %ld does not exist\r\n", object_handle);
    return MTP_RESP_INVALID_OBJECT_HANDLE;
  }

  memset(info, 0, sizeof(mtp_object_info_t));
  info->storage_id = STORAGE_ID(0x0001, 0x0001);
  if (obj->association) {
    info->object_format = MTP_OBJ_FORMAT_ASSOCIATION;
    info->protection_status = MTP_PROTECTION_STATUS_NO_PROTECTION;
    info->object_compressed_size = 0;
    info->association_type = MTP_ASSOCIATION_UNDEFINED;
  } else {
    info->object_format = MTP_OBJ_FORMAT_UNDEFINED;
    info->protection_status = MTP_PROTECTION_STATUS_NO_PROTECTION;
    info->object_compressed_size = obj->size;
    info->association_type = MTP_ASSOCIATION_UNDEFINED;
  }
  info->thumb_format = MTP_OBJ_FORMAT_UNDEFINED;
  info->parent_object = obj->parent;

  mtpd_gct_append_wstring(obj->name);
  mtpd_gct_append_wstring(obj->created); // date_created
  mtpd_gct_append_wstring(obj->modified); // date_modified
  mtpd_gct_append_wstring(""); // keywords, not used

  TU_LOG1("Retrieve object %s with handle %ld\r\n", obj->name, obj->handle);

  return MTP_RESP_OK;
}

mtp_response_t tud_mtp_storage_object_write(uint32_t object_handle, const uint8_t* buffer, uint32_t size) {
  fs_object_info_t* obj;

  obj = fs_object_get_from_handle(object_handle);
  if (obj == NULL) {
    TU_LOG1("ERR: Object with handle %ld does not exist\r\n", object_handle);
    return MTP_RESP_INVALID_OBJECT_HANDLE;
  }
  // It's a requirement that this command is preceded by a write info
  if (object_handle != _fs_operation.write_handle) {
    TU_LOG1("ERR: Object %ld not open for write\r\n", object_handle);
    return MTP_RESP_NO_VALID_OBJECTINFO;
  }

  TU_LOG1("Write object %ld: data chunk at %ld/%ld bytes at offset %ld\r\n", object_handle, _fs_operation.write_pos,
          obj->size, size);
  TU_ASSERT(obj->size >= _fs_operation.write_pos + size, MTP_RESP_INCOMPLETE_TRANSFER);
  if (_fs_operation.write_pos + size < FS_MAX_NODE_BYTES)
    memcpy(&obj->data[_fs_operation.write_pos], buffer, size);
  _fs_operation.write_pos += size;
  // Write operation completed
  if (_fs_operation.write_pos == obj->size) {
    _fs_operation.write_handle = 0;
    _fs_operation.write_pos = 0;
  }
  return MTP_RESP_OK;
}

mtp_response_t tud_mtp_storage_object_size(uint32_t object_handle, uint32_t* size) {
  const fs_object_info_t* obj;
  obj = fs_object_get_from_handle(object_handle);
  if (obj == NULL) {
    TU_LOG1("ERR: Object with handle %ld does not exist\r\n", object_handle);
    return MTP_RESP_INVALID_OBJECT_HANDLE;
  }
  *size = obj->size;
  return MTP_RESP_OK;
}

mtp_response_t tud_mtp_storage_object_read(uint32_t object_handle, void* buffer, uint32_t buffer_size,
                                           uint32_t* read_count) {
  const fs_object_info_t* obj;

  obj = fs_object_get_from_handle(object_handle);

  if (obj == NULL) {
    TU_LOG1("ERR: Object with handle %ld does not exist\r\n", object_handle);
    return MTP_RESP_INVALID_OBJECT_HANDLE;
  }
  // It's not a requirement that this command is preceded by a read info
  if (object_handle != _fs_operation.read_handle) {
    TU_LOG1("ERR: Object %ld not open for read\r\n", object_handle);
    _fs_operation.read_handle = object_handle;
    _fs_operation.read_pos = 0;
  }

  if (obj->size - _fs_operation.read_pos > buffer_size) {
    TU_LOG1("Read object %ld: %ld bytes at offset %ld\r\n", object_handle, buffer_size, _fs_operation.read_pos);
    *read_count = buffer_size;
    if (_fs_operation.read_pos + buffer_size < FS_MAX_NODE_BYTES) {
      memcpy(buffer, &obj->data[_fs_operation.read_pos], *read_count);
    }
    _fs_operation.read_pos += *read_count;
  } else {
    TU_LOG1("Read object %ld: %ld bytes at offset %ld\r\n", object_handle, obj->size - _fs_operation.read_pos,
            _fs_operation.read_pos);
    *read_count = obj->size - _fs_operation.read_pos;
    if (_fs_operation.read_pos + *read_count < FS_MAX_NODE_BYTES) {
      memcpy(buffer, &obj->data[_fs_operation.read_pos], *read_count);
    }
    // Read operation completed
    _fs_operation.read_handle = 0;
    _fs_operation.read_pos = 0;
  }
  return MTP_RESP_OK;
}

mtp_response_t tud_mtp_storage_object_move(uint32_t object_handle, uint32_t new_parent_object_handle) {
  fs_object_info_t* obj;

  if (new_parent_object_handle == 0xFFFFFFFF)
    new_parent_object_handle = 0;

  // Ensure we are not moving to an nonexisting parent
  if (new_parent_object_handle != 0) {
    obj = fs_object_get_from_handle(new_parent_object_handle);
    if (obj == NULL) {
      TU_LOG1("Parent %ld does not exist\r\n", new_parent_object_handle);
      return MTP_RESP_INVALID_PARENT_OBJECT;
    }
    if (!obj->association) {
      TU_LOG1("Parent %ld is not an association\r\n", new_parent_object_handle);
      return MTP_RESP_INVALID_PARENT_OBJECT;
    }
  }

  obj = fs_object_get_from_handle(object_handle);

  if (obj == NULL) {
    TU_LOG1("ERR: Object with handle %ld does not exist\r\n", object_handle);
    return MTP_RESP_INVALID_OBJECT_HANDLE;
  }
  TU_LOG1("Move object %ld to new parent %ld\r\n", object_handle, new_parent_object_handle);
  obj->parent = new_parent_object_handle;
  return MTP_RESP_OK;
}

mtp_response_t tud_mtp_storage_object_delete(uint32_t object_handle) {
  fs_object_info_t* obj;

  if (_fs_operation.session_id == 0) {
    TU_LOG1("ERR: Session not open\r\n");
    return MTP_RESP_SESSION_NOT_OPEN;
  }

  if (object_handle == 0xFFFFFFFF)
    object_handle = 0;

  if (object_handle != 0) {
    obj = fs_object_get_from_handle(object_handle);

    if (obj == NULL) {
      TU_LOG1("ERR: Object with handle %ld does not exist\r\n", object_handle);
      return MTP_RESP_INVALID_OBJECT_HANDLE;
    }
    obj->allocated = false;
    TU_LOG1("Delete object with handle %ld\r\n", object_handle);
  }

  if (object_handle == 0 || obj->association) {
    // Delete also children
    for (unsigned int i = 0; i < FS_MAX_NODES; i++) {
      obj = &fs_objects[i];
      if (obj->allocated && obj->parent == object_handle) {
        tud_mtp_storage_object_delete(obj->handle);
      }
    }
  }

  return MTP_RESP_OK;
}

void tud_mtp_storage_object_done(void) {
}

void tud_mtp_storage_cancel(void) {
  fs_object_info_t* obj;

  _fs_operation.traversal_parent = 0;
  _fs_operation.traversal_index = 0;
  _fs_operation.read_handle = 0;
  _fs_operation.read_pos = 0;
  // If write operation is canceled, discard object
  if (_fs_operation.write_handle) {
    obj = fs_object_get_from_handle(_fs_operation.write_handle);
    if (obj)
      obj->allocated = false;
  }
  _fs_operation.write_handle = 0;
  _fs_operation.write_pos = 0;
}

void tud_mtp_storage_reset(void) {
  tud_mtp_storage_cancel();
  _fs_operation.session_id = 0;
}
