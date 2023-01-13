Jan/13/2023 13:04

The FRDM-K32L2B3 Freedom development board provides a platform for
evaluation and development of the K32 L2B MCU Family. -
https://www.nxp.com/part/FRDM-K32L2B3#/

TinyUSB does not include the board specific drivers. Those drivers
need to be extracted from the MCUXpresso IDE and SDK.

Install MCUXPresso version 11.6 or later and SDK 2.12 or later.  Then
build the example project "frdmk32l2b_hellow_worldvirual_com".

From the frdmk32l2b_hellow_worldvirual_com project copy these files to
this directory structure, in this directory:

hw/mcu/nxp/mcux-sdk/devices/K32L2B31A

CMSIS/
config/
drivers/
gcc/
fsl_device_registers.h
K32L2B31A_features.h
K32L2B31A.h

./CMSIS:
cmsis_armcc.h     cmsis_armclang_ltm.h  cmsis_gcc.h     cmsis_version.h  mpu_armv7.h
cmsis_armclang.h  cmsis_compiler.h      cmsis_iccarm.h  core_cm0plus.h

./config:
clock_config.c  clock_config.h  system_K32L2B31A.c  system_K32L2B31A.h

./drivers:
fsl_clock.c       fsl_common_arm.h  fsl_gpio.c    fsl_lpuart.h  fsl_smc.h
fsl_clock.h       fsl_common.c      fsl_gpio.h    fsl_port.h    fsl_uart.c
fsl_common_arm.c  fsl_common.h      fsl_lpuart.c  fsl_smc.c     fsl_uart.h

./gcc:
frdmk32l2b.ld          startup_k32l2b31a.c
frdmk32l2b_library.ld  frdmk32l2b_memory.ld

The linker files have been renamed and the #include directive edited
to match.

Then to build a test project change to the directory
examples/devices/cdc_msc and do:

make BOARD=frdm_k32l2b

The resulting .hex file will be found in the _build directory, copy
that will to the FRDM board to run the demo.
