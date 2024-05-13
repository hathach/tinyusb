if (TOOLCHAIN STREQUAL "gcc")
  set(TOOLCHAIN_COMMON_FLAGS
    -mthumb
    -mcpu=cortex-m33
    -mfloat-abi=hard
    -mfpu=fpv5-sp-d16
    )
  set(FREERTOS_PORT GCC_ARM_CM33_NTZ_NONSECURE CACHE INTERNAL "")

elseif (TOOLCHAIN STREQUAL "clang")
  set(TOOLCHAIN_COMMON_FLAGS
    --target=arm-none-eabi
    -mcpu=cortex-m33
    -mfpu=fpv5-sp-d16
    )
  set(FREERTOS_PORT GCC_ARM_CM33_NTZ_NONSECURE CACHE INTERNAL "")

elseif (TOOLCHAIN STREQUAL "iar")
  set(TOOLCHAIN_COMMON_FLAGS
    --cpu cortex-m33
    --fpu VFPv5-SP
    )
  set(FREERTOS_PORT IAR_ARM_CM33_NTZ_NONSECURE CACHE INTERNAL "")

endif ()
