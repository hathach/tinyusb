if (TOOLCHAIN STREQUAL "gcc")
  set(TOOLCHAIN_COMMON_FLAGS
    -mthumb
    -mcpu=cortex-m7
    -mfloat-abi=hard
    -mfpu=fpv5-d16
    )
  set(FREERTOS_PORT GCC_ARM_CM7 CACHE INTERNAL "")

elseif (TOOLCHAIN STREQUAL "clang")
  set(TOOLCHAIN_COMMON_FLAGS
    --target=arm-none-eabi
    -mcpu=cortex-m7
    -mfpu=fpv5-d16
    )
  set(FREERTOS_PORT GCC_ARM_CM7 CACHE INTERNAL "")

elseif (TOOLCHAIN STREQUAL "iar")
  set(TOOLCHAIN_COMMON_FLAGS
    --cpu cortex-m7
    --fpu VFPv5_D16
    )
  set(FREERTOS_PORT IAR_ARM_CM7 CACHE INTERNAL "")

endif ()
