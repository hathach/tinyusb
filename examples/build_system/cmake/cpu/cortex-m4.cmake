if (TOOLCHAIN STREQUAL "gcc")
  set(TOOLCHAIN_COMMON_FLAGS
    -mthumb
    -mcpu=cortex-m4
    -mfloat-abi=hard
    -mfpu=fpv4-sp-d16
    )

  if (NOT DEFINED FREERTOS_PORT)
    set(FREERTOS_PORT GCC_ARM_CM4F CACHE INTERNAL "")
  endif ()

elseif (TOOLCHAIN STREQUAL "iar")
  set(TOOLCHAIN_COMMON_FLAGS
    --cpu cortex-m4
    --fpu VFPv4_sp
    )

  if (NOT DEFINED FREERTOS_PORT)
    set(FREERTOS_PORT IAR_ARM_CM4F CACHE INTERNAL "")
  endif ()

endif ()
