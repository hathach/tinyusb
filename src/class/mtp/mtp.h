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

#ifndef TUSB_MTP_H_
#define TUSB_MTP_H_

#include "common/tusb_common.h"

#if (CFG_TUD_ENABLED && CFG_TUD_MTP)

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Media Transfer Protocol Class Constant
//--------------------------------------------------------------------+

// Media Transfer Protocol Subclass
typedef enum {
  MTP_SUBCLASS_STILL_IMAGE = 1
} mtp_subclass_type_t;

// MTP Protocol.
typedef enum {
  MTP_PROTOCOL_PIMA_15470 = 1, ///< Picture Transfer Protocol (PIMA 15470)
} mtp_protocol_type_t;

// PTP/MTP protocol phases
typedef enum {
  MTP_PHASE_COMMAND = 0,
  MTP_PHASE_DATA,
  MTP_PHASE_RESPONSE,
  MTP_PHASE_ERROR
} mtp_phase_type_t;

// PTP/MTP Class requests, PIMA 15740-2000: D.5.2
typedef enum {
  MTP_REQ_CANCEL             = 0x64,
  MTP_REQ_GET_EXT_EVENT_DATA = 0x65,
  MTP_REQ_RESET              = 0x66,
  MTP_REQ_GET_DEVICE_STATUS  = 0x67,
} mtp_class_request_t;

// PTP/MTP Container type
typedef enum {
  MTP_CONTAINER_TYPE_UNDEFINED      = 0,
  MTP_CONTAINER_TYPE_COMMAND_BLOCK  = 1,
  MTP_CONTAINER_TYPE_DATA_BLOCK     = 2,
  MTP_CONTAINER_TYPE_RESPONSE_BLOCK = 3,
  MTP_CONTAINER_TYPE_EVENT_BLOCK    = 4,
} mtp_container_type_t;

