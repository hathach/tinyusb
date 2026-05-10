/* USB/IP wire protocol structures and constants.
 *
 * Layout matches the Linux kernel USB/IP wire format documented at
 * https://docs.kernel.org/usb/usbip_protocol.html.
 *
 * Wire fields are big-endian on the wire; consumers byte-swap on read
 * and write. The packed structs reflect that encoding directly so that
 * a byte-buffer cast lines up. The static_asserts at the bottom of
 * this file pin sizes against the kernel layout - if any of them fire
 * the parser is reading the wrong bytes.
 */

#ifndef MPY_HITL_USBIP_PROTOCOL_H
#define MPY_HITL_USBIP_PROTOCOL_H

#include <stdint.h>

#define USBIP_TCP_PORT 3240
#define USBIP_VERSION 0x0111

#define USBIP_OP_REQ_IMPORT 0x8003
#define USBIP_OP_REP_IMPORT 0x0003
#define USBIP_OP_REQ_DEVLIST 0x8005
#define USBIP_OP_REP_DEVLIST 0x0005

#define USBIP_CMD_SUBMIT 0x00000001u
#define USBIP_CMD_UNLINK 0x00000002u
#define USBIP_RET_SUBMIT 0x00000003u
#define USBIP_RET_UNLINK 0x00000004u

#define USBIP_DIR_OUT 0u
#define USBIP_DIR_IN 1u

/* Linux usbip kernel driver tags non-isochronous URBs with
 * number_of_packets = 0xFFFFFFFF. Some clients send 0 instead.
 * Both must be treated as non-iso. */
#define USBIP_NON_ISO_PACKETS 0xFFFFFFFFu

#define USBIP_MAX_INTERFACES 8u

#define USBIP_BUSID_SIZE 32u
#define USBIP_PATH_SIZE  256u

typedef struct __attribute__((packed)) {
    uint16_t version;
    uint16_t code;
    uint32_t status;
} usbip_op_common_t;

typedef struct __attribute__((packed)) {
    uint32_t command;
    uint32_t seqnum;
    uint32_t devid;
    uint32_t direction;
    uint32_t ep;
} usbip_header_basic_t;

typedef struct __attribute__((packed)) {
    uint32_t transfer_flags;
    int32_t  transfer_buffer_length;
    int32_t  start_frame;
    int32_t  number_of_packets;
    int32_t  interval;
    uint8_t  setup[8];
} usbip_cmd_submit_t;

typedef struct __attribute__((packed)) {
    int32_t  status;
    uint32_t actual_length;
    int32_t  start_frame;
    int32_t  number_of_packets;
    int32_t  error_count;
    uint64_t padding;
} usbip_ret_submit_t;

typedef struct __attribute__((packed)) {
    uint32_t unlink_seqnum;
    uint32_t padding[6];
} usbip_cmd_unlink_t;

typedef struct __attribute__((packed)) {
    int32_t status;
    uint8_t padding[24];
} usbip_ret_unlink_t;

typedef struct __attribute__((packed)) {
    usbip_header_basic_t base;
    union {
        usbip_cmd_submit_t cmd_submit;
        usbip_ret_submit_t ret_submit;
        usbip_cmd_unlink_t cmd_unlink;
        usbip_ret_unlink_t ret_unlink;
    } u;
} usbip_header_t;

typedef struct __attribute__((packed)) {
    char     path[USBIP_PATH_SIZE];
    char     busid[USBIP_BUSID_SIZE];
    uint32_t busnum;
    uint32_t devnum;
    uint32_t speed;
    uint16_t id_vendor;
    uint16_t id_product;
    uint16_t bcd_device;
    uint8_t  device_class;
    uint8_t  device_subclass;
    uint8_t  device_protocol;
    uint8_t  configuration_value;
    uint8_t  num_configurations;
    uint8_t  num_interfaces;
} usbip_device_desc_t;

typedef struct __attribute__((packed)) {
    uint8_t interface_class;
    uint8_t interface_subclass;
    uint8_t interface_protocol;
    uint8_t padding;
} usbip_interface_desc_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Compile-time guards: structures must match the kernel layout. */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(usbip_header_t) == 48, "USB/IP URB header must be 48 bytes");
_Static_assert(sizeof(usbip_op_common_t) == 8, "USB/IP op_common must be 8 bytes");
_Static_assert(sizeof(usbip_device_desc_t) == 0x138,
               "USB/IP device descriptor must be 0x138 bytes");
_Static_assert(sizeof(usbip_interface_desc_t) == 4, "interface descriptor must be 4 bytes");
#endif

#ifdef __cplusplus
}
#endif

#endif /* MPY_HITL_USBIP_PROTOCOL_H */
