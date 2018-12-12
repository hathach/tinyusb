To port tinyusb to support new MCU you need to implement all API in the
- tusb_hal.h   (mandatory for both device and host stack)
- device/dcd.h for device stack
- host/hcd.h for host stack
