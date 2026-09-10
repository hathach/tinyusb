/***********************************************************************************************************************
 * Copyright [2015-2025] Renesas Electronics Corporation and/or its licensors. All Rights Reserved.
 * 
 * This file is part of Renesas SynergyTM Software Package (SSP)
 *
 * The contents of this file (the "contents") are proprietary and confidential to Renesas Electronics Corporation
 * and/or its licensors ("Renesas") and subject to statutory and contractual protections.
 *
 * This file is subject to a Renesas SSP license agreement. Unless otherwise agreed in an SSP license agreement with
 * Renesas: 1) you may not use, copy, modify, distribute, display, or perform the contents; 2) you may not use any name
 * or mark of Renesas for advertising or publicity purposes or in connection with your use of the contents; 3) RENESAS
 * MAKES NO WARRANTY OR REPRESENTATIONS ABOUT THE SUITABILITY OF THE CONTENTS FOR ANY PURPOSE; THE CONTENTS ARE PROVIDED
 * "AS IS" WITHOUT ANY EXPRESS OR IMPLIED WARRANTY, INCLUDING THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE, AND NON-INFRINGEMENT; AND 4) RENESAS SHALL NOT BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL, OR
 * CONSEQUENTIAL DAMAGES, INCLUDING DAMAGES RESULTING FROM LOSS OF USE, DATA, OR PROJECTS, WHETHER IN AN ACTION OF
 * CONTRACT OR TORT, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THE CONTENTS. Third-party contents
 * included in this file may be subject to different terms.
 **********************************************************************************************************************/

/**********************************************************************************************************************
 * File Name    : hw_ioport_private.h
 * Description  : IOPORT private macros and HW private function definition
 **********************************************************************************************************************/

#ifndef HW_IOPORT_PRIVATE_H
#define HW_IOPORT_PRIVATE_H


/**********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "bsp_api.h"
#include "r_ioport.h"
#include "r_ioport_api.h"

/* Common macro for SSP header files. There is also a corresponding SSP_FOOTER macro at the end of this file. */
SSP_HEADER

/**********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#define IOPORT_PRV_PCNTR_OFFSET        0x00000020U

#define IOPORT_PRV_PORT_OUTPUT_HIGH    (1U)
#define IOPORT_PRV_PORT_INPUT          (1U << 1)
#define IOPORT_PRV_PORT_DIR_OUTPUT     (1U << 2)
#define IOPORT_PRV_PULL_UP_ENABLE      (1U << 4)
#define IOPORT_PRV_EVENT_RISING_EDGE   (1U << 12)
#define IOPORT_PRV_EVENT_FALLING_EDGE  (1U << 13)
#define IOPORT_PRV_EVENT_BOTH_EDGES    (3U << 12)
#define IOPORT_PRV_ANALOG_INPUT        (1U << 15)
#define IOPORT_PRV_PERIPHERAL_FUNCTION (1U << 16)
#define IOPORT_PRV_CLEAR_BITS_MASK     (0x1F01FCD5U)     ///< Zero bits in mask must be written as zero to PFS register

/**********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
/** PFS writing enable/disable. */
typedef enum e_ioport_pwpr
{
    IOPORT_PFS_WRITE_DISABLE = 0,       ///< Disable PFS write access
    IOPORT_PFS_WRITE_ENABLE  = 1        ///< Enable PFS write access
} ioport_pwpr_t;

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
extern volatile uint16_t g_protect_pfswe_counter;

/**********************************************************************************************************************
 * Function Prototypes
 **********************************************************************************************************************/
__STATIC_INLINE uint32_t *         ioport_port_address_get (uint32_t volatile * p_base_address, ioport_port_t index);

__STATIC_INLINE uint32_t *         ioport_pfs_address_get (uint32_t volatile * p_base_address, ioport_port_pin_t pin);

__STATIC_INLINE void             HW_IOPORT_PortWriteWithPCNTR3 (R_IOPORT1_Type * p_ioport_regs,
                                                        ioport_port_t port,
                                                        ioport_size_t set_value,
                                                        ioport_size_t reset_value);

__STATIC_INLINE void             HW_IOPORT_Init_Reference_Counter(void);

