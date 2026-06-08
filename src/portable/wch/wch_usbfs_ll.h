/*
 * wch_usbfs_ll.h — low-level WCH USBFS host transaction layer (extracted from
 * WCH's HOST_KM reference `ch32v30x_usbfs_host.c`). Provides the proven
 * transaction primitives (USBFSH_Transact / USBFSH_CtrlTransfer / GetEndpData /
 * SendEndpData) + root-port reset/enable, with the app/hid/hub framework removed.
 *
 * A TinyUSB HCD (hcd_ch32_usbfs.c) wraps these so TinyUSB's whole host stack
 * runs on top of WCH's silicon-correct transaction layer.
 *
 * Transaction code (c) WCH; this thin config header is MIT.
 */
#ifndef WCH_USBFS_LL_H
#define WCH_USBFS_LL_H

#include <stdint.h>
#include <stdio.h>          // printf (BSP retargets to USART1)
#include <string.h>         // memcpy
// MCU-portable: ch32_usbfs_reg.h selects the right SDK headers by CFG_TUSB_MCU
// (ch32v20x.h/ch32v30x.h + the matching _usb.h) — USBFSH register block,
// FunctionalState, RCC_*, SystemCoreClock, USB_PID_*, USB_SETUP_REQ, USBFS_* bits.
#include "ch32_usbfs_reg.h"

// Busy-loop delays (defined in hcd_ch32_usbfs.c) — WCH's debug.h Delay_*
// uses SysTick, which the TinyUSB BSP already owns; use a NOP busy-loop instead.
void Delay_Us(uint32_t n);
void Delay_Ms(uint32_t n);

#ifdef __cplusplus
extern "C" {
#endif

/* ---- framework constants that lived in usb_host_config.h ----------------- */
#ifndef USBFS_MAX_PACKET_SIZE
  #define USBFS_MAX_PACKET_SIZE       64
#endif

#define ROOT_DEV_DISCONNECT           0
#define ROOT_DEV_CONNECTED            1
#define ROOT_DEV_FAILED               2
#define ROOT_DEV_SUCCESS              3

// USB_LOW_SPEED / USB_FULL_SPEED come from ch32_usbfs_reg.h

#define ERR_SUCCESS                   0x00
#define ERR_USB_CONNECT               0x15
#define ERR_USB_DISCON                0x16
#define ERR_USB_BUF_OVER              0x17
#define ERR_USB_TRANSFER              0x20
#define ERR_USB_UNKNOWN               0xFE

#define DEF_BUS_RESET_TIME            11          // USB bus reset time (ms)
#define DEF_WAIT_USB_TRANSFER_CNT     1000        // per-transaction completion spin
#define DEF_CTRL_TRANS_TIMEOVER_CNT   (200000/20) // control transfer NAK timeout

/* ---- buffers + SETUP request (defined in wch_usbfs_ll.c) ----------------- */
extern uint8_t  USBFS_RX_Buf[ USBFS_MAX_PACKET_SIZE ];
extern uint8_t  USBFS_TX_Buf[ USBFS_MAX_PACKET_SIZE ];
extern PUSB_SETUP_REQ pUSBFS_SetupRequest;       // alias over USBFS_TX_Buf

/* ---- transaction + root-port primitives ---------------------------------- */
extern void    USBFS_RCC_Init( void );
extern void    USBFS_Host_Init( FunctionalState sta );
extern uint8_t USBFSH_CheckRootHubPortStatus( uint8_t dev_sta );
extern uint8_t USBFSH_CheckRootHubPortEnable( void );
extern uint8_t USBFSH_CheckRootHubPortSpeed( void );
extern void    USBFSH_SetSelfAddr( uint8_t addr );
extern void    USBFSH_SetSelfSpeed( uint8_t speed );
extern void    USBFSH_ResetRootHubPort( uint8_t mode );
extern uint8_t USBFSH_EnableRootHubPort( uint8_t *pspeed );
extern uint8_t USBFSH_Transact( uint8_t endp_pid, uint8_t endp_tog, uint32_t timeout );
extern uint8_t USBFSH_CtrlTransfer( uint8_t ep0_size, uint8_t *pbuf, uint16_t *plen );
extern uint8_t USBFSH_GetEndpData( uint8_t endp_num, uint8_t *pendp_tog, uint8_t *pbuf, uint16_t *plen );
extern uint8_t USBFSH_SendEndpData( uint8_t endp_num, uint8_t *pendp_tog, uint8_t *pbuf, uint16_t len );

#ifdef __cplusplus
}
#endif

#endif // WCH_USBFS_LL_H
