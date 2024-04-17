if (TOOLCHAIN STREQUAL "gcc")
  set(TOOLCHAIN_COMMON_FLAGS
    -mthumb
    -mcpu=cortex-m3
    )
  set(FREERTOS_PORT GCC_ARM_CM3 CACHE INTERNAL "")

elseif (TOOLCHAIN STREQUAL "clang")
  message(FATAL_ERROR "Clang is not supported for this target")

elseif (TOOLCHAIN STREQUAL "iar")
  set(TOOLCHAIN_COMMON_FLAGS
    --cpu cortex-m3
    )
  set(FREERTOS_PORT IAR_ARM_CM3 CACHE INTERNAL "")

endif ()