__STATIC_INLINE ioport_level_t     HW_IOPORT_PinRead (R_PFS_Type * p_pfs_reg, ioport_port_pin_t pin);

__STATIC_INLINE ioport_size_t      HW_IOPORT_PortRead (R_IOPORT1_Type * p_ioport_regs, ioport_port_t port);

__STATIC_INLINE ioport_size_t      HW_IOPORT_PortDirectionRead (R_IOPORT1_Type * p_ioport_regs, ioport_port_t port);

__STATIC_INLINE void               HW_IOPORT_PortDirectionSet (R_IOPORT1_Type * p_ioport_regs, ioport_port_t port, ioport_size_t value);

__STATIC_INLINE void               HW_IOPORT_PinDirectionSet (R_PFS_Type * p_pfs_reg, R_PMISC_Type * p_pmisc_reg, ioport_port_pin_t pin, ioport_direction_t direction);

__STATIC_INLINE ioport_size_t      HW_IOPORT_PortOutputDataRead (R_IOPORT1_Type * p_ioport_regs, ioport_port_t port);

__STATIC_INLINE ioport_size_t      HW_IOPORT_PortEventInputDataRead (R_IOPORT1_Type * p_ioport_regs, ioport_port_t port);

__STATIC_INLINE void               HW_IOPORT_PortEventOutputDataWrite (R_IOPORT1_Type * p_ioport_regs,
                                                                   ioport_port_t port,
                                                                   ioport_size_t set_value,
                                                                   ioport_size_t reset_value);

__STATIC_INLINE void               HW_IOPORT_PinEventOutputDataWrite (R_IOPORT1_Type * p_ioport_regs,
                                                                   ioport_port_t port,
                                                                   ioport_size_t set_value,
                                                                   ioport_size_t reset_value,ioport_level_t pin_level);

__STATIC_INLINE void             HW_IOPORT_EthernetModeCfg (R_PMISC_Type * p_pmisc_reg, ioport_ethernet_channel_t channel, ioport_ethernet_mode_t mode);

__STATIC_INLINE void         HW_IOPORT_PFSWrite (R_PFS_Type * p_pfs_reg, R_PMISC_Type * p_pmisc_reg, ioport_port_pin_t pin, uint32_t value);

__STATIC_INLINE void         HW_IOPORT_PFSSetDirection (R_PFS_Type * p_pfs_reg, R_PMISC_Type * p_pmisc_reg, ioport_port_pin_t pin, ioport_direction_t direction);

__STATIC_INLINE uint32_t     HW_IOPORT_PFSRead (R_PFS_Type * p_pfs_reg, ioport_port_pin_t pin);

__STATIC_INLINE void         HW_IOPORT_PFSAccess (R_PMISC_Type * p_pmisc_reg, ioport_pwpr_t value);



/***********************************************************************************************************************
 Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * Zero the reference counter used by the HW_IOPORT_PFSAccessDisable() and HW_IOPORT_PFSAccessEnable() functions.
 * This function will be replaced in a future release and has been added only to insure that the reference counter
 * is 0 when g_ioport_on_ioport.init() is called (prior to the C Runtime initialization) as part of the BSP init.
 *
 **********************************************************************************************************************/
__STATIC_INLINE void HW_IOPORT_Init_Reference_Counter(void)
{
    g_protect_pfswe_counter = 0U;
}

/*******************************************************************************************************************//**
 * @internal Returns the address of the specified port register.
 *
 * @param[in]    p_base_address    Base address of the required register
 * @param[in]    index             Required port
 *
 * @retval   Address of the port register
 * @endinternal
 **********************************************************************************************************************/
__STATIC_INLINE uint32_t * ioport_port_address_get (uint32_t volatile * p_base_address, ioport_port_t index)
{
    /** base address - First port register of this type */
    /** index        - Index off the base */
    /* Cast to ensure treated as unsigned */
    return (uint32_t *) ((uint32_t) (((((uint32_t) index >>
                                        8) &
                                       0xFFU) *
                                      (uint32_t) IOPORT_PRV_PCNTR_OFFSET) + ((uint32_t) p_base_address)));
}


/*******************************************************************************************************************//**
 * @internal Returns the address of the specified PFS register.
 *
 * @param[in]    p_base_address    Base address of the required register
 * @param[in]    pin               Required pin
 *
 * @retval   Address of the PFS register
 * @endinternal
 **********************************************************************************************************************/
