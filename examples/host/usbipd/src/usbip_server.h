/*
 * USB/IP-over-TCP server: OP_REQ_DEVLIST, OP_REQ_IMPORT, full URB
 * streaming (CMD_SUBMIT / CMD_UNLINK / RET_SUBMIT / RET_UNLINK).
 *
 * Single-connection: one Linux usbip client at a time. A second
 * accept while one is attached gets dropped on accept.
 */
#ifndef USBIP_SERVER_H_
#define USBIP_SERVER_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void usbip_server_init(void);

// Called from the TinyUSB host mount/umount callbacks to keep the
// server's view of attached devices in sync. tuh_*() is not safe to
// call from the lwIP TCP recv callback (the bus task drives it), so
// we cache the device descriptor here.
void usbip_server_on_mount(uint8_t daddr);
void usbip_server_on_umount(uint8_t daddr);

#ifdef __cplusplus
}
#endif

#endif /* USBIP_SERVER_H_ */
