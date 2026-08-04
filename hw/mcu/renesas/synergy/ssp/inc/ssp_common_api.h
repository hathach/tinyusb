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
 * File Name    : ssp_common_api.h
 * Description  : Common definitions used by all AMS layers.  Includes error codes and version structure.
 **********************************************************************************************************************/

#ifndef SSP_COMMON_API_H
#define SSP_COMMON_API_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include <assert.h>
#include <stdint.h>

/* Includes SSP version macros. */
#include "ssp_version.h"

/*******************************************************************************************************************//**
 * @ingroup Common_Error_Codes
 * @{
 **********************************************************************************************************************/

/**********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
/** This macro is used to suppress compiler messages about a parameter not being used in a function. The nice thing
 * about using this implementation is that it does not take any extra RAM or ROM. */
/*LDRA_INSPECTED 340 s */
#define SSP_PARAMETER_NOT_USED(p) (void) ((p))

/** Determine if a C++ compiler is being used.
 * If so, ensure that standard C is used to process the API information.  */
#if defined(__cplusplus)
#define SSP_CPP_HEADER    extern "C" {
#define SSP_CPP_FOOTER    }
#else
#define SSP_CPP_HEADER
#define SSP_CPP_FOOTER
#endif

/* SSP Header and Footer definitions */
#define SSP_HEADER    SSP_CPP_HEADER
#define SSP_FOOTER    SSP_CPP_FOOTER

