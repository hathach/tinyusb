if (TOOLCHAIN STREQUAL "gcc")
  set(TOOLCHAIN_COMMON_FLAGS
    -mthumb
    -mcpu=cortex-m0plus
    -mfloat-abi=soft
    )
  set(FREERTOS_PORT GCC_ARM_CM0 CACHE INTERNAL "")

elseif (TOOLCHAIN STREQUAL "clang")
  message(FATAL_ERROR "Clang is not supported for this target")

elseif (TOOLCHAIN STREQUAL "iar")
  set(TOOLCHAIN_COMMON_FLAGS
    --cpu cortex-m0
    )
  set(FREERTOS_PORT IAR_ARM_CM0 CACHE INTERNAL "")

endif ()
