if (TOOLCHAIN STREQUAL "gcc")
  list(APPEND TOOLCHAIN_COMMON_FLAGS
    -mthumb
    -mcpu=cortex-m3
    )

  set(FREERTOS_PORT GCC_ARM_CM3 CACHE INTERNAL "")
else ()
  # TODO support IAR
endif ()
