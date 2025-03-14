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
// FILEINFO LIST
//--------------------------------------------------------------------+
#define FS_MAX_CAPACITY (1024UL * 1024UL)
#define FS_FREE_SPACE   (256UL * 1024UL)
typedef struct
{
    uint32_t parent_object;
    uint32_t object_handle;
    const char *filename;
    uint32_t size;
    const char *date_created;
    const char *date_modified;
    bool  is_association;
    bool  is_readonly;
} mtpd_fileinfo_t;

#define MTP_FILE_NUM 8
static const mtpd_fileinfo_t mtpd_fileinfo[MTP_FILE_NUM] =
{
    {.parent_object = MTP_OBJH_ROOT, .object_handle = 0x5ff1, .filename = "folder1",     .size =    0, .date_created = "20240104T111134.0", .date_modified = "20241214T121110.0", .is_association = true,  .is_readonly = false},
    {.parent_object = MTP_OBJH_ROOT, .object_handle = 0x1b6a, .filename = "folder2",     .size =    0, .date_created = "20240609T111325.0", .date_modified = "20241214T121110.0", .is_association = true,  .is_readonly = false},
    {.parent_object = MTP_OBJH_ROOT, .object_handle = 0x7ef1, .filename = "file0-1.txt", .size =  149, .date_created = "20241211T111410.0", .date_modified = "20241211T121110.0", .is_association = false, .is_readonly = false},
    {.parent_object = MTP_OBJH_ROOT, .object_handle = 0x67bf, .filename = "file0-2.txt", .size =  149, .date_created = "20241015T131622.0", .date_modified = "20241211T121110.0", .is_association = false, .is_readonly = true},
    {.parent_object = 0x5ff1,        .object_handle = 0x1ab5, .filename = "file1-1.txt", .size = 1043, .date_created = "20240810T151040.0", .date_modified = "20241211T121110.0", .is_association = false, .is_readonly = false},
    {.parent_object = 0x5ff1,        .object_handle = 0x5e74, .filename = "file1-2.txt", .size =  963, .date_created = "20240222T164224.0", .date_modified = "20241211T121115.0", .is_association = false, .is_readonly = false},
    {.parent_object = 0x5ff1,        .object_handle = 0x31bc, .filename = "file1-3.txt", .size =  455, .date_created = "20240914T081224.0", .date_modified = "20241211T121120.0", .is_association = false, .is_readonly = false},
    {.parent_object = 0x1b6a,        .object_handle = 0xa542, .filename = "file2-1.txt", .size =  637, .date_created = "20241102T081224.0", .date_modified = "20241211T121120.0", .is_association = false, .is_readonly = false},
};

