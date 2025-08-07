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
 * This file is part of the TinyUSB stack.
 */

#ifndef _TUSB_MTP_H_
#define _TUSB_MTP_H_

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "tusb_option.h"

#if (CFG_TUD_ENABLED && CFG_TUD_MTP)

#ifdef __cplusplus
 extern "C" {
#endif

#define TU_ARRAY_LEN(a) (sizeof(a)/sizeof(a[0]))
#define STORAGE_ID(physical_id, logical_id) ( (((uint32_t)physical_id & 0xFFFF) << 16) | ((uint32_t)logical_id & 0x0000FFFF) )
typedef uint16_t wchar16_t;

//--------------------------------------------------------------------+
// Media Transfer Protocol Class Constant
//--------------------------------------------------------------------+

// Media Transfer Protocol Subclass
typedef enum
{
  MTP_SUBCLASS = 1
} mtp_subclass_type_t;

// MTP Protocol.
typedef enum
{
  MTP_PROTOCOL_STILL_IMAGE = 1,
} mtp_protocol_type_t;

// PTP/MTP protocol phases
typedef enum
{
  MTP_PHASE_IDLE = 0,
  MTP_PHASE_COMMAND,
  MTP_PHASE_DATA_IN,
  MTP_PHASE_DATA_OUT,
  MTP_PHASE_RESPONSE,
  MTP_PHASE_ERROR,
  MTP_PHASE_NONE,
} mtp_phase_type_t;

// PTP/MTP Class requests
typedef enum
{
  MTP_REQ_CANCEL             = 0x64,
  MTP_REQ_GET_EXT_EVENT_DATA = 0x65,
  MTP_REQ_RESET              = 0x66,
  MTP_REQ_GET_DEVICE_STATUS  = 0x67,
} mtp_class_request_t;

#define MTP_GENERIC_DATA_BLOCK_LENGTH 12
#define MTP_MAX_PACKET_SIZE 512

// PTP/MTP Generic container
typedef struct TU_ATTR_PACKED
{
  uint32_t container_length;
  uint16_t container_type;
  uint16_t code;
  uint32_t transaction_id;
  uint32_t data[MTP_MAX_PACKET_SIZE / sizeof(uint32_t)];
} mtp_generic_container_t;

// PTP/MTP Container type
typedef enum
{
  MTP_CONTAINER_TYPE_UNDEFINED      = 0,
  MTP_CONTAINER_TYPE_COMMAND_BLOCK  = 1,
  MTP_CONTAINER_TYPE_DATA_BLOCK     = 2,
  MTP_CONTAINER_TYPE_RESPONSE_BLOCK = 3,
  MTP_CONTAINER_TYPE_EVENT_BLOCK    = 4,
} mtp_container_type_t;

// Supported OperationCode
typedef enum
{
  MTP_OPEC_GET_DEVICE_INFO            = 0x1001u,
  MTP_OPEC_OPEN_SESSION               = 0x1002u,
  MTP_OPEC_CLOSE_SESSION              = 0x1003u,
  MTP_OPEC_GET_STORAGE_IDS            = 0x1004u,
  MTP_OPEC_GET_STORAGE_INFO           = 0x1005u,
  MTP_OPEC_GET_NUM_OBJECTS            = 0x1006u,
  MTP_OPEC_GET_OBJECT_HANDLES         = 0x1007u,
  MTP_OPEC_GET_OBJECT_INFO            = 0x1008u,
  MTP_OPEC_GET_OBJECT                 = 0x1009u,
  MTP_OPEC_GET_THUMB                  = 0x100Au,
  MTP_OPEC_DELETE_OBJECT              = 0x100Bu,
  MTP_OPEC_SEND_OBJECT_INFO           = 0x100Cu,
  MTP_OPEC_SEND_OBJECT                = 0x100Du,
  MTP_OPEC_INITIAL_CAPTURE            = 0x100Eu,
  MTP_OPEC_FORMAT_STORE               = 0x100Fu,
  MTP_OPEC_RESET_DEVICE               = 0x1010u,
  MTP_OPEC_SELF_TEST                  = 0x1011u,
  MTP_OPEC_SET_OBJECT_PROTECTION      = 0x1012u,
  MTP_OPEC_POWER_DOWN                 = 0x1013u,
  MTP_OPEC_GET_DEVICE_PROP_DESC       = 0x1014u,
  MTP_OPEC_GET_DEVICE_PROP_VALUE      = 0x1015u,
  MTP_OPEC_SET_DEVICE_PROP_VALUE      = 0x1016u,
  MTP_OPEC_RESET_DEVICE_PROP_VALUE    = 0x1017u,
  MTP_OPEC_TERMINATE_OPEN_CAPTURE     = 0x1018u,
  MTP_OPEC_MOVE_OBJECT                = 0x1019u,
  MTP_OPEC_COPY_OBJECT                = 0x101Au,
  MTP_OPEC_GET_PARTIAL_OBJECT         = 0x101Bu,
  MTP_OPEC_INITIATE_OPEN_CAPTURE      = 0x101Bu,
  MTP_OPEC_GET_OBJECT_PROPS_SUPPORTED = 0x9801u,
  MTP_OPEC_GET_OBJECT_PROP_DESC       = 0x9802u,
  MTP_OPEC_GET_OBJECT_PROP_VALUE      = 0x9803u,
  MTP_OPEC_SET_OBJECT_PROP_VALUE      = 0x9804u,
  MTP_OPEC_GET_OBJECT_PROPLIST        = 0x9805u,
  MTP_OPEC_GET_OBJECT_PROP_REFERENCES = 0x9810u,
  MTP_OPEC_GETSERVICEIDS              = 0x9301u,
  MTP_OPEC_GETSERVICEINFO             = 0x9302u,
  MTP_OPEC_GETSERVICECAPABILITIES     = 0x9303u,
  MTP_OPEC_GETSERVICEPROPDESC         = 0x9304u,
} mtp_operation_code_t;

// Supported EventCode
typedef enum
{
  MTP_EVTC_OBJECT_ADDED = 0x4002,
} mtp_event_code_t;


// Supported Device Properties
typedef enum
{
  MTP_DEVP_UNDEFINED            = 0x5000u,
  MTP_DEVP_BATTERY_LEVEL        = 0x5001u,
  MTP_DEVP_DEVICE_FRIENDLY_NAME = 0xD402u,
} mtp_event_properties_t;

// Supported Object Properties
typedef enum
{
  MTP_OBJP_STORAGE_ID                          = 0xDC01u,
  MTP_OBJP_OBJECT_FORMAT                       = 0xDC02u,
  MTP_OBJP_PROTECTION_STATUS                   = 0xDC03u,
  MTP_OBJP_OBJECT_SIZE                         = 0xDC04u,
  MTP_OBJP_ASSOCIATION_TYPE                    = 0xDC05u,
  MTP_OBJP_OBJECT_FILE_NAME                    = 0xDC07u,
  MTP_OBJP_PARENT_OBJECT                       = 0xDC0Bu,
  MTP_OBJP_PERSISTENT_UNIQUE_OBJECT_IDENTIFIER = 0xDC41u,
  MTP_OBJP_NAME                                = 0xDC44u,
} mtp_object_properties_t;

// Object formats
typedef enum
{
  MTP_OBJF_UNDEFINED   = 0x3000u,
  MTP_OBJF_ASSOCIATION = 0x3001u,
  MTP_OBJF_TEXT        = 0x3004u,
} mtp_object_formats_t;

// Predefined Object handles
typedef enum
{
  MTP_OBJH_ROOT = 0x0000,
} mtp_object_handles_t;

// Datatypes
typedef enum
{
  MTP_TYPE_UNDEFINED = 0x0000u,
  MTP_TYPE_INT8      = 0x0001u,
  MTP_TYPE_UINT8     = 0x0002u,
  MTP_TYPE_INT16     = 0x0003u,
  MTP_TYPE_UINT16    = 0x0004u,
  MTP_TYPE_INT32     = 0x0005u,
  MTP_TYPE_UINT32    = 0x0006u,
  MTP_TYPE_INT64     = 0x0007u,
  MTP_TYPE_UINT64    = 0x0008u,
  MTP_TYPE_STR       = 0xFFFFu,
} mtp_datatypes_t;

// Get/Set
typedef enum
{
  MTP_MODE_GET     = 0x00u,
  MTP_MODE_GET_SET = 0x01u,
} mtp_mode_get_set_t;

tu_static const uint16_t mtp_operations_supported[] = {
  MTP_OPEC_GET_DEVICE_INFO,
  MTP_OPEC_OPEN_SESSION,
  MTP_OPEC_CLOSE_SESSION,
  MTP_OPEC_GET_STORAGE_IDS,
  MTP_OPEC_GET_STORAGE_INFO,
  MTP_OPEC_GET_NUM_OBJECTS,
  MTP_OPEC_GET_OBJECT_HANDLES,
  MTP_OPEC_GET_OBJECT_INFO,
  MTP_OPEC_GET_OBJECT,
  MTP_OPEC_DELETE_OBJECT,
  MTP_OPEC_SEND_OBJECT_INFO,
  MTP_OPEC_SEND_OBJECT,
  MTP_OPEC_FORMAT_STORE,
  MTP_OPEC_RESET_DEVICE,
  MTP_OPEC_GET_DEVICE_PROP_DESC,
  MTP_OPEC_GET_DEVICE_PROP_VALUE,
  MTP_OPEC_SET_DEVICE_PROP_VALUE,
};

tu_static const uint16_t mtp_events_supported[] = {
  MTP_EVTC_OBJECT_ADDED,
};

tu_static const uint16_t mtp_device_properties_supported[] = {
  MTP_DEVP_DEVICE_FRIENDLY_NAME,
};

tu_static const uint16_t mtp_capture_formats[] = {
  MTP_OBJF_UNDEFINED,
  MTP_OBJF_ASSOCIATION,
  MTP_OBJF_TEXT,
};

tu_static const uint16_t mtp_playback_formats[] = {
  MTP_OBJF_UNDEFINED,
  MTP_OBJF_ASSOCIATION,
  MTP_OBJF_TEXT,
};

//--------------------------------------------------------------------+
// Data structures
//--------------------------------------------------------------------+

// DeviceInfo Dataset
#define MTP_EXTENSIONS "microsoft.com: 1.0; "
typedef struct TU_ATTR_PACKED {
  uint16_t standard_version;
  uint32_t mtp_vendor_extension_id;
  uint16_t mtp_version;
  uint8_t   mtp_extensions_len;
  wchar16_t mtp_extensions[TU_ARRAY_LEN(MTP_EXTENSIONS)] TU_ATTR_PACKED;

  uint16_t functional_mode;
  /* Operations supported */
  uint32_t operations_supported_len;
  uint16_t operations_supported[TU_ARRAY_LEN(mtp_operations_supported)] TU_ATTR_PACKED;
  /* Events supported */
  uint32_t events_supported_len;
  uint16_t events_supported[TU_ARRAY_LEN(mtp_events_supported)] TU_ATTR_PACKED;
  /* Device properties supported */
  uint32_t device_properties_supported_len;
  uint16_t device_properties_supported[TU_ARRAY_LEN(mtp_device_properties_supported)] TU_ATTR_PACKED;
  /* Capture formats */
  uint32_t capture_formats_len;
  uint16_t capture_formats[TU_ARRAY_LEN(mtp_capture_formats)] TU_ATTR_PACKED;
  /* Playback formats */
  uint32_t playback_formats_len;
  uint16_t playback_formats[TU_ARRAY_LEN(mtp_playback_formats)] TU_ATTR_PACKED;
} mtp_device_info_t;
// The following fields will be dynamically added to the struct at runtime:
// - wstring manufacturer
// - wstring model
// - wstring device_version
// - wstring serial_number


#define MTP_STRING_DEF(name, string) \
  uint8_t name##_len; \
  wchar16_t name[TU_ARRAY_LEN(string)];

#define MTP_ARRAY_DEF(name, array) \
  uint16_t name##_len; \
  typeof(name) name[TU_ARRAY_LEN(array)];

// StorageInfo dataset
typedef struct TU_ATTR_PACKED {
  uint16_t storage_type;
  uint16_t filesystem_type;
  uint16_t access_capability;
  uint64_t max_capacity_in_bytes;
  uint64_t free_space_in_bytes;
  uint32_t free_space_in_objects;
} mtp_storage_info_t;
// The following fields will be dynamically added to the struct at runtime:
// - wstring storage_description
// - wstring volume_identifier

// ObjectInfo Dataset
typedef struct TU_ATTR_PACKED {
  uint32_t storage_id;
  uint16_t object_format;
  uint16_t protection_status;
  uint32_t object_compressed_size;
  uint16_t thumb_format; // unused
  uint32_t thumb_compressed_size; // unused
  uint32_t thumb_pix_width; // unused
  uint32_t thumb_pix_height; // unused
  uint32_t image_pix_width; // unused
  uint32_t image_pix_height; // unused
  uint32_t image_bit_depth; // unused
  uint32_t parent_object; // 0: root
  uint16_t association_type;
  uint32_t association_description; // not used
  uint32_t sequence_number; // not used
} mtp_object_info_t;
// The following fields will be dynamically added to the struct at runtime:
// - wstring filename;
// - datetime_wstring date_created;
// - datetime_wstring date_modified;
// - wstring keywords;

// Storage IDs
typedef struct TU_ATTR_PACKED {
  uint32_t storage_ids_len;
  uint32_t storage_ids[CFG_MTP_STORAGE_ID_COUNT];
} mtp_storage_ids_t;

// DevicePropDesc Dataset
typedef struct TU_ATTR_PACKED {
  uint16_t device_property_code;
  uint16_t datatype;
  uint8_t  get_set;
} mtp_device_prop_desc_t;
// The following fields will be dynamically added to the struct at runtime:
// - wstring factory_def_value;
// - wstring current_value_len;
// - uint8_t form_flag;

typedef struct TU_ATTR_PACKED {
  uint16_t wLength;
  uint16_t code;
} mtp_device_status_res_t;

typedef struct TU_ATTR_PACKED {
  uint32_t object_handle;
  uint32_t storage_id;
  uint32_t parent_object_handle;
} mtp_basic_object_info_t;

//--------------------------------------------------------------------+
// Definitions
//--------------------------------------------------------------------+

typedef enum {
    MTP_STORAGE_TYPE_UNDEFINED     = 0x0000u,
    MTP_STORAGE_TYPE_FIXED_ROM     = 0x0001u,
    MTP_STORAGE_TYPE_REMOVABLE_ROM = 0x0002u,
    MTP_STORAGE_TYPE_FIXED_RAM     = 0x0003u,
    MTP_STORAGE_TYPE_REMOVABLE_RAM = 0x0004u,
} mtp_storage_type_t;

typedef enum {
    MTP_FILESYSTEM_TYPE_UNDEFINED            = 0x0000u,
    MTP_FILESYSTEM_TYPE_GENERIC_FLAT         = 0x0001u,
    MTP_FILESYSTEM_TYPE_GENERIC_HIERARCHICAL = 0x0002u,
    MTP_FILESYSTEM_TYPE_DCF                  = 0x0003u,
} mtp_filesystem_type_t;

typedef enum {
    MTP_ACCESS_CAPABILITY_READ_WRITE                        = 0x0000u,
    MTP_ACCESS_CAPABILITY_READ_ONLY_WITHOUT_OBJECT_DELETION = 0x0001u,
    MTP_ACCESS_CAPABILITY_READ_ONLY_WITH_OBJECT_DELETION    = 0x0002u,
} mtp_access_capability_t;

typedef enum {
    MTP_PROTECTION_STATUS_NO_PROTECTION  = 0x0000u,
    MTP_PROTECTION_STATUS_READ_ONLY      = 0x0001u,
    MTP_PROTECTION_STATUS_READ_ONLY_DATA = 0x8002u,
    MTP_PROTECTION_NON_TRANSFERABLE_DATA = 0x8003u,
} mtp_protection_status_t;

typedef enum {
    MTP_ASSOCIATION_UNDEFINED            = 0x0000u,
    MTP_ASSOCIATION_GENERIC_FOLDER       = 0x0001u,
    MTP_ASSOCIATION_GENERIC_ALBUM        = 0x0002u,
    MTP_ASSOCIATION_TIME_SEQUENCE        = 0x0003u,
    MTP_ASSOCIATION_HORIZONTAL_PANORAMIC = 0x0004u,
    MTP_ASSOCIATION_VERTICAL_PANORAMIC   = 0x0005u,
    MTP_ASSOCIATION_2D_PANORAMIC         = 0x0006u,
} mtp_association_t;

// Responses
typedef enum {
// Supported ResponseCode
  MTP_RESC_UNDEFINED                           = 0x2000u,
  MTP_RESC_OK                                  = 0x2001u,
  MTP_RESC_GENERAL_ERROR                       = 0x2002u,
  MTP_RESC_SESSION_NOT_OPEN                    = 0x2003u,
  MTP_RESC_INVALID_TRANSACTION_ID              = 0x2004u,
  MTP_RESC_OPERATION_NOT_SUPPORTED             = 0x2005u,
  MTP_RESC_PARAMETER_NOT_SUPPORTED             = 0x2006u,
  MTP_RESC_INCOMPLETE_TRANSFER                 = 0x2007u,
  MTP_RESC_INVALID_STORAGE_ID                  = 0x2008u,
  MTP_RESC_INVALID_OBJECT_HANDLE               = 0x2009u,
  MTP_RESC_STORE_FULL                          = 0x200Cu,
  MTP_RESC_OBJECT_WRITE_PROTECTED              = 0x200Du,
  MTP_RESC_STORE_NOT_AVAILABLE                 = 0x2013u,
  MTP_RESC_SPECIFICATION_BY_FORMAT_UNSUPPORTED = 0x2014u,
  MTP_RESC_NO_VALID_OBJECTINFO                 = 0x2015u,
  MTP_RESC_DEVICE_BUSY                         = 0x2019u,
  MTP_RESC_INVALID_PARENT_OBJECT               = 0x201Au,
  MTP_RESC_INVALID_DEVICE_PROP_FORMAT          = 0x201Bu,
  MTP_RESC_INVALID_DEVICE_PROP_VALUE           = 0x201Cu,
  MTP_RESC_INVALID_PARAMETER                   = 0x201Du,
  MTP_RESC_SESSION_ALREADY_OPEN                = 0x201Eu,
  MTP_RESC_TRANSACTION_CANCELLED               = 0x201Fu,
} mtp_response_t;

#ifdef __cplusplus
 }
#endif

#endif /* CFG_TUD_ENABLED && CFG_TUD_MTP */

#endif /* _TUSB_MTP_H_ */
