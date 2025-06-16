if (TOOLCHAIN STREQUAL "gcc")
  set(TOOLCHAIN_COMMON_FLAGS
    -mthumb
    -mcpu=cortex-m33+nodsp
    -mfloat-abi=hard
    -mfpu=fpv5-sp-d16
    )
  set(FREERTOS_PORT GCC_ARM_CM33_NTZ_NONSECURE CACHE INTERNAL "")

elseif (TOOLCHAIN STREQUAL "clang")
  set(TOOLCHAIN_COMMON_FLAGS
    --target=arm-none-eabi
    -mcpu=cortex-m33+nodsp
    -mfpu=fpv5-sp-d16
    )
  set(FREERTOS_PORT GCC_ARM_CM33_NTZ_NONSECURE CACHE INTERNAL "")

elseif (TOOLCHAIN STREQUAL "iar")
  set(TOOLCHAIN_COMMON_FLAGS
    --cpu cortex-m33+nodsp
    --fpu VFPv5-SP
    )
  set(FREERTOS_PORT IAR_ARM_CM33_NTZ_NONSECURE CACHE INTERNAL "")

endif ()