__STATIC_INLINE uint32_t * ioport_pfs_address_get (uint32_t volatile * p_base_address, ioport_port_pin_t pin)
{
    /** base address - First port register of this type */
    /** pin            - Pin for which address is required in format 0xTTPP where TT is the port and PP is the pin */

    /** (16 * 4) to jump a whole set of port addresses as 16 pins in a port and each PFS register is 4 bytes */
    /** (* 4) to jump a pin address as each PFS register is 4 bytes */
    /* Cast to ensure treated as unsigned */
    return (uint32_t *) ((uint32_t) ((((((uint32_t) pin >>
                                         8) &
                                        0x00FFU) *
                                       (16U * 4U))) + (((uint32_t) pin & 0x00FFU) * 4U) + ((uint32_t) p_base_address)));
}

/*******************************************************************************************************************//**
 * Writes the Ethernet PHY configuration.
 *
 * @param[in]    p_pmisc_reg    Base address of the PMISC registers
 * @param[in]    channel        Ethenet channel number
 * @param[in]    mode           Required PHY mode
 *
 **********************************************************************************************************************/
__STATIC_INLINE void HW_IOPORT_EthernetModeCfg (R_PMISC_Type * p_pmisc_reg, ioport_ethernet_channel_t channel, ioport_ethernet_mode_t mode)
{
    uint8_t value;

    if (IOPORT_ETHERNET_MODE_RMII == mode)
    {
        value           = p_pmisc_reg->PFENET;
        /* Cast to ensure unsigned value */
        value          &= (uint8_t) (~((uint8_t) channel));
        value          &= 0x30U;
        p_pmisc_reg->PFENET = value;
    }
    else if (IOPORT_ETHERNET_MODE_MII == mode)
    {
        value           = p_pmisc_reg->PFENET;
        value          |= channel;
        value          &= 0x30U;
        p_pmisc_reg->PFENET = value;
    }
}

/*******************************************************************************************************************//**
 * Writes to the pins in a port using the PCNTR3 register. This register allows individual bits to be set high or
 * cleared low while leaving other pins unchanged. This allows automic setting and clearing of pins.
 *
 * @param[in]    p_ioport_regs  Base address of the IOPORT registers
 * @param[in]    port           Port to write to
 * @param[in]    set_value      Bits in the port to set high (1 = that bit will be set high)
 * @param[in]    reset_value    Bits in the port to clear low (1 = that bit will be cleared low)
 *
 **********************************************************************************************************************/
__STATIC_INLINE void HW_IOPORT_PortWriteWithPCNTR3 (R_IOPORT1_Type * p_ioport_regs,
                                                    ioport_port_t port,
                                                    ioport_size_t set_value,
                                                    ioport_size_t reset_value)
{
    /** set_value contains the bits to be set high (bit mask) */

    /** reset_value contains the bits to be cleared low - a high bit indicates that bit should be cleared low (bit mask)
    **/

    volatile uint32_t * p_dest;

    p_dest = ioport_port_address_get((uint32_t volatile *) &p_ioport_regs->PCNTR3, port);

    /** PCNTR4 register: lower word = set data, upper word = reset_data */
    *p_dest = (uint32_t) (((uint32_t) reset_value << 16) | set_value);
}

/*******************************************************************************************************************//**
 * Returns the input event data for the specified port.
 *
 * @param[in]    p_ioport_regs  Base address of the IOPORT registers
 * @param[in]    port           Port to read event data
 *
 * @retval      Input event data for the specified port.
 **********************************************************************************************************************/
__STATIC_INLINE ioport_size_t HW_IOPORT_PortEventInputDataRead (R_IOPORT1_Type * p_ioport_regs, ioport_port_t port)
{
    /** Returns specified port's event input data from PCNTR2 */

    volatile uint32_t * p_dest;
    uint32_t          pcntr_reg_value;
    ioport_size_t     port_data;

    p_dest           = ioport_port_address_get((uint32_t volatile *) &p_ioport_regs->PCNTR2, port);
    /** Read current value of PCNTR register value for the specified port */
    pcntr_reg_value  = *p_dest;
    /** Only the upper 16-bits are required */
    pcntr_reg_value  = pcntr_reg_value >> 16;
    pcntr_reg_value &= 0x0000FFFFU;
    port_data        = (ioport_size_t) pcntr_reg_value;

    return port_data;
}

