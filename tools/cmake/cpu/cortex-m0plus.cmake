if (TOOLCHAIN STREQUAL "gcc")
  list(APPEND TOOLCHAIN_COMMON_FLAGS
    -mthumb
    -mcpu=cortex-m0plus
    -mfloat-abi=soft
    )

  set(FREERTOS_PORT GCC_ARM_CM0 CACHE INTERNAL "")
else ()
  # TODO support IAR
endif ()
