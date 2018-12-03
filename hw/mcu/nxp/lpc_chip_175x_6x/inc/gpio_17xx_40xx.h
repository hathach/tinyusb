/*
 * @brief LPC17xx/40xx GPIO driver
 *
 * @note
 * Copyright(C) NXP Semiconductors, 2014
 * All rights reserved.
 *
 * @par
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * LPC products.  This software is supplied "AS IS" without any warranties of
 * any kind, and NXP Semiconductors and its licensor disclaim any and
 * all warranties, express or implied, including all implied warranties of
 * merchantability, fitness for a particular purpose and non-infringement of
 * intellectual property rights.  NXP Semiconductors assumes no responsibility
 * or liability for the use of the software, conveys no license or rights under any
 * patent, copyright, mask work right, or any other intellectual property rights in
 * or to any products. NXP Semiconductors reserves the right to make changes
 * in the software without notification. NXP Semiconductors also makes no
 * representation or warranty that such application will be suitable for the
 * specified use without further testing or modification.
 *
 * @par
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, under NXP Semiconductors' and its
 * licensor's relevant copyrights in the software, without fee, provided that it
 * is used in conjunction with NXP Semiconductors microcontrollers.  This
 * copyright, permission, and disclaimer notice must appear in all copies of
 * this code.
 */