const char mtpd_filecontent[] = \
    "TinyUSB example content file\n"\
    "LgpHFNxGYy Zvbv7JsJPK pN2laq3F63 LkfhN01bwH NBNszybG8s 6LQmMeZtIg 58LYldVs9Q o8DxQAn5sR R7fxAR3bZ4\n"\
    "R18BmVfk1R CNFSSZeexW FjX6VNL4ti oaIv6nI6Vg FeSXtFLLYj hmbeznLxDL i3ruRE1xYf G7vz5d4N86 0HTghej4ME\n"\
    "xQ4rvmQGV3 GQDNXWaEiN uyeH2rrlgs jJPnnBzUz0 M7y8QnOH39 iiunjjwGej 1v8LP2tHvk eUeinbLdM4 B8WTBPZv9i\n"\
    "aa7NV35z0f PEfTtXSLvy bmkBvfiRxd jhMxdzYCXk 9KKubAScMN JGXftGkCac 2WLDR9HXvc EJ3G6dsK8B 5arz1WCgJW\n"\
    "eH818ZqFs6 iyjo6ewEvr EseV7OZiqe bLjyG4XDlY KKH7X45rEC 7iulgvD1Hf uvnknHpVFB 6MVgqPNnG7 9aaFq7L85z\n"\
    "cBOPGGSSjW l268GETzWe Pt3738YroR RUDFVWq6O7 rgSgmblusz 9GS3NWuhSu nAXRjkK3NT 1ApWRMLF8F wkCVEIkc5I\n"\
    "hA46uJuKuh YdJzZduNCw CtBjzKBzkU KNFL4feUJT 5Dx6V0nMYd eXrZw4lV9G PAT199yeO8 I8RriKIgbS PxqhiACeow\n"\
    "3YpLHOqc2G vsYM60ucKk Ophi0Y1QiR zHJqDiKNBU y92s8d3Rnw cmPjsGemif tYL6VGmo4l X8YpGMTfJJ 5GeUxgmHoA\n"\
    "LI89X8yJrY JHeNRJLVRD 7UJBgQH7SJ ec06d8rBjl qJ1aTqxmWY JS2RFlSysr cxTGFbQ5uG MeLWANNbdn VeJyvOH0Gi\n"\
    "P8xcCI5Fov rmK36tw9E6 fLAUDmBEla MLM1BCBdvS wY9CFWaW42 50aw1E49gy JiZtWt8idW DzXF5skono 15i7mkMLt6\n"\
    "gJoTH6cbc8 32g5cMMcUq yona4LvEBT LLCGznyWUK m7ND6rG5kJ KAqtDHdThV 6MUEQkzjzp VcQ1NF1Rks ebPjPZieDx\n"\
    "1RHp6HSZlA gW6rPIlJHW g0tJTPPA25 Ac2jz92CIl 5aixI4Cb6w bFrycTwz69 0m5BMYaPww 1YR8e0ReBe S7nEIHCGLZ\n"\
    "d4aYGcPbPT zfrwrVsQdl fc7w2zYb4a 2w07GERNgp 7clAd2kKEo TeCTN6F8tE UyaVCQfg0u PQtFD7ZyHM LyiKlqAJXY\n"\
    "nMfwqcQaeL cR5PYYS3YA 4iMWdlvJbn KY7CVW0bWN zJBsTAVXiz vUgIfhMxQb mstlnhQVCn qMZ0nSIX7p A6nePUrUQd\n"\
    "rhzckKgtIe P6IV3MJANR a6AGKVWIub qVK2vLI9ez P8wum3U12r P18KAwkbAZ ZCKuslh6xG SGctC8XmXJ YVBcJw6rnW\n"\
    "AQs0ZjrVRc WxssLuiMAI sY0GqFLGdV BYgjD6gsec z4wlNLvjWg VEURkm0NQe 4K6AzL1X8i Rd2k3Uw5V2 IywOcCJgm6\n"\
    "4z0C8wJHL9 m29dRd37xL MFwhC3B3yT eWtyIlVZCZ MkHuHVzJY1 Lg8lpUvHQG GretKf2Ryk FMNE10CfA1 ueAMantydr\n"\
    "Be6waQB14x QgwadoIp3i pTYmuLkB65 VDzEmIswD3 Utca5kNkkq elXoVThHL5 JmAPXc0BiU PmKZbpYqBh Xo7WssVpX7\n";

const mtpd_fileinfo_t *mtpd_get_fileinfo(uint32_t object_handle);

const mtpd_fileinfo_t *mtpd_get_fileinfo(uint32_t object_handle)
{
    for (int h = 0; h < MTP_FILE_NUM; h++)
    {
        if (mtpd_fileinfo[h].object_handle == object_handle)
            return &mtpd_fileinfo[h];
    }
    return NULL;
}

//--------------------------------------------------------------------+
// API
//--------------------------------------------------------------------+
mtp_response_t tud_mtp_storage_open_session(uint32_t *session_id)
{
    if (*session_id == 0)
        return MTP_RESC_INVALID_PARAMETER;
    if (curr_session_id != 0)
    {
        *session_id = curr_session_id;
        return MTP_RESC_SESSION_ALREADY_OPEN;
    }
    curr_session_id = *session_id;
    return MTP_RESC_OK;
}