/**********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/
/** Common error codes */
typedef enum e_ssp_err
{
    SSP_SUCCESS                     = 0,

    SSP_ERR_ASSERTION               = 1,        ///< A critical assertion has failed
    SSP_ERR_INVALID_POINTER         = 2,        ///< Pointer points to invalid memory location
    SSP_ERR_INVALID_ARGUMENT        = 3,        ///< Invalid input parameter
    SSP_ERR_INVALID_CHANNEL         = 4,        ///< Selected channel does not exist
    SSP_ERR_INVALID_MODE            = 5,        ///< Unsupported or incorrect mode
    SSP_ERR_UNSUPPORTED             = 6,        ///< Selected mode not supported by this API
    SSP_ERR_NOT_OPEN                = 7,        ///< Requested channel is not configured or API not open
    SSP_ERR_IN_USE                  = 8,        ///< Channel/peripheral is running/busy
    SSP_ERR_OUT_OF_MEMORY           = 9,        ///< Allocate more memory in the driver's cfg.h
    SSP_ERR_HW_LOCKED               = 10,       ///< Hardware is locked
    SSP_ERR_IRQ_BSP_DISABLED        = 11,       ///< IRQ not enabled in BSP
    SSP_ERR_OVERFLOW                = 12,       ///< Hardware overflow
    SSP_ERR_UNDERFLOW               = 13,       ///< Hardware underflow
    SSP_ERR_ALREADY_OPEN            = 14,       ///< Requested channel is already open in a different configuration
    SSP_ERR_APPROXIMATION           = 15,       ///< Could not set value to exact result
    SSP_ERR_CLAMPED                 = 16,       ///< Value had to be limited for some reason
    SSP_ERR_INVALID_RATE            = 17,       ///< Selected rate could not be met
    SSP_ERR_ABORTED                 = 18,       ///< An operation was aborted
    SSP_ERR_NOT_ENABLED             = 19,       ///< Requested operation is not enabled
    SSP_ERR_TIMEOUT                 = 20,       ///< Timeout error
    SSP_ERR_INVALID_BLOCKS          = 21,       ///< Invalid number of blocks supplied
    SSP_ERR_INVALID_ADDRESS         = 22,       ///< Invalid address supplied
    SSP_ERR_INVALID_SIZE            = 23,       ///< Invalid size/length supplied for operation
    SSP_ERR_WRITE_FAILED            = 24,       ///< Write operation failed
    SSP_ERR_ERASE_FAILED            = 25,       ///< Erase operation failed
    SSP_ERR_INVALID_CALL            = 26,       ///< Invalid function call is made
    SSP_ERR_INVALID_HW_CONDITION    = 27,       ///< Detected hardware is in invalid condition
    SSP_ERR_INVALID_FACTORY_FLASH   = 28,       ///< Factory flash is not available on this MCU
    SSP_ERR_INVALID_FMI_DATA        = 29,       ///< Linked FMI data table is not valid
    SSP_ERR_INVALID_STATE           = 30,       ///< API or command not valid in the current state
    SSP_ERR_NOT_ERASED              = 31,       ///< Erase verification failed
    SSP_ERR_SECTOR_RELEASE_FAILED   = 32,       ///< Sector release failed

    /** Start of RTOS only error codes */
    SSP_ERR_INTERNAL                = 100,      ///< Internal error
    SSP_ERR_WAIT_ABORTED            = 101,      ///< Wait

    /** Start of UART specific */
    SSP_ERR_FRAMING                 = 200,      ///< Framing error occurs
    SSP_ERR_BREAK_DETECT            = 201,      ///< Break signal detects
    SSP_ERR_PARITY                  = 202,      ///< Parity error occurs
    SSP_ERR_RXBUF_OVERFLOW          = 203,      ///< Receive queue overflow
    SSP_ERR_QUEUE_UNAVAILABLE       = 204,      ///< Can't open s/w queue
    SSP_ERR_INSUFFICIENT_SPACE      = 205,      ///< Not enough space in transmission circular buffer
    SSP_ERR_INSUFFICIENT_DATA       = 206,      ///< Not enough data in receive circular buffer

    /** Start of SPI specific */
    SSP_ERR_TRANSFER_ABORTED        = 300,      ///< The data transfer was aborted.
    SSP_ERR_MODE_FAULT              = 301,      ///< Mode fault error.
    SSP_ERR_READ_OVERFLOW           = 302,      ///< Read overflow.
    SSP_ERR_SPI_PARITY              = 303,      ///< Parity error.
    SSP_ERR_OVERRUN                 = 304,      ///< Overrun error.

    /** Start of CGC Specific */
    SSP_ERR_CLOCK_INACTIVE          = 400,      ///< Inactive clock specified as system clock.
    SSP_ERR_CLOCK_ACTIVE            = 401,      ///< Active clock source cannot be modified without stopping first.
    SSP_ERR_STABILIZED              = 402,      ///< Clock has stabilized after its been turned on/off
    SSP_ERR_NOT_STABILIZED          = 403,      ///< Clock has not stabilized after its been turned on/off
    SSP_ERR_MAIN_OSC_INACTIVE       = 404,      ///< PLL initialization attempted when main osc is turned off
    SSP_ERR_OSC_STOP_DET_ENABLED    = 405,      ///< Illegal attempt to stop LOCO when Oscillation stop is enabled
    SSP_ERR_OSC_STOP_DETECTED       = 406,      ///< The Oscillation stop detection status flag is set
    SSP_ERR_OSC_STOP_CLOCK_ACTIVE   = 407,      ///< Attempt to clear Oscillation Stop Detect Status with PLL/MAIN_OSC active
    SSP_ERR_CLKOUT_EXCEEDED         = 408,      ///< Output on target output clock pin exceeds maximum supported limit
    SSP_ERR_USB_MODULE_ENABLED      = 409,      ///< USB clock configure request with USB Module enabled
    SSP_ERR_HARDWARE_TIMEOUT        = 410,      ///< A register read or write timed out

    /** Start of FLASH Specific */
    SSP_ERR_PE_FAILURE              = 500,      ///< Unable to enter Programming mode.
    SSP_ERR_CMD_LOCKED              = 501,      ///< Peripheral in command locked state
    SSP_ERR_FCLK                    = 502,      ///< FCLK must be >= 4 MHz
    SSP_ERR_INVALID_LINKED_ADDRESS  = 503,      ///< Function or data are linked at an invalid region of memory

    /** Start of CAC Specific */
    SSP_ERR_INVALID_CAC_REF_CLOCK   = 600,      ///< Measured clock rate < reference clock rate

    /** Start of GLCD Specific */
    SSP_ERR_CLOCK_GENERATION        = 1000,     ///< Clock cannot be specified as system clock
    SSP_ERR_INVALID_TIMING_SETTING  = 1001,     ///< Invalid timing parameter
    SSP_ERR_INVALID_LAYER_SETTING   = 1002,     ///< Invalid layer parameter
    SSP_ERR_INVALID_ALIGNMENT       = 1003,     ///< Invalid memory alignment found
    SSP_ERR_INVALID_GAMMA_SETTING   = 1004,     ///< Invalid gamma correction parameter
    SSP_ERR_INVALID_LAYER_FORMAT    = 1005,     ///< Invalid color format in layer
    SSP_ERR_INVALID_UPDATE_TIMING   = 1006,     ///< Invalid timing for register update
    SSP_ERR_INVALID_CLUT_ACCESS     = 1007,     ///< Invalid access to CLUT entry
    SSP_ERR_INVALID_FADE_SETTING    = 1008,     ///< Invalid fade-in/fade-out setting

    /** Start of JPEG Specific */
    SSP_ERR_JPEG_ERR                                    = 1100,       ///< JPEG error
    SSP_ERR_JPEG_SOI_NOT_DETECTED                       = 1101,       ///< SOI not detected until EOI detected.
    SSP_ERR_JPEG_SOF1_TO_SOFF_DETECTED                  = 1102,       ///< SOF1 to SOFF detected.
    SSP_ERR_JPEG_UNSUPPORTED_PIXEL_FORMAT               = 1103,       ///< Unprovided pixel format detected.
    SSP_ERR_JPEG_SOF_ACCURACY_ERROR                     = 1104,       ///< SOF accuracy error: other than 8 detected.
    SSP_ERR_JPEG_DQT_ACCURACY_ERROR                     = 1105,       ///< DQT accuracy error: other than 0 detected.
    SSP_ERR_JPEG_COMPONENT_ERROR1                       = 1106,       ///< Component error1: the number of SOF0 header components detected is other than 1,3,or 4.
    SSP_ERR_JPEG_COMPONENT_ERROR2                       = 1107,       ///< Component error2: the number of components differs between SOF0 header and SOS.
    SSP_ERR_JPEG_SOF0_DQT_DHT_NOT_DETECTED              = 1108,       ///< SOF0, DQT, and DHT not detected when SOS detected.
    SSP_ERR_JPEG_SOS_NOT_DETECTED                       = 1109,       ///< SOS not detected: SOS not detected until EOI detected.
    SSP_ERR_JPEG_EOI_NOT_DETECTED                       = 1110,       ///< EOI not detected (default)
    SSP_ERR_JPEG_RESTART_INTERVAL_DATA_NUMBER_ERROR     = 1111,       ///< Restart interval data number error detected.
    SSP_ERR_JPEG_IMAGE_SIZE_ERROR                       = 1112,       ///< Image size error detected.
    SSP_ERR_JPEG_LAST_MCU_DATA_NUMBER_ERROR             = 1113,       ///< Last MCU data number error detected.
    SSP_ERR_JPEG_BLOCK_DATA_NUMBER_ERROR                = 1114,       ///< Block data number error detected.
    SSP_ERR_JPEG_BUFFERSIZE_NOT_ENOUGH                  = 1115,       ///< User provided buffer size not enough
    SSP_ERR_JPEG_UNSUPPORTED_IMAGE_SIZE                 = 1116,       ///< JPEG Image size is not aligned with MCU

	/** Start of touch panel framework specific */
	SSP_ERR_CALIBRATE_FAILED        = 1200,     ///< Calibration failed

	/** Start of FMI specific */
	SSP_ERR_IP_HARDWARE_NOT_PRESENT = 1400,     ///< Requested IP does not exist on this device
	SSP_ERR_IP_UNIT_NOT_PRESENT     = 1401,     ///< Requested unit does not exist on this device
	SSP_ERR_IP_CHANNEL_NOT_PRESENT  = 1402,     ///< Requested channel does not exist on this device

    /** Start of Message framework specific */
    SSP_ERR_NO_MORE_BUFFER          = 2000,     ///< No more buffer found in the memory block pool
	SSP_ERR_ILLEGAL_BUFFER_ADDRESS  = 2001,     ///< Buffer address is out of block memory pool
	SSP_ERR_INVALID_WORKBUFFER_SIZE = 2002,     ///< Work buffer size is invalid
	SSP_ERR_INVALID_MSG_BUFFER_SIZE = 2003,     ///< Message buffer size is invalid
	SSP_ERR_TOO_MANY_BUFFERS        = 2004,     ///< Number of buffer is too many
	SSP_ERR_NO_SUBSCRIBER_FOUND     = 2005,     ///< No message subscriber found
	SSP_ERR_MESSAGE_QUEUE_EMPTY     = 2006,     ///< No message found in the message queue
	SSP_ERR_MESSAGE_QUEUE_FULL      = 2007,     ///< No room for new message in the message queue
	SSP_ERR_ILLEGAL_SUBSCRIBER_LISTS= 2008,     ///< Message subscriber lists is illegal
	SSP_ERR_BUFFER_RELEASED         = 2009,     ///< Buffer has been released

    /** Start of 2DG Driver specific */
    SSP_ERR_D2D_ERROR_INIT          = 3000,     ///< Dave/2d has an error in the initialization
    SSP_ERR_D2D_ERROR_DEINIT        = 3001,     ///< Dave/2d has an error in the initialization
    SSP_ERR_D2D_ERROR_RENDERING     = 3002,     ///< Dave/2d has an error in the rendering
    SSP_ERR_D2D_ERROR_SIZE          = 3003,     ///< Dave/2d has an error in the rendering

    /** Start of BYTEQ library specific */
    SSP_ERR_QUEUE_FULL             = 10000,     ///< Queue is full, cannot queue another data
    SSP_ERR_QUEUE_EMPTY            = 10001,     ///< Queue is empty, no data to dequeue

    /** Sensor count overflowed when performing CTSU scan. @note User must clear the CTSUSCOVF bit manually. */
	SSP_ERR_CTSU_SC_OVERFLOW = 0x8010,
	/** Reference count overflowed when performing CTSU scan. @note User must clear the CTSURCOVF bit manually. */
	SSP_ERR_CTSU_RC_OVERFLOW = 0x8020,
	/** Abnormal TSCAP voltage. @note User must clear the CTSUICOMP bit manually. */
	SSP_ERR_CTSU_ICOMP = 0x8040,
	/** Auto tuning algorithm failed. */
	SSP_ERR_CTSU_OFFSET_ADJUSTMENT_FAILED = 0x8080,
	/** Safety check failed **/
	SSP_ERR_CTSU_SAFETY_CHECK_FAILED = 0x8100,

    /** Start of CTSUV2 Driver specific */
    SSP_ERR_CTSU_SCANNING              = 0x8200,    ///< Scanning.
    SSP_ERR_CTSU_NOT_GET_DATA          = 0x8201,    ///< Not processed previous scan data.
    SSP_ERR_CTSU_INCOMPLETE_TUNING     = 0x8202,    ///< Incomplete initial offset tuning.
    SSP_ERR_CTSU_DIAG_NOT_YET          = 6003,      ///< Diagnosis of data collected no yet.
    SSP_ERR_CTSU_DIAG_LDO_OVER_VOLTAGE = 6004,      ///< Diagnosis of LDO over voltage failed.
    SSP_ERR_CTSU_DIAG_CCO_HIGH         = 6005,      ///< Diagnosis of CCO into 19.2uA failed.
    SSP_ERR_CTSU_DIAG_CCO_LOW          = 6006,      ///< Diagnosis of CCO into 2.4uA failed.
    SSP_ERR_CTSU_DIAG_SSCG             = 6007,      ///< Diagnosis of SSCG frequency failed.
    SSP_ERR_CTSU_DIAG_DAC              = 6008,      ///< Diagnosis of non-touch count value failed.

    /** Start of SDMMC specific */
    SSP_ERR_CARD_INIT_FAILED     = 40000,       ///< SD card or eMMC device failed to initialize.
    SSP_ERR_CARD_NOT_INSERTED    = 40001,       ///< SD card not installed.
    SSP_ERR_SDHI_FAILED          = 40002,       ///< SD peripheral failed to respond properly.
    SSP_ERR_READ_FAILED          = 40003,       ///< Data read failed.
    SSP_ERR_CARD_NOT_READY       = 40004,       ///< SD card was removed.
    SSP_ERR_CARD_WRITE_PROTECTED = 40005,       ///< Media is write protected.
    SSP_ERR_TRANSFER_BUSY        = 40006,       ///< Transfer in progress.

    /** Start of FX_IO specific */
    SSP_ERR_MEDIA_FORMAT_FAILED  = 50000,       ///< Media format failed.
	SSP_ERR_MEDIA_OPEN_FAILED    = 50001,       ///< Media open failed.

	/** Start of CAN specific */
    SSP_ERR_CAN_DATA_UNAVAILABLE   = 60000,        ///< No data available.
    SSP_ERR_CAN_MODE_SWITCH_FAILED = 60001,        ///< Switching operation modes failed.
    SSP_ERR_CAN_INIT_FAILED        = 60002,        ///< Hardware initialization failed.
    SSP_ERR_CAN_TRANSMIT_NOT_READY = 60003,        ///< Transmit in progress.
    SSP_ERR_CAN_RECEIVE_MAILBOX    = 60004,        ///< Mailbox is setup as a receive mailbox.
    SSP_ERR_CAN_TRANSMIT_MAILBOX   = 60005,        ///< Mailbox is setup as a transmit mailbox.
    SSP_ERR_CAN_MESSAGE_LOST       = 60006,        ///< Receive message has been overwritten or overrun.

    /** Start of SF_WIFI Specific */
    SSP_ERR_WIFI_CONFIG_FAILED              = 70000,    ///< WiFi module Configuration failed.
    SSP_ERR_WIFI_INIT_FAILED                = 70001,    ///< WiFi module initialization failed.
    SSP_ERR_WIFI_TRANSMIT_FAILED            = 70002,    ///< Transmission failed
    SSP_ERR_WIFI_INVALID_MODE               = 70003,    ///< API called when provisioned in client mode
    SSP_ERR_WIFI_FAILED                     = 70004,    ///< WiFi Failed.
    SSP_ERR_WIFI_WPS_MULTIPLE_PB_SESSIONS   = 70005,    ///< Another Push button session is already in progress
    SSP_ERR_WIFI_WPS_M2D_RECEIVED           = 70006,    ///< M2D Error code received which means Registrar is unable to authenticate with the Enrollee
    SSP_ERR_WIFI_WPS_AUTHENTICATION_FAILED  = 70007,    ///< WPS authentication failed
    SSP_ERR_WIFI_WPS_CANCELLED              = 70008,    ///< WPS Request was not accepted by underlying driver
    SSP_ERR_WIFI_WPS_INVALID_PIN            = 70009,    ///< Invalid WPS Pin

    /** Start of SF_CELLULAR Specific */
    SSP_ERR_CELLULAR_CONFIG_FAILED       = 80000,     ///< Cellular module Configuration failed.
    SSP_ERR_CELLULAR_INIT_FAILED         = 80001,     ///< Cellular module initialization failed.
    SSP_ERR_CELLULAR_TRANSMIT_FAILED     = 80002,     ///< Transmission failed
    SSP_ERR_CELLULAR_FW_UPTODATE         = 80003,     ///< Firmware is uptodate
    SSP_ERR_CELLULAR_FW_UPGRADE_FAILED   = 80004,     ///< Firmware upgrade failed
    SSP_ERR_CELLULAR_FAILED              = 80005,     ///< Cellular Failed.
    SSP_ERR_CELLULAR_INVALID_STATE       = 80006,     ///< API Called in invalid state.
    SSP_ERR_CELLULAR_REGISTRATION_FAILED = 80007,     ///< Cellular Network registration failed

    /** Start of SF_BLE specific */
    SSP_ERR_BLE_FAILED              = 90001,        ///< BLE operation failed
    SSP_ERR_BLE_INIT_FAILED         = 90002,        ///< BLE device initialization failed
    SSP_ERR_BLE_CONFIG_FAILED       = 90003,        ///< BLE device configuration failed
    SSP_ERR_BLE_PRF_ALREADY_ENABLED = 90004,        ///< BLE device Profile already enabled
    SSP_ERR_BLE_PRF_NOT_ENABLED     = 90005,        ///< BLE device not enabled

    /** Start of Crypto specific (0x10000) @note Refer to sf_cryoto_err.h for Crypto error code. */
    SSP_ERR_CRYPTO_CONTINUE              = 0x10000, ///< Continue executing function
    SSP_ERR_CRYPTO_SCE_RESOURCE_CONFLICT = 0x10001, ///< Hardware resource busy */
    SSP_ERR_CRYPTO_SCE_FAIL              = 0x10002, ///< Internal I/O buffer is not empty */
    SSP_ERR_CRYPTO_SCE_HRK_INVALID_INDEX = 0x10003, ///< Invalid index */
    SSP_ERR_CRYPTO_SCE_RETRY             = 0x10004, ///< Retry
    SSP_ERR_CRYPTO_SCE_VERIFY_FAIL       = 0x10005, ///< Verify is failed
    SSP_ERR_CRYPTO_SCE_ALREADY_OPEN      = 0x10006, ///< Crypto Module is already opened
    SSP_ERR_CRYPTO_NOT_OPEN              = 0x10007, ///< Hardware module is not initialized
    SSP_ERR_CRYPTO_UNKNOWN               = 0x10008, ///< Some unknown error occurred */
    SSP_ERR_CRYPTO_NULL_POINTER          = 0x10009, ///< Null pointer input as a parameter */
    SSP_ERR_CRYPTO_NOT_IMPLEMENTED       = 0x1000a, ///< Algorithm/size not implemented */
    SSP_ERR_CRYPTO_RNG_INVALID_PARAM     = 0x1000b, ///< An invalid parameter is specified */
    SSP_ERR_CRYPTO_RNG_FATAL_ERROR       = 0x1000c, ///< A fatal error occurred */
    SSP_ERR_CRYPTO_INVALID_SIZE          = 0x1000d, ///< Size specified is invalid */
    SSP_ERR_CRYPTO_INVALID_STATE         = 0x1000e, ///< Function used in an valid state */
    SSP_ERR_CRYPTO_ALREADY_OPEN          = 0x1000f, ///< control block is already opened */
    SSP_ERR_CRYPTO_INSTALL_KEY_FAILED    = 0x10010, ///< Specified input key is invalid. */
    SSP_ERR_CRYPTO_AUTHENTICATION_FAILED = 0x10011, ///< Authentication failed */

    /** Start of SF_CRYPTO specific */
    SSP_ERR_CRYPTO_COMMON_NOT_OPENED        = 0x20000,  ///< Crypto Framework Common is not opened
    SSP_ERR_CRYPTO_HAL_ERROR                = 0x20001,  ///< Cryoto HAL module returned an error
    SSP_ERR_CRYPTO_KEY_BUF_NOT_ENOUGH       = 0x20002,  ///< Key buffer size is not enough to generate a key
    SSP_ERR_CRYPTO_BUF_OVERFLOW             = 0x20003,  ///< Attempt to write data larger than what the buffer can hold
    SSP_ERR_CRYPTO_INVALID_OPERATION_MODE   = 0x20004,  ///< Invalid operation mode.
    SSP_ERR_MESSAGE_TOO_LONG                = 0x20005,  ///< Message for RSA encryption is too long.
    SSP_ERR_RSA_DECRYPTION_ERROR            = 0x20006,  ///< RSA Decryption error.

    /** @note SF_CRYPTO APIs may return an error code starting from 0x10000 which is of Crypto module.
     *        Refer to sf_cryoto_err.h for Crypto error codes.
     */

} ssp_err_t;

