/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Altera Corporation
 *
 * SPDX-License-Identifier: MIT-0
 *
 * Header file for xHCI implementation
 */

#ifndef __USB3_MAIN_H__
#define __USB3_MAIN_H__

/**
 * @file usb_main.h
 * @brief USB HAL driver header file
 */

#include <stdio.h>

/**
 * @defgroup usb USB
 * @ingroup drivers
 * @{
 */

/**
 * @defgroup usb_fns Functions
 * @ingroup usb
 * USB3 HAL APIs
 */

/**
 * @addtogroup usb_fns
 * @{
 */

/**
 * @brief  Wait until the specified timeout, for the usb device to be enumerated
 *         and mounted properly. If the device is not mounted within the time, timeout
 *         occurs.
 *
 * @param[in] timeout Timeout in seconds
 * @return
 * - 0 if device is successfully mounted before timeout.
 * - -ETIMEDOUT if device fails to mount within the timeout period.
 */

int usb_wait_to_mount(int timeout);

/**
 * @brief  This api should be invoked as a thread to initialize the tinyusb stack. This thread
 *         initializes the USB2.0 and USB3.1 driver followed by the enumeration of the attached device.
 */
void usb_task(void *arg);
/**
 * @}
 */
/* end of group usb_fns */

/**
 * @}
 */
/* end of group usb */

#endif /* __USB3_MAIN_H__ */