/*******************************************************************************************************************//**
 * Writes the set and clear values for pin on a port to be written when an event occurs. This allows accurate timing of
 * pin output level.
 *
 * @param[in]    p_ioport_regs  Base address of the IOPORT registers
 * @param[in]    set_value      Bits in the port to set high (1 = that bit will be set high)
 * @param[in]    reset_value    Bits in the port to clear low (1 = that bit will be cleared low)
 *
 **********************************************************************************************************************/
__STATIC_INLINE void HW_IOPORT_PortEventOutputDataWrite (R_IOPORT1_Type * p_ioport_regs,
                                                         ioport_port_t port,
                                                         ioport_size_t set_value,
                                                         ioport_size_t reset_value)
{
    /** set_value contains the bits to be set high (bit mask) */

    /** reset_value contains the bits to be cleared low - a high bit indicates that bit should be cleared low (bit mask)
    **/

    volatile uint32_t * p_dest;

    /** IOPORT0 does not have a PCNTR4 register. So, address of PCNTR3 is used then 4 bytes added */
    p_dest  = ioport_port_address_get((uint32_t volatile *) &p_ioport_regs->PCNTR3, port);
    p_dest += 1;

    /** PCNTR4 register: lower word = set data, upper word = reset_data */
    *p_dest = (uint32_t) (((uint32_t) reset_value << 16) | (uint32_t) set_value);
}

/*******************************************************************************************************************//**
 * Writes the set and clear values on a pin of the port when an ELC event occurs. This allows accurate timing of
 * pin output level.
 *
 * @param[in]    p_ioport_regs  Base address of the IOPORT registers
 * @param[in]    port           Port to read event data
 * @param[in]    set_value      Bit in the port to set high (1 = that bit will be set high)
 * @param[in]    reset_value    Bit in the port to clear low (1 = that bit will be cleared low)
 * @param[in]    pin_level      Event data for pin
 **********************************************************************************************************************/
__STATIC_INLINE void HW_IOPORT_PinEventOutputDataWrite (R_IOPORT1_Type * p_ioport_regs,
                                                        ioport_port_t port,
                                                        ioport_size_t set_value,
                                                        ioport_size_t reset_value,ioport_level_t pin_level)
{
    volatile uint32_t * p_dest;
    uint32_t port_value = 0;

    /** IOPORT0 does not have a PCNTR4 register. So, address of PCNTR3 is used then 4 bytes added */
    p_dest = ioport_port_address_get((uint32_t volatile *) &p_ioport_regs->PCNTR3, port);
    p_dest += 1;
    port_value = *p_dest;

    if (IOPORT_LEVEL_HIGH == pin_level)
    {
        /** set value contains the bit to be set high (bit mask) */
       port_value |= (uint32_t) (set_value);
        /** reset value contains the mask to clear the corresponding bit in EOSR because both EOSR and EORR
            bit of a particular pin should not be high at the same time */
       port_value &= (uint32_t) ((reset_value << 16) | 0x0000FFFF);
    }
    else
    {
        /** reset_value contains the bit to be cleared low */
    	port_value |= (uint32_t) reset_value << 16;
        /** set value contains the mask to clear the corresponding bit in EOSR because both EOSR and EORR bit of a
            particular pin should not be high at the same time */
    	port_value &= (uint32_t) ((set_value | 0xFFFF0000));
    }

    *p_dest = port_value;
}

/*******************************************************************************************************************//**
 * Returns the current output levels of pin on a port.
 *
 * @param[in]    p_ioport_regs  Base address of the IOPORT registers
 * @param[in]    port           Port to read
 *
 * @retval      Output data for the specified port
 **********************************************************************************************************************/