#ifndef __GPIO_17XX_40XX_H_
#define __GPIO_17XX_40XX_H_

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup GPIO_17XX_40XX CHIP: LPC17xx/40xx GPIO driver
 * @ingroup CHIP_17XX_40XX_Drivers
 * @{
 */

#define GPIO_PORT_BITS 32

/**
 * @brief GPIO port  (GPIO_PORT) for LPC175x_6x, LPC177x_8x and LPC407x_8x
 */

typedef struct {				/* GPIO_PORT Structure */
	__IO uint32_t DIR;			/*!< Offset 0x0000: GPIO Port Direction control register */
	uint32_t RESERVED0[3];
	__IO uint32_t MASK;			/*!< Offset 0x0010: GPIO Mask register */
	__IO uint32_t PIN;			/*!< Offset 0x0014: Pin value register using FIOMASK */
	__IO uint32_t SET;			/*!< Offset 0x0018: Output Set register using FIOMASK */
	__O  uint32_t CLR;			/*!< Offset 0x001C: Output Clear register using FIOMASK */
} LPC_GPIO_T;

/**
 * @brief	Initialize GPIO block
 * @param	pGPIO	: The base of GPIO peripheral on the chip
 * @return	Nothing
 */
STATIC INLINE void Chip_GPIO_Init(LPC_GPIO_T *pGPIO)
{
	Chip_Clock_EnablePeriphClock(SYSCTL_CLOCK_GPIO);
}

/**
 * @brief	De-Initialize GPIO block
 * @param	pGPIO	: The base of GPIO peripheral on the chip
 * @return	Nothing
 */
STATIC INLINE void Chip_GPIO_DeInit(LPC_GPIO_T *pGPIO)
{
	Chip_Clock_DisablePeriphClock(SYSCTL_CLOCK_GPIO);
}

/**
 * @brief	Set a GPIO pin state
 * @param	pGPIO	: The base of GPIO peripheral on the chip
 * @param	port	: GPIO Port number where pin is located
 * @param	pin		: GPIO pin to set
 * @param	setting	: true for high, false for low
 * @return	Nothing
 * @note	This function replaces Chip_GPIO_WritePortBit()
 */
STATIC INLINE void Chip_GPIO_SetPinState(LPC_GPIO_T *pGPIO, uint8_t port, uint8_t pin, bool setting)
{
	if (setting) {	/* Set Port */
		pGPIO[port].SET |= 1UL << pin;
	}
	else {	/* Clear Port */
		pGPIO[port].CLR |= 1UL << pin;
	}
}

/**
 * @brief	Set a GPIO port/bit state
 * @param	pGPIO	: The base of GPIO peripheral on the chip
 * @param	port	: GPIO port to set
 * @param	pin		: GPIO pin to set
 * @param	setting	: true for high, false for low
 * @return	Nothing
 */
STATIC INLINE void Chip_GPIO_WritePortBit(LPC_GPIO_T *pGPIO, uint32_t port, uint8_t pin, bool setting)
{
	Chip_GPIO_SetPinState(pGPIO, port, pin, setting);
}

/**
 * @brief	Get a GPIO pin state via the GPIO byte register
 * @param	pGPIO	: The base of GPIO peripheral on the chip
 * @param	port	: GPIO Port number where pin is located
 * @param	pin		: GPIO pin to get state for
 * @return	true if the GPIO is high, false if low
 * @note	This function replaces Chip_GPIO_ReadPortBit()
 */
STATIC INLINE bool Chip_GPIO_GetPinState(LPC_GPIO_T *pGPIO, uint8_t port, uint8_t pin)
{
	return (bool) ((pGPIO[port].PIN >> pin) & 1);
}

/**
 * @brief	Read a GPIO state
 * @param	pGPIO	: The base of GPIO peripheral on the chip
 * @param	port	: GPIO port to read
 * @param	pin		: GPIO pin to read
 * @return	true of the GPIO is high, false if low
 * @note	It is recommended to use the Chip_GPIO_GetPinState() function instead.
 */
STATIC INLINE bool Chip_GPIO_ReadPortBit(LPC_GPIO_T *pGPIO, uint32_t port, uint8_t pin)
{
	return Chip_GPIO_GetPinState(pGPIO, port, pin);
}

/**
 * @brief	Set GPIO direction for a single GPIO pin to an output
 * @param	pGPIO	: The base of GPIO peripheral on the chip
 * @param	port	: GPIO Port number where a pin is located
 * @param	pin		: GPIO pin to set direction on as output
 * @return	Nothing
 */
STATIC INLINE void Chip_GPIO_SetPinDIROutput(LPC_GPIO_T *pGPIO, uint8_t port, uint8_t pin)
{
	pGPIO[port].DIR |= 1UL << pin;
}

/**
 * @brief	Set GPIO direction for a single GPIO pin to an input
 * @param	pGPIO	: The base of GPIO peripheral on the chip
 * @param	port	: GPIO Port number where pin is located
 * @param	pin		: GPIO pin to set direction on as input
 * @return	Nothing
 */
STATIC INLINE void Chip_GPIO_SetPinDIRInput(LPC_GPIO_T *pGPIO, uint8_t port, uint8_t pin)
{
	pGPIO[port].DIR &= ~(1UL << pin);
}

/**
 * @brief	Set GPIO direction for a single GPIO pin
 * @param	pGPIO	: The base of GPIO peripheral on the chip
 * @param	port	: GPIO Port number where pin is located
 * @param	pin		: GPIO pin to set direction for
 * @param	output	: true for output, false for input
 * @return	Nothing
 */
STATIC INLINE void Chip_GPIO_SetPinDIR(LPC_GPIO_T *pGPIO, uint8_t port, uint8_t pin, bool output)
{
	if (output) {
		Chip_GPIO_SetPinDIROutput(pGPIO, port, pin);
	}
	else {
		Chip_GPIO_SetPinDIRInput(pGPIO, port, pin);
	}
}

/**
 * @brief	Set a GPIO direction
 * @param	pGPIO	: The base of GPIO peripheral on the chip
 * @param	port	: GPIO port to set
 * @param	bit		: GPIO bit to set
 * @param	setting	: true for output, false for input
 * @return	Nothing
 * @note	It is recommended to use the Chip_GPIO_SetPinDIROutput(),
 * Chip_GPIO_SetPinDIRInput() or Chip_GPIO_SetPinDIR() functions instead
 * of this function.
 */
STATIC INLINE void Chip_GPIO_WriteDirBit(LPC_GPIO_T *pGPIO, uint8_t port, uint8_t bit, bool setting)
{
	Chip_GPIO_SetPinDIR(pGPIO, port, bit, setting);
}

/**
 * @brief	Get GPIO direction for a single GPIO pin
 * @param	pGPIO	: The base of GPIO peripheral on the chip
 * @param	port	: GPIO Port number where pin is located
 * @param	pin		: GPIO pin to get direction for
 * @return	true if the GPIO is an output, false if input
 */
STATIC INLINE bool Chip_GPIO_GetPinDIR(LPC_GPIO_T *pGPIO, uint8_t port, uint8_t pin)
{
	return (bool) (((pGPIO[port].DIR) >> pin) & 1);
}

/**
 * @brief	Read a GPIO direction (out or in)
 * @param	pGPIO	: The base of GPIO peripheral on the chip
 * @param	port	: GPIO port to read
 * @param	bit		: GPIO bit to read
 * @return	true of the GPIO is an output, false if input
 * @note	It is recommended to use the Chip_GPIO_GetPinDIR() function instead.
 */
STATIC INLINE bool Chip_GPIO_ReadDirBit(LPC_GPIO_T *pGPIO, uint32_t port, uint8_t bit)
{
	return Chip_GPIO_GetPinDIR(pGPIO, port, bit);
}

/**
 * @brief	Set Direction for a GPIO port
 * @param	pGPIO		: The base of GPIO peripheral on the chip
 * @param	portNum		: port Number
 * @param	bitValue	: GPIO bit to set
 * @param	out			: Direction value, 0 = input, !0 = output
 * @return	None
 * @note	Bits set to '0' are not altered. It is recommended to use the
 * Chip_GPIO_SetPortDIR() function instead.
 */
STATIC INLINE void Chip_GPIO_SetDir(LPC_GPIO_T *pGPIO, uint8_t portNum, uint32_t bitValue, uint8_t out)
{
	Chip_GPIO_SetPinDIR(pGPIO, portNum, bitValue, out);
}

/**
 * @brief	Set GPIO direction for a all selected GPIO pins to an output
 * @param	pGPIO	: The base of GPIO peripheral on the chip
 * @param	port	: GPIO Port number where pin is located
 * @param	pinMask	: GPIO pin mask to set direction on as output (bits 0..b for pins 0..n)
 * @return	Nothing
 * @note	Sets multiple GPIO pins to the output direction, each bit's position that is
 * high sets the corresponding pin number for that bit to an output.
 */
STATIC INLINE void Chip_GPIO_SetPortDIROutput(LPC_GPIO_T *pGPIO, uint8_t port, uint32_t pinMask)
{
	pGPIO[port].DIR |= pinMask;
}

/**
 * @brief	Set GPIO direction for a all selected GPIO pins to an input
 * @param	pGPIO	: The base of GPIO peripheral on the chip
 * @param	port	: GPIO Port number where pin is located
 * @param	pinMask	: GPIO pin mask to set direction on as input (bits 0..b for pins 0..n)
 * @return	Nothing
 * @note	Sets multiple GPIO pins to the input direction, each bit's position that is
 * high sets the corresponding pin number for that bit to an input.
 */
STATIC INLINE void Chip_GPIO_SetPortDIRInput(LPC_GPIO_T *pGPIO, uint8_t port, uint32_t pinMask)
{
	pGPIO[port].DIR &= ~pinMask;
}

/**
 * @brief	Set GPIO direction for a all selected GPIO pins to an input or output
 * @param	pGPIO	: The base of GPIO peripheral on the chip
 * @param	port	: GPIO Port number where pin is located
 * @param	pinMask	: GPIO pin mask to set direction on (bits 0..b for pins 0..n)
 * @param	outSet	: Direction value, false = set as inputs, true = set as outputs
 * @return	Nothing
 * @note	Sets multiple GPIO pins to the input direction, each bit's position that is
 * high sets the corresponding pin number for that bit to an input.
 */
STATIC INLINE void Chip_GPIO_SetPortDIR(LPC_GPIO_T *pGPIO, uint8_t port, uint32_t pinMask, bool outSet)
{
	if (outSet) {
		Chip_GPIO_SetPortDIROutput(pGPIO, port, pinMask);
	}
	else {
		Chip_GPIO_SetPortDIRInput(pGPIO, port, pinMask);
	}
}

/**
 * @brief	Get GPIO direction for a all GPIO pins
 * @param	pGPIO	: The base of GPIO peripheral on the chip
 * @param	port	: GPIO Port number where pin is located
 * @return	a bitfield containing the input and output states for each pin
 * @note	For pins 0..n, a high state in a bit corresponds to an output state for the
 * same pin, while a low  state corresponds to an input state.
 */
STATIC INLINE uint32_t Chip_GPIO_GetPortDIR(LPC_GPIO_T *pGPIO, uint8_t port)
{
	return pGPIO[port].DIR;
}

/**
 * @brief	Set GPIO port mask value for GPIO masked read and write
 * @param	pGPIO	: The base of GPIO peripheral on the chip
 * @param	port	: port Number
 * @param	mask	: Mask value for read and write (only low bits are enabled)
 * @return	Nothing
 * @note	Controls which bits are set or unset when using the masked
 * GPIO read and write functions. A low state indicates the pin is settable
 * and readable via the masked write and read functions.
 */
STATIC INLINE void Chip_GPIO_SetPortMask(LPC_GPIO_T *pGPIO, uint8_t port, uint32_t mask)
{
	pGPIO[port].MASK = mask;
}

/**
 * @brief	Get GPIO port mask value used for GPIO masked read and write
 * @param	pGPIO	: The base of GPIO peripheral on the chip
 * @param	port	: port Number
 * @return	Returns value set with the Chip_GPIO_SetPortMask() function.
 * @note	A high bit in the return value indicates that that GPIO pin for the
 * port cannot be set using the masked write function.
 */
STATIC INLINE uint32_t Chip_GPIO_GetPortMask(LPC_GPIO_T *pGPIO, uint8_t port)
{
	return pGPIO[port].MASK;
}

/**
 * @brief	Set all GPIO pin states, but mask via the MASK register
 * @param	pGPIO	: The base of GPIO peripheral on the chip
 * @param	port	: GPIO Port number where pin is located
 * @param	value	: Value to set all GPIO pin states (0..n) to
 * @return	Nothing
 */
STATIC INLINE void Chip_GPIO_SetMaskedPortValue(LPC_GPIO_T *pGPIO, uint8_t port, uint32_t value)
{
	pGPIO[port].PIN = value;
}

/**
 * @brief	Get all GPIO pin states but mask via the MASK register
 * @param	pGPIO	: The base of GPIO peripheral on the chip
 * @param	port	: GPIO Port number where pin is located
 * @return	Current (masked) state of all GPIO pins
 */
STATIC INLINE uint32_t Chip_GPIO_GetMaskedPortValue(LPC_GPIO_T *pGPIO, uint8_t port)
{
	return pGPIO[port].PIN;
}

/**
 * @brief	Set all GPIO raw pin states (does not bypass masking on this chip!)
 * @param	pGPIO	: The base of GPIO peripheral on the chip
 * @param	port	: GPIO Port number where pin is located
 * @param	value	: Value to set all GPIO pin states (0..n) to
 * @return	Nothing
 */
STATIC INLINE void Chip_GPIO_SetPortValue(LPC_GPIO_T *pGPIO, uint8_t port, uint32_t value)
{
	Chip_GPIO_SetMaskedPortValue(pGPIO, port, value);
}

/**
 * @brief	Get all GPIO raw pin states (does not bypass masking on this chip!)
 * @param	pGPIO	: The base of GPIO peripheral on the chip
 * @param	port	: GPIO Port number where pin is located
 * @return	Current (raw) state of all GPIO pins
 */
STATIC INLINE uint32_t Chip_GPIO_GetPortValue(LPC_GPIO_T *pGPIO, uint8_t port)
{
	return Chip_GPIO_GetMaskedPortValue(pGPIO, port);
}

/**
 * @brief	Set selected GPIO output pins to the high state
 * @param	pGPIO	: The base of GPIO peripheral on the chip
 * @param	port	: GPIO Port number where pin is located
 * @param	pins	: pins (0..n) to set high
 * @return	None
 * @note	Any bit set as a '0' will not have it's state changed. This only
 * applies to ports configured as an output.
 */
STATIC INLINE void Chip_GPIO_SetPortOutHigh(LPC_GPIO_T *pGPIO, uint8_t port, uint32_t pins)
{
	pGPIO[port].SET = pins;
}

/**
 * @brief	Set a GPIO port/bit to the high state
 * @param	pGPIO		: The base of GPIO peripheral on the chip
 * @param	portNum		: port number
 * @param	bitValue	: bit(s) in the port to set high
 * @return	None
 * @note	Any bit set as a '0' will not have it's state changed. This only
 * applies to ports configured as an output. It is recommended to use the
 * Chip_GPIO_SetPortOutHigh() function instead.
 */
STATIC INLINE void Chip_GPIO_SetValue(LPC_GPIO_T *pGPIO, uint8_t portNum, uint32_t bitValue)
{
	Chip_GPIO_SetPortOutHigh(pGPIO,portNum,bitValue);
}

/**
 * @brief	Set an individual GPIO output pin to the high state
 * @param	pGPIO	: The base of GPIO peripheral on the chip'
 * @param	port	: GPIO Port number where pin is located
 * @param	pin		: pin number (0..n) to set high
 * @return	None
 * @note	Any bit set as a '0' will not have it's state changed. This only
 * applies to ports configured as an output.
 */
STATIC INLINE void Chip_GPIO_SetPinOutHigh(LPC_GPIO_T *pGPIO, uint8_t port, uint8_t pin)
{
	pGPIO[port].SET = (1 << pin);
}

/**
 * @brief	Set selected GPIO output pins to the low state
 * @param	pGPIO	: The base of GPIO peripheral on the chip
 * @param	port	: GPIO Port number where pin is located
 * @param	pins	: pins (0..n) to set low
 * @return	None
 * @note	Any bit set as a '0' will not have it's state changed. This only
 * applies to ports configured as an output.
 */
STATIC INLINE void Chip_GPIO_SetPortOutLow(LPC_GPIO_T *pGPIO, uint8_t port, uint32_t pins)
{
	pGPIO[port].CLR = pins;
}

/**
 * @brief	Set a GPIO port/bit to the low state
 * @param	pGPIO		: The base of GPIO peripheral on the chip
 * @param	portNum		: port number
 * @param	bitValue	: bit(s) in the port to set low
 * @return	None
 * @note	Any bit set as a '0' will not have it's state changed. This only
 * applies to ports configured as an output.
 */
STATIC INLINE void Chip_GPIO_ClearValue(LPC_GPIO_T *pGPIO, uint8_t portNum, uint32_t bitValue)
{
	Chip_GPIO_SetPortOutLow(pGPIO, portNum, bitValue);
}

/**
 * @brief	Set an individual GPIO output pin to the low state
 * @param	pGPIO	: The base of GPIO peripheral on the chip
 * @param	port	: GPIO Port number where pin is located
 * @param	pin		: pin number (0..n) to set low
 * @return	None
 * @note	Any bit set as a '0' will not have it's state changed. This only
 * applies to ports configured as an output.
 */
STATIC INLINE void Chip_GPIO_SetPinOutLow(LPC_GPIO_T *pGPIO, uint8_t port, uint8_t pin)
{
	pGPIO[port].CLR = (1 << pin);
}

/**
 * @brief	Toggle an individual GPIO output pin to the opposite state
 * @param	pGPIO	: The base of GPIO peripheral on the chip
 * @param	port	: GPIO Port number where @a pin is located
 * @param	pin		: pin number (0..n) to toggle
 * @return	None
 * @note	Any bit set as a '0' will not have it's state changed. This only
 * applies to ports configured as an output.
 */
STATIC INLINE void Chip_GPIO_SetPinToggle(LPC_GPIO_T *pGPIO, uint8_t port, uint8_t pin)
{
	bool setting = !Chip_GPIO_GetPinState(pGPIO, port, pin);
	Chip_GPIO_SetPinState(pGPIO, port, pin, setting);
}

/**
 * @brief	Read current bit states for the selected port
 * @param	pGPIO	: The base of GPIO peripheral on the chip
 * @param	portNum	: port number to read
 * @return	Current value of GPIO port
 * @note	The current states of the bits for the port are read, regardless of
 * whether the GPIO port bits are input or output.
 */
STATIC INLINE uint32_t Chip_GPIO_ReadValue(LPC_GPIO_T *pGPIO, uint8_t portNum)
{
	return pGPIO[portNum].PIN;
}

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __GPIO_17XX_40XX_H_ */
