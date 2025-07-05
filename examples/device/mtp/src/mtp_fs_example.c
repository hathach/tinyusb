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

static uint32_t curr_session_id;

//--------------------------------------------------------------------+
// RAM FILESYSTEM
//--------------------------------------------------------------------+
#define FS_MAX_NODES 5UL
#define FS_MAX_NODE_BYTES 128UL
#define FS_MAX_NODE_NAME_LEN 64UL
#define FS_ISODATETIME_LEN 26UL

typedef struct
{
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
static fs_object_info_t _fs_objects[FS_MAX_NODES] = {
    {
        .handle = 1,
        .parent = 0,
        .allocated = true,
        .association = false,
        .name = "readme.txt",
        .created = "20240104T111134.0",
        .modified = "20241214T121110.0",
        .data = "USB MTP on RAM Filesystem example\n",
        .size = 34
    }
};

// A unique object identifier
static uint32_t _fs_handle_last = 1;


//--------------------------------------------------------------------+
// INTERNAL FUNCTIONS
//--------------------------------------------------------------------+

// Get pointer to object info from handle
fs_object_info_t *fs_object_get_from_handle(uint32_t handle);

// Get the number of allocated nodes in filesystem
unsigned int fs_get_object_count(void);

fs_object_info_t *fs_object_get_from_handle(uint32_t handle)
{
    fs_object_info_t *obj;
    for (unsigned int i=0; i<FS_MAX_NODES; i++)
    {
        obj = &_fs_objects[i];
        if (obj->allocated && obj->handle == handle)
            return obj;
    }
    return NULL;
}

unsigned int fs_get_object_count(void)
{
    unsigned int s = 0;
    for (unsigned int i = 0; i<FS_MAX_NODES; i++)
    {
        if (_fs_objects[i].allocated)
            s++;
    }
    return s;
}

//--------------------------------------------------------------------+
// API
//--------------------------------------------------------------------+
mtp_response_t tud_mtp_storage_open_session(uint32_t *session_id)
{
    if (*session_id == 0)
    {
        TU_LOG1("Invalid session ID\r\n");
        return MTP_RESC_INVALID_PARAMETER;
    }
    if (curr_session_id != 0)
    {
        *session_id = curr_session_id;
        TU_LOG1("ERR: Session %ld already open\r\n", curr_session_id);
        return MTP_RESC_SESSION_ALREADY_OPEN;
    }
    curr_session_id = *session_id;
    TU_LOG1("Open session with id %ld\r\n", curr_session_id);
    return MTP_RESC_OK;
}

mtp_response_t tud_mtp_storage_close_session(uint32_t session_id)
{
    if (session_id != curr_session_id)
    {
        TU_LOG1("ERR: Session %ld not open\r\n", session_id);
        return MTP_RESC_SESSION_NOT_OPEN;
    }
    curr_session_id = 0;
    TU_LOG1("Session closed\r\n");
    return MTP_RESC_OK;
}

mtp_response_t tud_mtp_get_storage_id(uint32_t *storage_id)
{
    if (curr_session_id == 0)
    {
        TU_LOG1("ERR: Session not open\r\n");
        return MTP_RESC_SESSION_NOT_OPEN;
    }
    *storage_id = STORAGE_ID(0x0001, 0x0001);
    TU_LOG1("Retrieved storage identifier %ld\r\n", *storage_id);
    return MTP_RESC_OK;
}

mtp_response_t tud_mtp_get_storage_info(uint32_t storage_id, mtp_storage_info_t *info)
{
    if (curr_session_id == 0)
    {
        TU_LOG1("ERR: Session not open\r\n");
        return MTP_RESC_SESSION_NOT_OPEN;
    }
    if (storage_id != STORAGE_ID(0x0001, 0x0001))
    {
        TU_LOG1("ERR: Unexpected storage id %ld\r\n", storage_id);
        return MTP_RESC_INVALID_STORAGE_ID;
    }
    info->storage_type = MTP_STORAGE_TYPE_FIXED_ROM;
    info->filesystem_type = MTP_FILESYSTEM_TYPE_GENERIC_HIERARCHICAL;
    info->access_capability = MTP_ACCESS_CAPABILITY_READ_WRITE;
    info->max_capacity_in_bytes = FS_MAX_NODES * FS_MAX_NODE_BYTES;
    info->free_space_in_objects = FS_MAX_NODES - fs_get_object_count();
    info->free_space_in_bytes = info->free_space_in_objects * FS_MAX_NODE_BYTES;
    mtpd_gct_append_wstring(MTPD_STORAGE_DESCRIPTION);
    mtpd_gct_append_wstring(MTPD_VOLUME_IDENTIFIER);
    return MTP_RESC_OK;
}

mtp_response_t tud_mtp_storage_format(uint32_t storage_id)
{
    if (curr_session_id == 0)
    {
        TU_LOG1("ERR: Session not open\r\n");
        return MTP_RESC_SESSION_NOT_OPEN;
    }
    if (storage_id != STORAGE_ID(0x0001, 0x0001))
    {
        TU_LOG1("ERR: Unexpected storage id %ld\r\n", storage_id);
        return MTP_RESC_INVALID_STORAGE_ID;
    }

    // Simply deallocate all entries
    for (unsigned int i=0; i<FS_MAX_NODES; i++)
        _fs_objects[i].allocated = false;
    TU_LOG1("Format completed\r\n");
    return MTP_RESC_OK;
}

mtp_response_t tud_mtp_storage_association_get_object_handle(uint32_t storage_id, uint32_t parent_object_handle, uint32_t *next_child_handle)
{
    static uint32_t current_parent_object = 0;
    static unsigned int next_object_index = 0;
    fs_object_info_t *obj;

    if (curr_session_id == 0)
    {
        TU_LOG1("ERR: Session not open\r\n");
        return MTP_RESC_SESSION_NOT_OPEN;
    }
    if (storage_id != STORAGE_ID(0x0001, 0x0001))
    {
        TU_LOG1("ERR: Unexpected storage id %ld\r\n", storage_id);
        return MTP_RESC_INVALID_STORAGE_ID;
    }

    // Request for objects with no parent (0xFFFFFFFF) are considered root objects
    // Note: implementation may pass 0 as parent_object_handle
    if (parent_object_handle == 0xFFFFFFFF)
        parent_object_handle = 0;

    if (parent_object_handle != current_parent_object)
    {
        current_parent_object = parent_object_handle;
        next_object_index = 0;
    }

    for (unsigned int i=next_object_index; i<FS_MAX_NODES; i++)
    {
        obj = &_fs_objects[i];
        if (obj->allocated && obj->parent == parent_object_handle)
        {
            next_object_index = i+1;
            *next_child_handle = obj->handle;
            TU_LOG1("Association %ld -> child %ld\r\n", parent_object_handle, obj->handle);
            return MTP_RESC_OK;
        }
    }
    TU_LOG1("Association traversal completed\r\n");
    next_object_index = 0;
    *next_child_handle = 0;
    return MTP_RESC_OK;
}

mtp_response_t tud_mtp_storage_object_write_info(uint32_t storage_id, uint32_t parent_object, uint32_t *new_object_handle, const mtp_object_info_t *info)
{
    fs_object_info_t *obj = NULL;

    if (curr_session_id == 0)
    {
        TU_LOG1("ERR: Session not open\r\n");
        return MTP_RESC_SESSION_NOT_OPEN;
    }
    if (storage_id != STORAGE_ID(0x0001, 0x0001))
    {
        TU_LOG1("ERR: Unexpected storage id %ld\r\n", storage_id);
        return MTP_RESC_INVALID_STORAGE_ID;
    }

    if (info->object_compressed_size > FS_MAX_NODE_BYTES)
    {
        TU_LOG1("Object size %ld is more than maximum %ld\r\n", info->object_compressed_size, FS_MAX_NODE_BYTES);
        return MTP_RESC_STORE_FULL;
    }

    // Request for objects with no parent (0xFFFFFFFF) are considered root objects
    if (parent_object == 0xFFFFFFFF)
        parent_object = 0;

    // Ensure we are not creating an orphaned object outside root
    if (parent_object != 0)
    {
        obj = fs_object_get_from_handle(parent_object);
        if (obj == NULL)
        {
            TU_LOG1("Parent %ld does not exist\r\n", parent_object);
            return MTP_RESC_INVALID_PARENT_OBJECT;
        }
        if (!obj->association)
        {
            TU_LOG1("Parent %ld is not an association\r\n", parent_object);
            return MTP_RESC_INVALID_PARENT_OBJECT;
        }
    }

    // Search for first free object
    for (unsigned int i=0; i<FS_MAX_NODES; i++)
    {
        if (!_fs_objects[i].allocated)
        {
            obj = &_fs_objects[i];
            break;
        }
    }

    if (obj == NULL)
    {
        TU_LOG1("No space left on device\r\n");
        return MTP_RESC_STORE_FULL;
    }

    // Fill-in structure
    obj->allocated = true;
    obj->handle = ++_fs_handle_last;
    obj->parent = parent_object;
    obj->size = info->object_compressed_size;
    obj->association = info->object_format == MTP_OBJF_ASSOCIATION;

    // Extract variable data
    uint16_t offset_data = sizeof(mtp_object_info_t);
    mtpd_gct_get_string(&offset_data, obj->name, FS_MAX_NODE_NAME_LEN);
    mtpd_gct_get_string(&offset_data, obj->created, FS_ISODATETIME_LEN);
    mtpd_gct_get_string(&offset_data, obj->modified, FS_ISODATETIME_LEN);

    TU_LOG1("Create %s %s with handle %ld, parent %ld and size %ld\r\n",
        obj->association ? "association" : "object",
         obj->name, obj->handle, obj->parent, obj->size);
    *new_object_handle = obj->handle;
    return MTP_RESC_OK;
}

mtp_response_t tud_mtp_storage_object_read_info(uint32_t object_handle, mtp_object_info_t *info)
{
    const fs_object_info_t *obj;

    if (curr_session_id == 0)
    {
        TU_LOG1("ERR: Session not open\r\n");
        return MTP_RESC_SESSION_NOT_OPEN;
    }

    obj = fs_object_get_from_handle(object_handle);
    if (obj == NULL)
    {
        TU_LOG1("ERR: Object with handle %ld does not exist\r\n", object_handle);
        return MTP_RESC_INVALID_OBJECT_HANDLE;
    }

    memset(info, 0, sizeof(mtp_object_info_t));
    info->storage_id = STORAGE_ID(0x0001, 0x0001);
    if (obj->association)
    {
        info->object_format = MTP_OBJF_ASSOCIATION;
        info->protection_status = MTP_PROTECTION_STATUS_NO_PROTECTION;
        info->object_compressed_size = 0;
        info->association_type = MTP_ASSOCIATION_UNDEFINED;
    }
    else
    {
        info->object_format = MTP_OBJF_UNDEFINED;
        info->protection_status = MTP_PROTECTION_STATUS_NO_PROTECTION;
        info->object_compressed_size = obj->size;
        info->association_type = MTP_ASSOCIATION_UNDEFINED;
    }
    info->thumb_format = MTP_OBJF_UNDEFINED;
    info->parent_object = obj->parent;

    mtpd_gct_append_wstring(obj->name);
    mtpd_gct_append_wstring(obj->created); // date_created
    mtpd_gct_append_wstring(obj->modified); // date_modified
    mtpd_gct_append_wstring(""); // keywords, not used

    TU_LOG1("Retrieve object %s with handle %ld\r\n", obj->name, obj->handle);

    return MTP_RESC_OK;
}

mtp_response_t tud_mtp_storage_object_write(uint32_t object_handle, const uint8_t *buffer, uint32_t size)
{
    static uint32_t current_object_handle = 0xffff;
    static unsigned int pos = 0;
    fs_object_info_t *obj;

    obj = fs_object_get_from_handle(object_handle);
    if (obj == NULL)
    {
        TU_LOG1("ERR: Object with handle %ld does not exist\r\n", object_handle);
        return MTP_RESC_INVALID_OBJECT_HANDLE;
    }
    if (object_handle != current_object_handle)
    {
        current_object_handle = object_handle;
        pos = 0;
    }

    TU_LOG1("Write object %ld: data chunk %ld/%ld bytes at offset %d\r\n", object_handle, size, obj->size, pos);
    TU_ASSERT(obj->size >= pos + size, MTP_RESC_INCOMPLETE_TRANSFER);
    memcpy(&obj->data[pos], buffer, size);
    pos += size;
    return MTP_RESC_OK;
}

mtp_response_t tud_mtp_storage_object_size(uint32_t object_handle, uint32_t *size)
{
    const fs_object_info_t *obj;
    obj = fs_object_get_from_handle(object_handle);
    if (obj == NULL)
    {
        TU_LOG1("ERR: Object with handle %ld does not exist\r\n", object_handle);
        return MTP_RESC_INVALID_OBJECT_HANDLE;
    }
    *size = obj->size;
    return MTP_RESC_OK;
}

mtp_response_t tud_mtp_storage_object_read(uint32_t object_handle, void *buffer, uint32_t buffer_size, uint32_t *read_count)
{
    static uint32_t current_object_handle = 0xffff;
    static unsigned int pos = 0;
    const fs_object_info_t *obj;

    obj = fs_object_get_from_handle(object_handle);

    if (obj == NULL)
    {
        TU_LOG1("ERR: Object with handle %ld does not exist\r\n", object_handle);
        return MTP_RESC_INVALID_OBJECT_HANDLE;
    }

    if (object_handle != current_object_handle)
    {
        current_object_handle = object_handle;
        pos = 0;
    }

    if (obj->size - pos > buffer_size)
    {
        TU_LOG1("Read object %ld: %d bytes at offset %ld\r\n", object_handle, pos, buffer_size);
        *read_count = buffer_size;
        memcpy(buffer, &obj->data[pos], *read_count);
        pos += *read_count;
    }
    else
    {
        TU_LOG1("Read object %ld: %d bytes at offset %ld\r\n", object_handle, pos, obj->size - pos);
        *read_count = obj->size - pos;
        memcpy(buffer, &obj->data[pos], *read_count);
        pos = 0;
    }
    return MTP_RESC_OK;
}

mtp_response_t tud_mtp_storage_object_move(uint32_t object_handle, uint32_t new_parent_object_handle)
{
    fs_object_info_t *obj;

    if (new_parent_object_handle == 0xFFFFFFFF)
        new_parent_object_handle = 0;

    // Ensure we are not moving to an nonexisting parent
    if (new_parent_object_handle != 0)
    {
        obj = fs_object_get_from_handle(new_parent_object_handle);
        if (obj == NULL)
        {
            TU_LOG1("Parent %ld does not exist\r\n", new_parent_object_handle);
            return MTP_RESC_INVALID_PARENT_OBJECT;
        }
        if (!obj->association)
        {
            TU_LOG1("Parent %ld is not an association\r\n", new_parent_object_handle);
            return MTP_RESC_INVALID_PARENT_OBJECT;
        }
    }

    obj = fs_object_get_from_handle(object_handle);

    if (obj == NULL)
    {
        TU_LOG1("ERR: Object with handle %ld does not exist\r\n", object_handle);
        return MTP_RESC_INVALID_OBJECT_HANDLE;
    }
    TU_LOG1("Move object %ld to new parent %ld\r\n", object_handle, new_parent_object_handle);
    obj->parent = new_parent_object_handle;
    return MTP_RESC_OK;
}

mtp_response_t tud_mtp_storage_object_delete(uint32_t object_handle)
{
    fs_object_info_t *obj;

    if (curr_session_id == 0)
    {
        TU_LOG1("ERR: Session not open\r\n");
        return MTP_RESC_SESSION_NOT_OPEN;
    }

    if (object_handle == 0xFFFFFFFF)
        object_handle = 0;

    if (object_handle != 0)
    {
        obj = fs_object_get_from_handle(object_handle);

        if (obj == NULL)
        {
            TU_LOG1("ERR: Object with handle %ld does not exist\r\n", object_handle);
            return MTP_RESC_INVALID_OBJECT_HANDLE;
        }
        obj->allocated = false;
        TU_LOG1("Delete object with handle %ld\r\n", object_handle);
    }

    if (object_handle == 0 || obj->association)
    {
        // Delete also children
        for (unsigned int i=0; i<FS_MAX_NODES; i++)
        {
            obj = &_fs_objects[i];
            if (obj->allocated && obj->parent == object_handle)
            {
                tud_mtp_storage_object_delete(obj->handle);
            }
        }
    }

    return MTP_RESC_OK;
}

void tud_mtp_storage_object_done(void)
{
}
