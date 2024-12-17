if (TOOLCHAIN STREQUAL "gcc")
  set(TOOLCHAIN_COMMON_FLAGS
    -mthumb
    -mcpu=cortex-m85
    -mfloat-abi=hard
    -mfpu=fpv5-d16
    )
  set(FREERTOS_PORT GCC_ARM_CM85_NTZ_NONSECURE CACHE INTERNAL "")

elseif (TOOLCHAIN STREQUAL "clang")
  set(TOOLCHAIN_COMMON_FLAGS
    --target=arm-none-eabi
    -mcpu=cortex-m85
    -mfpu=fpv5-d16
    )
  set(FREERTOS_PORT GCC_ARM_CM85_NTZ_NONSECURE CACHE INTERNAL "")

elseif (TOOLCHAIN STREQUAL "iar")
  set(TOOLCHAIN_COMMON_FLAGS
    --cpu cortex-m85
    --fpu VFPv5_D16
    )
  set(FREERTOS_PORT IAR_ARM_CM85_NTZ_NONSECURE CACHE INTERNAL "")

endif ()
