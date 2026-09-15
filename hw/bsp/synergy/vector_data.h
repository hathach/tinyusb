/* vector numbers are configurable/dynamic, hence this, it will be used inside the port */
#ifndef VECTOR_DATA_H
#define VECTOR_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(BSP_MCU_GROUP_S5D9) || defined(BSP_MCU_GROUP_S7G2)
#define BOARD_HAS_USB_HIGHSPEED
#endif

/* ISR prototypes */
void usbfs_interrupt_handler(void);
void usbfs_resume_handler(void);

#ifdef BOARD_HAS_USB_HIGHSPEED
void usbhs_interrupt_handler(void);
#endif

/* Vector table allocations */
#define USBFS_INT_IRQn             0
#define USBFS_RESUME_IRQn          1

#define USBHS_USB_INT_RESUME_IRQn  2 /* USBHS USB INT RESUME (USBHS interrupt) */

#ifdef __cplusplus
}
#endif

#endif