__STATIC_INLINE ioport_size_t HW_IOPORT_PortOutputDataRead (R_IOPORT1_Type * p_ioport_regs, ioport_port_t port)
{
    /** Returns value of specified's port Output Data from PCNTR1 */
    /** Used by set pin level function to read current output levels on port */

    volatile uint32_t * p_dest;
    uint32_t          pcntr_reg_value;

    p_dest = ioport_port_address_get((uint32_t volatile *) &p_ioport_regs->PCNTR1, port);

    /** Read current value of PCNTR register value for the specified port */
    pcntr_reg_value  = *p_dest;
    /** Only the upper 16-bits are required */
    pcntr_reg_value  = pcntr_reg_value >> 16;
    pcntr_reg_value &= 0x0000FFFFU;

    return (ioport_size_t) pcntr_reg_value;
}

/*******************************************************************************************************************//**
 * Returns the level of an individual pin.
 *
 * @param[in]    p_pfs_reg      Base address of the PFS registers
 * @param[in]    pin            Pin to read the level from
 *
 * @retval      Level of the specified pin.
 **********************************************************************************************************************/
__STATIC_INLINE ioport_level_t HW_IOPORT_PinRead (R_PFS_Type * p_pfs_reg, ioport_port_pin_t pin)
{
    uint32_t pfs;

    pfs = HW_IOPORT_PFSRead(p_pfs_reg, pin);

    if ((uint32_t) IOPORT_PRV_PORT_INPUT == (pfs & (uint32_t) IOPORT_PRV_PORT_INPUT))
    {
        return IOPORT_LEVEL_HIGH;
    }
    else
    {
        return IOPORT_LEVEL_LOW;
    }
}

/*******************************************************************************************************************//**
 * Returns the value of all pins on the specified port.
 *
 * @param[in]    p_ioport_regs  Base address of the IOPORT registers
 * @param[in]    port           Port to read
 *
 * @retval      Value of the pins on the specified port.
 **********************************************************************************************************************/
__STATIC_INLINE ioport_size_t HW_IOPORT_PortRead (R_IOPORT1_Type * p_ioport_regs, ioport_port_t port)
{
    volatile uint32_t * p_dest;
    uint32_t          pcntr_reg_value;
    uint16_t          port_data;

    p_dest           = ioport_port_address_get((uint32_t volatile *) &p_ioport_regs->PCNTR2, port);
    /** Read current value of PCNTR register value for the specified port */
    pcntr_reg_value  = *p_dest;
    /** Only the lower 16-bits are required */
    pcntr_reg_value &= 0x0000FFFFU;
    port_data        = (ioport_size_t) pcntr_reg_value;

    return port_data;
}

/*******************************************************************************************************************//**
 * Returns the direction of pins on the specified port.
 *
 * @param[in]    p_ioport_regs  Base address of the IOPORT registers
 * @param[in]    port           Port to read direction information from
 *
 * @retval      Direction information of the specified port.
 **********************************************************************************************************************/
__STATIC_INLINE ioport_size_t HW_IOPORT_PortDirectionRead (R_IOPORT1_Type * p_ioport_regs, ioport_port_t port)
{
    volatile uint32_t * p_dest;
    uint32_t          pcntr_reg_value;

    p_dest           = ioport_port_address_get((uint32_t volatile *) &p_ioport_regs->PCNTR1, port);
    /** Read current value of PCNTR register value for the specified port */
    pcntr_reg_value  = *p_dest;
    /** Only the lower 16-bits should be written too */
    pcntr_reg_value &= 0x0000FFFFU;

    return (ioport_size_t) pcntr_reg_value;
}

/*******************************************************************************************************************//**
 * Sets the direction of an individual pin.
 *
 * @param[in]    p_pfs_reg    Base address of the PFS registers
 * @param[in]    pin          Pin to set direction for
 * @param[in]    direction    Direction for the specified pin
 *
 **********************************************************************************************************************/
__STATIC_INLINE void HW_IOPORT_PinDirectionSet (R_PFS_Type * p_pfs_reg, R_PMISC_Type * p_pmisc_reg, ioport_port_pin_t pin, ioport_direction_t direction)
{
    uint32_t pfs;

    pfs = HW_IOPORT_PFSRead(p_pfs_reg, pin);

    if (IOPORT_DIRECTION_INPUT == direction)
    {
        /** Clear PDR bit (2) */
        pfs &= (~((uint32_t) IOPORT_PRV_PORT_DIR_OUTPUT));
    }
    else
    {
        /** Set PDR bit (2) */
        pfs |= IOPORT_PRV_PORT_DIR_OUTPUT;
    }

    /** mask out bits which must be written as zero */
    pfs &= IOPORT_PRV_CLEAR_BITS_MASK;

    HW_IOPORT_PFSWrite(p_pfs_reg, p_pmisc_reg, pin, pfs);
}