// MTP 1.1 Appendix A: Object formats
typedef enum {
  // ---- Base formats ----
  MTP_OBJ_FORMAT_UNDEFINED        = 0x3000u, // Undefined object
  MTP_OBJ_FORMAT_ASSOCIATION      = 0x3001u, // Association (for example, a folder)
  MTP_OBJ_FORMAT_SCRIPT           = 0x3002u, // Device model-specific script
  MTP_OBJ_FORMAT_EXECUTABLE       = 0x3003u, // Device model-specific binary executable
  MTP_OBJ_FORMAT_TEXT             = 0x3004u, // Text file
  MTP_OBJ_FORMAT_HTML             = 0x3005u, // Hypertext Markup Language file (text)
  MTP_OBJ_FORMAT_DPOF             = 0x3006u, // Digital Print Order Format file (text)
  MTP_OBJ_FORMAT_AIFF             = 0x3007u, // Audio clip (AIFF)
  MTP_OBJ_FORMAT_WAV              = 0x3008u, // Audio clip (WAV)
  MTP_OBJ_FORMAT_MP3              = 0x3009u, // MPEG-1 Layer III audio (ISO/IEC 13818-3)
  MTP_OBJ_FORMAT_AVI              = 0x300Au, // Video clip (AVI)
  MTP_OBJ_FORMAT_MPEG             = 0x300Bu, // Video clip (MPEG)
  MTP_OBJ_FORMAT_ASF              = 0x300Cu, // Microsoft Advanced Streaming Format (video)

  // ---- Image formats ----
  MTP_OBJ_FORMAT_UNDEFINED_IMAGE  = 0x3800u, // Undefined image object
  MTP_OBJ_FORMAT_EXIF_JPEG        = 0x3801u, // Exchangeable Image Format, JEIDA standard
  MTP_OBJ_FORMAT_TIFF_EP          = 0x3802u, // Tag Image File Format for Electronic Photography
  MTP_OBJ_FORMAT_FLASHPIX         = 0x3803u, // Structured Storage Image Format (FlashPix)
  MTP_OBJ_FORMAT_BMP              = 0x3804u, // Microsoft Windows Bitmap file
  MTP_OBJ_FORMAT_CIFF             = 0x3805u, // Canon Camera Image File Format
  MTP_OBJ_FORMAT_UNDEFINED_3806   = 0x3806u, // Reserved / Undefined
  MTP_OBJ_FORMAT_GIF              = 0x3807u, // Graphics Interchange Format
  MTP_OBJ_FORMAT_JFIF             = 0x3808u, // JPEG File Interchange Format
  MTP_OBJ_FORMAT_CD               = 0x3809u, // PhotoCD Image Pac
  MTP_OBJ_FORMAT_PICT             = 0x380Au, // Quickdraw Image Format
  MTP_OBJ_FORMAT_PNG              = 0x380Bu, // Portable Network Graphics
  MTP_OBJ_FORMAT_UNDEFINED_380C   = 0x380Cu, // Reserved / Undefined
  MTP_OBJ_FORMAT_TIFF             = 0x380Du, // Tag Image File Format (baseline)
  MTP_OBJ_FORMAT_TIFF_IT          = 0x380Eu, // Tag Image File Format for IT (graphic arts)
  MTP_OBJ_FORMAT_JP2              = 0x380Fu, // JPEG2000 Baseline File Format
  MTP_OBJ_FORMAT_JPX              = 0x3810u, // JPEG2000 Extended File Format

  // ---- Firmware & misc ----
  MTP_OBJ_FORMAT_UNDEFINED_FIRMWARE = 0xB802u, // Undefined Firmware
  MTP_OBJ_FORMAT_WBMP               = 0xB803u, // Wireless Application Protocol Bitmap Format (.wbmp)
  MTP_OBJ_FORMAT_WINDOWS_IMAGE      = 0xB881u, // Windows Image Format
  MTP_OBJ_FORMAT_JPEGXR             = 0xB804u, // JPEG XR (.hdp, .jxr, .wdp)

  // ---- Audio formats ----
  MTP_OBJ_FORMAT_UNDEFINED_AUDIO  = 0xB900u, // Undefined audio object
  MTP_OBJ_FORMAT_WMA              = 0xB901u, // Windows Media Audio
  MTP_OBJ_FORMAT_OGG              = 0xB902u, // OGG container
  MTP_OBJ_FORMAT_AAC              = 0xB903u, // Advanced Audio Coding (.aac)
  MTP_OBJ_FORMAT_AUDIBLE          = 0xB904u, // Audible format
  MTP_OBJ_FORMAT_FLAC             = 0xB906u, // Free Lossless Audio Codec
  MTP_OBJ_FORMAT_QCELP            = 0xB907u, // Qualcomm Code Excited Linear Prediction (.qcp)
  MTP_OBJ_FORMAT_AMR              = 0xB908u, // Adaptive Multi-Rate audio (.amr)

  // ---- Video formats ----
  MTP_OBJ_FORMAT_UNDEFINED_VIDEO  = 0xB980u, // Undefined video object
  MTP_OBJ_FORMAT_WMV              = 0xB981u, // Windows Media Video
  MTP_OBJ_FORMAT_MP4              = 0xB982u, // MP4 Container (ISO 14496-1)
  MTP_OBJ_FORMAT_MP2              = 0xB983u, // MPEG-1 Layer II audio
  MTP_OBJ_FORMAT_3GP              = 0xB984u, // 3GP Container
  MTP_OBJ_FORMAT_3G2              = 0xB985u, // 3GPP2 Container
  MTP_OBJ_FORMAT_AVCHD            = 0xB986u, // AVCHD (MPEG-4 AVC + Dolby Digital)
  MTP_OBJ_FORMAT_ATSC_TS          = 0xB987u, // ATSC-compliant MPEG-2 Transport Stream
  MTP_OBJ_FORMAT_DVB_TS           = 0xB988u, // DVB-compliant MPEG-2 Transport Stream

  // ---- Collections ----
  MTP_OBJ_FORMAT_UNDEFINED_COLLECTION          = 0xBA00u, // Undefined collection
  MTP_OBJ_FORMAT_ABSTRACT_MULTIMEDIA_ALBUM     = 0xBA01u, // Abstract Multimedia Album
  MTP_OBJ_FORMAT_ABSTRACT_IMAGE_ALBUM          = 0xBA02u, // Abstract Image Album
  MTP_OBJ_FORMAT_ABSTRACT_AUDIO_ALBUM          = 0xBA03u, // Abstract Audio Album
  MTP_OBJ_FORMAT_ABSTRACT_VIDEO_ALBUM          = 0xBA04u, // Abstract Video Album
  MTP_OBJ_FORMAT_ABSTRACT_AV_PLAYLIST          = 0xBA05u, // Abstract Audio & Video Playlist
  MTP_OBJ_FORMAT_ABSTRACT_CONTACT_GROUP        = 0xBA06u, // Abstract Contact Group
  MTP_OBJ_FORMAT_ABSTRACT_MESSAGE_FOLDER       = 0xBA07u, // Abstract Message Folder
  MTP_OBJ_FORMAT_ABSTRACT_CHAPTERED_PRODUCTION = 0xBA08u, // Abstract Chaptered Production
  MTP_OBJ_FORMAT_ABSTRACT_AUDIO_PLAYLIST       = 0xBA09u, // Abstract Audio Playlist
  MTP_OBJ_FORMAT_ABSTRACT_VIDEO_PLAYLIST       = 0xBA0Au, // Abstract Video Playlist
  MTP_OBJ_FORMAT_ABSTRACT_MEDIACAST            = 0xBA0Bu, // Abstract Mediacast (RSS enclosure)

  // ---- Playlist formats ----
  MTP_OBJ_FORMAT_WPL_PLAYLIST     = 0xBA10u, // Windows Media Player Playlist (.wpl)
  MTP_OBJ_FORMAT_M3U_PLAYLIST     = 0xBA11u, // M3U Playlist
  MTP_OBJ_FORMAT_MPL_PLAYLIST     = 0xBA12u, // MPL Playlist
  MTP_OBJ_FORMAT_ASX_PLAYLIST     = 0xBA13u, // ASX Playlist
  MTP_OBJ_FORMAT_PLS_PLAYLIST     = 0xBA14u, // PLS Playlist

  // ---- Document formats ----
  MTP_OBJ_FORMAT_UNDEFINED_DOC    = 0xBA80u, // Undefined Document
  MTP_OBJ_FORMAT_ABSTRACT_DOC     = 0xBA81u, // Abstract Document
  MTP_OBJ_FORMAT_XML_DOC          = 0xBA82u, // XML Document
  MTP_OBJ_FORMAT_DOC              = 0xBA83u, // Microsoft Word Document
  MTP_OBJ_FORMAT_MHT_DOC          = 0xBA84u, // MHT Compiled HTML Document
  MTP_OBJ_FORMAT_XLS              = 0xBA85u, // Microsoft Excel Spreadsheet
  MTP_OBJ_FORMAT_PPT              = 0xBA86u, // Microsoft PowerPoint Presentation

  // ---- Messaging ----
  MTP_OBJ_FORMAT_UNDEFINED_MSG    = 0xBB00u, // Undefined Message
  MTP_OBJ_FORMAT_ABSTRACT_MSG     = 0xBB01u, // Abstract Message

  // ---- Bookmarks ----
  MTP_OBJ_FORMAT_UNDEFINED_BOOKMARK = 0xBB10u, // Undefined Bookmark
  MTP_OBJ_FORMAT_ABSTRACT_BOOKMARK  = 0xBB11u, // Abstract Bookmark

  // ---- Appointments ----
  MTP_OBJ_FORMAT_UNDEFINED_APPT   = 0xBB20u, // Undefined Appointment
  MTP_OBJ_FORMAT_ABSTRACT_APPT    = 0xBB21u, // Abstract Appointment
  MTP_OBJ_FORMAT_VCALENDAR1       = 0xBB22u, // vCalendar 1.0

  // ---- Tasks ----
  MTP_OBJ_FORMAT_UNDEFINED_TASK   = 0xBB40u, // Undefined Task
  MTP_OBJ_FORMAT_ABSTRACT_TASK    = 0xBB41u, // Abstract Task
  MTP_OBJ_FORMAT_ICALENDAR        = 0xBB42u, // iCalendar

  // ---- Notes ----
  MTP_OBJ_FORMAT_UNDEFINED_NOTE   = 0xBB60u, // Undefined Note
  MTP_OBJ_FORMAT_ABSTRACT_NOTE    = 0xBB61u, // Abstract Note

  // ---- Contacts ----
  MTP_OBJ_FORMAT_UNDEFINED_CONTACT= 0xBB80u, // Undefined Contact
  MTP_OBJ_FORMAT_ABSTRACT_CONTACT = 0xBB81u, // Abstract Contact
  MTP_OBJ_FORMAT_VCARD2           = 0xBB82u, // vCard 2.1
  MTP_OBJ_FORMAT_VCARD3           = 0xBB83u, // vCard 3.0
} mtp_object_formats_t;

