if (TOOLCHAIN STREQUAL "gcc")
  set(TOOLCHAIN_COMMON_FLAGS
    -mthumb
    -mcpu=cortex-m23
    -mfloat-abi=soft
    )
  set(FREERTOS_PORT GCC_ARM_CM23_NTZ_NONSECURE CACHE INTERNAL "")

elseif (TOOLCHAIN STREQUAL "clang")
  message(FATAL_ERROR "Clang is not supported for this target")

elseif (TOOLCHAIN STREQUAL "iar")
  set(TOOLCHAIN_COMMON_FLAGS
    --cpu cortex-m23
    )
  set(FREERTOS_PORT IAR_ARM_CM23_NTZ_NONSECURE CACHE INTERNAL "")

endif ()
