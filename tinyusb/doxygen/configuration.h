/** \addtogroup group_configuration
 *  @{ */

/**
  USB controller in MCU often has limited access to specific RAM section. The Stack will use this macro to place internal variables
  into the USB RAM section as follows. if your mcu's usb controller has no such limit, define TUSB_CFG_ATTR_USBRAM as empty macro.

  @code
  TUSB_CFG_ATTR_USBRAM uint8_t tinyusb_data[10];
  @endcode
 */
#define TUSB_CFG_ATTR_USBRAM


/** \defgroup TUSB_CFG_HOST Host
 *  @{ */

/// Maximum number of device host stack can manage
/// - If hub class is not enabled, set this equal to number of controllers in host mode
/// - if hub class is enabled, make sure hub is also counted
#define TUSB_CFG_HOST_DEVICE_MAX

/// Buffer size used for getting device configuration descriptor. You may want to increase this from default
/// to support lengthy composite device especially with Audio or Video class
#define TUSB_CFG_HOST_ENUM_BUFFER_SIZE

#define TUSB_CFG_HOST_HUB           ///< Enable Hub Class
#define TUSB_CFG_HOST_HID_KEYBOARD  ///< Enable HID Class for Keyboard
#define TUSB_CFG_HOST_HID_MOUSE     ///< Enable HID Class for Mouse
#define TUSB_CFG_HOST_HID_GENERIC   ///< Enable HID Class for Generic (not supported yet)
#define TUSB_CFG_HOST_MSC           ///< Enable Mass Storage Class (SCSI subclass only)
#define TUSB_CFG_HOST_CDC           ///< Enable Virtual Serial (Communication Device Class)
#define TUSB_CFG_HOST_CDC_RNDIS     ///< Enable Remote Network Device (require TUSB_CFG_HOST_CDC to be enabled)

/** @} */ // group Host



/** @} */
