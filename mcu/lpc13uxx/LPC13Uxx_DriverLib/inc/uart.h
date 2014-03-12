/****************************************************************************
 *   $Id:: uart.h 6172 2011-01-13 18:22:51Z usb00423                        $
 *   Project: NXP LPC13Uxx software example
 *
 *   Description:
 *     This file contains definition and prototype for UART configuration.
 *
 ****************************************************************************
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * products. This software is supplied "AS IS" without any warranties.
 * NXP Semiconductors assumes no responsibility or liability for the
 * use of the software, conveys no license or title under any patent,
 * copyright, or mask work right to the product. NXP Semiconductors
 * reserves the right to make changes in the software without
 * notification. NXP Semiconductors also make no representation or
 * warranty that such application will be suitable for the specified
 * use without further testing or modification.
****************************************************************************/
#ifndef __UART_H
#define __UART_H

#define AUTOBAUD_ENABLE 0
#define FDR_CALIBRATION 0
#define RS485_ENABLED   0
#define TX_INTERRUPT    0		/* 0 if TX uses polling, 1 interrupt driven. */
#define MODEM_TEST      0

#define IER_RBR         (0x01<<0)
#define IER_THRE        (0x01<<1)
#define IER_RLS         (0x01<<2)
#define IER_ABEO        (0x01<<8)
#define IER_ABTO        (0x01<<9)

#define IIR_PEND        0x01
#define IIR_RLS         0x03
#define IIR_RDA         0x02
#define IIR_CTI         0x06
#define IIR_THRE        0x01
#define IIR_ABEO        (0x01<<8)
#define IIR_ABTO        (0x01<<9)

#define LSR_RDR         (0x01<<0)
#define LSR_OE          (0x01<<1)
#define LSR_PE          (0x01<<2)
#define LSR_FE          (0x01<<3)
#define LSR_BI          (0x01<<4)
#define LSR_THRE        (0x01<<5)
#define LSR_TEMT        (0x01<<6)
#define LSR_RXFE        (0x01<<7)

#define BUFSIZE         0x40

/* RS485 mode definition. */
#define RS485_NMMEN		(0x1<<0)
#define RS485_RXDIS		(0x1<<1)
#define RS485_AADEN		(0x1<<2)
#define RS485_SEL             (0x1<<3)
#define RS485_DCTRL		(0x1<<4)
#define RS485_OINV		(0x1<<5)

void ModemInit( void );
void UARTInit(uint32_t Baudrate);
void USART_IRQHandler(void);
void UARTSend(uint8_t *BufferPtr, uint32_t Length);
void print_string( uint8_t *str_ptr );
uint8_t get_key( void );

#endif /* end __UART_H */
/*****************************************************************************
**                            End Of File
******************************************************************************/
