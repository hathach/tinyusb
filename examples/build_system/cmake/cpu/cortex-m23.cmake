if (TOOLCHAIN STREQUAL "gcc")
  set(TOOLCHAIN_COMMON_FLAGS
    -mthumb
    -mcpu=cortex-m23
    -mfloat-abi=soft
    )
  set(FREERTOS_PORT GCC_ARM_CM23_NTZ_NONSECURE CACHE INTERNAL "")

elseif (TOOLCHAIN STREQUAL "clang")
  set(TOOLCHAIN_COMMON_FLAGS
    --target=arm-none-eabi
    -mcpu=cortex-m23
    )
  set(FREERTOS_PORT GCC_ARM_CM23_NTZ_NONSECURE CACHE INTERNAL "")

elseif (TOOLCHAIN STREQUAL "iar")
  set(TOOLCHAIN_COMMON_FLAGS
    --cpu cortex-m23
    )
  set(FREERTOS_PORT IAR_ARM_CM23_NTZ_NONSECURE CACHE INTERNAL "")

endif ()