/*******************************************************************************************************************//**
 * Sets the direction of multiple pins on the specified port.
 *
 * @param[in]    p_ioport_regs  Base address of the IOPORT registers
 * @param[in]    port           Port to write direction information to
 * @param[in]    value          Direction data to write to the port
 *
 **********************************************************************************************************************/
__STATIC_INLINE void HW_IOPORT_PortDirectionSet (R_IOPORT1_Type * p_ioport_regs, ioport_port_t port, ioport_size_t value)
{
    volatile uint32_t * p_dest;
    uint32_t          pcntr_reg_value;

    p_dest           = ioport_port_address_get((uint32_t volatile *) &p_ioport_regs->PCNTR1, port);
    /** Read current value of PCNTR register value for the specified port */
    pcntr_reg_value  = *p_dest;
    /** Only the lower 16-bits should be written too */
    pcntr_reg_value &= 0xFFFF0000U;
    pcntr_reg_value |= (uint32_t) ((value) & 0x0000FFFFU);

    *p_dest          = pcntr_reg_value;
}


/*******************************************************************************************************************//**
 * Enable access to the PFS registers. Uses a reference counter to protect against interrupts that could occur
 * via multiple threads or an ISR re-entering this code.
 *
 * @param[in]    p_pmisc_reg  Base address of the PMISC registers
 *
 **********************************************************************************************************************/
__STATIC_INLINE void HW_IOPORT_PFSAccessEnable(R_PMISC_Type * p_pmisc_reg)
{
    /** Get the current state of interrupts */
    SSP_CRITICAL_SECTION_DEFINE;
    SSP_CRITICAL_SECTION_ENTER;

    /** If this is first entry then allow writing of PFS. */
    if (0 == g_protect_pfswe_counter)
    {
        p_pmisc_reg->PWPR = 0;      ///< Clear BOWI bit - writing to PFSWE bit enabled
        p_pmisc_reg->PWPR = 0x40;   ///< Set PFSWE bit - writing to PFS register enabled
    }

    /** Increment the protect counter */
    g_protect_pfswe_counter++;

    /** Restore the interrupt state */
    SSP_CRITICAL_SECTION_EXIT;
}

/*******************************************************************************************************************//**
 * Disable access to the PFS registers. Uses a reference counter to protect against interrupts that could occur
 * via multiple threads or an ISR re-entering this code.
 *
 * @param[in]    p_pmisc_reg  Base address of the PMISC registers
 *
 **********************************************************************************************************************/
__STATIC_INLINE void HW_IOPORT_PFSAccessDisable(R_PMISC_Type * p_pmisc_reg)
{
    /** Get the current state of interrupts */
    SSP_CRITICAL_SECTION_DEFINE;
    SSP_CRITICAL_SECTION_ENTER;

    /** Is it safe to disable PFS register? */
    if (0 != g_protect_pfswe_counter)
    {
        /* Decrement the protect counter */
        g_protect_pfswe_counter--;
    }

    /** Is it safe to disable writing of PFS? */
    if (0 == g_protect_pfswe_counter)
    {
        p_pmisc_reg->PWPR = 0;      ///< Clear PFSWE bit - writing to PFS register disabled
        p_pmisc_reg->PWPR = 0x80;   ///< Set BOWI bit - writing to PFSWE bit disabled
    }

    /** Restore the interrupt state */
    SSP_CRITICAL_SECTION_EXIT;
}
/*******************************************************************************************************************//**
 * Enable/disable access to the PFS registers.
 *
 * @param[in]    p_pmisc_reg  Base address of the PMISC registers
 * @param[in]    value        Enable/disable access state
 *
 **********************************************************************************************************************/