// MTP 1.1 Appendix B: Object Properties
typedef enum {
  MTP_OBJ_PROP_STORAGE_ID                 = 0xDC01u, // StorageID
  MTP_OBJ_PROP_OBJECT_FORMAT              = 0xDC02u, // Object Format
  MTP_OBJ_PROP_PROTECTION_STATUS          = 0xDC03u, // Protection Status
  MTP_OBJ_PROP_OBJECT_SIZE                = 0xDC04u, // Object Size
  MTP_OBJ_PROP_ASSOCIATION_TYPE           = 0xDC05u, // Association Type
  MTP_OBJ_PROP_ASSOCIATION_DESC           = 0xDC06u, // Association Description
  MTP_OBJ_PROP_OBJECT_FILE_NAME           = 0xDC07u, // Object File Name
  MTP_OBJ_PROP_DATE_CREATED               = 0xDC08u, // Date Created
  MTP_OBJ_PROP_DATE_MODIFIED              = 0xDC09u, // Date Modified
  MTP_OBJ_PROP_KEYWORDS                   = 0xDC0Au, // Keywords
  MTP_OBJ_PROP_PARENT_OBJECT              = 0xDC0Bu, // Parent Object
  MTP_OBJ_PROP_ALLOWED_FOLDER_CONTENTS    = 0xDC0Cu, // Allowed Folder Contents
  MTP_OBJ_PROP_HIDDEN                     = 0xDC0Du, // Hidden
  MTP_OBJ_PROP_SYSTEM_OBJECT              = 0xDC0Eu, // System Object
  // 0xDC0F-0xDC40 is reserved

  MTP_OBJ_PROP_PERSISTENT_UID             = 0xDC41u, // Persistent Unique Object Identifier
  MTP_OBJ_PROP_SYNC_ID                    = 0xDC42u, // SyncID
  MTP_OBJ_PROP_PROPERTY_BAG               = 0xDC43u, // Property Bag
  MTP_OBJ_PROP_NAME                       = 0xDC44u, // Name
  MTP_OBJ_PROP_CREATED_BY                 = 0xDC45u, // Created By
  MTP_OBJ_PROP_ARTIST                     = 0xDC46u, // Artist
  MTP_OBJ_PROP_DATE_AUTHORED              = 0xDC47u, // Date Authored
  MTP_OBJ_PROP_DESCRIPTION                = 0xDC48u, // Description
  MTP_OBJ_PROP_URL_REFERENCE              = 0xDC49u, // URL Reference
  MTP_OBJ_PROP_LANGUAGE_LOCALE            = 0xDC4Au, // Language-Locale
  MTP_OBJ_PROP_COPYRIGHT_INFO             = 0xDC4Bu, // Copyright Information
  MTP_OBJ_PROP_SOURCE                     = 0xDC4Cu, // Source
  MTP_OBJ_PROP_ORIGIN_LOCATION            = 0xDC4Du, // Origin Location
  MTP_OBJ_PROP_DATE_ADDED                 = 0xDC4Eu, // Date Added
  MTP_OBJ_PROP_NON_CONSUMABLE             = 0xDC4Fu, // Non-Consumable
  MTP_OBJ_PROP_CORRUPT_UNPLAYABLE         = 0xDC50u, // Corrupt/Unplayable
  MTP_OBJ_PROP_PRODUCER_SERIAL_NUMBER     = 0xDC51u, // ProducerSerialNumber
  // 0xDC52-0xDC80 is reserved

  MTP_OBJ_PROP_REP_SAMPLE_FORMAT          = 0xDC81u, // Representative Sample Format
  MTP_OBJ_PROP_REP_SAMPLE_SIZE            = 0xDC82u, // Representative Sample Size
  MTP_OBJ_PROP_REP_SAMPLE_HEIGHT          = 0xDC83u, // Representative Sample Height
  MTP_OBJ_PROP_REP_SAMPLE_WIDTH           = 0xDC84u, // Representative Sample Width
  MTP_OBJ_PROP_REP_SAMPLE_DURATION        = 0xDC85u, // Representative Sample Duration
  MTP_OBJ_PROP_REP_SAMPLE_DATA            = 0xDC86u, // Representative Sample Data
  MTP_OBJ_PROP_WIDTH                      = 0xDC87u, // Width
  MTP_OBJ_PROP_HEIGHT                     = 0xDC88u, // Height
  MTP_OBJ_PROP_DURATION                   = 0xDC89u, // Duration
  MTP_OBJ_PROP_RATING                     = 0xDC8Au, // Rating
  MTP_OBJ_PROP_TRACK                      = 0xDC8Bu, // Track
  MTP_OBJ_PROP_GENRE                      = 0xDC8Cu, // Genre
  MTP_OBJ_PROP_CREDITS                    = 0xDC8Du, // Credits
  MTP_OBJ_PROP_LYRICS                     = 0xDC8Eu, // Lyrics
  MTP_OBJ_PROP_SUBSCRIPTION_CONTENT_ID    = 0xDC8Fu, // Subscription Content ID
  MTP_OBJ_PROP_PRODUCED_BY                = 0xDC90u, // Produced By
  MTP_OBJ_PROP_USE_COUNT                  = 0xDC91u, // Use Count
  MTP_OBJ_PROP_SKIP_COUNT                 = 0xDC92u, // Skip Count
  MTP_OBJ_PROP_LAST_ACCESSED              = 0xDC93u, // Last Accessed
  MTP_OBJ_PROP_PARENTAL_RATING            = 0xDC94u, // Parental Rating
  MTP_OBJ_PROP_META_GENRE                 = 0xDC95u, // Meta Genre
  MTP_OBJ_PROP_COMPOSER                   = 0xDC96u, // Composer
  MTP_OBJ_PROP_EFFECTIVE_RATING           = 0xDC97u, // Effective Rating
  MTP_OBJ_PROP_SUBTITLE                   = 0xDC98u, // Subtitle
  MTP_OBJ_PROP_ORIGINAL_RELEASE_DATE      = 0xDC99u, // Original Release Date
  MTP_OBJ_PROP_ALBUM_NAME                 = 0xDC9Au, // Album Name
  MTP_OBJ_PROP_ALBUM_ARTIST               = 0xDC9Bu, // Album Artist
  MTP_OBJ_PROP_MOOD                       = 0xDC9Cu, // Mood
  MTP_OBJ_PROP_DRM_STATUS                 = 0xDC9Du, // DRM Status
  MTP_OBJ_PROP_SUB_DESCRIPTION            = 0xDC9Eu, // Sub Description
  // 0xDC9F-0xDCD0 is reserved

  MTP_OBJ_PROP_IS_CROPPED                 = 0xDCD1u, // Is Cropped
  MTP_OBJ_PROP_IS_COLOUR_CORRECTED        = 0xDCD2u, // Is Colour Corrected
  MTP_OBJ_PROP_IMAGE_BIT_DEPTH            = 0xDCD3u, // Image Bit Depth
  MTP_OBJ_PROP_FNUMBER                    = 0xDCD4u, // Fnumber (aperture ×100)
  MTP_OBJ_PROP_EXPOSURE_TIME              = 0xDCD5u, // Exposure Time (sec ×10,000)
  MTP_OBJ_PROP_EXPOSURE_INDEX             = 0xDCD6u, // Exposure Index (ISO)
  // 0xDCD7-0xDCDF is reserved

  MTP_OBJ_PROP_DISPLAY_NAME               = 0xDCE0u, // Display Name
  MTP_OBJ_PROP_BODY_TEXT                  = 0xDCE1u, // Body Text
  MTP_OBJ_PROP_SUBJECT                    = 0xDCE2u, // Subject
  MTP_OBJ_PROP_PRIORITY                   = 0xDCE3u, // Priority
  // 0xDCE4-0xDCFF is reserved

  MTP_OBJ_PROP_GIVEN_NAME                 = 0xDD00u, // Given Name
  MTP_OBJ_PROP_MIDDLE_NAMES               = 0xDD01u, // Middle Names
  MTP_OBJ_PROP_FAMILY_NAME                = 0xDD02u, // Family Name
  MTP_OBJ_PROP_PREFIX                     = 0xDD03u, // Prefix
  MTP_OBJ_PROP_SUFFIX                     = 0xDD04u, // Suffix
  MTP_OBJ_PROP_PHONETIC_GIVEN_NAME        = 0xDD05u, // Phonetic Given Name
  MTP_OBJ_PROP_PHONETIC_FAMILY_NAME       = 0xDD06u, // Phonetic Family Name
  MTP_OBJ_PROP_EMAIL_PRIMARY              = 0xDD07u, // Email Primary
  MTP_OBJ_PROP_EMAIL_PERSONAL_1           = 0xDD08u, // Email Personal 1
  MTP_OBJ_PROP_EMAIL_PERSONAL_2           = 0xDD09u, // Email Personal 2
  MTP_OBJ_PROP_EMAIL_BUSINESS_1           = 0xDD0Au, // Email Business 1
  MTP_OBJ_PROP_EMAIL_BUSINESS_2           = 0xDD0Bu, // Email Business 2
  MTP_OBJ_PROP_EMAIL_OTHERS               = 0xDD0Cu, // Email Others
  MTP_OBJ_PROP_PHONE_PRIMARY              = 0xDD0Du, // Phone Number Primary
  MTP_OBJ_PROP_PHONE_PERSONAL_1           = 0xDD0Eu, // Phone Number Personal
  MTP_OBJ_PROP_PHONE_PERSONAL_2           = 0xDD0Fu, // Phone Number Personal 2
  MTP_OBJ_PROP_PHONE_BUSINESS_1           = 0xDD10u, // Phone Number Business
  MTP_OBJ_PROP_PHONE_BUSINESS_2           = 0xDD11u, // Phone Number Business 2
  MTP_OBJ_PROP_PHONE_MOBILE_1             = 0xDD12u, // Phone Number Mobile
  MTP_OBJ_PROP_PHONE_MOBILE_2             = 0xDD13u, // Phone Number Mobile 2
  MTP_OBJ_PROP_FAX_PRIMARY                = 0xDD14u, // Fax Number Primary
  MTP_OBJ_PROP_FAX_PERSONAL               = 0xDD15u, // Fax Number Personal
  MTP_OBJ_PROP_FAX_BUSINESS               = 0xDD16u, // Fax Number Business
  MTP_OBJ_PROP_PAGER_NUMBER               = 0xDD17u, // Pager Number
  MTP_OBJ_PROP_PHONE_OTHERS               = 0xDD18u, // Phone Number Others
  MTP_OBJ_PROP_WEB_PRIMARY                = 0xDD19u, // Primary Web Address
  MTP_OBJ_PROP_WEB_PERSONAL               = 0xDD1Au, // Personal Web Address
  MTP_OBJ_PROP_WEB_BUSINESS               = 0xDD1Bu, // Business Web Address
  MTP_OBJ_PROP_IM_ADDRESS_1               = 0xDD1Cu, // Instant Messenger Address
  MTP_OBJ_PROP_IM_ADDRESS_2               = 0xDD1Du, // Instant Messenger Address 2
  MTP_OBJ_PROP_IM_ADDRESS_3               = 0xDD1Eu, // Instant Messenger Address 3
  MTP_OBJ_PROP_ADDR_PERSONAL_FULL         = 0xDD1Fu, // Postal Address Personal Full
  MTP_OBJ_PROP_ADDR_PERSONAL_LINE1        = 0xDD20u, // Postal Address Personal Line 1
  MTP_OBJ_PROP_ADDR_PERSONAL_LINE2        = 0xDD21u, // Postal Address Personal Line 2
  MTP_OBJ_PROP_ADDR_PERSONAL_CITY         = 0xDD22u, // Postal Address Personal City
  MTP_OBJ_PROP_ADDR_PERSONAL_REGION       = 0xDD23u, // Postal Address Personal Region
  MTP_OBJ_PROP_ADDR_PERSONAL_POSTAL_CODE  = 0xDD24u, // Postal Address Personal Postal Code
  MTP_OBJ_PROP_ADDR_PERSONAL_COUNTRY      = 0xDD25u, // Postal Address Personal Country
  MTP_OBJ_PROP_ADDR_BUSINESS_FULL         = 0xDD26u, // Postal Address Business Full
  MTP_OBJ_PROP_ADDR_BUSINESS_LINE1        = 0xDD27u, // Postal Address Business Line 1
  MTP_OBJ_PROP_ADDR_BUSINESS_LINE2        = 0xDD28u, // Postal Address Business Line 2
  MTP_OBJ_PROP_ADDR_BUSINESS_CITY         = 0xDD29u, // Postal Address Business City
  MTP_OBJ_PROP_ADDR_BUSINESS_REGION       = 0xDD2Au, // Postal Address Business Region
  MTP_OBJ_PROP_ADDR_BUSINESS_POSTAL_CODE  = 0xDD2Bu, // Postal Address Business Postal Code
  MTP_OBJ_PROP_ADDR_BUSINESS_COUNTRY      = 0xDD2Cu, // Postal Address Business Country
  MTP_OBJ_PROP_ADDR_OTHER_FULL            = 0xDD2Du, // Postal Address Other Full
  MTP_OBJ_PROP_ADDR_OTHER_LINE1           = 0xDD2Eu, // Postal Address Other Line 1
  MTP_OBJ_PROP_ADDR_OTHER_LINE2           = 0xDD2Fu, // Postal Address Other Line 2
  MTP_OBJ_PROP_ADDR_OTHER_CITY            = 0xDD30u, // Postal Address Other City
  MTP_OBJ_PROP_ADDR_OTHER_REGION          = 0xDD31u, // Postal Address Other Region
  MTP_OBJ_PROP_ADDR_OTHER_POSTAL_CODE     = 0xDD32u, // Postal Address Other Postal Code
  MTP_OBJ_PROP_ADDR_OTHER_COUNTRY         = 0xDD33u, // Postal Address Other Country
  MTP_OBJ_PROP_ORGANIZATION_NAME          = 0xDD34u, // Organization Name
  MTP_OBJ_PROP_PHONETIC_ORG_NAME          = 0xDD35u, // Phonetic Organization Name
  MTP_OBJ_PROP_ROLE                       = 0xDD36u, // Role
  MTP_OBJ_PROP_BIRTHDATE                  = 0xDD37u, // Birthdate
  // 0xDD38-0xDD3F is reserved

  MTP_OBJ_PROP_MESSAGE_TO                 = 0xDD40u, // Message To
  MTP_OBJ_PROP_MESSAGE_CC                 = 0xDD41u, // Message CC
  MTP_OBJ_PROP_MESSAGE_BCC                = 0xDD42u, // Message BCC
  MTP_OBJ_PROP_MESSAGE_READ               = 0xDD43u, // Message Read
  MTP_OBJ_PROP_MESSAGE_RECEIVED_TIME      = 0xDD44u, // Message Received Time
  MTP_OBJ_PROP_MESSAGE_SENDER             = 0xDD45u, // Message Sender
  // 0xDD46-0xDD4F is reserved

  MTP_OBJ_PROP_ACTIVITY_BEGIN_TIME        = 0xDD50u, // Activity Begin Time
  MTP_OBJ_PROP_ACTIVITY_END_TIME          = 0xDD51u, // Activity End Time
  MTP_OBJ_PROP_ACTIVITY_LOCATION          = 0xDD52u, // Activity Location
  // 0xDD53 is reserved
  MTP_OBJ_PROP_ACTIVITY_REQUIRED_ATTENDEES= 0xDD54u, // Activity Required Attendees
  MTP_OBJ_PROP_ACTIVITY_OPTIONAL_ATTENDEES= 0xDD55u, // Activity Optional Attendees
  MTP_OBJ_PROP_ACTIVITY_RESOURCES         = 0xDD56u, // Activity Resources
  MTP_OBJ_PROP_ACTIVITY_ACCEPTED          = 0xDD57u, // Activity Accepted
  MTP_OBJ_PROP_ACTIVITY_TENTATIVE         = 0xDD58u, // Activity Tentative
  MTP_OBJ_PROP_ACTIVITY_DECLINED          = 0xDD59u, // Activity Declined
  MTP_OBJ_PROP_ACTIVITY_REMINDER_TIME     = 0xDD5Au, // Activity Reminder Time
  MTP_OBJ_PROP_ACTIVITY_OWNER             = 0xDD5Bu, // Activity Owner
  MTP_OBJ_PROP_ACTIVITY_STATUS            = 0xDD5Cu, // Activity Status
  MTP_OBJ_PROP_OWNER                      = 0xDD5Du, // Owner
  MTP_OBJ_PROP_EDITOR                     = 0xDD5Eu, // Editor
  MTP_OBJ_PROP_WEBMASTER                  = 0xDD5Fu, // Webmaster

  MTP_OBJ_PROP_URL_SOURCE                 = 0xDD60u, // URL Source
  MTP_OBJ_PROP_URL_DESTINATION            = 0xDD61u, // URL Destination
  MTP_OBJ_PROP_TIME_BOOKMARK              = 0xDD62u, // Time Bookmark
  MTP_OBJ_PROP_OBJECT_BOOKMARK            = 0xDD63u, // Object Bookmark
  MTP_OBJ_PROP_BYTE_BOOKMARK              = 0xDD64u, // Byte Bookmark
  // 0xDD65-0xDD6F is reserved

  MTP_OBJ_PROP_LAST_BUILD_DATE            = 0xDD70u, // Last Build Date
  MTP_OBJ_PROP_TIME_TO_LIVE               = 0xDD71u, // Time to Live (minutes)
  MTP_OBJ_PROP_MEDIA_GUID                 = 0xDD72u, // Media GUID
  // 0xDD73-0xDDFF is reserved

  // media encoding
  MTP_OBJ_PROP_TOTAL_BITRATE              = 0xDE91u, // Total BitRate
  MTP_OBJ_PROP_BITRATE_TYPE               = 0xDE92u, // Bitrate Type
  MTP_OBJ_PROP_SAMPLE_RATE                = 0xDE93u, // Sample Rate
  MTP_OBJ_PROP_NUM_CHANNELS               = 0xDE94u, // Number Of Channels
  MTP_OBJ_PROP_AUDIO_BITDEPTH             = 0xDE95u, // Audio BitDepth
  // 0xDE96 is reserved
  MTP_OBJ_PROP_SCAN_TYPE                  = 0xDE97u, // Scan Type
  // 0xDE98 is reserved
  MTP_OBJ_PROP_AUDIO_WAVE_CODEC           = 0xDE99u, // Audio WAVE Codec
  MTP_OBJ_PROP_AUDIO_BITRATE              = 0xDE9Au, // Audio BitRate
  MTP_OBJ_PROP_VIDEO_FOURCC_CODEC         = 0xDE9Bu, // Video FourCC Codec
  MTP_OBJ_PROP_VIDEO_BITRATE              = 0xDE9Cu, // Video BitRate
  MTP_OBJ_PROP_FRAMES_PER_KSEC            = 0xDE9Du, // Frames Per Thousand Seconds
  MTP_OBJ_PROP_KEYFRAME_DISTANCE          = 0xDE9Eu, // KeyFrame Distance (ms)
  MTP_OBJ_PROP_BUFFER_SIZE                = 0xDE9Fu, // Buffer Size
  MTP_OBJ_PROP_ENCODING_QUALITY           = 0xDEA0u, // Encoding Quality
  MTP_OBJ_PROP_ENCODING_PROFILE           = 0xDEA1u  // Encoding Profile
} mtp_object_properties_t;


