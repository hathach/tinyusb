if (TOOLCHAIN STREQUAL "gcc")
  set(TOOLCHAIN_COMMON_FLAGS
    -mthumb
    -mcpu=cortex-m33+nodsp
    -mfloat-abi=soft
    )

  set(FREERTOS_PORT GCC_ARM_CM33_NTZ_NONSECURE CACHE INTERNAL "")

elseif (TOOLCHAIN STREQUAL "iar")
  set(TOOLCHAIN_COMMON_FLAGS
    --cpu cortex-m33+nodsp
    )

  set(FREERTOS_PORT IAR_ARM_CM33_NTZ_NONSECURE CACHE INTERNAL "")

endif ()
