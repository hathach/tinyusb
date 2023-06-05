if (TOOLCHAIN STREQUAL "gcc")
  set(TOOLCHAIN_COMMON_FLAGS
    -mthumb
    -mcpu=cortex-m4
    -mfloat-abi=hard
    -mfpu=fpv4-sp-d16
    )

  set(FREERTOS_PORT GCC_ARM_CM4F CACHE INTERNAL "")

elseif (TOOLCHAIN STREQUAL "iar")
  set(TOOLCHAIN_COMMON_FLAGS
    --cpu cortex-m4
    --fpu VFPv4
    )

  set(FREERTOS_PORT IAR_ARM_CM4F CACHE INTERNAL "")

endif ()