// MTP 1.1 Appendeix C: Device Properties
typedef enum {
  MTP_DEV_PROP_UNDEFINED                      = 0x5000u,
  MTP_DEV_PROP_BATTERY_LEVEL                  = 0x5001u,
  MTP_DEV_PROP_FUNCTIONAL_MODE                = 0x5002u,
  MTP_DEV_PROP_IMAGE_SIZE                     = 0x5003u,
  MTP_DEV_PROP_COMPRESSION_SETTING            = 0x5004u,
  MTP_DEV_PROP_WHITE_BALANCE                  = 0x5005u,
  MTP_DEV_PROP_RGB_GAIN                       = 0x5006u,
  MTP_DEV_PROP_F_NUMBER                       = 0x5007u,
  MTP_DEV_PROP_FOCAL_LENGTH                   = 0x5008u,
  MTP_DEV_PROP_FOCUS_DISTANCE                 = 0x5009u,
  MTP_DEV_PROP_FOCUS_MODE                     = 0x500Au,
  MTP_DEV_PROP_EXPOSURE_METERING_MODE         = 0x500Bu,
  MTP_DEV_PROP_FLASH_MODE                     = 0x500Cu,
  MTP_DEV_PROP_EXPOSURE_TIME                  = 0x500Du,
  MTP_DEV_PROP_EXPOSURE_PROGRAM_MODE          = 0x500Eu,
  MTP_DEV_PROP_EXPOSURE_INDEX                 = 0x500Fu,
  MTP_DEV_PROP_EXPOSURE_BIAS_COMPENSATION     = 0x5010u,
  MTP_DEV_PROP_DATE_TIME                      = 0x5011u,
  MTP_DEV_PROP_CAPTURE_DELAY                  = 0x5012u,
  MTP_DEV_PROP_STILL_CAPTURE_MODE             = 0x5013u,
  MTP_DEV_PROP_CONTRAST                       = 0x5014u,
  MTP_DEV_PROP_SHARPNESS                      = 0x5015u,
  MTP_DEV_PROP_DIGITAL_ZOOM                   = 0x5016u,
  MTP_DEV_PROP_EFFECT_MODE                    = 0x5017u,
  MTP_DEV_PROP_BURST_NUMBER                   = 0x5018u,
  MTP_DEV_PROP_BURST_INTERVAL                 = 0x5019u,
  MTP_DEV_PROP_TIMELAPSE_NUMBER               = 0x501Au,
  MTP_DEV_PROP_TIMELAPSE_INTERVAL             = 0x501Bu,
  MTP_DEV_PROP_FOCUS_METERING_MODE            = 0x501Cu,
  MTP_DEV_PROP_UPLOAD_URL                     = 0x501Du,
  MTP_DEV_PROP_ARTIST                         = 0x501Eu,
  MTP_DEV_PROP_COPYRIGHT_INFO                 = 0x501Fu,
  MTP_DEV_PROP_SYNCHRONIZTION_PARTNER         = 0xD401,
  MTP_DEV_PROP_DEVICE_FRIENDLY_NAME           = 0xD402u,
  MTP_DEV_PROP_VOLUME                         = 0xD403u,
  MTP_DEV_PROP_SUPPORTED_FORMATS_ORDERED      = 0xD404u,
  MTP_DEV_PROP_DEVICE_ICON                    = 0xD405u,
  MTP_DEV_PROP_SECTION_INITIATOR_VERSION_INFO = 0xD406u,
  MTP_DEV_PROP_PERCEIVED_DEVICE_TYPE          = 0xD407u,
  MTP_DEV_PROP_PLAYBACK_RATE                  = 0xD410u,
  MTP_DEV_PROP_PLAYBACK_OBJECT                = 0xD411u,
  MTP_DEV_PROP_PLAYBACK_CONTAINER_INDEX       = 0xD412u,
} mtp_event_properties_t;

