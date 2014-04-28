This folder contains 
- **FreeRTOSConfig.h** configuration file for freeRTOS
- **freertos_hook.c** implemenation of freeRTOS to application hooks
- **freertos** an *unmodified copy* of the popular open source FreeRTOS. This will help to ease the upgrade to later version. However, due to Keil unable to have duplicated filenames, I have to change the name of port.c in freertos/Source/portable/RVDS for example ARM_CM3/port.c to ARM_CM3/port_cm3.c to have ability to support/switch among different mcu in one project. 