__STATIC_INLINE void HW_IOPORT_PFSAccess (R_PMISC_Type * p_pmisc_reg, ioport_pwpr_t value)
{
    if (IOPORT_PFS_WRITE_ENABLE == value)
    {
        p_pmisc_reg->PWPR = 0;      ///< Clear BOWI bit - writing to PFSWE bit enabled
        p_pmisc_reg->PWPR = 0x40;   ///< Set PFSWE bit - writing to PFS register enabled
    }
    else
    {
        p_pmisc_reg->PWPR = 0;      ///< Clear PFSWE bit - writing to PFS register disabled
        p_pmisc_reg->PWPR = 0x80;   ///< Set BOWI bit - writing to PFSWE bit disabled
    }
}

/*******************************************************************************************************************//**
 * Returns the contents of the specified pin's PFS register.
 *
 * @param[in]    p_pfs_reg  Base address of the PFS registers
 * @param[in]    pin        Pin to read the PFS data for
 *
 * @retval      PFS contents for the specified pin.
 **********************************************************************************************************************/
__STATIC_INLINE uint32_t HW_IOPORT_PFSRead (R_PFS_Type * p_pfs_reg, ioport_port_pin_t pin)
{
    volatile uint32_t * p_dest;

    p_dest = ioport_pfs_address_get((uint32_t volatile *) p_pfs_reg, pin);

    return *p_dest;
}

/*******************************************************************************************************************//**
 * Writes to the specified pin's PFS register
 *
 * @param[in]    p_pfs_reg  Base address of the PFS registers
 * @param[in]    pin        Pin to write PFS data for
 * @param[in]    value      Value to be written to the PFS register
 *
 **********************************************************************************************************************/
__STATIC_INLINE void HW_IOPORT_PFSWrite (R_PFS_Type * p_pfs_reg, R_PMISC_Type * p_pmisc_reg, ioport_port_pin_t pin, uint32_t value)
{
    volatile uint32_t * p_dest;
    uint32_t          pfs_original;
    uint32_t          pfs_new;

    p_dest = ioport_pfs_address_get((uint32_t volatile *) p_pfs_reg, pin);

    HW_IOPORT_PFSAccessEnable(p_pmisc_reg);     // Protect PWPR from re-entrancy

    /* Read the current PFS value */
    pfs_original = *p_dest;

    /* Check if new PMR bit is set to 1 */
    if ((value & IOPORT_PRV_PERIPHERAL_FUNCTION) > 0)
    {
        pfs_new = pfs_original;
        /* Safely change PSEL bits to minimise risk of glitching. */
        /* Clear PMR bit to zero */
        pfs_new &= ~((uint32_t) IOPORT_PRV_PERIPHERAL_FUNCTION);
        *p_dest  = pfs_new;
        /* New PFS value  - zero PMR bit */
        pfs_new  = (value & ~((uint32_t) IOPORT_PRV_PERIPHERAL_FUNCTION));
        *p_dest  = pfs_new;
    }

    /* New value can be safely written to PFS. */
    *p_dest = value;

    HW_IOPORT_PFSAccessDisable(p_pmisc_reg);
}

/*******************************************************************************************************************//**
 * Sets a pin's direction using its PFS register
 *
 * @param[in]    p_pfs_reg    Base address of the PFS registers
 * @param[in]    p_pmisc_reg  Base address of the PMISC registers
 * @param[in]    pin          Pin to set direction for
 * @param[in]    direction    Direction to set for the pin
 *
 **********************************************************************************************************************/
__STATIC_INLINE void HW_IOPORT_PFSSetDirection (R_PFS_Type * p_pfs_reg, R_PMISC_Type * p_pmisc_reg, ioport_port_pin_t pin, ioport_direction_t direction)
{
    uint32_t pfs;

    pfs = HW_IOPORT_PFSRead(p_pfs_reg, pin);

    if (direction == IOPORT_DIRECTION_INPUT)
    {
        /* Cast to ensure unsigned */
        pfs &= (uint32_t) (~(IOPORT_PRV_PORT_DIR_OUTPUT));
    }
    else
    {
        pfs |= IOPORT_PRV_PORT_DIR_OUTPUT;
    }

    /** mask out bits which must be written as zero */
    pfs &= IOPORT_PRV_CLEAR_BITS_MASK;

    HW_IOPORT_PFSWrite(p_pfs_reg, p_pmisc_reg, pin, pfs);
}

/* Common macro for SSP header files. There is also a corresponding SSP_HEADER macro at the top of this file. */
SSP_FOOTER

#endif /* HW_IOPORT_PRIVATE_H */
