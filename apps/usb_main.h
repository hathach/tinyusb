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
 * @brief USB3 HAL driver header file
 */

#include <stdio.h>

/**
 * @defgroup usb3 USB3
 * @ingroup drivers
 * @{
 */

/**
 * @defgroup usb3_fns Functions
 * @ingroup usb3
 * USB3 HAL APIs
 */

/**
 * @addtogroup usb3_fns
 * @{
 */

/**
 * @brief  Wait until the specified timeout, for the usb3 device to be enumerated
 *         and mounted properly. If the device is not mounted within the time, timeout
 *         occurs.
 *
 * @param[in] timeout Timeout in seconds
 * @return
 * - 0 if device is successfully mounted before timeout.
 * - -ETIMEDOUT if device fails to mount within the timeout period.
 */

int usb3_wait_to_mount(int timeout);

/**
 * @brief  This api should be invoked as a thread to initialize the tinyusb stack. This thread
 *         initializes the usb3 driver followed by the enumeration of the attached device.
 */
void usb3_task(void *arg);

/**
 * @}
 */
/* end of group usb3_fns */

/**
 * @}
 */
/* end of group usb3 */

#endif /* __USB3_MAIN_H__ */