// MTP 1.1 Appendix D: Operations
typedef enum {
  MTP_OP_UNDEFINED                  = 0x1000u,
  MTP_OP_GET_DEVICE_INFO            = 0x1001u,
  MTP_OP_OPEN_SESSION               = 0x1002u,
  MTP_OP_CLOSE_SESSION              = 0x1003u,
  MTP_OP_GET_STORAGE_IDS            = 0x1004u,
  MTP_OP_GET_STORAGE_INFO           = 0x1005u,
  MTP_OP_GET_NUM_OBJECTS            = 0x1006u,
  MTP_OP_GET_OBJECT_HANDLES         = 0x1007u,
  MTP_OP_GET_OBJECT_INFO            = 0x1008u,
  MTP_OP_GET_OBJECT                 = 0x1009u,
  MTP_OP_GET_THUMB                  = 0x100Au,
  MTP_OP_DELETE_OBJECT              = 0x100Bu,
  MTP_OP_SEND_OBJECT_INFO           = 0x100Cu,
  MTP_OP_SEND_OBJECT                = 0x100Du,
  MTP_OP_INITIATE_CAPTURE           = 0x100Eu,
  MTP_OP_FORMAT_STORE               = 0x100Fu,
  MTP_OP_RESET_DEVICE               = 0x1010u,
  MTP_OP_SELF_TEST                  = 0x1011u,
  MTP_OP_SET_OBJECT_PROTECTION      = 0x1012u,
  MTP_OP_POWER_DOWN                 = 0x1013u,
  MTP_OP_GET_DEVICE_PROP_DESC       = 0x1014u,
  MTP_OP_GET_DEVICE_PROP_VALUE      = 0x1015u,
  MTP_OP_SET_DEVICE_PROP_VALUE      = 0x1016u,
  MTP_OP_RESET_DEVICE_PROP_VALUE    = 0x1017u,
  MTP_OP_TERMINATE_OPEN_CAPTURE     = 0x1018u,
  MTP_OP_MOVE_OBJECT                = 0x1019u,
  MTP_OP_COPY_OBJECT                = 0x101Au,
  MTP_OP_GET_PARTIAL_OBJECT         = 0x101Bu,
  MTP_OP_INITIATE_OPEN_CAPTURE      = 0x101Bu,
  MTP_OP_GET_OBJECT_PROPS_SUPPORTED = 0x9801u,
  MTP_OP_GET_OBJECT_PROP_DESC       = 0x9802u,
  MTP_OP_GET_OBJECT_PROP_VALUE      = 0x9803u,
  MTP_OP_SET_OBJECT_PROP_VALUE      = 0x9804u,
  MTP_OP_GET_OBJECT_PROPLIST        = 0x9805u,
  MTP_OP_GET_OBJECT_PROP_REFERENCES = 0x9810u,

  MTP_OP_GET_SERVICE_IDS            = 0x9301u,
  MTP_OP_GET_SERVICE_INFO           = 0x9302u,
  MTP_OP_GET_SERVICE_CAPABILITIES   = 0x9303u,
  MTP_OP_GET_SERVICE_PROP_DESC      = 0x9304u,

  // Appendix E: Enhanced Operations
  MTP_OP_GET_OBJECT_PROP_LIST         = 0x9805u,
  MTP_OP_SET_OBJECT_PROP_LIST         = 0x9806u,
  MTP_OP_GET_INTERDEPENDENT_PROP_DESC = 0x9807u,
  MTP_OP_SEND_OBJECT_PROP_LIST        = 0x9808u,
} mtp_operation_code_t;