/** ioctl commands. */
typedef enum e_ssp_command
{
    SSP_COMMAND_GET_SECTOR_COUNT     =1,     ///< Get media sector count.
    SSP_COMMAND_GET_SECTOR_SIZE      =2,     ///< Get sector size.
    SSP_COMMAND_GET_BLOCK_SIZE       =3,     ///< Get erase block size.
    SSP_COMMAND_CTRL_ERASE_SECTOR    =4,     ///< Erase sectors.
    SSP_COMMAND_GET_WRITE_PROTECTED  =5,     ///< Get Write Protection status.
    SSP_COMMAND_SET_BLOCK_SIZE       =6,     ///< Set block size
    SSP_COMMAND_GET_SECTOR_RELEASE   =7,     ///< Get flash sector release information.
    SSP_COMMAND_CTRL_SECTOR_RELEASE  =8,     ///< Release sectors.
} ssp_command_t;


/** Common version structure */
typedef union st_ssp_version
{
    /** Version id */
    uint32_t version_id;
    /** Code version parameters */
    /*LDRA_INSPECTED 381 S Anonymous structures and unions are allowed in SSP code. */
    struct
    {
        uint8_t code_version_minor;    ///< Code minor version
        uint8_t code_version_major;    ///< Code major version
        uint8_t api_version_minor;     ///< API minor version
        uint8_t api_version_major;     ///< API major version
    };
} ssp_version_t;

/** @} (end ingroup Common_Error_Codes) */

/***********************************************************************************************************************
 * Function prototypes
 **********************************************************************************************************************/

#endif /* SSP_COMMON_API_H */