mtp_response_t tud_mtp_storage_close_session(uint32_t session_id)
{
    if (session_id != curr_session_id)
        return MTP_RESC_SESSION_NOT_OPEN;
    curr_session_id = 0;
    return MTP_RESC_OK;
}

mtp_response_t tud_mtp_get_storage_id(uint32_t *storage_id)
{
    if (curr_session_id == 0)
        return MTP_RESC_SESSION_NOT_OPEN;
    *storage_id = STORAGE_ID(0x0001, 0x0001);
    return MTP_RESC_OK;
}

mtp_response_t tud_mtp_get_storage_info(uint32_t storage_id, mtp_storage_info_t *info)
{
    if (curr_session_id == 0)
        return MTP_RESC_SESSION_NOT_OPEN;
    if (storage_id != STORAGE_ID(0x0001, 0x0001))
        return MTP_RESC_INVALID_STORAGE_ID;
    info->storage_type = MTP_STORAGE_TYPE_FIXED_ROM;
    info->filesystem_type = MTP_FILESYSTEM_TYPE_GENERIC_HIERARCHICAL;
    info->access_capability = MTP_ACCESS_CAPABILITY_READ_WRITE;
    info->max_capacity_in_bytes = FS_MAX_CAPACITY;
    info->free_space_in_bytes = FS_FREE_SPACE;
    // Free space in objects is unsupported
    info->free_space_in_objects = 0xFFFFFFFFul;

    mtpd_gct_append_wstring(MTPD_STORAGE_DESCRIPTION);
    mtpd_gct_append_wstring(MTPD_VOLUME_IDENTIFIER);
    return MTP_RESC_OK;
}

mtp_response_t tud_mpt_storage_format(uint32_t storage_id)
{
    if (curr_session_id == 0)
        return MTP_RESC_SESSION_NOT_OPEN;
    if (storage_id != STORAGE_ID(0x0001, 0x0001))
        return MTP_RESC_INVALID_STORAGE_ID;
    // TODO execute device format
    return MTP_RESC_OK;
}

mtp_response_t tud_mtp_storage_association_get_object_handle(uint32_t storage_id, uint32_t parent_object_handle, uint32_t *next_child_handle)
{
    static uint32_t current_parent_object = 0xffff;
    static int object_count = 0;

    if (curr_session_id == 0)
        return MTP_RESC_SESSION_NOT_OPEN;
    if (storage_id != STORAGE_ID(0x0001, 0x0001))
        return MTP_RESC_INVALID_STORAGE_ID;
    // Request for objects with no parent (0xFFFFFFFF) are considered root objects
    if (parent_object_handle == 0xFFFFFFFF)
        parent_object_handle = 0;

    if (parent_object_handle != current_parent_object)
    {
        current_parent_object = parent_object_handle;
        object_count = 0;
    }

    for (int h = 0, cnt = 0; h < MTP_FILE_NUM; h++)
    {
        if (mtpd_fileinfo[h].parent_object == current_parent_object)
            cnt++;
        if (cnt > object_count)
        {
            object_count = cnt;
            *next_child_handle = mtpd_fileinfo[h].object_handle;
            return MTP_RESC_OK;
        }
    }
    *next_child_handle = 0;
    return MTP_RESC_OK;
}

mtp_response_t tud_mtp_storage_object_write_info(uint32_t storage_id, uint32_t parent_object, uint32_t *new_object_handle, const mtp_object_info_t *info)
{
    (void)parent_object;
    (void)new_object_handle;
    (void)info;
    if (curr_session_id == 0)
        return MTP_RESC_SESSION_NOT_OPEN;
    if (storage_id != STORAGE_ID(0x0001, 0x0001))
        return MTP_RESC_INVALID_STORAGE_ID;
    // TODO Object write not implemented
    return MTP_RESC_OPERATION_NOT_SUPPORTED;
}