// Appendix F: Responses
typedef enum {
  MTP_RESP_UNDEFINED                           = 0x2000u,
  MTP_RESP_OK                                  = 0x2001u,
  MTP_RESP_GENERAL_ERROR                       = 0x2002u,
  MTP_RESP_SESSION_NOT_OPEN                    = 0x2003u,
  MTP_RESP_INVALID_TRANSACTION_ID              = 0x2004u,
  MTP_RESP_OPERATION_NOT_SUPPORTED             = 0x2005u,
  MTP_RESP_PARAMETER_NOT_SUPPORTED             = 0x2006u,
  MTP_RESP_INCOMPLETE_TRANSFER                 = 0x2007u,
  MTP_RESP_INVALID_STORAGE_ID                  = 0x2008u,
  MTP_RESP_INVALID_OBJECT_HANDLE               = 0x2009u,
  MTP_RESP_DEVICE_PROP_NOT_SUPPORTED           = 0x200Au,
  MTP_RESP_INVALID_OBJECT_FORMAT_CODE          = 0x200Bu,
  MTP_RESP_STORE_FULL                          = 0x200Cu,
  MTP_RESP_OBJECT_WRITE_PROTECTED              = 0x200Du,
  MPT_RESC_STORE_READ_ONLY                     = 0x200Eu,
  MTP_RESP_ACCESS_DENIED                       = 0x200Fu,
  MTP_RESP_NO_THUMBNAIL_PRESENT                = 0x2010u,
  MTP_RESP_SELF_TEST_FAILED                    = 0x2011u,
  MTP_RESP_PARTIAL_DELETION                    = 0x2012u,
  MTP_RESP_STORE_NOT_AVAILABLE                 = 0x2013u,
  MTP_RESP_SPECIFICATION_BY_FORMAT_UNSUPPORTED = 0x2014u,
  MTP_RESP_NO_VALID_OBJECTINFO                 = 0x2015u,
  MTP_RESP_INVALID_CODE_FORMAT                 = 0x2016u,
  MTP_RESP_UNKNOWN_VENDOR_CODE                 = 0x2017u,
  MTP_RESP_CAPTURE_ALREADY_TERMINATED          = 0x2018u,
  MTP_RESP_DEVICE_BUSY                         = 0x2019u,
  MTP_RESP_INVALID_PARENT_OBJECT               = 0x201Au,
  MTP_RESP_INVALID_DEVICE_PROP_FORMAT          = 0x201Bu,
  MTP_RESP_INVALID_DEVICE_PROP_VALUE           = 0x201Cu,
  MTP_RESP_INVALID_PARAMETER                   = 0x201Du,
  MTP_RESP_SESSION_ALREADY_OPEN                = 0x201Eu,
  MTP_RESP_TRANSACTION_CANCELLED               = 0x201Fu,
  MTP_RESP_SPEC_OF_DESTINATION_UNSUPPORTED     = 0x2020u,

  MTP_RESP_INVALID_OBJECT_PROP_CODE            = 0xA801u,
  MTP_RESP_INVALID_OBJECT_PROP_FORMAT          = 0xA802u,
  MTP_RESP_INVALID_OBJECT_PROP_VALUE           = 0xA803u,
  MTP_RESP_INVALID_OBJECT_REFERENCE            = 0xA804u,
  MTP_RESP_GROUP_NOT_SUPPORTED                 = 0xA805u,
  MTP_RESP_INVALID_DATASET                     = 0xA806u,
  MTP_RESP_SPEC_BY_GROUP_UNSUPPORTED           = 0xA807u,
  MTP_RESP_SPEC_BY_DEPTH_UNSUPPORTED           = 0xA808u,
  MTP_RESP_OBJECT_TOO_LARGE                    = 0xA809u,
  MTP_RESP_OBJECT_PROP_NOT_SUPPORTED           = 0xA80Au,
} mtp_response_t;

