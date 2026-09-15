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

#ifndef R_IOPORT_PRIVATE_API_H
#define R_IOPORT_PRIVATE_API_H

/* Common macro for SSP header files. There is also a corresponding SSP_FOOTER macro at the end of this file. */
SSP_HEADER

/***********************************************************************************************************************
 * Private Instance API Functions. DO NOT USE! Use functions through Interface API structure instead.
 **********************************************************************************************************************/
ssp_err_t R_IOPORT_EthernetModeCfg (ioport_ethernet_channel_t channel, ioport_ethernet_mode_t mode);
ssp_err_t R_IOPORT_Init (const ioport_cfg_t * p_cfg);
ssp_err_t R_IOPORT_PinsCfg (const ioport_cfg_t * p_cfg);
ssp_err_t R_IOPORT_PinCfg (ioport_port_pin_t pin, uint32_t cfg);
ssp_err_t R_IOPORT_PinDirectionSet (ioport_port_pin_t pin, ioport_direction_t direction);
ssp_err_t R_IOPORT_PinEventInputRead (ioport_port_pin_t pin, ioport_level_t * p_pin_event);
ssp_err_t R_IOPORT_PinEventOutputWrite (ioport_port_pin_t pin, ioport_level_t pin_value);
ssp_err_t R_IOPORT_PinRead (ioport_port_pin_t pin, ioport_level_t * p_pin_value);
ssp_err_t R_IOPORT_PinWrite (ioport_port_pin_t pin, ioport_level_t level);
ssp_err_t R_IOPORT_PortDirectionSet (ioport_port_t port, ioport_size_t direction_values, ioport_size_t mask);
ssp_err_t R_IOPORT_PortEventInputRead (ioport_port_t port, ioport_size_t * event_data);
ssp_err_t R_IOPORT_PortEventOutputWrite (ioport_port_t port, ioport_size_t event_data, ioport_size_t mask_value);
ssp_err_t R_IOPORT_PortRead (ioport_port_t port, ioport_size_t * p_port_value);
ssp_err_t R_IOPORT_PortWrite (ioport_port_t port, ioport_size_t value, ioport_size_t mask);
ssp_err_t R_IOPORT_VersionGet (ssp_version_t * p_data);

/* Common macro for SSP header files. There is also a corresponding SSP_HEADER macro at the top of this file. */
SSP_FOOTER

#endif /* R_IOPORT_PRIVATE_API_H */
