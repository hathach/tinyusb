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

#ifndef _TUSB_MTP_DEVICE_STORAGE_H_
#define _TUSB_MTP_DEVICE_STORAGE_H_

#include "common/tusb_common.h"
#include "mtp.h"

#if (CFG_TUD_ENABLED && CFG_TUD_MTP)

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------+
// Storage Application Callbacks
//--------------------------------------------------------------------+

/*
 * The entire MTP functionality is based on object handles, as described in MTP Specs v. 1.1 under 3.4.1.
 * The major weakness of the protocol is that those handles are supposed to be unique within a session
 * and for every enumerated object. There is no specified lifetime limit or way to control the expiration:
 * once given, they have to persist for an indefinite time and number of iterations.
 * If the filesystem does not provide unique persistent object handle for every entry, the best approach
 * would be to keep a full association between generated handles and full file paths. The suggested
 * approach with memory constrained devices is to keep a hard ID associated with each file or a volatile
 * ID generated on the fly and invalidated on each operation that may rearrange the order.
 * In order to invalidate existing IDS, it might be necessary to invalidate the whole session from
 * the device side.
 * Depending on the application, the handle could be also be the file name or a tag (i.e. host-only file access)
 */


// Initialize MTP storage subsystem
//
// The function shall check if the session is already opened and, in case, set session_id to the
// ID of the current session.

// Close an open session
mtp_response_t tud_mtp_storage_close_session(uint32_t session_id);

// Format the specified storage
mtp_response_t tud_mtp_storage_format(uint32_t storage_id);

// Traverse the given parent object handle and return a child handle for each call
//
// If the parent object has not been opened (or closed before) the function returns the first handle.
// When next_child_handle is 0 all the handles have been listed.
// TODO: traverse by ObjectFormatCode and ObjectHandle association. For now they are unsupported.
mtp_response_t tud_mtp_storage_association_get_object_handle(uint32_t session_handle, uint32_t parent_object_handle, uint32_t *next_child_handle);

// Called with the creation of a new object is requested.
// The handle of the new object shall be returned in new_object_handle.
// The structure info contains the information to be used for file creation, as passted by the host.
// Note that the variable information (e.g. wstring file name, dates and tags shall be retrieved by using the library functions)
mtp_response_t tud_mtp_storage_object_write_info(uint32_t storage_id, uint32_t parent_object, uint32_t *new_object_handle, const mtp_object_info_t *info);

// Get object information related to a given object handle
//
// The structure info shall be filled according to MTP specifications. Note that
// in addition to filling the fixed mtp_object_info_t structure, the caller must add the following fields via
// library calls
// - Filename (string, use tud_mtp_gct_append_wstring)
// - Date created (string, use tud_gct_append_date or empty string)
// - Date modified (string, use tud_gct_append_date or empty string)
// - Keywords (string containing list of kw, separated by space, use tud_mtp_gct_append_wstring)
// Note that the variable information (e.g. wstring file name, dates and tags shall be written by using the library functions)
mtp_response_t tud_mtp_storage_object_read_info(uint32_t object_handle, mtp_object_info_t *info);

// Get the object size.
//
// The object may be already open when this function is called.
// The implementation shall not assume a specific call order between this function and tud_mtp_storage_object_read.
// The function may leave the file open.
mtp_response_t tud_mtp_storage_object_size(uint32_t object_handle, uint32_t *size);

// Write object data
//
// The function shall open the object for writing if not already open.
// The binary data shall be written to the file in full before this function is returned.
mtp_response_t tud_mtp_storage_object_write(uint32_t object_handle, const uint8_t *buffer, uint32_t buffer_size);

// Get object data
//
// The function shall open the object for reading if not already open.
// The amount of data returned shall be the given size parameter.
// read_count shall contain the effective number of bytes written. Iteration is terminated when read_count < buffer_size.
mtp_response_t tud_mtp_storage_object_read(uint32_t object_handle, void *buffer, uint32_t buffer_size, uint32_t *read_count);

// Move an object to a new parent
mtp_response_t tud_mtp_storage_object_move(uint32_t object_handle, uint32_t new_parent_object_handle);

// Delete the specified object
mtp_response_t tud_mtp_storage_object_delete(uint32_t object_handle);

// Issued when IO operation has been terminated (e.g. read, traverse), close open file handles
void tud_mtp_storage_object_done(void);

// Cancel any pending operation. Current operation shall be discarded.
void tud_mtp_storage_cancel(void);

// Restore the operation out of reset. Cancel any pending operation and close the session.
void tud_mtp_storage_reset(void);

#ifdef __cplusplus
 }
#endif

#endif /* CFG_TUD_ENABLED && CFG_TUD_MTP */

#endif /* _TUSB_MTP_DEVICE_STORAGE_H_ */