// Appendix G: Events
typedef enum {
  MTP_EVENT_UNDEFINED                 = 0x4000,
  MTP_EVENT_CANCEL_TRANSACTION        = 0x4001,
  MTP_EVENT_OBJECT_ADDED              = 0x4002,
  MTP_EVENT_OBJECT_REMOVED            = 0x4003,
  MTP_EVENT_STORE_ADDED               = 0x4004,
  MTP_EVENT_STORE_REMOVED             = 0x4005,
  MTP_EVENT_DEVICE_PROP_CHANGED       = 0x4006,
  MTP_EVENT_OBJECT_INFO_CHANGED       = 0x4007,
  MTP_EVENT_DEVICE_INFO_CHANGED       = 0x4008,
  MTP_EVENT_REQUEST_OBJECT_TRANSFER   = 0x4009,
  MTP_EVENT_STORE_FULL                = 0x400Au,
  MTP_EVENT_DEVICE_RESET              = 0x400Bu,
  MTP_EVENT_STORAGE_INFO_CHANGED      = 0x400Cu,
  MTP_EVENT_CAPTURE_COMPLETE          = 0x400Du,
  MTP_EVENT_UNREPORTED_STATUS         = 0x400Eu,
  MTP_EVENT_OBJECT_PROP_CHANGED       = 0xC801u,
  MTP_EVENT_OBJECT_PROP_DESC_CHANGED  = 0xC802u,
  MTP_EVENT_OBJECT_REFERENCES_CHANGED = 0xC803u,
} mtp_event_code_t;

// Datatypes
typedef enum {
  MTP_DATA_TYPE_UNDEFINED = 0x0000u,
  // scalars
  MTP_DATA_TYPE_INT8      = 0x0001u,
  MTP_DATA_TYPE_UINT8     = 0x0002u,
  MTP_DATA_TYPE_INT16     = 0x0003u,
  MTP_DATA_TYPE_UINT16    = 0x0004u,
  MTP_DATA_TYPE_INT32     = 0x0005u,
  MTP_DATA_TYPE_UINT32    = 0x0006u,
  MTP_DATA_TYPE_INT64     = 0x0007u,
  MTP_DATA_TYPE_UINT64    = 0x0008u,
  MTP_DATA_TYPE_INT128    = 0x0009u,
  MTP_DATA_TYPE_UINT128   = 0x000Au,
  // array
  MTP_DATA_TYPE_AINT8     = 0x4001u,
  MTP_DATA_TYPE_AUINT8    = 0x4002u,
  MTP_DATA_TYPE_AINT16    = 0x4003u,
  MTP_DATA_TYPE_AUINT16   = 0x4004u,
  MTP_DATA_TYPE_AINT32    = 0x4005u,
  MTP_DATA_TYPE_AUINT32   = 0x4006u,
  MTP_DATA_TYPE_AINT64    = 0x4007u,
  MTP_DATA_TYPE_AUINT64   = 0x4008u,
  MTP_DATA_TYPE_AINT128   = 0x4009u,
  MTP_DATA_TYPE_AUINT128  = 0x400Au,
  MTP_DATA_TYPE_STR       = 0xFFFFu,
} mtp_data_type_t;

// Get/Set
typedef enum {
  MTP_MODE_GET     = 0x00u,
  MTP_MODE_GET_SET = 0x01u,
} mtp_mode_get_set_t;

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
  MTP_ASSOCIATION_ALBUM                = 0x0002u,
  MTP_ASSOCIATION_TIME_SEQUENCE        = 0x0003u,
  MTP_ASSOCIATION_HORIZONTAL_PANORAMIC = 0x0004u,
  MTP_ASSOCIATION_VERTICAL_PANORAMIC   = 0x0005u,
  MTP_ASSOCIATION_2D_PANORAMIC         = 0x0006u,
  MTP_ASSOCIATION_ANCILLARY_DATA       = 0x0007u,
} mtp_association_t;

//--------------------------------------------------------------------+
// Data structures
//--------------------------------------------------------------------+
typedef struct TU_ATTR_PACKED {
  uint32_t len;
  uint16_t type;
  uint16_t code;
  uint32_t transaction_id;
} mtp_container_header_t;
TU_VERIFY_STATIC(sizeof(mtp_container_header_t) == 12, "size is not correct");

typedef struct TU_ATTR_PACKED {
  mtp_container_header_t header;
  uint32_t params[5];
} mtp_container_command_t;
TU_VERIFY_STATIC(sizeof(mtp_container_command_t) == 32, "size is not correct");

// PTP/MTP Generic container
typedef struct TU_ATTR_PACKED {
  mtp_container_header_t header;
  uint8_t payload[(CFG_TUD_MTP_EP_BUFSIZE - sizeof(mtp_container_header_t))];
} mtp_generic_container_t;

typedef struct {
  mtp_container_header_t* header;
  union {
    uint8_t*  payload;
    uint16_t* payload16;
    uint32_t* payload32;
  };
  uint32_t payload_bytes; // available bytes for read/write
} mtp_container_info_t;

typedef struct TU_ATTR_PACKED {
  uint16_t code;
  uint32_t session_id;
  uint32_t transaction_id;
  uint32_t params[3];
} mtp_event_t;
TU_VERIFY_STATIC(sizeof(mtp_event_t) == 22, "size is not correct");

#define mtp_string_t(_nchars) \
  struct TU_ATTR_PACKED { \
    uint8_t count; /* in characters including null */ \
    uint16_t utf16[_nchars]; \
  }

#define mtp_array_t(_type, _count) \
   struct TU_ATTR_PACKED { \
     uint32_t count; \
     _type arr[_count];\
   }

#define mtp_aint8_t(_count)  mtp_array_t(int8_t, _count)
#define mtp_auint16_t(_count) mtp_array_t(uint16_t, _count)
#define mtp_auint32_t(_count) mtp_array_t(uint32_t, _count)
#define mtp_auint64_t(_count) mtp_array_t(uint64_t, _count)