mtp_response_t tud_mtp_storage_object_read_info(uint32_t object_handle, mtp_object_info_t *info)
{
    if (curr_session_id == 0)
        return MTP_RESC_SESSION_NOT_OPEN;

    const mtpd_fileinfo_t *fileinfo = mtpd_get_fileinfo(object_handle);
    if (fileinfo == NULL)
        return MTP_RESC_INVALID_OBJECT_HANDLE;

    memset(info, 0, sizeof(mtp_object_info_t));
    info->storage_id = STORAGE_ID(0x0001, 0x0001);
    if (fileinfo->is_association)
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
        info->object_compressed_size = fileinfo->size;
        info->association_type = MTP_ASSOCIATION_UNDEFINED;
    }
    info->thumb_format = MTP_OBJF_UNDEFINED;
    info->parent_object = fileinfo->parent_object;

    mtpd_gct_append_wstring(fileinfo->filename);
    mtpd_gct_append_wstring(fileinfo->date_created); // date_created
    mtpd_gct_append_wstring(fileinfo->date_modified); // date_modified
    mtpd_gct_append_wstring(""); // keywords, not used

    return MTP_RESC_OK;
}

mtp_response_t tud_mtp_storage_object_write(uint32_t object_handle, const uint8_t *buffer, uint32_t size)
{
    (void)object_handle;
    (void)buffer;
    (void)size;
    if (curr_session_id == 0)
        return MTP_RESC_SESSION_NOT_OPEN;
    // TODO Object write not implemented
    return MTP_RESC_OPERATION_NOT_SUPPORTED;
}

mtp_response_t tud_mtp_storage_object_size(uint32_t object_handle, uint32_t *size)
{
    const mtpd_fileinfo_t *fileinfo = mtpd_get_fileinfo(object_handle);
    if (fileinfo == NULL)
        return MTP_RESC_INVALID_OBJECT_HANDLE;
    *size = fileinfo->size;
    return MTP_RESC_OK;
}

mtp_response_t tud_mtp_storage_object_read(uint32_t object_handle, void *buffer, uint32_t buffer_size, uint32_t *read_count)
{
    static uint32_t current_object_handle = 0xffff;
    static int pos = 0;

    const mtpd_fileinfo_t *fileinfo = mtpd_get_fileinfo(object_handle);
    if (fileinfo == NULL)
        return MTP_RESC_INVALID_OBJECT_HANDLE;
    if (object_handle != current_object_handle)
    {
        current_object_handle = object_handle;
        pos = 0;
    }
    if (sizeof(mtpd_filecontent) - pos > buffer_size)
    {
        *read_count = buffer_size;
        memcpy(buffer, &mtpd_filecontent[pos], *read_count);
        pos += *read_count;
    }
    else
    {
        *read_count = sizeof(mtpd_filecontent) - pos;
        memcpy(buffer, &mtpd_filecontent[pos], *read_count);
        pos = 0;
    }
    return MTP_RESC_OK;
}

mtp_response_t tud_mtp_storage_object_move(uint32_t object_handle, uint32_t new_parent_object_handle)
{
    (void)object_handle;
    (void)new_parent_object_handle;
    return MTP_RESC_OPERATION_NOT_SUPPORTED;
}

mtp_response_t tud_mtp_storage_object_delete(uint32_t object_handle)
{
    if (curr_session_id == 0)
        return MTP_RESC_SESSION_NOT_OPEN;
    if (object_handle == 0xFFFFFFFF)
    {
        // TODO delete all objects
        return MTP_RESC_OK;
    }
    const mtpd_fileinfo_t *fileinfo = mtpd_get_fileinfo(object_handle);
    if (fileinfo == NULL)
        return MTP_RESC_INVALID_OBJECT_HANDLE;

    // TODO delete object
    return MTP_RESC_OK;
}

void tud_mtp_storage_object_done(void)
{
}