#define MTP_STORAGE_INFO_STRUCT(_storage_desc_chars, _volume_id_chars) \
  struct TU_ATTR_PACKED { \
    uint16_t storage_type; \
    uint16_t filesystem_type; \
    uint16_t access_capability; \
    uint64_t max_capacity_in_bytes; \
    uint64_t free_space_in_bytes; \
    uint32_t free_space_in_objects; \
    mtp_string_t(_storage_desc_chars) storage_description; \
    mtp_string_t(_volume_id_chars) volume_identifier; \
  }

// Object Info Dataset without dynamic string: filename, date_created, date_modified, keywords
typedef struct TU_ATTR_PACKED {
  uint32_t storage_id;
  uint16_t object_format;
  uint16_t protection_status;
  uint32_t object_compressed_size;
  uint16_t thumb_format;
  uint32_t thumb_compressed_size;
  uint32_t thumb_pix_width;
  uint32_t thumb_pix_height;
  uint32_t image_pix_width;
  uint32_t image_pix_height;
  uint32_t image_bit_depth;
  uint32_t parent_object; // 0: root
  uint16_t association_type;
  uint32_t association_desc;
  uint32_t sequence_number;
  // mtp_string_t() filename
  // mtp_string_t() date_created
  // mtp_string_t() date_modified
  // mtp_string_t() keywords
} mtp_object_info_header_t;

// Device property desc up to get/set
typedef struct TU_ATTR_PACKED {
  uint16_t device_property_code;
  uint16_t datatype;
  uint8_t  get_set;
} mtp_device_prop_desc_header_t;

// The following fields will be dynamically added to the struct at runtime:
// - wstring factory_def_value;
// - wstring current_value_len;
// - uint8_t form_flag;

// no form
#define MTP_DEVICE_PROPERTIES_STRUCT(_type) \
   struct TU_ATTR_PACKED { \
     uint16_t device_property_code; \
     uint16_t datatype; \
     uint8_t get_set; \
     _type factory_default; \
     _type current_value; \
     uint8_t form_flag; /* 0: none, 1: range, 2: enum */ \
   };

typedef struct TU_ATTR_PACKED {
  uint16_t code;
  uint32_t transaction_id;
} mtp_request_reset_cancel_data_t;
TU_VERIFY_STATIC(sizeof(mtp_request_reset_cancel_data_t) == 6, "size is not correct");

//--------------------------------------------------------------------+
// Container helper function
// return number of bytes added
//--------------------------------------------------------------------+

// return payload buffer for next write
TU_ATTR_ALWAYS_INLINE static inline uint8_t* mtp_container_payload_ptr(mtp_container_info_t* p_container) {
  // only 1st packet include header
  uint32_t pos = p_container->header->len - sizeof(mtp_container_header_t);
  while (pos > CFG_TUD_MTP_EP_BUFSIZE) {
    pos -= CFG_TUD_MTP_EP_BUFSIZE;
  }
  return p_container->payload + pos;
}

// only add_raw does partial copy
TU_ATTR_ALWAYS_INLINE static inline uint32_t mtp_container_add_raw(mtp_container_info_t* p_container, const void* data, uint32_t len) {
  uint8_t* buf = mtp_container_payload_ptr(p_container);
  const uint32_t added_len = tu_min32(len, CFG_TUD_MTP_EP_BUFSIZE - p_container->header->len);
  if (added_len > 0) {
    memcpy(buf, data, added_len);
  }
  p_container->header->len += len; // always increase len, even partial copy
  return added_len;
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t mtp_container_add_array(mtp_container_info_t* p_container, uint8_t scalar_size, uint32_t count, const void* data) {
  const uint32_t added_len = 4 + count * scalar_size;
  TU_ASSERT(p_container->header->len + added_len < CFG_TUD_MTP_EP_BUFSIZE, 0);
  uint8_t* buf = p_container->payload + p_container->header->len - sizeof(mtp_container_header_t);

  tu_unaligned_write32(buf, count);
  p_container->header->len += 4;
  buf += 4;

  memcpy(buf, data, count * scalar_size);
  p_container->header->len += count * scalar_size;

  return added_len;
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t mtp_container_add_string(mtp_container_info_t* p_container, uint16_t* utf16) {
  uint8_t count = 0;
  while (utf16[count]) {
    count++;
  }
  const uint32_t added_len = 1u + 2u * count;
  TU_ASSERT(p_container->header->len + added_len < CFG_TUD_MTP_EP_BUFSIZE, 0);
  uint8_t* buf = p_container->payload + p_container->header->len - sizeof(mtp_container_header_t);

  *buf++ = count;
  p_container->header->len++;

  memcpy(buf, utf16, 2 * count);
  p_container->header->len += 2 * count;

  return added_len;
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t mtp_container_add_cstring(mtp_container_info_t* p_container, const char* str) {
  const uint8_t len = (uint8_t) (strlen(str) + 1); // include null
  TU_ASSERT(p_container->header->len + 1 + 2 * len < CFG_TUD_MTP_EP_BUFSIZE, 0);
  uint8_t* buf = p_container->payload + p_container->header->len - sizeof(mtp_container_header_t);

  if (len == 1) {
    // empty string (null only): single zero byte
    *buf = 0;
    p_container->header->len++;
    return 1;
  } else {
    *buf++ = len;
    p_container->header->len++;

    for (uint8_t i = 0; i < len; i++) {
      buf[0] = str[i];
      buf[1] = 0;
      buf += 2;
      p_container->header->len += 2;
    }
    return 1u + 2u * len;
  }
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t mtp_container_add_uint8(mtp_container_info_t* p_container, uint8_t data) {
  return mtp_container_add_raw(p_container, &data, 1);
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t mtp_container_add_uint16(mtp_container_info_t* p_container, uint16_t data) {
  return mtp_container_add_raw(p_container, &data, 2);
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t mtp_container_add_uint32(mtp_container_info_t* p_container, uint32_t data) {
  return mtp_container_add_raw(p_container, &data, 4);
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t mtp_container_add_uint64(mtp_container_info_t* p_container, uint64_t data) {
  return mtp_container_add_raw(p_container, &data, 8);
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t mtp_container_add_uint128(mtp_container_info_t* p_container, const void* data) {
  return mtp_container_add_raw(p_container, data, 16);
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t mtp_container_add_auint8(mtp_container_info_t* p_container, uint32_t count, const uint8_t* data) {
  return mtp_container_add_array(p_container, sizeof(uint8_t), count, data);
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t mtp_container_add_auint16(mtp_container_info_t* p_container, uint32_t count, const uint16_t* data) {
  return mtp_container_add_array(p_container, sizeof(uint16_t), count, data);
}

TU_ATTR_ALWAYS_INLINE static inline uint32_t mtp_container_add_auint32(mtp_container_info_t* p_container, uint32_t count, const uint32_t* data) {
  return mtp_container_add_array(p_container, sizeof(uint32_t), count, data);
}

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+
TU_ATTR_ALWAYS_INLINE static inline uint32_t mtp_container_get_string(uint8_t* buf, uint16_t utf16[]) {
  uint8_t nchars = *buf++;
  memcpy(utf16, buf, 2 * nchars);
  return 1u + 2u * nchars;
}

#ifdef __cplusplus
 }
#endif

#endif
#endif
